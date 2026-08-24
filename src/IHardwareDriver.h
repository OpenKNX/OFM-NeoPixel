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
    WS2811,        // 12V RGB, modern 800kHz, RGB order
    WS2811_400KHZ, // Legacy half-speed WS2811/WS2812, 400kHz, GRB order
    SK6812,  // 5V/12V RGBW, 800kHz, GRBW order
    SK6805,  // 5V RGBW, 800kHz, GRBW order
    WS2814,  // 12V RGBW, 800kHz, GRBW order
    TM1814,  // 12V RGBW, 800kHz, GRBW order
    GS8208,  // 12V RGB, 800kHz, GRB order

    // 5-Channel Protocols (RGB + Warm White + Cool White = CCT)
    SK6812_RGBCCT, // 5V RGBCCT (5-channel), 800kHz, GRBCCT order
    WS2814_RGBCCT, // 12V RGBCCT (5-channel), 800kHz, GRBCCT order
    WS2805_RGBCCT, // 12V/24V RGBCCT (5-channel), 800kHz, RGBCCT (RGBW1W2) order
    SM16825,       // 5V/12V RGBCW, 16-bit channels, 800kHz, RGBCTW order

    // SPI Protocols (Separate clock and data)
    APA102,       // 5V RGB+Brightness, up to 20MHz (original chip)
    APA102_CLONE, // 5V RGB+Brightness, clone chips with RGB update bug (requires minRgbValue workaround)
    SK9822,       // 5V RGB+Brightness, up to 15MHz (APA102 clone)
    WS2801,       // 5V RGB, up to 25MHz
    LPD8806,      // 5V RGB (7-bit), up to 20MHz
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
};

/**
 * Hardware Driver Capabilities
 */
struct DriverCapabilities
{
    bool supportsRGBW;     // Supports 32-bit RGBW (4 channels)
    bool supportsRGBCCT;   // Supports five-channel RGBCCT/RGBCW LEDs
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

    /**
     * @brief Conservative, bounded deadline for one complete frame transfer.
     *
     * The default keeps legacy/synchronous drivers safe. Clockless drivers
     * override it with their actual payload and protocol timing so callers do
     * not impose a fixed strip-length limit.
     */
    virtual uint32_t getTransferTimeoutUs() const { return 1000000U; }

    // Get driver implementation type (for runtime detection)
    virtual DriverImplementation getDriverType() const = 0;

    // Check if driver supports hardware brightness byte (APA102/SK9822)
    virtual bool supportsHardwareBrightness() const { return false; }

    // Allow changing ColorOrder after construction (default: no-op for drivers that don't support it)
    virtual void setColorOrder(ColorOrder) { /* Default: ignore */ }
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
    inline bool is1Wire(LedProtocol protocol)
    {
        // Standard 1-Wire protocols (WS2812 to GS8208)
        if (protocol >= LedProtocol::WS2812 && protocol <= LedProtocol::GS8208)
            return true;
        // 5-Channel RGBCCT protocols (also 1-Wire)
        if (protocol == LedProtocol::SK6812_RGBCCT ||
            protocol == LedProtocol::WS2814_RGBCCT ||
            protocol == LedProtocol::WS2805_RGBCCT ||
            protocol == LedProtocol::SM16825)
            return true;
        return false;
    }

    /**
     * Check if protocol is SPI
     */
    inline bool isSPI(LedProtocol protocol)
    {
        return protocol >= LedProtocol::APA102 &&
               protocol <= LedProtocol::LPD8806;
    }

    /**
     * Check if protocol supports RGBW (4 channels)
     */
    inline bool isRGBW(LedProtocol protocol)
    {
        return protocol == LedProtocol::SK6812 ||
               protocol == LedProtocol::SK6805 ||
               protocol == LedProtocol::WS2814 ||
               protocol == LedProtocol::TM1814;
    }

    /**
     * Check if protocol supports RGBCCT (5 channels - RGB + Warm White + Cool White)
     */
    inline bool isRGBCCT(LedProtocol protocol)
    {
        return protocol == LedProtocol::SK6812_RGBCCT ||
               protocol == LedProtocol::WS2814_RGBCCT ||
               protocol == LedProtocol::WS2805_RGBCCT ||
               protocol == LedProtocol::SM16825;
    }

