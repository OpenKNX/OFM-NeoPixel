/**
 * @file OneWireTimingProfile.h
 * @brief Authoritative timing, polarity and latch profiles for clockless LED ICs.
 *
 * A profile describes the signal at the LED input. Backends may quantise it to
 * their hardware grid, but must retain the profile's common bit-cell duration
 * and reset interval. Keep protocol facts here instead of duplicating them in
 * the PIO, RMT, console and ETS layers.
 */

#pragma once

#include "IHardwareDriver.h"
#include <stdint.h>

enum class OneWirePioCadence : uint8_t
{
    CANONICAL_10, // T0H:T0L:T1H:T1L = 3:7:6:4
    THREE_STEP,   // short/long HIGH = 1/3 and 2/3 of one bit cell
    FOUR_STEP,    // short/long HIGH = 1/4 and 3/4 of one bit cell
    SIX_STEP      // short/long HIGH = 1/6 and 3/6 of one bit cell
};

struct OneWireTimingProfile
{
    const char* name;
    uint32_t bitRateHz;
    uint16_t t0hNs;
    uint16_t t0lNs;
    uint16_t t1hNs;
    uint16_t t1lNs;
    uint32_t resetTimeUs;
    bool inverted;
    OneWirePioCadence pioCadence;
    uint8_t channelCount;
    uint8_t bytesPerChannel;
    uint8_t frameSettingsBytes;
    ColorOrder defaultColorOrder;
    bool refreshRequired;
};

/** A portable, full one-wire waveform generated for a requested bitrate. */
struct OneWireTimingOverride
{
    uint16_t t0hNs;
    uint16_t t0lNs;
    uint16_t t1hNs;
    uint16_t t1lNs;
};

/**
 * Convert a requested bitrate to the waveform represented by a profile's PIO
 * cadence. This makes an override mean the same serial clock on RMT and PIO:
 * RMT applies all four values, while PIO derives its divider from T1H and
 * retains the profile-specific cadence.
 */
inline bool oneWireMakeBitrateOverride(const OneWireTimingProfile& profile,
                                       uint32_t bitRateHz,
                                       OneWireTimingOverride& timing)
{
    if (bitRateHz == 0) return false;

    const uint64_t bitCellNs = (1000000000ULL + bitRateHz / 2U) / bitRateHz;
    if (bitCellNs < 2U || bitCellNs > UINT16_MAX) return false;

    uint8_t denominator = 10;
    uint8_t zeroHighNumerator = 3;
    uint8_t oneHighNumerator = 6;
    switch (profile.pioCadence)
    {
        case OneWirePioCadence::THREE_STEP:
            denominator = 3;
            zeroHighNumerator = 1;
            oneHighNumerator = 2;
            break;
        case OneWirePioCadence::FOUR_STEP:
            denominator = 4;
            zeroHighNumerator = 1;
            oneHighNumerator = 3;
            break;
        case OneWirePioCadence::SIX_STEP:
            denominator = 6;
            zeroHighNumerator = 1;
            oneHighNumerator = 3;
            break;
        case OneWirePioCadence::CANONICAL_10:
        default:
            break;
    }

    const uint32_t periodNs = static_cast<uint32_t>(bitCellNs);
    const uint32_t t0hNs = periodNs * zeroHighNumerator / denominator;
    // FOUR_STEP encodes one as the complement of zero. Preserve that
    // relationship after integer conversion so WS2805 at 800 kHz remains the
    // exact 312/938 ns profile instead of becoming a 937/313 ns variant.
    const uint32_t t1hNs = (profile.pioCadence == OneWirePioCadence::FOUR_STEP)
                               ? periodNs - t0hNs
                               : periodNs * oneHighNumerator / denominator;
    if (t0hNs == 0 || t1hNs == 0 || t0hNs >= periodNs || t1hNs >= periodNs)
        return false;

    timing.t0hNs = static_cast<uint16_t>(t0hNs);
    timing.t0lNs = static_cast<uint16_t>(periodNs - t0hNs);
    timing.t1hNs = static_cast<uint16_t>(t1hNs);
    timing.t1lNs = static_cast<uint16_t>(periodNs - t1hNs);
    return true;
}

/**
 * Return the signal profile for a supported clockless LED protocol.
 *
 * Values are deliberately conservative where vendors publish broad timing
 * windows. The selected profile is also the one printed and measured by the
 * diagnostics work; protocol additions must be made here first.
 */
inline const OneWireTimingProfile& getOneWireTimingProfile(LedProtocol protocol)
{
    static constexpr OneWireTimingProfile ws2812x = {
        "WS2812x", 800000, 400, 850, 800, 450, 300, false, OneWirePioCadence::CANONICAL_10,
        3, 1, 0, ColorOrder::GRB, false};
    static constexpr OneWireTimingProfile sk6812_rgbw = {
        "SK6812/SK6805", 800000, 400, 850, 800, 450, 80, false, OneWirePioCadence::CANONICAL_10,
        4, 1, 0, ColorOrder::GRBW, false};
    static constexpr OneWireTimingProfile sk6812_rgbcct = {
        "SK6812/WS2814 RGBCCT", 800000, 400, 850, 800, 450, 80, false, OneWirePioCadence::CANONICAL_10,
        5, 1, 0, ColorOrder::GRBCCT, false};
    static constexpr OneWireTimingProfile ws2811 = {
        "WS2811", 800000, 300, 950, 900, 350, 300, false, OneWirePioCadence::FOUR_STEP,
        3, 1, 0, ColorOrder::RGB, false};
    static constexpr OneWireTimingProfile ws2811_400 = {
        "WS2811-400", 400000, 500, 2000, 1200, 1300, 50, false, OneWirePioCadence::SIX_STEP,
        3, 1, 0, ColorOrder::GRB, false};
    static constexpr OneWireTimingProfile ws2805 = {
        // WS2805 needs a 1.25 us bit cell. The former 917 kHz profile produced
        // a 1.09 us cell and caused periodic colour corruption and white flashes.
        "WS2805", 800000, 312, 938, 938, 312, 300, false, OneWirePioCadence::FOUR_STEP,
        5, 1, 0, ColorOrder::RGBCCT, false};
    static constexpr OneWireTimingProfile sm16825 = {
        // SM16825 uses the WS2812x waveform, 16-bit MSB-first channels and
        // a four-byte current-control trailer at the end of each frame.
        "SM16825", 800000, 400, 850, 800, 450, 300, false, OneWirePioCadence::CANONICAL_10,
        5, 2, 4, ColorOrder::RGBCTW, false};
    static constexpr OneWireTimingProfile tm1814 = {
        "TM1814", 800000, 360, 890, 720, 530, 200, true, OneWirePioCadence::THREE_STEP,
        4, 1, 0, ColorOrder::GRBW, false};

    switch (protocol)
    {
        case LedProtocol::SK6812:
        case LedProtocol::SK6805:
        case LedProtocol::WS2814:
            return sk6812_rgbw;

        case LedProtocol::SK6812_RGBCCT:
        case LedProtocol::WS2814_RGBCCT:
            return sk6812_rgbcct;

        case LedProtocol::WS2811:
            return ws2811;

        case LedProtocol::WS2811_400KHZ:
            return ws2811_400;

        case LedProtocol::WS2805_RGBCCT:
            return ws2805;

        case LedProtocol::SM16825:
            return sm16825;

        case LedProtocol::TM1814:
            return tm1814;

        case LedProtocol::WS2812:
        case LedProtocol::WS2812B:
        case LedProtocol::WS2813:
        case LedProtocol::WS2815:
        case LedProtocol::GS8208:
        default:
            return ws2812x;
    }
}
