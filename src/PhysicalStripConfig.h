/**
 * @file PhysicalStripConfig.h
 * @brief Configuration structures for PhysicalStrip hardware settings
 *
 * Provides type-safe configuration objects for different LED strip types:
 * - SpiStripConfig: APA102/SK9822 SPI-based strips
 * - SerialStripConfig: WS2812B/SK6812 1-wire strips
 *
 * Design Philosophy:
 * - Each driver type has its own config struct
 * - Getter/Setter methods with built-in validation
 * - Config stored in PhysicalStrip, applied to driver
 * - Test routines included for auto-detection
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "IHardwareDriver.h"
#include "TimingMode.h"
#include <stdint.h>

// Forward declaration
class PhysicalStrip;

/**
 * @brief Base configuration for all PhysicalStrip types
 *
 * Contains common properties shared across all strip types.
 * Derive from this class for driver-specific configurations.
 */
struct PhysicalStripConfig
{
    virtual ~PhysicalStripConfig() = default;

    /**
     * @brief Set color byte order for this strip
     * @param order Color order (RGB, GRB, BGR, RGBW, GRBW, etc.)
     */
    void setColorOrder(ColorOrder order) { _colorOrder = order; }

    /**
     * @brief Get current color byte order
     * @return Color order setting
     */
    ColorOrder getColorOrder() const { return _colorOrder; }

  protected:
    ColorOrder _colorOrder = ColorOrder::NONE; // NONE = use protocol default
};

/**
 * @brief Configuration for SPI-based LED strips (APA102/SK9822)
 *
 * SPI strips use separate clock and data lines with dedicated brightness byte.
 * This config manages all APA102/SK9822-specific settings including:
 * - Hardware brightness control
 * - Protocol framing (start/end frames)
 * - Dummy LED mode for first-LED color corruption workaround
 * - SPI frequency tuning
 * - Auto-detection of chip variants
 */
struct SpiStripConfig : public PhysicalStripConfig
{

    // ===== Brightness Control =====

    /**
     * @brief Set hardware brightness (APA102/SK9822 5-bit brightness byte)
     * @param brightness Brightness value (0-31 protocol range, auto-clamped)
     * @note Values below 16 may flicker on clones, 31 may cause sync issues
     */
    void setHwBrightness(uint8_t brightness)
    {
        // Clamp to valid protocol range
        if (brightness < _hwBrightnessMin) brightness = _hwBrightnessMin;
        if (brightness > _hwBrightnessMax) brightness = _hwBrightnessMax;
        _hwBrightness = brightness;
    }

    /**
     * @brief Get current hardware brightness
     * @return Brightness value (0-31, default: 16)
     */
    uint8_t getHwBrightness() const { return _hwBrightness; }
    inline uint8_t getHwBrightnessMin() const { return _hwBrightnessMin; }
    inline uint8_t getHwBrightnessMax() const { return _hwBrightnessMax; }
    inline uint8_t getHwBrightnessDefault() const { return _hwBrightnessDefault; }

    // ===== Protocol Framing =====

    /**
     * @brief Set dummy LED mode (workaround for first LED color corruption)
     * @param mode 0=none (accept wrong color), 1=physical (sacrifice LED#0), 2=virtual (force LED#0 black)
     * @note Mode 1 requires buffer re-allocation (call before init!)
     */
    void setDummyLedMode(uint8_t mode)
    {
        if (mode > 2) mode = 1; // Invalid → default to physical
        _dummyLedMode = mode;
    }

    /**
     * @brief Get dummy LED mode
     * @return Mode (0=none, 1=physical, 2=virtual)
     */
    uint8_t getDummyLedMode() const { return _dummyLedMode; }

    /**
     * @brief Set start frame count (synchronization frames)
     * @param count Number of start frames (1-8, typical: 8)
     * @note More frames = more reliable sync, fewer frames = faster updates
     */
    void setStartFrameCount(uint8_t count)
    {
        if (count < 1) count = 1;
        if (count > 8) count = 8;
        _startFrameCount = count;
    }

    /**
     * @brief Get start frame count
     * @return Number of start frames (1-8)
     */
    uint8_t getStartFrameCount() const { return _startFrameCount; }

    /**
     * @brief Set end frame count (latch frames)
     * @param count Number of end frames (1-80, typical: 1 for APA102, varies for SK9822)
     * @note Formula for long strips: (LED_COUNT / 2 / 8) rounded up
     */
    void setEndFrameCount(uint8_t count)
    {
        if (count < 1) count = 1;
        if (count > 80) count = 80;
        _endFrameCount = count;
    }