    /** Check whether a protocol stores each colour channel as 16 bits. */
    inline bool is16Bit(LedProtocol protocol)
    {
        return protocol == LedProtocol::SM16825;
    }

    /**
     * Check if color order has white channel (4+ bytes)
     */
    inline bool hasWhiteChannel(ColorOrder order)
    {
        return order == ColorOrder::RGBW || order == ColorOrder::GRBW ||
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

    /** Map logical RGB values to the first three transmitted channels. */
    inline bool mapRgbChannels(ColorOrder order, uint8_t r, uint8_t g, uint8_t b,
                               uint8_t& first, uint8_t& second, uint8_t& third)
    {
        switch (order)
        {
            case ColorOrder::RGB: first = r; second = g; third = b; return true;
            case ColorOrder::RBG: first = r; second = b; third = g; return true;
            case ColorOrder::GRB: first = g; second = r; third = b; return true;
            case ColorOrder::GBR: first = g; second = b; third = r; return true;
            case ColorOrder::BGR: first = b; second = g; third = r; return true;
            case ColorOrder::BRG: first = b; second = r; third = g; return true;
            default: return false;
        }
    }

    /**
     * Get color order for protocol
     */
    inline ColorOrder getColorOrder(LedProtocol protocol)
    {
        switch (protocol)
        {
            case LedProtocol::WS2811:
                return ColorOrder::RGB;

            case LedProtocol::WS2811_400KHZ:
                return ColorOrder::GRB;

            case LedProtocol::APA102:
            case LedProtocol::APA102_CLONE:
                return ColorOrder::BGR;

            case LedProtocol::SK9822:
            case LedProtocol::WS2801:
                return ColorOrder::RGB;

            case LedProtocol::SK6812:
            case LedProtocol::SK6805:
            case LedProtocol::WS2814:
            case LedProtocol::TM1814:
                return ColorOrder::GRBW;

            // 5-Channel protocols
            case LedProtocol::SK6812_RGBCCT:
            case LedProtocol::WS2814_RGBCCT:
                return ColorOrder::GRBCCT;

            case LedProtocol::WS2805_RGBCCT:
                return ColorOrder::RGBCCT;

            case LedProtocol::SM16825:
                return ColorOrder::RGBCTW;

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
    inline uint8_t getBytesPerLed(LedProtocol protocol)
    {
        if (isRGBCCT(protocol)) return is16Bit(protocol) ? 10 : 5;
        if (isRGBW(protocol)) return 4;
        return 3;
    }

    /**
     * Get bytes per LED for color order
     */
    inline uint8_t getBytesPerLed(ColorOrder order)
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

    /** Get number of logical colour channels transmitted by a protocol. */
    inline uint8_t getChannelCount(LedProtocol protocol)
    {
        if (isRGBCCT(protocol)) return 5;
        if (isRGBW(protocol)) return 4;
        return 3;
    }

    /**
     * Check whether a color order fits in the protocol's physical frame.
     * Narrower orders are valid and intentionally disable unused white channels.
     */
    inline bool isColorOrderCompatible(LedProtocol protocol, ColorOrder order)
    {
        return order == ColorOrder::NONE || getChannelCount(order) <= getChannelCount(protocol);
    }

    /**
     * Fall back to the protocol default when an order would exceed the frame width.
     */
    inline ColorOrder normalizeColorOrder(LedProtocol protocol, ColorOrder order)
    {
        return isColorOrderCompatible(protocol, order) ? order : getColorOrder(protocol);
    }

    /**
     * Get default frequency for protocol (Hz)
     */
    inline uint32_t getDefaultFrequency(LedProtocol protocol)
    {
        if (protocol == LedProtocol::WS2805_RGBCCT)
        {
            return 800000; // Worldsemi minimum 1.25 us bit cell
        }
        if (protocol == LedProtocol::WS2811_400KHZ)
        {
            return 400000; // 400kHz legacy half-speed mode
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
