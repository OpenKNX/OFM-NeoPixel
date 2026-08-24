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
    ColorOrder defaultColorOrder;
    bool refreshRequired;
};

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
        3, ColorOrder::GRB, false};
    static constexpr OneWireTimingProfile sk6812_rgbw = {
        "SK6812/SK6805", 800000, 400, 850, 800, 450, 80, false, OneWirePioCadence::CANONICAL_10,
        4, ColorOrder::GRBW, false};
    static constexpr OneWireTimingProfile sk6812_rgbcct = {
        "SK6812/WS2814 RGBCCT", 800000, 400, 850, 800, 450, 80, false, OneWirePioCadence::CANONICAL_10,
        5, ColorOrder::GRBCCT, false};
    static constexpr OneWireTimingProfile ws2811_400 = {
        "WS2811-400", 400000, 500, 2000, 1200, 1300, 50, false, OneWirePioCadence::SIX_STEP,
        3, ColorOrder::RGB, false};
    static constexpr OneWireTimingProfile ws2805 = {
        // WS2805 needs a 1.25 us bit cell. The former 917 kHz profile produced
        // a 1.09 us cell and caused periodic colour corruption and white flashes.
        "WS2805", 800000, 312, 938, 938, 312, 300, false, OneWirePioCadence::FOUR_STEP,
        5, ColorOrder::GRBCCT, false};
    static constexpr OneWireTimingProfile tm1814 = {
        "TM1814", 800000, 360, 890, 720, 530, 200, true, OneWirePioCadence::THREE_STEP,
        4, ColorOrder::GRBW, false};

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
            return ws2811_400;

        case LedProtocol::WS2805_RGBCCT:
            return ws2805;

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
