/**
 * @file IHardwareDriver.h
 * @brief Hardware Abstraction Layer Interface for NeoPixel drivers
 *
 * This interface provides a platform-agnostic API for controlling addressable LEDs.
 * Implementations exist for:
 * - PIO (RP2040/RP2350): 1-Wire and SPI
 * - RMT (ESP32 all variants): 1-Wire
 * - Hardware SPI (All platforms)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// Forward declaration
struct PhysicalStripConfig;

/**
 * LED Protocol Types
 */
enum class LedProtocol
{
    // 1-Wire Protocols (Clock embedded in data)
    WS2812,  // 5V RGB, 800kHz, GRB order
    WS2812B, // Same as WS2812 (common variant)
    WS2813,  // 5V RGB, 800kHz, GRB, data backup
    WS2815,  // 12V RGB, 800kHz, GRB, data backup
    WS2811,  // 12V RGB, 800kHz, RGB order
    WS2811_400KHZ, // Legacy WS2811/WS2812 RGB, 400kHz, GRB order
    SK6812,  // 5V/12V RGBW, 800kHz, GRBW order
    SK6805,  // 5V RGBW, 800kHz, GRBW order
    WS2814,  // 12V RGBW, 800kHz, GRBW order
    TM1814,  // 12V RGBW, 800kHz, GRBW order
    GS8208,  // 12V RGB, 800kHz, GRB order

    TM1829,  // 12V RGB, 800kHz, BRG order, inverted line
    TM1914,  // 12V RGB, 800kHz, inverted line
    APA106,  // 5V RGB, 800kHz, RGB order (PL9823 equivalent)

    // 5-Channel Protocols (RGB + Warm White + Cool White = CCT)
    SK6812_RGBCCT, // 5V RGBCCT (5-channel), 800kHz, GRBCCT order
    WS2814_RGBCCT, // 12V RGBCCT (5-channel), 800kHz, GRBCCT order
    WS2805_RGBCCT, // 12V/24V RGBCCT (5-channel), 800kHz, GRBCCT order

    // 16-bit-per-channel 1-Wire protocols. WS2812x bit timing, wider frame.
    UCS8903, // 5V RGB,    16 bit/channel ->  6 bytes/LED
    UCS8904, // 5V RGBW,   16 bit/channel ->  8 bytes/LED
    SM16825, // RGB+CW+WW, 16 bit/channel -> 10 bytes/LED

    FW1906,  // RGB + CW + WW + unused, 8 bit/channel -> 6 bytes/LED

    // SPI Protocols (Separate clock and data)
    APA102,       // 5V RGB+Brightness, up to 20MHz (original chip)
    APA102_CLONE, // 5V RGB+Brightness, clone chips with RGB update bug (requires minRgbValue workaround)
    SK9822,       // 5V RGB+Brightness, up to 15MHz (APA102 clone)
    WS2801,       // 5V RGB, up to 25MHz
    LPD8806,      // 5V RGB (7-bit), up to 20MHz
    LPD6803,      // 5V RGB, 2 bytes/LED (5-5-5), no APA102 start/end frames
    P9813,        // 5V RGB, 4 bytes/LED with a checksum flag byte
};

/**
 * Driver Type Selection
 */
enum class DriverType
{
    AUTO,         // Automatic selection based on protocol and platform
    SERIAL_1WIRE, // Force 1-Wire driver (PIO or RMT)
    SPI_HARDWARE, // Force Hardware SPI
    SPI_PIO,      // Force PIO-based SPI (RP2040/50 only)
    NATIVE,       // Force native/software implementation
};

/**
 * Driver Implementation Type (for runtime detection)
 */
enum class DriverImplementation
{
    PIO_SERIAL,   // RP2040 PIO 1-Wire
    PIO_SPI,      // RP2040 PIO SPI
    RMT_SERIAL,   // ESP32 RMT 1-Wire
    HARDWARE_SPI, // Hardware SPI
    NATIVE,       // Native/Software implementation
    UNKNOWN,      // Unknown driver type
};

/**
 * Color Order for internal buffer
 */
