#pragma once
/**
 * @file        ProtocolTableAsserts.h
 * @brief       Compile-time checks for the protocol tables and SPI frame packing
 *
 * Included from PhysicalStrip.cpp so the compiler verifies the tables in IHardwareDriver.h
 * on every build. Nothing is emitted; a wrong value fails the build instead of shipping a
 * red/green swap. Keep the include - unincluded, these checks never run.
 *
 * Expected values follow the chip datasheets and NeoPixelBus, the library WLED ships.
 *
 * @copyright Copyright (c) 2026 Erkan Colak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include "IHardwareDriver.h"

namespace ProtocolTableAsserts
{
    using namespace ProtocolHelper;

    // ---- frame width per protocol ----
    static_assert(getBytesPerLed(LedProtocol::WS2812B) == 3, "WS2812B is 3 bytes");
    static_assert(getBytesPerLed(LedProtocol::WS2811) == 3, "WS2811 is 3 bytes");
    static_assert(getBytesPerLed(LedProtocol::SK6812) == 4, "SK6812 is 4 bytes");
    static_assert(getBytesPerLed(LedProtocol::TM1814) == 4, "TM1814 is 4 bytes");
    static_assert(getBytesPerLed(LedProtocol::WS2805_RGBCCT) == 5, "WS2805 is 5 bytes");
    static_assert(getBytesPerLed(LedProtocol::SK6812_RGBCCT) == 5, "SK6812 RGBCCT is 5 bytes");
    static_assert(getBytesPerLed(LedProtocol::UCS8903) == 6, "UCS8903 is 3 channels at 16 bit");
    static_assert(getBytesPerLed(LedProtocol::UCS8904) == 8, "UCS8904 is 4 channels at 16 bit");
    static_assert(getBytesPerLed(LedProtocol::SM16825) == 10, "SM16825 is 5 channels at 16 bit");
    static_assert(getBytesPerLed(LedProtocol::FW1906) == 6, "FW1906 is 6 channels at 8 bit");
    static_assert(getBytesPerLed(LedProtocol::APA102) == 4, "APA102 is brightness plus BGR");
    static_assert(getBytesPerLed(LedProtocol::APA102_CLONE) == 4, "APA102 clone is 4 bytes");
    static_assert(getBytesPerLed(LedProtocol::SK9822) == 4, "SK9822 is 4 bytes");
    static_assert(getBytesPerLed(LedProtocol::WS2801) == 3, "WS2801 is raw RGB");
    static_assert(getBytesPerLed(LedProtocol::LPD8806) == 3, "LPD8806 is 3 seven-bit bytes");
    static_assert(getBytesPerLed(LedProtocol::LPD6803) == 2, "LPD6803 is one 5-5-5 word");
    static_assert(getBytesPerLed(LedProtocol::P9813) == 4, "P9813 is flag plus BGR");

    // ---- bit depth ----
    static_assert(getBitsPerChannel(LedProtocol::WS2812B) == 8, "WS2812B is 8 bit per channel");
    static_assert(getBitsPerChannel(LedProtocol::UCS8903) == 16, "UCS8903 is 16 bit per channel");
    static_assert(getBitsPerChannel(LedProtocol::SM16825) == 16, "SM16825 is 16 bit per channel");

    // ---- every protocol is either 1-wire or SPI, never both, never neither ----
#define NEO_ASSERT_ONEWIRE(p) \
    static_assert(is1Wire(LedProtocol::p) && !isSPI(LedProtocol::p), #p " must be 1-wire");
#define NEO_ASSERT_SPI(p) \
    static_assert(isSPI(LedProtocol::p) && !is1Wire(LedProtocol::p), #p " must be SPI");

    NEO_ASSERT_ONEWIRE(WS2812)
    NEO_ASSERT_ONEWIRE(WS2812B)
    NEO_ASSERT_ONEWIRE(WS2813)
    NEO_ASSERT_ONEWIRE(WS2815)
    NEO_ASSERT_ONEWIRE(WS2811)
    NEO_ASSERT_ONEWIRE(SK6812)
    NEO_ASSERT_ONEWIRE(SK6805)
    NEO_ASSERT_ONEWIRE(WS2814)
    NEO_ASSERT_ONEWIRE(TM1814)
    NEO_ASSERT_ONEWIRE(GS8208)
    NEO_ASSERT_ONEWIRE(TM1829)
    NEO_ASSERT_ONEWIRE(TM1914)
    NEO_ASSERT_ONEWIRE(APA106)
    NEO_ASSERT_ONEWIRE(SK6812_RGBCCT)
    NEO_ASSERT_ONEWIRE(WS2814_RGBCCT)
    NEO_ASSERT_ONEWIRE(WS2805_RGBCCT)
    NEO_ASSERT_ONEWIRE(UCS8903)
    NEO_ASSERT_ONEWIRE(UCS8904)
    NEO_ASSERT_ONEWIRE(SM16825)
    NEO_ASSERT_ONEWIRE(FW1906)

    NEO_ASSERT_SPI(APA102)
    NEO_ASSERT_SPI(APA102_CLONE)
    NEO_ASSERT_SPI(SK9822)
    NEO_ASSERT_SPI(WS2801)
    NEO_ASSERT_SPI(LPD8806)
    NEO_ASSERT_SPI(LPD6803)
    NEO_ASSERT_SPI(P9813)

#undef NEO_ASSERT_ONEWIRE
#undef NEO_ASSERT_SPI

    // ---- SPI frame packing against the datasheets ----
    static_assert(packLpd8806(0) == 0x80, "LPD8806 zero keeps the data marker bit");
    static_assert(packLpd8806(255) == 0xFF, "LPD8806 full scale");
    static_assert(packLpd8806(128) == 0xC0, "LPD8806 half scale");
    static_assert(packLpd8806(2) == 0x81, "LPD8806 smallest visible step");

    static_assert(packLpd6803(0, 0, 0) == 0x8000, "LPD6803 black keeps the leading one bit");
    static_assert(packLpd6803(255, 255, 255) == 0xFFFF, "LPD6803 white");
    static_assert(packLpd6803(255, 0, 0) == 0xFC00, "LPD6803 first channel only");
    static_assert(packLpd6803(0, 255, 0) == 0x83E0, "LPD6803 second channel only");
    static_assert(packLpd6803(0, 0, 255) == 0x801F, "LPD6803 third channel only");

    static_assert(packP9813Flag(0, 0, 0) == 0xFF, "P9813 black inverts to all ones");
    static_assert(packP9813Flag(255, 255, 255) == 0xC0, "P9813 white leaves only the marker");
    static_assert(packP9813Flag(255, 0, 0) == 0xFC, "P9813 first channel only");
    static_assert(packP9813Flag(0, 255, 0) == 0xF3, "P9813 second channel only");
    static_assert(packP9813Flag(0, 0, 255) == 0xCF, "P9813 third channel only");

    // ---- channel ordering, including the orders that used to fall through ----
    constexpr bool checkOrder(ColorOrder order, uint8_t e0, uint8_t e1, uint8_t e2)
    {
        uint8_t out[6] = {0};
        orderChannels(order, 1, 2, 3, 4, 5, out);
        return out[0] == e0 && out[1] == e1 && out[2] == e2;
    }
    constexpr bool checkOrder4(ColorOrder order, uint8_t e0, uint8_t e1, uint8_t e2, uint8_t e3)
    {
        uint8_t out[6] = {0};
        orderChannels(order, 1, 2, 3, 4, 5, out);
        return out[0] == e0 && out[1] == e1 && out[2] == e2 && out[3] == e3;
    }

    static_assert(checkOrder(ColorOrder::RGB, 1, 2, 3), "RGB order");
    static_assert(checkOrder(ColorOrder::GRB, 2, 1, 3), "GRB order");
    static_assert(checkOrder(ColorOrder::BGR, 3, 2, 1), "BGR order");
    static_assert(checkOrder(ColorOrder::RBG, 1, 3, 2), "RBG order");
    static_assert(checkOrder(ColorOrder::GBR, 2, 3, 1), "GBR order");
    static_assert(checkOrder(ColorOrder::BRG, 3, 1, 2), "BRG order");
    static_assert(checkOrder(ColorOrder::NONE, 2, 1, 3), "NONE falls back to GRB");
    // ---- TM1814 constant-current level, datasheet 6.5 mA to 38 mA over 64 steps ----
    static_assert(tm1814CurrentLevel(65) == 0, "TM1814 6.5 mA is level 0");
    static_assert(tm1814CurrentLevel(380) == 63, "TM1814 38 mA is level 63");
    static_assert(tm1814CurrentLevel(50) == 0, "TM1814 clamps below the minimum");
    static_assert(tm1814CurrentLevel(500) == 63, "TM1814 clamps above the maximum");
    static_assert(tm1814CurrentLevel(180) == 23, "TM1814 18 mA");
    static_assert((uint8_t)(tm1814CurrentLevel(380) & 0xC0) == 0, "TM1814 leaves bits 7 and 6 clear");

    // ---- TM1814 frame order is W R G B, not GRBW ----
    static_assert(getColorOrder(LedProtocol::TM1814) == ColorOrder::WRGB, "TM1814 sends white first");
    static_assert(checkOrder4(ColorOrder::WRGB, 4, 1, 2, 3), "WRGB order");
    static_assert(checkOrder4(ColorOrder::GRBW, 2, 1, 3, 4), "GRBW order");
    static_assert(getBytesPerLed(ColorOrder::WRGB) == 4, "WRGB is 4 bytes");

    // ---- per-strip channel swap, matching the NEOSwap enum in NeoPixel.share.xml ----
    constexpr bool checkSwap(uint8_t mode, uint8_t er, uint8_t eg, uint8_t eb, uint8_t eww, uint8_t ecw)
    {
        uint8_t r = 1, g = 2, b = 3, ww = 4, cw = 5;
        applyChannelSwap(mode, r, g, b, ww, cw);
        return r == er && g == eg && b == eb && ww == eww && cw == ecw;
    }
    static_assert(checkSwap(0, 1, 2, 3, 4, 5), "no swap leaves every channel alone");
    static_assert(checkSwap(1, 1, 2, 4, 3, 5), "W and B");
    static_assert(checkSwap(2, 1, 4, 3, 2, 5), "W and G");
    static_assert(checkSwap(3, 4, 2, 3, 1, 5), "W and R");
    static_assert(checkSwap(4, 1, 2, 3, 5, 4), "WW and CW");
    static_assert(checkSwap(7, 1, 2, 3, 4, 5), "out of range is a no-op");
} // namespace ProtocolTableAsserts
