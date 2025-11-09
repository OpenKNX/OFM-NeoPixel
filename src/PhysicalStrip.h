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
    // Advanced
    // ====================================================================
    bool setUpdateFrequency(uint32_t frequencyHz);
    IHardwareDriver* getDriver() const { return _driver; }

  private:
    IHardwareDriver* _driver; // Underlying driver
    uint32_t _dataPin;        // GPIO pin (MOSI/Data)
    uint32_t _clockPin;       // GPIO pin (Clock for SPI)
    uint16_t _ledCount;       // Number of LEDs
    LedProtocol _protocol;    // LED protocol
    bool _initialized;        // Initialized?

    bool createDriver(DriverType driverType); // Create appropriate driver
};