enum class ColorOrder
{
    NONE, // Not set - use protocol default
    RGB,  // Red, Green, Blue
    RBG,  // Red, Blue, Green (some LED clones)
    GRB,  // Green, Red, Blue (WS2812, SK6812 standard)
    GBR,  // Green, Blue, Red (some WS2812B clones)
    BGR,  // Blue, Green, Red
    BRG,  // Blue, Red, Green (rare)
    RGBW, // Red, Green, Blue, White (SK6812)
    GRBW, // Green, Red, Blue, White (SK6812 standard)

    // 5-Channel: RGB + Warm White + Cool White (CCT)
    RGBCCT, // Red, Green, Blue, Warm White, Cool White
    GRBCCT, // Green, Red, Blue, Warm White, Cool White (standard for 5-channel)
    RGBCTW, // Red, Green, Blue, Cool White, Warm White (CW first)
    GRBCTW, // Green, Red, Blue, Cool White, Warm White

    WRGB,   // White, Red, Green, Blue (TM1814 frame order)
};

/**
 * Hardware Driver Capabilities
 */
struct DriverCapabilities
{
    bool supportsRGBW;     // Supports 32-bit RGBW (4 channels)
    bool supportsRGBCCT;   // Supports 40-bit RGBCCT (5 channels)
    bool supportsDMA;      // DMA transfer available
    bool supportsAsync;    // Non-blocking show() possible
    uint32_t maxFrequency; // Maximum update frequency (Hz)
    uint16_t maxLeds;      // Maximum LEDs per strip
};

/**
 * Hardware Driver Interface
 *
 * All platform-specific drivers must implement this interface.
 */
class IHardwareDriver
{
  public:
    virtual ~IHardwareDriver() = default;

    virtual bool init() = 0;
    virtual bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) = 0;
    virtual bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) = 0; // 5-channel RGBCCT
    virtual bool show() = 0;
    virtual bool isBusy() = 0;
    virtual void clear() = 0;

    virtual uint16_t getLedCount() const = 0;
    virtual LedProtocol getProtocol() const = 0;
    virtual DriverCapabilities getCapabilities() const = 0;
    virtual uint8_t* getBuffer() = 0;
    virtual size_t getBufferSize() const = 0;
    virtual bool isInitialized() const = 0;

    // Conservative deadline for one complete frame. Clockless drivers override
    // this from their real payload length and realised bit rate so long, healthy
    // strips are not mistaken for stalled transfers.
    virtual uint32_t getTransferTimeoutUs() const { return 1000000U; }

    // Get driver implementation type (for runtime detection)
    virtual DriverImplementation getDriverType() const = 0;

    // Check if driver supports hardware brightness byte (APA102/SK9822)
    virtual bool supportsHardwareBrightness() const { return false; }

    /**
     * @brief Set the signal polarity override before init()
     * @param mode 0 = follow the chip profile, 1 = force normal, 2 = force inverted
     * @note Called before init() because a backend may fix the polarity when it creates
     *       its hardware channel. Drivers without polarity support ignore it.
     */
    virtual void setPolarityOverride(uint8_t mode) { (void)mode; }

    // Allow changing ColorOrder after construction (default: no-op for drivers that don't support it)
    virtual void setColorOrder(ColorOrder order) { /* Default: ignore */ }
    virtual ColorOrder getColorOrder() const { return ColorOrder::RGB; } // Default fallback

    // Configuration management (NEW API)
    /**
     * @brief Create default configuration for this driver type
     * @return New config object (caller must delete)
     * @note Each driver creates its appropriate config type (SpiStripConfig/SerialStripConfig)
     */
    virtual PhysicalStripConfig* createDefaultConfig() const = 0;

    /**
     * @brief Apply configuration to driver
     * @param config Configuration to apply (must match driver type)
     * @return true if config applied successfully
     * @note Driver reads config and applies hardware settings
     */
    virtual bool applyConfig(const PhysicalStripConfig* config) = 0;
};

/**
 * Helper functions for protocol detection
 */
namespace ProtocolHelper
{
    /**
     * Check if protocol is 1-Wire
     */
    constexpr bool isSPI(LedProtocol protocol);

