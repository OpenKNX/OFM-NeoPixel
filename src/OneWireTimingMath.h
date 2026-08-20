/**
 * @file OneWireTimingMath.h
 * @brief Platform-independent calculations shared by clockless LED backends.
 *
 * Keep these helpers free of Arduino, PIO and ESP-IDF dependencies so the
 * timing and byte-stream invariants can be regression-tested on a host.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

struct OneWireBalancedSymbolTicks
{
    uint16_t zeroHigh;
    uint16_t zeroLow;
    uint16_t oneHigh;
    uint16_t oneLow;
    uint16_t period;
};

inline uint32_t oneWireSaturateUs(uint64_t value)
{
    return value > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(value);
}

inline uint32_t oneWireCeilDurationUs(uint32_t bits, uint32_t bitRateHz)
{
    if (bits == 0 || bitRateHz == 0) return 0;
    return oneWireSaturateUs(((uint64_t)bits * 1000000ULL + bitRateHz - 1ULL) / bitRateHz);
}

inline uint32_t oneWireTransferDeadlineUs(size_t payloadBytes, uint32_t bitRateHz,
                                          uint32_t finalWordBits, uint32_t resetTimeUs,
                                          uint32_t schedulerMarginUs = 2000U)
{
    if (payloadBytes == 0 || bitRateHz == 0) return 1000000U;
    const uint64_t payloadUs = ((uint64_t)payloadBytes * 8ULL * 1000000ULL + bitRateHz - 1ULL) / bitRateHz;
    const uint64_t finalWordUs = ((uint64_t)finalWordBits * 1000000ULL + bitRateHz - 1ULL) / bitRateHz;
    return oneWireSaturateUs(payloadUs + finalWordUs + resetTimeUs + schedulerMarginUs);
}

inline uint32_t oneWireFinalWordDrainUs(uint32_t finalWordBits, uint32_t bitRateHz)
{
    if (finalWordBits == 0 || bitRateHz == 0) return 0;
    // One extra microsecond covers output-shift/observation jitter after the
    // rounded serial duration.
    const uint64_t drainUs = ((uint64_t)finalWordBits * 1000000ULL + bitRateHz - 1ULL) / bitRateHz;
    return oneWireSaturateUs(drainUs + 1ULL);
}

/** True when now has reached deadline, including a uint32_t micros() rollover. */
inline bool oneWireDeadlineReached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

inline bool oneWireDurationToTicks(uint32_t durationNs, uint32_t resolutionHz,
                                   uint16_t maxTicks, uint16_t& ticks)
{
    if (durationNs == 0 || resolutionHz == 0) return false;
    uint64_t rounded = ((uint64_t)durationNs * resolutionHz + 500000000ULL) / 1000000000ULL;
    if (rounded == 0) rounded = 1;
    if (rounded > maxTicks) return false;
    ticks = static_cast<uint16_t>(rounded);
    return true;
}

inline bool oneWireMakeBalancedSymbols(uint16_t t0hNs, uint16_t t0lNs,
                                       uint16_t t1hNs, uint16_t t1lNs,
                                       uint32_t resolutionHz, uint16_t maxTicks,
                                       OneWireBalancedSymbolTicks& symbols)
{
    const uint32_t targetPeriodNs = ((uint32_t)t0hNs + t0lNs + t1hNs + t1lNs + 1U) / 2U;
    if (!oneWireDurationToTicks(targetPeriodNs, resolutionHz, maxTicks, symbols.period) ||
        !oneWireDurationToTicks(t0hNs, resolutionHz, maxTicks, symbols.zeroHigh) ||
        !oneWireDurationToTicks(t1hNs, resolutionHz, maxTicks, symbols.oneHigh) ||
        symbols.zeroHigh >= symbols.period || symbols.oneHigh >= symbols.period)
    {
        return false;
    }
    symbols.zeroLow = symbols.period - symbols.zeroHigh;
    symbols.oneLow = symbols.period - symbols.oneHigh;
    return true;
}

inline size_t oneWirePackedWordCount(size_t payloadBytes, uint8_t bytesPerLed)
{
    if (bytesPerLed == 5) return payloadBytes;
    return bytesPerLed > 0 && (payloadBytes % bytesPerLed) == 0 ? payloadBytes / bytesPerLed : 0;
}

/** Return one PIO FIFO word with the first serial byte in bits 31..24. */
inline uint32_t oneWirePackedWordAt(const uint8_t* payload, uint8_t bytesPerLed, size_t wordIndex)
{
    if (!payload) return 0;
    if (bytesPerLed == 5) return (uint32_t)payload[wordIndex] << 24;

    const size_t offset = wordIndex * bytesPerLed;
    if (bytesPerLed == 3)
        return ((uint32_t)payload[offset] << 24) | ((uint32_t)payload[offset + 1] << 16) |
               ((uint32_t)payload[offset + 2] << 8);
    if (bytesPerLed == 4)
        return ((uint32_t)payload[offset] << 24) | ((uint32_t)payload[offset + 1] << 16) |
               ((uint32_t)payload[offset + 2] << 8) | payload[offset + 3];
    return 0;
}
