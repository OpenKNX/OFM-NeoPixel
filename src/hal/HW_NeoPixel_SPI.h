/**
 * @file HW_NeoPixel_SPI.h
 * @brief Hardware SPI NeoPixel Driver for all platforms
 *
 * This driver provides a unified interface for controlling NeoPixel LEDs
 * using hardware SPI. It abstracts away platform-specific details and
 * provides a unified interface for driver creation.
 *
 * * This driver utilizes the native Hardware SPI controllers:
 * - RP2040/RP2350: SPI0 + SPI1 (2 Controllers)
 * - ESP32: VSPI + HSPI (2 Controllers)
 * - ESP32-S2/S3/C3: 2 SPI Controllers
 *
 * Supported Protocols (SPI):
 * - APA102 (RGB + Global Brightness, up to 20MHz)
 * - SK9822 (APA102 clone, up to 15MHz)
 * - WS2801 (RGB only, up to 25MHz)
 * - LPD8806 (RGB 7-bit, up to 20MHz)
 *
 * Features:
 * - Hardware-accelerated SPI
 * - Standard Arduino SPI API
 * - Automatic SPI instance selection
 * - Support for Chip Select (optional)
 * - Faster than PIO-SPI for standard pins
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../IHardwareDriver.h"
#include <SPI.h>
#include <stdint.h>

struct hw_neopixel_spi_inst
{
    SPIClass* spi;    // SPI Instance (SPI, SPI1)
    uint32_t mosiPin; // MOSI Pin
    uint32_t sckPin;  // Clock Pin
    int csPin;        // Chip Select Pin (-1 if not used)

    uint16_t ledCount;        // Number of LEDs
    uint8_t bytesPerLed;      // Bytes per LED
    LedProtocol protocol;     // LED Protocol
    bool hasGlobalBrightness; // APA102/SK9822 have global brightness
    bool needs7bit;           // LPD8806 uses 7-bit colors

    uint8_t* buffer;   // LED Data Buffer
    size_t bufferSize; // Buffer Size

    uint32_t spiFrequency; // SPI Frequency (Hz)

    bool initialized;   // Initialized?
    volatile bool busy; // Transfer in progress?
};
typedef struct hw_neopixel_spi_inst hw_neopixel_spi_inst_t;

//
// Hardware SPI NeoPixel Driver Uses Arduino SPI API for maximum compatibility
//
class HW_NeoPixel_SPI : public IHardwareDriver
{
  public:
    HW_NeoPixel_SPI(
        uint16_t ledCount,            // Number of LEDs
        LedProtocol protocol,         // LED Protocol: APA102, WS2801, etc.
        uint32_t frequency = 10000000 // Default 10MHz
    );

    HW_NeoPixel_SPI(
        uint32_t mosiPin,              // MOSI/Data Pin
        uint32_t sckPin,               // Clock Pin
        uint16_t ledCount,             // Number of LEDs
        LedProtocol protocol,          // LED Protocol: APA102, WS2801, etc.
        uint32_t frequency = 10000000, // Default 10MHz
        int csPin = -1                 // Chip Select Pin (-1 if not used)
    );

    virtual ~HW_NeoPixel_SPI();

    // IHardwareDriver interface
    bool init() override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override;
    bool show() override;
    bool isBusy() override;
    void clear() override;
    uint16_t getLedCount() const override { return _inst ? _inst->ledCount : 0; }
    LedProtocol getProtocol() const override { return _inst ? _inst->protocol : LedProtocol::APA102; }
    DriverCapabilities getCapabilities() const override;
    uint8_t* getBuffer() override { return _inst ? _inst->buffer : nullptr; }
    size_t getBufferSize() const override { return _inst ? _inst->bufferSize : 0; }
    bool isInitialized() const override { return _inst ? _inst->initialized : false; }
    DriverImplementation getDriverType() const override { return DriverImplementation::HARDWARE_SPI; }

    /**
     * @brief Set SPI frequency in Hz (only before init())
     */
    void setFrequency(uint32_t frequency)
    {
        if (_inst) _inst->spiFrequency = frequency;
    }

    /**
     * Get the used SPI instance (for advanced use)
     */
    SPIClass* getSPI() const { return _inst ? _inst->spi : nullptr; }

  private:
    hw_neopixel_spi_inst_t* _inst;

    SPIClass* selectSPI();
    void sendStartFrame();
    void sendEndFrame();
    void rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 31);

    static bool _spi0Used; // Track SPI0 usage
    static bool _spi1Used; // Track SPI1 usage
};