    /**
     * @brief Whether a protocol carries its clock in the data line
     * @note Defined against isSPI so a new protocol cannot fall out of both sets.
     */
    constexpr bool is1Wire(LedProtocol protocol)
    {
        return !isSPI(protocol);
    }

    /**
     * Check if protocol is SPI
     */
    constexpr bool isSPI(LedProtocol protocol)
    {
        return protocol >= LedProtocol::APA102 &&
               protocol <= LedProtocol::P9813;
    }

    /**
     * Check if protocol supports RGBW (4 channels)
     */
    constexpr bool isRGBW(LedProtocol protocol)
    {
        return protocol == LedProtocol::SK6812 ||
               protocol == LedProtocol::SK6805 ||
               protocol == LedProtocol::WS2814 ||
               protocol == LedProtocol::TM1814 ||
               protocol == LedProtocol::UCS8904;
    }

    /**
     * Check if protocol supports RGBCCT (5 channels - RGB + Warm White + Cool White)
     */
    constexpr bool isRGBCCT(LedProtocol protocol)
    {
        return protocol == LedProtocol::SK6812_RGBCCT ||
               protocol == LedProtocol::WS2814_RGBCCT ||
               protocol == LedProtocol::WS2805_RGBCCT ||
               protocol == LedProtocol::SM16825;
    }

    /**
     * Check if color order has white channel (4+ bytes)
     */
    inline bool hasWhiteChannel(ColorOrder order)
    {
        return order == ColorOrder::RGBW || order == ColorOrder::GRBW || order == ColorOrder::WRGB ||
               order == ColorOrder::RGBCCT || order == ColorOrder::GRBCCT ||
               order == ColorOrder::RGBCTW || order == ColorOrder::GRBCTW;
    }

    /**
     * Check if color order has dual white channels (5 bytes - WW + CW)
     */
    inline bool hasDualWhiteChannel(ColorOrder order)
    {
        return order == ColorOrder::RGBCCT || order == ColorOrder::GRBCCT ||
               order == ColorOrder::RGBCTW || order == ColorOrder::GRBCTW;
    }

    /**
     * Get color order for protocol
     */
    constexpr ColorOrder getColorOrder(LedProtocol protocol)
    {
        switch (protocol)
        {
            case LedProtocol::WS2811:
            case LedProtocol::TM1914:
            case LedProtocol::APA106:
            case LedProtocol::UCS8903:
                return ColorOrder::RGB;

            case LedProtocol::WS2811_400KHZ:
                return ColorOrder::GRB;

            case LedProtocol::TM1829:
                return ColorOrder::BRG;

            case LedProtocol::UCS8904:
                return ColorOrder::RGBW;

            case LedProtocol::SM16825:
                return ColorOrder::RGBCTW;

            case LedProtocol::FW1906:
                return ColorOrder::GRBCCT;

            case LedProtocol::TM1814:
                return ColorOrder::WRGB; // datasheet frame order is W R G B

            case LedProtocol::SK6812:
            case LedProtocol::SK6805:
            case LedProtocol::WS2814:
                return ColorOrder::GRBW;

            // 5-Channel protocols
            case LedProtocol::SK6812_RGBCCT:
            case LedProtocol::WS2814_RGBCCT:
            case LedProtocol::WS2805_RGBCCT:
                return ColorOrder::GRBCCT;

            case LedProtocol::WS2812:
            case LedProtocol::WS2812B:
            case LedProtocol::WS2813:
            case LedProtocol::WS2815:
            case LedProtocol::GS8208:
            default:
                return ColorOrder::GRB;
        }
    }

    /**
     * Get bytes per LED for protocol
     */
    constexpr uint8_t getBitsPerChannel(LedProtocol protocol)
    {
        switch (protocol)
        {
            case LedProtocol::UCS8903:
            case LedProtocol::UCS8904:
            case LedProtocol::SM16825:
                return 16;
            default:
                return 8;
        }
    }

    /**
     * @brief Colour channels a protocol clocks in per LED
     */
    constexpr uint8_t getChannelCount(LedProtocol protocol)
    {
        if (protocol == LedProtocol::FW1906) return 6; // RGB + CW + WW + unused
        if (isRGBCCT(protocol)) return 5;
        if (isRGBW(protocol)) return 4;
        return 3;
    }