    /**
     * @brief Get end frame count
     * @return Number of end frames (1-80)
     */
    uint8_t getEndFrameCount() const { return _endFrameCount; }

    /**
     * @brief Set end frame bit pattern
     * @param pattern Bit pattern for end frames (0x00=APA102, 0xFF=SK9822)
     * @note APA102: 0x00 (all zeros), SK9822: 0xFF (all ones)
     */
    void setEndFramePattern(uint8_t pattern)
    {
        _endFramePattern = pattern;
    }

    /**
     * @brief Get end frame bit pattern
     * @return Pattern byte (0x00 or 0xFF)
     */
    uint8_t getEndFramePattern() const { return _endFramePattern; }

    /**
     * @brief Set delay after start frames
     * @param delayUs Delay in microseconds (0-1000, typical: 0)
     * @note Use for strips that need extra settling time
     */
    void setStartFrameDelayUs(uint32_t delayUs)
    {
        if (delayUs > 1000) delayUs = 1000;
        _startFrameDelayUs = delayUs;
    }

    /**
     * @brief Get start frame delay
     * @return Delay in microseconds (0-1000)
     */
    uint32_t getStartFrameDelayUs() const { return _startFrameDelayUs; }

    // ===== Chip Detection =====

    /**
     * @brief Enable/disable automatic chip detection on init
     * @param enable true=auto-detect APA102 vs SK9822, false=use manual config
     * @note Clone chips often mislabeled - auto-detect helps identify actual chip
     */
    void setAutoDetectChip(bool enable)
    {
        _autoDetectChip = enable;
    }

    /**
     * @brief Check if auto-detection is enabled
     * @return true if enabled, false otherwise
     */
    bool getAutoDetectChip() const { return _autoDetectChip; }

    /**
     * @brief Get detected chip type (after auto-detect or manual set)
     * @return Detected protocol (APA102 or SK9822)
     */
    LedProtocol getDetectedChip() const { return _detectedChip; }

    /**
     * @brief Manually set detected chip type (for test routines)
     * @param chip Detected protocol
     */
    void setDetectedChip(LedProtocol chip)
    {
        _detectedChip = chip;
    }

    // ===== SPI Frequency =====

    /**
     * @brief Set SPI clock frequency
     * @param freq Frequency in Hz (typical: 7.5-15 MHz)
     * @note APA102: up to 20MHz, SK9822: up to 15MHz (clones may be slower)
     */
    void setSpiFrequency(uint32_t freq)
    {
        _spiFrequency = freq;
    }

    /**
     * @brief Get SPI clock frequency
     * @return Frequency in Hz
     */
    uint32_t getSpiFrequency() const { return _spiFrequency; }

    // ===== Test Routines =====

    /**
     * @brief Auto-detect chip type (APA102 vs SK9822)
     * @param strip PhysicalStrip instance for hardware access
     * @return Detected chip type
     * @note Destructive test - LEDs will flash during detection
     */
    LedProtocol detectChipType(PhysicalStrip* strip);

    /**
     * @brief Test SPI communication
     * @param strip PhysicalStrip instance for hardware access
     * @return true if strip responds correctly
     */
    bool testCommunication(PhysicalStrip* strip);

    /**
     * @brief Find optimal SPI frequency for this strip
     * @param strip PhysicalStrip instance for hardware access
     * @return Best working frequency in Hz
     */
    uint32_t findOptimalFrequency(PhysicalStrip* strip);

    /**
     * @brief Analyze first LED color corruption
     * @param strip PhysicalStrip instance for hardware access
     * @return Recommended dummy mode (0, 1, or 2)
     */
    uint8_t analyzeFirstLedIssue(PhysicalStrip* strip);

  private:
    uint8_t _hwBrightness = 16;                      // Default: 16 (50% brightness)
    uint8_t _hwBrightnessMin = 0;                    // Protocol minimum (5-bit = 0)
    uint8_t _hwBrightnessMax = 31;                   // Protocol maximum (5-bit = 31)
    uint8_t _hwBrightnessDefault = 16;               // Default value: 16
    uint8_t _dummyLedMode = 1;                       // Default: Physical dummy (sacrifice LED#0)
    uint8_t _startFrameCount = 8;                    // Default: 8 frames (robust sync)
    uint8_t _endFrameCount = 1;                      // Default: 1 frame (APA102 standard)
    uint8_t _endFramePattern = 0x00;                 // Default: 0x00 (APA102 pattern)
    uint32_t _startFrameDelayUs = 0;                 // Default: No delay
    bool _autoDetectChip = false;                    // Default: Manual chip selection
    LedProtocol _detectedChip = LedProtocol::APA102; // Default: APA102
    uint32_t _spiFrequency = 7500000;                // Default: 7.5MHz (safe for clones)
};

