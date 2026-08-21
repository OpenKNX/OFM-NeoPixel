#include "OneWireTimingMath.h"
#include "OneWireTimingProfile.h"

#include <assert.h>
#include <stdio.h>

static void testProfiles()
{
    const OneWireTimingProfile& ws2812 = getOneWireTimingProfile(LedProtocol::WS2812B);
    assert(ws2812.bitRateHz == 800000 && ws2812.resetTimeUs == 300 && !ws2812.inverted);
    const OneWireTimingProfile& ws2811 = getOneWireTimingProfile(LedProtocol::WS2811);
    assert(ws2811.bitRateHz == 400000 && ws2811.pioCadence == OneWirePioCadence::SIX_STEP);
    const OneWireTimingProfile& rgbcct = getOneWireTimingProfile(LedProtocol::SK6812_RGBCCT);
    assert(rgbcct.channelCount == 5 && rgbcct.defaultColorOrder == ColorOrder::GRBCCT);
    const OneWireTimingProfile& tm1814 = getOneWireTimingProfile(LedProtocol::TM1814);
    assert(tm1814.inverted);
}

static void testColorOrderCompatibility()
{
    assert(ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRB));
    assert(ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRBW));
    assert(!ProtocolHelper::isColorOrderCompatible(LedProtocol::WS2812B, ColorOrder::GRBW));
    assert(!ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRBCCT));
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::WS2812B, ColorOrder::GRBW) == ColorOrder::GRB);
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::APA102, ColorOrder::GRBW) == ColorOrder::BGR);
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::WS2805_RGBCCT, ColorOrder::GRBCCT) == ColorOrder::GRBCCT);
}

static void testQuantization()
{
    OneWireBalancedSymbolTicks symbols = {};
    assert(oneWireMakeBalancedSymbols(400, 850, 800, 450, 40000000U, 32767U, symbols));
    assert(symbols.period == 50 && symbols.zeroHigh == 16 && symbols.zeroLow == 34);
    assert(symbols.oneHigh == 32 && symbols.oneLow == 18);
    assert(!oneWireMakeBalancedSymbols(1250, 0, 1250, 0, 40000000U, 32767U, symbols));
    uint16_t ticks = 0;
    assert(oneWireDurationToTicks(13, 40000000U, 32767U, ticks) && ticks == 1);
    assert(!oneWireDurationToTicks(1000000, 40000000U, 32767U, ticks));
}

static void testDeadlines()
{
    // 3 RGB bytes at 800 kHz: 30 us payload + 30 us final word + 300 us reset + 2 ms margin.
    assert(oneWireTransferDeadlineUs(3, 800000U, 24, 300U) == 2360U);
    assert(oneWireFinalWordDrainUs(24, 800000U) == 31U);
    assert(oneWireTransferDeadlineUs(0, 800000U, 24, 300U) == 1000000U);
    assert(oneWireTransferDeadlineUs(UINT32_MAX, 1U, 32U, 300U) == UINT32_MAX);
    assert(oneWireDeadlineReached(2U, 0xfffffffeU));
    assert(!oneWireDeadlineReached(0xfffffffeU, 2U));
}

static void testExactPacking()
{
    const uint8_t rgb[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
    assert(oneWirePackedWordCount(sizeof(rgb), 3) == 2);
    assert(oneWirePackedWordAt(rgb, 3, 0) == 0x12345600U);
    assert(oneWirePackedWordAt(rgb, 3, 1) == 0x789abc00U);

    const uint8_t rgbw[] = {0x12, 0x34, 0x56, 0x78};
    assert(oneWirePackedWordCount(sizeof(rgbw), 4) == 1);
    assert(oneWirePackedWordAt(rgbw, 4, 0) == 0x12345678U);

    const uint8_t rgbcct[] = {0x12, 0x34, 0x56, 0x78, 0x9a};
    assert(oneWirePackedWordCount(sizeof(rgbcct), 5) == sizeof(rgbcct));
    for (size_t i = 0; i < sizeof(rgbcct); ++i)
        assert(oneWirePackedWordAt(rgbcct, 5, i) == ((uint32_t)rgbcct[i] << 24));
}

int main()
{
    testProfiles();
    testColorOrderCompatibility();
    testQuantization();
    testDeadlines();
    testExactPacking();
    puts("one-wire timing regression tests passed");
    return 0;
}