    constexpr uint8_t getBytesPerLed(LedProtocol protocol)
    {
        // SPI frame widths are set by the frame format, not by channels x bit depth.
        switch (protocol)
        {
            case LedProtocol::APA102:
            case LedProtocol::APA102_CLONE:
            case LedProtocol::SK9822:  return 4; // global-brightness byte + BGR
            case LedProtocol::WS2801:  return 3; // raw RGB, no framing
            case LedProtocol::LPD8806: return 3; // 7-bit channels, MSB set
            case LedProtocol::LPD6803: return 2; // one 5-5-5 word
            case LedProtocol::P9813:   return 4; // flag byte with checksum + B + G + R
            default: break;
        }
        return (uint8_t)(getChannelCount(protocol) * (getBitsPerChannel(protocol) / 8));
    }

    // =====================================================================
    // SPI frame packing. constexpr so the expected bytes can be asserted at
    // compile time against the datasheets.
    // =====================================================================

    /**
     * @brief TM1814 constant-current level from tenths of a mA
     * @param mA10 current in 0.1 mA steps, 65 (6.5 mA) to 380 (38 mA)
     * @return level 0..63, the value carried in bits [5:0] of a C1 byte
     */
    constexpr uint8_t tm1814CurrentLevel(uint16_t mA10)
    {
        return (mA10 <= 65) ? (uint8_t)0
             : (mA10 >= 380) ? (uint8_t)63
             : (uint8_t)((((uint32_t)mA10 - 65) * 63 + 157) / 315);
    }

    /** SM16825 four-byte frame-settings trailer (normal action, default gains). */
    constexpr uint8_t sm16825FrameSettingsByte(uint8_t index)
    {
        return index == 3 ? 0x1F : 0x00;
    }

    /**
     * @brief LPD8806 data byte: 7-bit value with bit 7 marking data
     */
    constexpr uint8_t packLpd8806(uint8_t value)
    {
        return (uint8_t)(0x80 | (value >> 1));
    }

    /**
     * @brief LPD6803 word: leading 1 bit then 5 bits per channel, MSB first
     */
    constexpr uint16_t packLpd6803(uint8_t c0, uint8_t c1, uint8_t c2)
    {
        return (uint16_t)(0x8000 |
                          ((uint16_t)(c0 >> 3) << 10) |
                          ((uint16_t)(c1 >> 3) << 5) |
                          (uint16_t)(c2 >> 3));
    }

    /**
     * @brief P9813 flag byte: 0xC0 plus the inverted top two bits of each channel
     * @note Channel order in the frame is flag, then c2, c1, c0.
     */
    constexpr uint8_t packP9813Flag(uint8_t c0, uint8_t c1, uint8_t c2)
    {
        return (uint8_t)(0xC0 |
                         ((uint8_t)((uint8_t)(~c2) >> 6) << 4) |
                         ((uint8_t)((uint8_t)(~c1) >> 6) << 2) |
                         (uint8_t)((uint8_t)(~c0) >> 6));
    }

    /**
     * @brief Channel swap offered per strip in ETS
     * @note Clone strips sometimes wire the white channel to a different output.
     */
    enum class ChannelSwap : uint8_t
    {
        None = 0,
        WhiteBlue = 1,
        WhiteGreen = 2,
        WhiteRed = 3,
        WarmCoolWhite = 4,
    };

    /**
     * @brief Swap a channel pair before the colour order is applied
     * @note Operates on the logical components, so the result is order independent.
     */
    constexpr void applyChannelSwap(uint8_t mode, uint8_t& r, uint8_t& g, uint8_t& b,
                                    uint8_t& ww, uint8_t& cw)
    {
        uint8_t tmp = 0;
        switch ((ChannelSwap)mode)
        {
            case ChannelSwap::WhiteBlue:     tmp = ww; ww = b;  b = tmp;  break;
            case ChannelSwap::WhiteGreen:    tmp = ww; ww = g;  g = tmp;  break;
            case ChannelSwap::WhiteRed:      tmp = ww; ww = r;  r = tmp;  break;
            case ChannelSwap::WarmCoolWhite: tmp = ww; ww = cw; cw = tmp; break;
            case ChannelSwap::None:
            default: break;
        }
    }

