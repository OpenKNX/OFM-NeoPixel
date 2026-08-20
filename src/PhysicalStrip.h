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
#include "PhysicalStripConfig.h"
#include "TimingMode.h"
#include <cstdint>
#include <stddef.h>
#include <stdint.h>

class PhysicalStrip
{
  public:
    PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol = LedProtocol::WS2812B, DriverType driverType = DriverType::AUTO, TimingMode timingMode = TimingMode::AUTO);
    PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, uint32_t sckPin, DriverType driverType = DriverType::AUTO, TimingMode timingMode = TimingMode::AUTO);
    PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, uint32_t sckPin, int csPin, uint32_t frequencyHz, DriverType driverType = DriverType::AUTO);
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
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw);
    void clear();

    // ====================================================================
    // Display Control
    // ====================================================================
    bool show();
    bool waitForTransfer(uint32_t timeoutMs = 0);
    uint32_t getTransferTimeoutMs() const;
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

        // Keep _config in sync, else a later applyConfig(_config) reverts to the stale default (R/G swap).
        if (_config)
        {
            _config->setColorOrder(order);
        }

        // Pass ColorOrder to driver (all drivers support this now via interface)
        if (_driver)
        {
            _driver->setColorOrder(order);
        }
    }
    ColorOrder getColorOrder() const { return _colorOrder; }

    /**
     * @brief Check if this strip has a white channel (RGBW or RGBCCT)
     * @return true if color order has at least one white channel
     */
    bool hasWhiteChannel() const
    {
        return ProtocolHelper::hasWhiteChannel(_colorOrder);
    }

    /**
     * @brief Check if this strip has dual white channels (RGBCCT - 5 channel)
     * @return true if color order is RGBCCT, GRBCCT, RGBCTW, or GRBCTW
     */
    bool hasDualWhiteChannel() const
    {
        return ProtocolHelper::hasDualWhiteChannel(_colorOrder);
    }

    // ====================================================================
    // Voltage Configuration (for Power Calculation)
    // ====================================================================
    /**
     * @brief Set operating voltage for this strip (5V, 12V, 24V)
     * @param voltage Supply voltage (default: 5V)
     * @note Used for power calculation: Power[W] = Current[mA] * Voltage / 1000
     */
    void setVoltage(uint8_t voltage) { _voltage = voltage; }

    /**
     * @brief Get operating voltage for this strip
     * @return Supply voltage in volts (5, 12, or 24)
     */
    uint8_t getVoltage() const { return _voltage; }

    /**
     * @brief Calculate power consumption in watts
     * @param currentMa Current consumption in milliamps
     * @return Power in watts
     */
    float getPowerWatts(uint32_t currentMa) const
    {
        return (currentMa * _voltage) / 1000.0f;
    }

    // ====================================================================
    // Hardware Configuration (NEW API)
    // ====================================================================
    /**
     * @brief Get hardware configuration object
     * @return Config pointer (PhysicalStripConfig base class)
     * @note Cast to SpiStripConfig* or SerialStripConfig* as needed
     *
     * Example:
     *   auto* cfg = strip->getConfig();
     *   if (auto* spiCfg = dynamic_cast<SpiStripConfig*>(cfg)) {
     *       spiCfg->setHwBrightness(25);
     *       strip->applyConfig();
     *   }
     */
    PhysicalStripConfig* getConfig() { return _config; }
    const PhysicalStripConfig* getConfig() const { return _config; }

    /**
     * @brief Check if this is a SPI strip (APA102/SK9822)
     * @return true if SPI strip, false otherwise
     */
    bool isSpiStrip() const;

    /**
     * @brief Check if this is a Serial strip (WS2812B/SK6812)
     * @return true if Serial strip, false otherwise
     */
    bool isSerialStrip() const;

    /**
     * @brief Apply current config to driver
     * @return true if config applied successfully
     * @note Call this after modifying config via getConfig()
     *
     * Example:
     *   auto* cfg = strip->getConfig();
     *   if (auto* spiCfg = dynamic_cast<SpiStripConfig*>(cfg)) {
     *       spiCfg->setHwBrightness(25);
     *       strip->applyConfig();  // Apply changes to hardware
     *   }
     */
    bool applyConfig();

    // ====================================================================
    // Advanced
    // ====================================================================
    bool setUpdateFrequency(uint32_t frequencyHz);
    IHardwareDriver* getDriver() const { return _driver; }

    // ====================================================================
    // Timing Mode (RP2040/RP2350 only)
    // ====================================================================
    TimingMode getTimingMode() const { return _timingMode; }
    bool setTimingMode(TimingMode mode);

    // ====================================================================
    // Custom Timing (PIO + RMT)
    // Set explicit T0H/T0L/T1H/T1L values in nanoseconds.
    // PIO:  only T1H determines the bit period (3:7:6:4 cycle ratio is fixed)
    // RMT:  all four values are applied independently
    // Pass resetUs = 0 to keep the protocol default.
    // ====================================================================
    bool setCustomTiming(uint16_t t0h, uint16_t t0l, uint16_t t1h, uint16_t t1l, uint32_t resetUs = 0);
    bool clearCustomTiming(); ///< Revert to AUTO timing and apply it to hardware

  private:
    IHardwareDriver* _driver;     // Underlying driver
    uint32_t _dataPin;            // GPIO pin (MOSI/Data)
    uint32_t _clockPin;           // GPIO pin (Clock for SPI)
    uint16_t _ledCount;           // Number of LEDs
    LedProtocol _protocol;        // LED protocol
    bool _initialized;            // Initialized?
    ColorOrder _colorOrder;       // Color order for this strip
    bool _hasColorOrder;          // Whether ColorOrder was explicitly set
    PhysicalStripConfig* _config; // Hardware configuration (SpiStripConfig or SerialStripConfig)
    TimingMode _timingMode;       // Timing mode for clock divider calculation
    uint8_t _voltage;             // Operating voltage (5V, 12V, 24V) - default: 5V

    bool createDriver(DriverType driverType); // Create appropriate driver
};
