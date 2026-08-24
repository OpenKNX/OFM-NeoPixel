#include "OneWireTimingMath.h"
#include "OneWireTimingProfile.h"
#include "SpiFrameMath.h"

#include <assert.h>
#include <stdio.h>

static void testProfiles()
{
    const OneWireTimingProfile& ws2812 = getOneWireTimingProfile(LedProtocol::WS2812B);
    assert(ws2812.bitRateHz == 800000 && ws2812.resetTimeUs == 300 && !ws2812.inverted);
    const OneWireTimingProfile& ws2811 = getOneWireTimingProfile(LedProtocol::WS2811);
    assert(ws2811.bitRateHz == 800000 && ws2811.pioCadence == OneWirePioCadence::FOUR_STEP);
    assert(ws2811.t0hNs == 300 && ws2811.t0lNs == 950);
    assert(ws2811.t1hNs == 900 && ws2811.t1lNs == 350);
    assert(ws2811.defaultColorOrder == ColorOrder::RGB);
    const OneWireTimingProfile& ws2811Legacy = getOneWireTimingProfile(LedProtocol::WS2811_400KHZ);
    assert(ws2811Legacy.bitRateHz == 400000 && ws2811Legacy.pioCadence == OneWirePioCadence::SIX_STEP);
    assert(ws2811Legacy.defaultColorOrder == ColorOrder::GRB);
    const OneWireTimingProfile& rgbcct = getOneWireTimingProfile(LedProtocol::SK6812_RGBCCT);
    assert(rgbcct.channelCount == 5 && rgbcct.defaultColorOrder == ColorOrder::GRBCCT);
    const OneWireTimingProfile& ws2805 = getOneWireTimingProfile(LedProtocol::WS2805_RGBCCT);
    assert(ws2805.bitRateHz == 800000 && ws2805.resetTimeUs == 300);
    assert(ws2805.pioCadence == OneWirePioCadence::WS2805_10);
    assert(ws2805.channelCount == 5 && ws2805.defaultColorOrder == ColorOrder::RGBCCT);
    assert(ws2805.t0hNs == 300 && ws2805.t0lNs == 950);
    assert(ws2805.t1hNs == 650 && ws2805.t1lNs == 600);
    assert(ws2805.t0hNs + ws2805.t0lNs == 1250);
    assert(ws2805.t1hNs + ws2805.t1lNs == 1250);
    const OneWireTimingProfile& sm16825 = getOneWireTimingProfile(LedProtocol::SM16825);
    assert(sm16825.bitRateHz == 800000 && sm16825.channelCount == 5);
    assert(sm16825.bytesPerChannel == 2 && sm16825.frameSettingsBytes == 4);
    assert(sm16825.defaultColorOrder == ColorOrder::RGBCTW);
    const OneWireTimingProfile& tm1814 = getOneWireTimingProfile(LedProtocol::TM1814);
    assert(tm1814.inverted);
}

static void testColorOrderCompatibility()
{
    uint8_t first = 0, second = 0, third = 0;
    const struct { ColorOrder order; uint8_t first; uint8_t second; uint8_t third; } cases[] = {
        {ColorOrder::RGB, 1, 2, 3}, {ColorOrder::RBG, 1, 3, 2},
        {ColorOrder::GRB, 2, 1, 3}, {ColorOrder::GBR, 2, 3, 1},
        {ColorOrder::BGR, 3, 2, 1}, {ColorOrder::BRG, 3, 1, 2},
    };
    for (const auto& value : cases)
    {
        assert(ProtocolHelper::mapRgbChannels(value.order, 1, 2, 3, first, second, third));
        assert(first == value.first && second == value.second && third == value.third);
    }
    assert(!ProtocolHelper::mapRgbChannels(ColorOrder::RGBW, 1, 2, 3, first, second, third));
    assert(ProtocolHelper::getDefaultFrequency(LedProtocol::WS2811) == 800000);
    assert(ProtocolHelper::getDefaultFrequency(LedProtocol::WS2811_400KHZ) == 400000);
    assert(ProtocolHelper::getDefaultFrequency(LedProtocol::WS2805_RGBCCT) == 800000);
    assert(ProtocolHelper::getColorOrder(LedProtocol::WS2811) == ColorOrder::RGB);
    assert(ProtocolHelper::getColorOrder(LedProtocol::WS2811_400KHZ) == ColorOrder::GRB);
    assert(ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRB));
    assert(ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRBW));
    assert(!ProtocolHelper::isColorOrderCompatible(LedProtocol::WS2812B, ColorOrder::GRBW));
    assert(!ProtocolHelper::isColorOrderCompatible(LedProtocol::SK6812, ColorOrder::GRBCCT));
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::WS2812B, ColorOrder::GRBW) == ColorOrder::GRB);
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::APA102, ColorOrder::GRBW) == ColorOrder::BGR);
    assert(ProtocolHelper::getColorOrder(LedProtocol::WS2805_RGBCCT) == ColorOrder::RGBCCT);
    assert(ProtocolHelper::normalizeColorOrder(LedProtocol::WS2805_RGBCCT, ColorOrder::RGBCCT) == ColorOrder::RGBCCT);
    assert(ProtocolHelper::getBytesPerLed(LedProtocol::SM16825) == 10);
    assert(ProtocolHelper::isColorOrderCompatible(LedProtocol::SM16825, ColorOrder::RGBCTW));
}