    /**
     * @brief Move the common part of R, G and B onto the white channel
     * @param mode 0 = off, 1 = move the common part onto white and reduce RGB
     * @param w receives the white value; untouched unless it is still zero
     * @note Only fills an unused white channel. A caller that set white itself keeps it.
     */
    constexpr void applyWhiteMode(uint8_t mode, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w)
    {
        if (mode == 0 || w != 0) return;

        const uint8_t common = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
        if (common == 0) return;

        w = common;
        r = (uint8_t)(r - common);
        g = (uint8_t)(g - common);
        b = (uint8_t)(b - common);
    }

    /**
     * @brief Lay out colour components in a strip's channel order
     * @param out receives up to 6 channel values
     * @return number of channels written
     * @note NONE falls back to GRB, the order most 1-wire parts use.
     */
    constexpr uint8_t orderChannels(ColorOrder order, uint8_t r, uint8_t g, uint8_t b,
                                    uint8_t ww, uint8_t cw, uint8_t* out)
    {
        switch (order)
        {
            case ColorOrder::RGB: out[0] = r; out[1] = g; out[2] = b; return 3;
            case ColorOrder::RBG: out[0] = r; out[1] = b; out[2] = g; return 3;
            case ColorOrder::GBR: out[0] = g; out[1] = b; out[2] = r; return 3;
            case ColorOrder::BGR: out[0] = b; out[1] = g; out[2] = r; return 3;
            case ColorOrder::BRG: out[0] = b; out[1] = r; out[2] = g; return 3;

            case ColorOrder::WRGB: out[0] = ww; out[1] = r; out[2] = g; out[3] = b; return 4;
            case ColorOrder::RGBW: out[0] = r; out[1] = g; out[2] = b; out[3] = ww; return 4;
            case ColorOrder::GRBW: out[0] = g; out[1] = r; out[2] = b; out[3] = ww; return 4;

            case ColorOrder::RGBCCT: out[0] = r; out[1] = g; out[2] = b; out[3] = ww; out[4] = cw; return 5;
            case ColorOrder::GRBCCT: out[0] = g; out[1] = r; out[2] = b; out[3] = ww; out[4] = cw; return 5;
            case ColorOrder::RGBCTW: out[0] = r; out[1] = g; out[2] = b; out[3] = cw; out[4] = ww; return 5;
            case ColorOrder::GRBCTW: out[0] = g; out[1] = r; out[2] = b; out[3] = cw; out[4] = ww; return 5;

            case ColorOrder::GRB:
            case ColorOrder::NONE:
            default: out[0] = g; out[1] = r; out[2] = b; return 3;
        }
    }

    /**
     * Get bytes per LED for color order
     */
    constexpr uint8_t getBytesPerLed(ColorOrder order)
    {
        switch (order)
        {
            case ColorOrder::RGBCCT:
            case ColorOrder::GRBCCT:
            case ColorOrder::RGBCTW:
            case ColorOrder::GRBCTW:
                return 5;
            case ColorOrder::RGBW:
            case ColorOrder::GRBW:
            case ColorOrder::WRGB:
                return 4;
            default:
                return 3;
        }
    }

    /**
     * Get number of color channels for color order
     */
    inline uint8_t getChannelCount(ColorOrder order)
    {
        return getBytesPerLed(order);
    }

    /**
     * Get default frequency for protocol (Hz)
     */
    inline uint32_t getDefaultFrequency(LedProtocol protocol)
    {
        if (protocol == LedProtocol::WS2811_400KHZ)
        {
            return 400000; // Legacy 400kHz mode
        }
        if (protocol == LedProtocol::WS2805_RGBCCT)
        {
            return 800000; // TI-verified WS2805 four-step waveform
        }
        if (isSPI(protocol))
        {
            switch (protocol)
            {
                case LedProtocol::APA102: return 20000000;  // 20MHz
                case LedProtocol::SK9822: return 15000000;  // 15MHz
                case LedProtocol::WS2801: return 25000000;  // 25MHz
                case LedProtocol::LPD8806: return 20000000; // 20MHz
                default: return 10000000;                   // 10MHz default
            }
        }
        return 800000; // 800kHz for most 1-Wire protocols
    }

