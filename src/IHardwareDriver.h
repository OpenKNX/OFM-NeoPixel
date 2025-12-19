/**
 * @file IHardwareDriver.h
 * @brief Hardware Abstraction Layer Interface for NeoPixel drivers
 *
 * This interface provides a platform-agnostic API for controlling addressable LEDs.
 * Implementations exist for:
 * - PIO (RP2040/RP2350): 1-Wire and SPI
 * - RMT (ESP32-S3): 1-Wire
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
    WS2805,  // 12V RGB, 400kHz, RGB order
    WS2812,  // 5V RGB, 800kHz, GRB order
    WS2812B, // Same as WS2812 (common variant)
    WS2813,  // 5V RGB, 800kHz, GRB, data backup
    WS2815,  // 12V RGB, 800kHz, GRB, data backup
    WS2811,  // 12V RGB, 400kHz, RGB order
    SK6812,  // 5V/12V RGBW, 800kHz, GRBW order
    SK6805,  // 5V RGBW, 800kHz, GRBW order
    WS2814,  // 12V RGBW, 800kHz, GRBW order
    TM1814,  // 12V RGBW, 800kHz, GRBW order
    GS8208,  // 12V RGB, 800kHz, GRB order

    // SPI Protocols (Separate clock and data)
    APA102,  // 5V RGB+Brightness, up to 20MHz
    SK9822,  // 5V RGB+Brightness, up to 15MHz (APA102 clone)
    WS2801,  // 5V RGB, up to 25MHz
    LPD8806, // 5V RGB (7-bit), up to 20MHz
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
};

/**
 * Hardware Driver Capabilities
 */
struct DriverCapabilities
{
    bool supportsRGBW;     // Supports 32-bit RGBW
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
    virtual bool show() = 0;
    virtual bool isBusy() = 0;
    virtual void clear() = 0;

    virtual uint16_t getLedCount() const = 0;
    virtual LedProtocol getProtocol() const = 0;
    virtual DriverCapabilities getCapabilities() const = 0;
    virtual uint8_t* getBuffer() = 0;
    virtual size_t getBufferSize() const = 0;
    virtual bool isInitialized() const = 0;

    // Get driver implementation type (for runtime detection)
    virtual DriverImplementation getDriverType() const = 0;

    // Check if driver supports hardware brightness byte (APA102/SK9822)
    virtual bool supportsHardwareBrightness() const { return false; }

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
    inline bool is1Wire(LedProtocol protocol)
    {
        return protocol >= LedProtocol::WS2812 &&
               protocol <= LedProtocol::GS8208;
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
     * Get color order for protocol
     */
    inline ColorOrder getColorOrder(LedProtocol protocol)
    {
        switch (protocol)
        {
            case LedProtocol::WS2811:
                return ColorOrder::RGB;

            case LedProtocol::SK6812:
            case LedProtocol::SK6805:
            case LedProtocol::WS2814:
            case LedProtocol::TM1814:
                return ColorOrder::GRBW;

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
        return isRGBW(protocol) ? 4 : 3;
    }

    /**
     * Get default frequency for protocol (Hz)
     */
    inline uint32_t getDefaultFrequency(LedProtocol protocol)
    {
        if (protocol == LedProtocol::WS2811)
        {
            return 400000; // 400kHz
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
}; // namespace ProtocolHelper