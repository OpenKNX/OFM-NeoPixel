/**
 * @file PhysicalStrip.h
 * @brief Wrapper around IHardwareDriver for unified API
 *
 * PhysicalStrip is the main entry point for applications.
 * It abstracts the underlying driver (PIO, RMT, Hardware SPI)
 * and provides a consistent API across all platforms.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
/**
 * PhysicalStrip - Wrapper around a physical LED strip
 *
 * Provides a simple, consistent API for:
 * - Pixel manipulation (RGB/RGBW)
 * - Buffer management
 * - Data transfer
 */

#include "IHardwareDriver.h"
#include <cstdint>
#include <stddef.h>
#include <stdint.h>

class PhysicalStrip
{
  public:
    PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol = LedProtocol::WS2812B, DriverType driverType = DriverType::AUTO);
    PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, uint32_t sckPin, DriverType driverType = DriverType::AUTO);
    ~PhysicalStrip();

    // ====================================================================
    // Initialization
    // ====================================================================
    bool init();
    bool isInitialized() const;

    // ====================================================================
    // Pixel Control
    // ====================================================================
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void clear();

    // ====================================================================
    // Display Control
    // ====================================================================
    bool show();
    bool waitForTransfer(uint32_t timeoutMs = 0);
    bool isBusy() const;

    // ====================================================================
    // Buffer Access
    // ====================================================================
    uint8_t* getBuffer();
    size_t getBufferSize() const;

    // ====================================================================
    // Information
    // ====================================================================
    uint16_t getLedCount() const;
    LedProtocol getProtocol() const;
    uint32_t getDataPin() const { return _dataPin; }
    uint32_t getClockPin() const { return _clockPin; }
    DriverCapabilities getCapabilities() const;
    const char* getDriverName() const;

    // ====================================================================
    // Color Order
    // ====================================================================
    void setColorOrder(ColorOrder order)
    {
        _colorOrder = order;
        _hasColorOrder = true;
    }
    ColorOrder getColorOrder() const { return _colorOrder; }
    bool hasColorOrder() const { return _hasColorOrder; }

    // ====================================================================
    // Hardware Brightness (APA102/SK9822 only)
    // ====================================================================
    /**
     * @brief Set hardware brightness for APA102/SK9822 (0-255)
     * Only effective for SPI protocols with global brightness support
     * Silently ignored for WS2812B, SK6812, etc.
     */
    void setHardwareBrightness(uint8_t brightness);
    uint8_t getHardwareBrightness() const { return _hardwareBrightness; }
    bool supportsHardwareBrightness() const;

    // ====================================================================
    // Advanced
    // ====================================================================
    bool setUpdateFrequency(uint32_t frequencyHz);
    IHardwareDriver* getDriver() const { return _driver; }

  private:
    IHardwareDriver* _driver;    // Underlying driver
    uint32_t _dataPin;           // GPIO pin (MOSI/Data)
    uint32_t _clockPin;          // GPIO pin (Clock for SPI)
    uint16_t _ledCount;          // Number of LEDs
    LedProtocol _protocol;       // LED protocol
    bool _initialized;           // Initialized?
    ColorOrder _colorOrder;      // Color order for this strip
    bool _hasColorOrder;         // Whether ColorOrder was explicitly set
    uint8_t _hardwareBrightness; // Hardware brightness (0-255, default 255 = max, only APA102/SK9822)

    bool createDriver(DriverType driverType); // Create appropriate driver
};