static void testCctMix()
{
    uint8_t ww = 0;
    uint8_t cw = 0;
    ProtocolHelper::kelvinToWWCW(2700, ww, cw);
    assert(ww == 255 && cw == 0);
    ProtocolHelper::kelvinToWWCW(6500, ww, cw);
    assert(ww == 0 && cw == 255);
    ProtocolHelper::kelvinToWWCW(4600, ww, cw);
    assert((uint16_t)ww + cw == 255);
    assert(ProtocolHelper::wwcwToKelvin(ww, cw) >= 4590 && ProtocolHelper::wwcwToKelvin(ww, cw) <= 4610);
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

    const OneWireTimingProfile& ws2805 = getOneWireTimingProfile(LedProtocol::WS2805_RGBCCT);
    assert(oneWireMakeBalancedSymbols(ws2805.t0hNs, ws2805.t0lNs,
                                      ws2805.t1hNs, ws2805.t1lNs,
                                      40000000U, 32767U, symbols));
    assert(symbols.period == 50 && symbols.zeroHigh == 12 && symbols.zeroLow == 38);
    assert(symbols.oneHigh == 26 && symbols.oneLow == 24);
}

static void testBitrateOverrides()
{
    OneWireTimingOverride timing = {};

    const OneWireTimingProfile& ws2812 = getOneWireTimingProfile(LedProtocol::WS2812B);
    assert(oneWireMakeBitrateOverride(ws2812, 800000U, timing));
    assert(timing.t0hNs == 375 && timing.t0lNs == 875);
    assert(timing.t1hNs == 750 && timing.t1lNs == 500);

    const OneWireTimingProfile& ws2805 = getOneWireTimingProfile(LedProtocol::WS2805_RGBCCT);
    assert(oneWireMakeBitrateOverride(ws2805, 800000U, timing));
    assert(timing.t0hNs == 375 && timing.t0lNs == 875);
    assert(timing.t1hNs == 625 && timing.t1lNs == 625);

    const OneWireTimingProfile& ws2811 = getOneWireTimingProfile(LedProtocol::WS2811_400KHZ);
    assert(oneWireMakeBitrateOverride(ws2811, 400000U, timing));
    assert(timing.t0hNs == 416 && timing.t0lNs == 2084);
    assert(timing.t1hNs == 1250 && timing.t1lNs == 1250);

    const OneWireTimingProfile& tm1814 = getOneWireTimingProfile(LedProtocol::TM1814);
    assert(oneWireMakeBitrateOverride(tm1814, 800000U, timing));
    assert(timing.t0hNs == 416 && timing.t0lNs == 834);
    assert(timing.t1hNs == 833 && timing.t1lNs == 417);

    assert(!oneWireMakeBitrateOverride(ws2805, 0U, timing));
}

