#pragma once
/**
 * @file        SerialTimingProfile.h
 * @brief       Bit-timing math shared by the 1-wire backends (PIO and RMT)
 *
 * One source of truth for turning a protocol timing profile in nanoseconds into
 * hardware settings. Both backends resolve through here so a strip driven by PIO
 * and the same strip driven by RMT produce the same waveform.
 *
 * @copyright Copyright (c) 2026 Erkan Colak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include "IHardwareDriver.h"
#include <stdint.h>

namespace SerialTiming
{
    /**
     * @brief Protocol bit timing in nanoseconds plus latch and polarity
     * @note inverted covers chips that idle high and send a complemented waveform (TM1814).
     */
    struct Profile
    {
        uint16_t t0h;     ///< 0-bit high time (ns)
        uint16_t t0l;     ///< 0-bit low time (ns)
        uint16_t t1h;     ///< 1-bit high time (ns)
        uint16_t t1l;     ///< 1-bit low time (ns)
        uint32_t resetUs; ///< latch time after the last bit (us)
        bool     inverted;///< true = output drives the complemented waveform
    };

    /**
     * @brief Nominal bit period of a profile (ns)
     * @note Averages both halves: a datasheet may round them apart by a few ns.
     */
    inline uint32_t bitPeriodNs(const Profile& p)
    {
        const uint32_t zero = (uint32_t)p.t0h + p.t0l;
        const uint32_t one  = (uint32_t)p.t1h + p.t1l;
        return (zero + one + 1) / 2;
    }

    /**
     * @brief Whether both halves of a profile agree on the bit period
     * @note A profile that fails this cannot be driven by any NRZ backend.
     */
    inline bool isCoherent(const Profile& p, uint32_t toleranceNs = 150)
    {
        const int32_t zero = (int32_t)p.t0h + p.t0l;
        const int32_t one  = (int32_t)p.t1h + p.t1l;
        const int32_t diff = zero > one ? zero - one : one - zero;
        return (uint32_t)diff <= toleranceNs;
    }

    inline uint32_t divRound(uint32_t value, uint32_t divisor)
    {
        return divisor ? (value + divisor / 2) / divisor : 0;
    }

    // =========================================================================
    // RMT (ESP32) - symbol durations in ticks
    // =========================================================================

    /**
     * @brief Symbol durations in RMT ticks
     * @note bit is the shared period: t0h+t0l and t1h+t1l both equal it by construction.
     */
    struct Ticks
    {
        uint16_t t0h;
        uint16_t t0l;
        uint16_t t1h;
        uint16_t t1l;
        uint16_t bit;
    };

    /**
     * @brief Convert a profile to RMT ticks with equal 0-bit and 1-bit periods
     *
     * Rounds the bit period ONCE and derives both low times from it. Rounding the
     * four values independently lets the two halves land on different periods,
     * which is a protocol violation rather than a rounding error.
     *
     * @param p       protocol profile
     * @param tickNs  duration of one RMT tick in ns (resolution dependent)
     * @param maxTicks largest value the RMT duration field can hold
     * @return ticks with every field clamped to at least 1
     */
    inline Ticks toTicks(const Profile& p, uint32_t tickNs, uint16_t maxTicks = 32767)
    {
        Ticks t{};
        if (tickNs == 0) tickNs = 1;

        uint32_t bit = divRound(bitPeriodNs(p), tickNs);
        if (bit < 2) bit = 2; // one tick per half is the floor
        if (bit > maxTicks) bit = maxTicks;

        uint32_t t0h = divRound(p.t0h, tickNs);
        uint32_t t1h = divRound(p.t1h, tickNs);

        // Every half needs at least one tick: a zero duration is the RMT end marker.
        if (t0h < 1) t0h = 1;
        if (t1h < 1) t1h = 1;
        if (t0h > bit - 1) t0h = bit - 1;
        if (t1h > bit - 1) t1h = bit - 1;

        t.bit = (uint16_t)bit;
        t.t0h = (uint16_t)t0h;
        t.t1h = (uint16_t)t1h;
        t.t0l = (uint16_t)(bit - t0h);
        t.t1l = (uint16_t)(bit - t1h);
        return t;
    }

    // =========================================================================
    // PIO (RP2040 / RP2350) - instruction delays plus an integer clock divider
    // =========================================================================

    /// Largest delay a PIO instruction can carry with one side-set bit and no side-set enable.
    static constexpr uint8_t kMaxPioDelay = 15;
    /// Therefore the longest a single timing segment can be, in PIO cycles.
    static constexpr uint8_t kMaxSegmentCycles = kMaxPioDelay + 1;

    /**
     * @brief Solved PIO parameters for one profile
     *
     * Segment cycles map onto the 4-instruction program as
     * T0H = a, T1H = a + b, T1L = c, T0L = b + c, bit = a + b + c.
     */
    struct PioSolution
    {
        uint8_t  a;             ///< high cycles common to both bit values
        uint8_t  b;             ///< extra high cycles that make a 1-bit
        uint8_t  c;             ///< trailing low cycles
        uint16_t clkdiv;        ///< integer clock divider (no fractional dither)
        uint16_t cyclesPerBit;  ///< a + b + c
        uint32_t realizedT0h;   ///< ns actually produced
        uint32_t realizedT1h;   ///< ns actually produced
        uint32_t realizedBit;   ///< ns actually produced
        bool     valid;
    };

    /// Index of the two jmp instructions inside the encoded program.
    static constexpr uint8_t kJmpNotXIndex = 1;
    static constexpr uint8_t kJmpWrapIndex = 2;

    /**
     * @brief Relocate the jmp targets of an encoded program to a load offset
     *
     * pio_add_program does this when it loads a program. Writing instruction memory
     * directly does not, and an unrelocated jmp sends the state machine into whatever
     * program sits at address 0.
     */
    inline void relocateProgram(uint16_t words[4], uint8_t offset)
    {
        words[kJmpNotXIndex] = (uint16_t)((words[kJmpNotXIndex] & 0xFFE0) | ((offset + 3) & 0x1F));
        words[kJmpWrapIndex] = (uint16_t)((words[kJmpWrapIndex] & 0xFFE0) | (offset & 0x1F));
    }

    /**
     * @brief Encode the 4 program words for a solution
     * @note Targets are relative to program start; call relocateProgram when writing
     *       instruction memory directly.
     */
    inline void encodeProgram(const PioSolution& s, uint16_t out[4])
    {
        const uint8_t d0 = (uint8_t)(s.c - 1); // trailing low, side 0
        const uint8_t d1 = (uint8_t)(s.a - 1); // common high, side 1
        const uint8_t d2 = (uint8_t)(s.b - 1); // extra high for a 1-bit, side 1
        const uint8_t d3 = d2;                 // 0-bit low stub must match d2, else the periods differ

        out[0] = (uint16_t)(0x6021 | ((uint16_t)d0 << 8)); // out x, 1     side 0 [d0]
        out[1] = (uint16_t)(0x1023 | ((uint16_t)d1 << 8)); // jmp !x, 3    side 1 [d1]
        out[2] = (uint16_t)(0x1000 | ((uint16_t)d2 << 8)); // jmp 0        side 1 [d2]
        out[3] = (uint16_t)(0xa042 | ((uint16_t)d3 << 8)); // nop          side 0 [d3]
    }

    /**
     * @brief Find integer clkdiv and segment cycles that best reproduce a profile
     *
     * Searches every divider that keeps all three segments within the PIO delay
     * range and scores candidates on the combined T0H, T1H and bit-period error.
     * An integer divider is required: a fractional one dithers the bit period.
     *
     * @param p         protocol profile
     * @param sysClkHz  system clock feeding the PIO
     * @return solution with valid=false when no divider fits
     */
    inline PioSolution solvePio(const Profile& p, uint32_t sysClkHz)
    {
        PioSolution best{};
        best.valid = false;
        if (sysClkHz == 0) return best;

        const uint32_t bitNs = bitPeriodNs(p);
        if (bitNs == 0) return best;

        uint64_t bestScore = ~(uint64_t)0;

        for (uint32_t div = 1; div <= 65535; ++div)
        {
            // Cycle length in picoseconds: integer math, no floats.
            const uint64_t cycPs = ((uint64_t)div * 1000000000000ULL) / sysClkHz;
            if (cycPs == 0) continue;

            const uint32_t total = (uint32_t)(((uint64_t)bitNs * 1000 + cycPs / 2) / cycPs);
            if (total < 3) break; // dividers only get coarser from here
            if (total > 3u * kMaxSegmentCycles) continue;

            uint32_t a = (uint32_t)(((uint64_t)p.t0h * 1000 + cycPs / 2) / cycPs);
            uint32_t ab = (uint32_t)(((uint64_t)p.t1h * 1000 + cycPs / 2) / cycPs);
            if (a < 1) a = 1;
            if (ab <= a) ab = a + 1;
            const uint32_t b = ab - a;
            if (total <= ab) continue;
            const uint32_t c = total - ab;

            if (a > kMaxSegmentCycles || b > kMaxSegmentCycles || c > kMaxSegmentCycles) continue;

            const uint32_t gotT0h = (uint32_t)((a * cycPs) / 1000);
            const uint32_t gotT1h = (uint32_t)((ab * cycPs) / 1000);
            const uint32_t gotBit = (uint32_t)((total * cycPs) / 1000);

            const uint64_t e0 = gotT0h > p.t0h ? gotT0h - p.t0h : p.t0h - gotT0h;
            const uint64_t e1 = gotT1h > p.t1h ? gotT1h - p.t1h : p.t1h - gotT1h;
            const uint64_t eb = gotBit > bitNs ? gotBit - bitNs : bitNs - gotBit;

            // Bit period carries the most weight: it is what a clone chip samples against.
            const uint64_t score = e0 * 2 + e1 * 2 + eb * 3;
            if (score < bestScore)
            {
                bestScore = score;
                best.a = (uint8_t)a;
                best.b = (uint8_t)b;
                best.c = (uint8_t)c;
                best.clkdiv = (uint16_t)div;
                best.cyclesPerBit = (uint16_t)total;
                best.realizedT0h = gotT0h;
                best.realizedT1h = gotT1h;
                best.realizedBit = gotBit;
                best.valid = true;
            }
        }

        return best;
    }
    // =========================================================================
    // Per-protocol profiles
    // =========================================================================

    /**
     * @brief Bit timing and latch time for a 1-wire protocol
     *
     * Values follow NeoPixelBus/WLED, which drive the same chips in the field at
     * large scale. Where a datasheet is stricter the WLED value is kept: it is the
     * one with proven tolerance against clone chips.
     *
     * @param protocol LED protocol
     * @return profile; SPI protocols return a zeroed profile (no NRZ bit timing)
     */
    inline Profile profileFor(LedProtocol protocol)
    {
        switch (protocol)
        {
            // SK6812, datasheet SPC/SK6812 Rev.06: T0H 200..400, T0L >=800, T1H 580..1000,
            // T1L >=200, code period >=1200, Trst >80.
            case LedProtocol::SK6812:
            case LedProtocol::SK6812_RGBCCT:
                return { 400, 850, 800, 450, 80, false };

            // SK6805 has its own, tighter window (SPC/SK6805-2427 Rev.01): T0H 150..450,
            // T0L 750..1050, T1H 450..750, T1L 450..750. The SK6812 profile above exceeds
            // T1H max by 50 ns, so this family cannot share one entry.
            case LedProtocol::SK6805:
                return { 350, 900, 650, 600, 80, false };

            // WS2814B datasheet: T0H 220..380, T0L 580..1000, T1H 580..1000, T1L 580..1000,
            // data cycle >=1250, RES >=280. Despite the name this is not WS2812 timing - the
            // WS2812 profile misses T0H max by 20 ns and T1L min by 130 ns.
            case LedProtocol::WS2814:
            case LedProtocol::WS2814_RGBCCT:
                return { 300, 950, 670, 580, 300, false };

            // NeoPixelBus Ws2811, 800 kHz variant.
            case LedProtocol::WS2811:
                return { 300, 950, 900, 350, 300, false };

            // Legacy WS2811/WS2812 timing, 400 kHz.
            case LedProtocol::WS2811_400KHZ:
                return { 500, 2000, 1200, 1300, 50, false };

            // WS2805 V0.3: T0H 220..380 ns, T0L/T1H 580..1000 ns and T1L
            // 220..420 ns. Keep the four-step 800 kbit/s waveform used by the
            // hardware-verified TI build. RP PIO realizes 312.5/937.5 ns and
            // 937.5/312.5 ns with an exact fractional divider.
            case LedProtocol::WS2805_RGBCCT:
                return { 312, 938, 938, 312, 300, false };

            // TM1814: inverted line and C1/C2 constant-current prefix.
            case LedProtocol::TM1814:
                return { 360, 890, 720, 530, 200, true };

            // NeoPixelBus Gs1903 timing; GS8208 is the 12 V part of that family.
            case LedProtocol::GS8208:
                return { 300, 900, 900, 300, 300, false };

            // NeoPixelBus Tm1829: inverted line, BRG order.
            case LedProtocol::TM1829:
                return { 300, 900, 800, 400, 200, true };

            // NeoPixelBus Tm1914 shares the Tm1814 waveform.
            case LedProtocol::TM1914:
                return { 360, 890, 720, 530, 200, true };

            // NeoPixelBus Apa106: 1700 ns bit, PL9823 equivalent.
            case LedProtocol::APA106:
                return { 350, 1350, 1350, 350, 50, false };

            // 16-bit and 6-channel parts clock WS2812x bit timing, only the frame is wider
            // (WLED drives all four through its Ws2813 method).
            case LedProtocol::UCS8903:
            case LedProtocol::UCS8904:
            case LedProtocol::SM16825:
            case LedProtocol::FW1906:
                return { 400, 850, 800, 450, 300, false };

            // WS2812/WS2812B/WS2813/WS2815 all run the NeoPixelBus Ws2812x profile.
            // The 300 us latch is what WS2813 and WS2815 need; it is harmless for the others.
            case LedProtocol::WS2812:
            case LedProtocol::WS2812B:
            case LedProtocol::WS2813:
            case LedProtocol::WS2815:
                return { 400, 850, 800, 450, 300, false };

            default:
                return { 0, 0, 0, 0, 0, false };
        }
    }

    /**
     * @brief Whether a protocol has a usable NRZ profile
     */
    inline bool hasProfile(LedProtocol protocol)
    {
        return profileFor(protocol).t1h > 0;
    }

    /**
     * @brief Scale a profile to a different bit rate, keeping its pulse ratios
     * @note Backs the expert bit-rate override: the chip profile is bent, not replaced.
     */
    inline Profile scaledTo(const Profile& p, uint32_t bitrateHz)
    {
        if (bitrateHz == 0) return p;
        const uint32_t targetBit = 1000000000UL / bitrateHz;
        const uint32_t nominal   = bitPeriodNs(p);
        if (nominal == 0) return p;

        Profile out = p;
        out.t0h = (uint16_t)divRound((uint32_t)p.t0h * targetBit, nominal);
        out.t0l = (uint16_t)divRound((uint32_t)p.t0l * targetBit, nominal);
        out.t1h = (uint16_t)divRound((uint32_t)p.t1h * targetBit, nominal);
        out.t1l = (uint16_t)divRound((uint32_t)p.t1l * targetBit, nominal);
        return out;
    }
    namespace detail
    {
        constexpr uint16_t relocated(uint16_t word, uint8_t offset, uint8_t target)
        {
            return (uint16_t)((word & 0xFFE0) | ((offset + target) & 0x1F));
        }
    }

    // A jmp that keeps its unrelocated target sends the state machine into whatever program
    // sits at address 0. Lock the arithmetic down here rather than at the call site.
    static_assert(detail::relocated(0x1023, 0, 3) == 0x1023, "offset 0 leaves the target alone");
    static_assert(detail::relocated(0x1023, 4, 3) == 0x1027, "jmp !x follows the load offset");
    static_assert(detail::relocated(0x1000, 4, 0) == 0x1004, "wrap jmp follows the load offset");
    static_assert((detail::relocated(0x1223, 4, 3) & 0xFFE0) == (0x1223 & 0xFFE0),
                  "relocation must not touch delay or side-set bits");
} // namespace SerialTiming