/**
 * @brief Configuration for 1-wire LED strips (WS2812B/SK6812/etc.)
 *
 * Serial strips use embedded clock in data signal with precise timing requirements.
 * This config manages timing modes and custom timing parameters.
 */
struct SerialStripConfig : public PhysicalStripConfig
{

    // ===== Timing Mode =====

    /**
     * @brief Set timing mode (predefined timing profiles)
     * @param mode Timing mode (AUTO, LEGACY_125MHZ, SLOW_5-20%, FAST_5-25%)
     * @note AUTO: 800kHz standard, LEGACY: 960kHz for RP2350 onboard LEDs
     */
    void setTimingMode(TimingMode mode)
    {
        _timingMode = mode;
    }

    /**
     * @brief Get current timing mode
     * @return Timing mode
     */
    TimingMode getTimingMode() const { return _timingMode; }

    // ===== Custom Timing (Advanced) =====

    /**
     * @brief Set custom timing parameters (nanoseconds)
     * @param t0h T0H: high time for 0-bit (typical: 350ns)
     * @param t0l T0L: low time for 0-bit (typical: 800ns)
     * @param t1h T1H: high time for 1-bit (typical: 700ns)
     * @param t1l T1L: low time for 1-bit (typical: 600ns)
     * @note Set to 0 for auto-calculation from timing mode
     */
    void setTiming(uint16_t t0h, uint16_t t0l, uint16_t t1h, uint16_t t1l)
    {
        _t0h = t0h;
        _t0l = t0l;
        _t1h = t1h;
        _t1l = t1l;
    }

    /**
     * @brief Get T0H timing (high time for 0-bit)
     * @return Time in nanoseconds (0 = auto)
     */
    uint16_t getT0H() const { return _t0h; }

    /**
     * @brief Get T0L timing (low time for 0-bit)
     * @return Time in nanoseconds (0 = auto)
     */
    uint16_t getT0L() const { return _t0l; }

    /**
     * @brief Get T1H timing (high time for 1-bit)
     * @return Time in nanoseconds (0 = auto)
     */
    uint16_t getT1H() const { return _t1h; }

    /**
     * @brief Get T1L timing (low time for 1-bit)
     * @return Time in nanoseconds (0 = auto)
     */
    uint16_t getT1L() const { return _t1l; }

    /**
     * @brief Set reset time (latch delay)
     * @param resetUs Reset time in microseconds (typical: 50-80us)
     * @note Set to 0 for auto-calculation from protocol
     */
    void setResetTime(uint32_t resetUs)
    {
        _resetTime = resetUs;
    }

    /**
     * @brief Get reset time
     * @return Time in microseconds (0 = auto)
     */
    uint32_t getResetTime() const { return _resetTime; }

    // ===== Test Routines =====

    /**
     * @brief Auto-detect protocol from timing (WS2812B vs SK6812 vs ...)
     * @param strip PhysicalStrip instance for hardware access
     * @return Detected protocol
     */
    LedProtocol detectTiming(PhysicalStrip* strip);

    /**
     * @brief Check if strip is RGBW (4-channel) or RGB (3-channel)
     * @param strip PhysicalStrip instance for hardware access
     * @return true if RGBW detected, false if RGB
     */
    bool detectRGBW(PhysicalStrip* strip);

    /**
     * @brief Measure actual timing values
     * @param strip PhysicalStrip instance for hardware access
     * @param t0h Output: measured T0H in ns
     * @param t0l Output: measured T0L in ns
     * @param t1h Output: measured T1H in ns
     * @param t1l Output: measured T1L in ns
     * @return true if measurement successful
     */
    bool measureTiming(PhysicalStrip* strip, uint16_t& t0h, uint16_t& t0l, uint16_t& t1h, uint16_t& t1l);

  private:
    TimingMode _timingMode = TimingMode::AUTO; // Default: 800kHz standard
    uint16_t _t0h = 0;                         // Default: 0 = auto-calculate
    uint16_t _t0l = 0;                         // Default: 0 = auto-calculate
    uint16_t _t1h = 0;                         // Default: 0 = auto-calculate
    uint16_t _t1l = 0;                         // Default: 0 = auto-calculate
    uint32_t _resetTime = 0;                   // Default: 0 = auto-calculate (typically 50us)
};