static void testPioDivider()
{
    float divider = 0.0f;
    float realizedBitrate = 0.0f;

    // WS2805 uses a datasheet-safe ten-cycle cadence at 800 kHz.
    assert(oneWireMakePioClockDivider(125000000U, 800000.0f, 10,
                                      divider, realizedBitrate));
    assert(divider == 15.625f && realizedBitrate == 800000.0f);
    assert(oneWireMakePioClockDivider(150000000U, 800000.0f, 10,
                                      divider, realizedBitrate));
    assert(divider == 18.75f && realizedBitrate == 800000.0f);

    // The custom bitrate path must use the same cadence calculation.
    assert(oneWireMakePioClockDivider(125000000U, 600000.0f, 10,
                                      divider, realizedBitrate));
    assert(divider == (125000000.0f / 6000000.0f) && realizedBitrate == 600000.0f);
    assert(oneWireMakePioClockDivider(150000000U, 600000.0f, 10,
                                      divider, realizedBitrate));
    assert(divider == 25.0f && realizedBitrate == 600000.0f);
    assert(!oneWireMakePioClockDivider(125000000U, 0.0f, 10,
                                       divider, realizedBitrate));
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

    // Two pixels ensure the byte-stream path cannot hide an inserted FIFO
    // padding word or a missing fifth channel at a pixel boundary.
    const uint8_t rgbcctPixels[] = {
        0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xaa,
    };
    assert(oneWirePackedWordCount(sizeof(rgbcctPixels), 5) == sizeof(rgbcctPixels));
    for (size_t i = 0; i < sizeof(rgbcctPixels); ++i)
        assert(oneWirePackedWordAt(rgbcctPixels, 5, i) == ((uint32_t)rgbcctPixels[i] << 24));

    uint8_t sm16825[14] = {};
    // Default RGBCTW ordering: R, G, B, cool white, warm white.
    oneWireStoreChannel(sm16825, 2, 0, 0x01);
    oneWireStoreChannel(sm16825, 2, 1, 0x02);
    oneWireStoreChannel(sm16825, 2, 2, 0x03);
    oneWireStoreChannel(sm16825, 2, 3, 0x05);
    oneWireStoreChannel(sm16825, 2, 4, 0x04);
    oneWireWriteSm16825Settings(sm16825 + 10);
    const uint8_t expected[] = {0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x05, 0x05,
                                0x04, 0x04, 0x00, 0x00, 0x00, 0x1f};
    for (size_t i = 0; i < sizeof(sm16825); ++i)
        assert(sm16825[i] == expected[i]);
    assert(oneWirePackedWordCount(sizeof(sm16825), 10) == sizeof(sm16825));
    for (size_t i = 0; i < sizeof(sm16825); ++i)
        assert(oneWirePackedWordAt(sm16825, 10, i) == ((uint32_t)sm16825[i] << 24));
}

static void testSpiFrameLayouts()
{
    SpiFrameLayout layout = {};

    assert(spiMakeFrameLayout(LedProtocol::APA102, 2, 8, 1, 1, layout));
    assert(layout.hasGlobalBrightness && layout.bytesPerLed == 4);
    assert(layout.startFrameSize == 32 && layout.dummyLedSize == 4);
    assert(layout.pixelDataSize == 8 && layout.endFrameSize == 4);
    assert(layout.bufferSize == 48);

    assert(spiMakeFrameLayout(LedProtocol::SK9822, 1, 1, 0, 1, layout));
    assert(layout.bufferSize == 12); // start frame + pixel frame + end frame

    assert(spiMakeFrameLayout(LedProtocol::WS2801, 2, 8, 1, 1, layout));
    assert(!layout.hasGlobalBrightness && layout.bytesPerLed == 3);
    assert(layout.startFrameSize == 0 && layout.dummyLedSize == 0 && layout.endFrameSize == 0);
    assert(layout.bufferSize == 6); // no APA framing or padding

    assert(spiMakeFrameLayout(LedProtocol::LPD8806, 1, 8, 1, 1, layout));
    assert(layout.bufferSize == 3);
    assert(spiEncodeLpd8806Channel(0x00) == 0x80);
    assert(spiEncodeLpd8806Channel(0xff) == 0xff);
    assert(spiPioWordForByte(0xa5) == 0xa5000000U);
}

int main()
{
    testProfiles();
    testColorOrderCompatibility();
    testCctMix();
    testQuantization();
    testBitrateOverrides();
    testPioDivider();
    testDeadlines();
    testExactPacking();
    testSpiFrameLayouts();
    puts("one-wire timing regression tests passed");
    return 0;
}