    // =========================================================================
    // Color Temperature Utilities (for RGBCCT strips)
    // =========================================================================

    /**
     * Convert color temperature (Kelvin) to Warm/Cool White values
     *
     * Maps a Kelvin temperature to WW (warm white) and CW (cool white) channel values.
     * The function performs linear interpolation between the warm and cool endpoints.
     *
     * @param kelvin Color temperature in Kelvin (typically 2700-6500K)
     * @param ww Output: Warm White value (0-255)
     * @param cw Output: Cool White value (0-255)
     * @param minKelvin Minimum temperature (default: 2700K = warm white 100%)
     * @param maxKelvin Maximum temperature (default: 6500K = cool white 100%)
     *
     * @note At minKelvin: ww=255, cw=0
     * @note At maxKelvin: ww=0, cw=255
     * @note At midpoint: ww=127, cw=127 (mixed)
     *
     * Example usage:
     *   uint8_t ww, cw;
     *   kelvinToWWCW(4000, ww, cw); // Returns ww≈170, cw≈85 (warm-ish)
     */
    inline void kelvinToWWCW(uint16_t kelvin, uint8_t& ww, uint8_t& cw,
                             uint16_t minKelvin = 2700, uint16_t maxKelvin = 6500)
    {
        // Clamp to valid range
        if (kelvin <= minKelvin)
        {
            ww = 255;
            cw = 0;
            return;
        }
        if (kelvin >= maxKelvin)
        {
            ww = 0;
            cw = 255;
            return;
        }

        // Linear interpolation between warm and cool
        uint32_t range = maxKelvin - minKelvin;
        uint32_t position = kelvin - minKelvin;

        // CW increases as temperature increases
        cw = (uint8_t)((position * 255UL) / range);
        // WW decreases as temperature increases
        ww = 255 - cw;
    }

    /**
     * Convert WW/CW values to approximate color temperature (Kelvin)
     *
     * Inverse of kelvinToWWCW - estimates the color temperature from
     * the warm/cool white channel values.
     *
     * @param ww Warm White value (0-255)
     * @param cw Cool White value (0-255)
     * @param minKelvin Minimum temperature (default: 2700K)
     * @param maxKelvin Maximum temperature (default: 6500K)
     * @return Estimated color temperature in Kelvin
     */
    inline uint16_t wwcwToKelvin(uint8_t ww, uint8_t cw,
                                 uint16_t minKelvin = 2700, uint16_t maxKelvin = 6500)
    {
        // Handle edge cases
        if (ww == 0 && cw == 0) return (minKelvin + maxKelvin) / 2; // Neutral
        if (cw == 0) return minKelvin;
        if (ww == 0) return maxKelvin;

        // Calculate ratio and interpolate
        uint32_t total = (uint32_t)ww + (uint32_t)cw;
        uint32_t range = maxKelvin - minKelvin;
        uint32_t kelvin = minKelvin + (range * cw) / total;

        return (uint16_t)kelvin;
    }

    /**
     * Set white channels from color temperature and brightness
     *
     * Convenience function that combines kelvinToWWCW with brightness scaling.
     *
     * @param kelvin Color temperature in Kelvin
     * @param brightness Overall white brightness (0-255)
     * @param ww Output: Warm White value (0-255)
     * @param cw Output: Cool White value (0-255)
     */
    inline void kelvinToWWCWWithBrightness(uint16_t kelvin, uint8_t brightness,
                                           uint8_t& ww, uint8_t& cw)
    {
        kelvinToWWCW(kelvin, ww, cw);

        // Scale by brightness
        ww = (uint8_t)(((uint16_t)ww * brightness) / 255);
        cw = (uint8_t)(((uint16_t)cw * brightness) / 255);
    }

}; // namespace ProtocolHelper
