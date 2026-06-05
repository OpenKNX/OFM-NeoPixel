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
#include <cmath>
#include <stdint.h>
#include <vector>

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
    PhysicalStripConfig()
    {
        // Initialize lookup table to identity mapping (no gamma correction)
        for (int i = 0; i < 256; i++)
        {
            _gammaLookupTable[i] = (uint8_t)i;
        }
    }

    virtual ~PhysicalStripConfig() = default;

    /**
     * @brief Check if this is an SPI strip config (without RTTI)
     * @return true if this is SpiStripConfig
     */
    virtual bool isSpiConfig() const { return false; }

    /**
     * @brief Check if this is a Serial strip config (without RTTI)
     * @return true if this is SerialStripConfig
     */
    virtual bool isSerialConfig() const { return false; }

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

    /**
     * @brief Set how many first LEDs to skip (force to black)
     * @param count Number of LEDs to skip (0-255)
     * @note Works for all strip types (WS2812B, SK6812, APA102, etc.)
     */
    void setSkipFirstLeds(uint8_t count)
    {
        _skipFirstLeds = count;
    }

    /**
     * @brief Get how many first LEDs are skipped
     * @return Number of LEDs forced to black (0 = none)
     */
    uint8_t getSkipFirstLeds() const { return _skipFirstLeds; }

    /**
     * @brief Initialize skip mask for arbitrary LED skipping
     * @param ledCount Number of LEDs in the strip
     * @note Allocates bitset (1 bit per LED). Call clearSkipMask() to free memory.
     */
    void initSkipMask(uint16_t ledCount)
    {
        _skipMask.clear();
        _skipMask.resize(ledCount, false); // All LEDs enabled by default
    }

    /**
     * @brief Clear skip mask (frees memory)
     * @note After clearing, isLedSkipped() returns false for all LEDs
     */
    void clearSkipMask()
    {
        _skipMask.clear();
        _skipMask.shrink_to_fit(); // Free allocated memory
    }

    /**
     * @brief Set skip status for individual LED
     * @param index LED index (0-based)
     * @param skip true = skip LED (force black), false = enable LED
     * @note Skip mask must be initialized first with initSkipMask()
     */
    void setLedSkip(uint16_t index, bool skip)
    {
        if (index < _skipMask.size())
        {
            _skipMask[index] = skip;
        }
    }

    /**
     * @brief Check if LED should be skipped (flexible mask)
     * @param index LED index (0-based)
     * @return true if LED should be forced to black
     * @note Returns false if skip mask is not initialized
     */
    bool isLedSkipped(uint16_t index) const
    {
        if (_skipMask.empty() || index >= _skipMask.size())
        {
            return false; // No mask or out of bounds
        }
        return _skipMask[index];
    }

    /**
     * @brief Get number of skipped LEDs in mask
     * @return Count of LEDs marked for skipping
     */
    uint16_t getSkipMaskCount() const
    {
        uint16_t count = 0;
        for (bool skip : _skipMask)
        {
            if (skip) count++;
        }
        return count;
    }

    /**
     * @brief Check if skip mask is initialized
     * @return true if mask is allocated
     */
    bool hasSkipMask() const { return !_skipMask.empty(); }

    /**
     * @brief Set gamma correction value
     * @param gamma Gamma value (typically 2.0-3.0, default: 2.8)
     * @note Formula: output = input^(1/gamma)
     * @note Lookup table is calculated once at startup
     */
    void setGammaCorrection(float gamma)
    {
        if (gamma < 1.0f) gamma = 1.0f;
        if (gamma > 4.0f) gamma = 4.0f;
        _gammaCorrection = gamma;
        _gammaCorrectionEnabled = (gamma != 1.0f); // Disable if gamma=1.0 (linear)

        // Pre-calculate lookup table for this gamma value
        if (_gammaCorrectionEnabled)
        {
            calculateGammaLookupTable();
        }
    }

    /**
     * @brief Get gamma correction value
     * @return Gamma value (0.0 = disabled, 1.0+ = enabled)
     */
    float getGammaCorrection() const { return _gammaCorrection; }

    /**
     * @brief Check if gamma correction is enabled
     * @return true if gamma != 1.0
     */
    bool isGammaCorrectionEnabled() const { return _gammaCorrectionEnabled; }

    /**
     * @brief Get gamma-corrected value from lookup table (for colors)
     * @param value Input color value (0-255)
     * @return Gamma-corrected value (0-255)
     */
    uint8_t getGammaCorrectedValue(uint8_t value) const
    {
        if (!_gammaCorrectionEnabled) return value;
        return _gammaLookupTable[value];
    }

    // ===== White Balance =====

    /**
     * @brief Set white balance correction for RGB channels
     * @param r Red channel multiplier (0-255, 255 = no change)
     * @param g Green channel multiplier (0-255, 255 = no change)
     * @param b Blue channel multiplier (0-255, 255 = no change)
     * @note Values below 255 reduce channel intensity
     */
    void setWhiteBalance(uint8_t r, uint8_t g, uint8_t b)
    {
        _whiteBalanceRed = r;
        _whiteBalanceGreen = g;
        _whiteBalanceBlue = b;
        _whiteBalanceEnabled = (r != 255 || g != 255 || b != 255);
    }

    /**
     * @brief Enable/disable white balance correction
     * @param enabled true to enable, false to disable
     */
    void setWhiteBalanceEnabled(bool enabled) { _whiteBalanceEnabled = enabled; }

    /**
     * @brief Check if white balance correction is enabled
     * @return true if white balance is active
     */
    bool isWhiteBalanceEnabled() const { return _whiteBalanceEnabled; }

    /**
     * @brief Get white balance R value
     * @return Red channel multiplier (0-255)
     */
    uint8_t getWhiteBalanceRed() const { return _whiteBalanceRed; }

    /**
     * @brief Get white balance G value
     * @return Green channel multiplier (0-255)
     */
    uint8_t getWhiteBalanceGreen() const { return _whiteBalanceGreen; }

    /**
     * @brief Get white balance B value
     * @return Blue channel multiplier (0-255)
     */
    uint8_t getWhiteBalanceBlue() const { return _whiteBalanceBlue; }

    /**
     * @brief Apply white balance to RGB values in-place
     * @param r Red value (modified in-place)
     * @param g Green value (modified in-place)
     * @param b Blue value (modified in-place)
     */
    void applyWhiteBalance(uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        if (!_whiteBalanceEnabled) return;
        r = (uint8_t)(((uint16_t)r * _whiteBalanceRed) / 255);
        g = (uint8_t)(((uint16_t)g * _whiteBalanceGreen) / 255);
        b = (uint8_t)(((uint16_t)b * _whiteBalanceBlue) / 255);
    }

    // ===== Power Limiting =====

    /**
     * @brief Set power limit mode for this strip
     * @param mode 0=Disabled, 1=UseGlobal, 2=FixedValue, 3=PerLED
     */
    void setPowerLimitMode(uint8_t mode) { _powerLimitMode = mode; }

    /**
     * @brief Get power limit mode
     * @return 0=Disabled, 1=UseGlobal, 2=FixedValue, 3=PerLED
     */
    uint8_t getPowerLimitMode() const { return _powerLimitMode; }

    /**
     * @brief Set maximum current in mA (for mode=2 FixedValue)
     * @param mA Maximum current in milliamperes
     */
    void setMaxCurrentMa(uint16_t mA) { _maxCurrentMa = mA; }

    /**
     * @brief Get maximum current in mA
     * @return Maximum current in milliamperes
     */
    uint16_t getMaxCurrentMa() const { return _maxCurrentMa; }

    /**
     * @brief Set current per LED in mA (for mode=3 PerLED)
     * @param mA Current per LED in milliamperes (typical: 60)
     */
    void setCurrentPerLedMa(uint8_t mA) { _currentPerLedMa = mA; }

    /**
     * @brief Get current per LED in mA
     * @return Current per LED in milliamperes
     */
    uint8_t getCurrentPerLedMa() const { return _currentPerLedMa; }

    // ===== ABL (Automatic Brightness Limiting) =====

    /**
     * @brief Set auto brightness limit cap (%)
     * @param percent Maximum brightness percentage (0-100, default: 100)
     * @note Only used when power limiting mode > 1 (own settings)
     */
    void setAutoBrightnessLimit(uint8_t percent) { _autoBrightnessLimit = percent; }

    /**
     * @brief Get auto brightness limit cap
     * @return Maximum brightness percentage (0-100)
     */
    uint8_t getAutoBrightnessLimit() const { return _autoBrightnessLimit; }

    /**
     * @brief Set power limit threshold for ABL activation (%)
     * @param percent Threshold percentage (0-100, default: 80)
     * @note ABL starts dimming when current exceeds this threshold
     */
    void setPowerLimitThreshold(uint8_t percent) { _powerLimitThreshold = percent; }

    /**
     * @brief Get power limit threshold
     * @return Threshold percentage (0-100)
     */
    uint8_t getPowerLimitThreshold() const { return _powerLimitThreshold; }

    /**
     * @brief Set ABL slew rate for smooth dimming (%)
     * @param percent Slew rate percentage (0-100, default: 20)
     * @note Higher = faster dimming response
     */
    void setAblSlewRate(uint8_t percent) { _ablSlewRate = percent; }

    /**
     * @brief Get ABL slew rate
     * @return Slew rate percentage (0-100)
     */
    uint8_t getAblSlewRate() const { return _ablSlewRate; }

  protected:
    /**
     * @brief Calculate gamma lookup table using the formula: output = (input/255)^gamma * 255
     *
     * For LED gamma correction (compensating for human eye perception):
     * - LEDs have linear brightness output
     * - Human eye perceives brightness logarithmically (more sensitive to dark)
     * - Apply gamma directly: output = input^gamma - this COMPRESSES low values
     * - Makes perceptual brightness changes appear linear
     * - Gamma 2.2-2.8 darkens mid-tones for smooth gradients
     * - Example: gamma=2.8, input=128 → output=34 (darkens mid-tones)
     */
    void calculateGammaLookupTable()
    {
        if (_gammaCorrection <= 1.0f)
        {
            // Linear: no correction
            for (int i = 0; i < 256; i++)
            {
                _gammaLookupTable[i] = i;
            }
            return;
        }

        // Apply gamma directly for LED output correction
        // This compresses low/mid values, making gradients perceptually linear
        for (int i = 0; i < 256; i++)
        {
            float normalized = i / 255.0f;
            float corrected = powf(normalized, _gammaCorrection);
            _gammaLookupTable[i] = (uint8_t)(corrected * 255.0f + 0.5f);
        }
    }

  protected:
    ColorOrder _colorOrder = ColorOrder::NONE; // NONE = use protocol default
    uint8_t _skipFirstLeds = 0;                // Default: 0 (no LEDs skipped)
    std::vector<bool> _skipMask;               // Flexible skip mask (empty = disabled)
    float _gammaCorrection = 1.0f;             // Default: 1.0 (linear, no correction)
    bool _gammaCorrectionEnabled = false;      // Default: disabled
    uint8_t _gammaLookupTable[256];            // Lookup table for color gamma
    // White balance
    bool _whiteBalanceEnabled = false; // Default: disabled
    uint8_t _whiteBalanceRed = 255;    // Default: no change
    uint8_t _whiteBalanceGreen = 255;  // Default: no change
    uint8_t _whiteBalanceBlue = 255;   // Default: no change
    // Power limiting
    uint8_t _powerLimitMode = 1;   // Default: 1 (use global settings), 0=disabled, 2=fixed mA, 3=per LED
    uint16_t _maxCurrentMa = 0;    // Default: 0 (not used when mode=1)
    uint8_t _currentPerLedMa = 60; // Default: 60 mA per LED (only used when mode=3)
    // ABL (Automatic Brightness Limiting) - per strip
    uint8_t _autoBrightnessLimit = 100; // Default: 100% (no limiting)
    uint8_t _powerLimitThreshold = 80;  // Default: 80% (start ABL at 80% of limit)
    uint8_t _ablSlewRate = 20;          // Default: 20% (slew rate for ABL dimming)
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
    /**
     * @brief Override to identify this as SPI config without RTTI
     */
    bool isSpiConfig() const override { return true; }

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

    // ===== RGB Value Clamping (Clone Chip Workaround) =====

    /**
     * @brief Set minimum RGB value (workaround for clone chip update bug)
     * @param minValue Minimum RGB value (0-255, default 0)
     * @note Some APA102/SK9822 clones don't update LEDs when RGB < 6-8.
     *       Set to 8 for APA102_CLONE protocol to prevent color artifacts.
     */
    void setMinRgbValue(uint8_t minValue) { _minRgbValue = minValue; }

    /**
     * @brief Get minimum RGB value
     * @return Minimum RGB value (0=no clamping, 8=clone chip workaround)
     */
    uint8_t getMinRgbValue() const { return _minRgbValue; }

    /**
     * @brief Set maximum RGB value (for power limiting)
     * @param maxValue Maximum RGB value (0-255, default 255)
     * @note Can limit max RGB to reduce power consumption
     */
    void setMaxRgbValue(uint8_t maxValue) { _maxRgbValue = maxValue; }

    /**
     * @brief Get maximum RGB value
     * @return Maximum RGB value (255=no clamping)
     */
    uint8_t getMaxRgbValue() const { return _maxRgbValue; }

    /**
     * @brief Set hardware brightness range (for clone chip compatibility)
     * @param min Minimum brightness (0-31, default 0)
     * @param max Maximum brightness (0-31, default 31)
     * @note Clone chips: min=16 (prevents flicker), max=30 (prevents sync issues)
     */
    void setHwBrightnessRange(uint8_t min, uint8_t max)
    {
        if (min > 31) min = 31;
        if (max > 31) max = 31;
        if (min > max) min = max;
        _hwBrightnessMin = min;
        _hwBrightnessMax = max;
        _hwBrightnessDefault = (min + max) / 2;
        // Clamp current brightness to new range
        if (_hwBrightness < _hwBrightnessMin) _hwBrightness = _hwBrightnessMin;
        if (_hwBrightness > _hwBrightnessMax) _hwBrightness = _hwBrightnessMax;
    }

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
    uint8_t _minRgbValue = 0;                        // Default: 0 (no clamping). Clone chips may need 6-8
    uint8_t _maxRgbValue = 255;                      // Default: 255 (no clamping). Can limit for power management
};

#include "LevelShifterType.h"

/**
 * @brief Configuration for 1-wire LED strips (WS2812B/SK6812/etc.)
 *
 * Serial strips use embedded clock in data signal with precise timing requirements.
 * This config manages timing modes and custom timing parameters.
 */
struct SerialStripConfig : public PhysicalStripConfig
{
    /**
     * @brief Override to identify this as Serial config without RTTI
     */
    bool isSerialConfig() const override { return true; }

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

    // ===== Effective Timing (resolved values for display / driver use) =====

    /**
     * @brief Resolved timing values — either from custom override or protocol defaults.
     * Used by drivers and the console to display/apply actual ns values.
     *
     * For PIO the ratios T0H:T0L:T1H:T1L = 3:7:6:4 are fixed by the PIO program.
     * Setting T1H overrides the bit period; all other values are derived.
     * For RMT all four values are independent.
     */
    struct EffectiveTimingNs
    {
        uint16_t t0h;     ///< T0H in ns (0-bit high time)
        uint16_t t0l;     ///< T0L in ns (0-bit low time)
        uint16_t t1h;     ///< T1H in ns (1-bit high time)
        uint16_t t1l;     ///< T1L in ns (1-bit low time)
        uint32_t resetUs; ///< Reset/latch time in µs
        bool isCustom;    ///< true = user override active, false = protocol default
    };

    /**
     * @brief Return effective timing values for the given protocol.
     *
     * When custom values are stored (_t0h > 0) they are returned directly.
     * Otherwise, standard protocol defaults are used.
     *
     * @param protocol LED protocol (determines defaults when in AUTO mode)
     * @return EffectiveTimingNs with resolved timing values
     */
    EffectiveTimingNs getEffectiveTimings(LedProtocol protocol) const
    {
        if (_t0h > 0)
        {
            // Custom values set — return as-is
            return { _t0h, _t0l, _t1h, _t1l, _resetTime, true };
        }

        // Protocol defaults
        switch (protocol)
        {
            case LedProtocol::SK6812:
            case LedProtocol::SK6812_RGBCCT:
                return { 300, 900, 600, 600, _resetTime > 0 ? _resetTime : 80, false };

            case LedProtocol::WS2811:
                return { 500, 2000, 1200, 1300, _resetTime > 0 ? _resetTime : 50, false };

            case LedProtocol::WS2812:
            case LedProtocol::WS2812B:
            case LedProtocol::WS2805_RGBCCT:
            default:
                return { 300, 900, 750, 600, _resetTime > 0 ? _resetTime : 50, false };
        }
    }

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

    // ===== Level Shifter =====

    /**
     * @brief Set level-shifter type for this strip's data line
     * @param type Level-shifter type (NONE or TXS0108E)
     * @note Driver applies GPIO optimizations on next applyConfig() call
     */
    void setLevelShifter(LevelShifterType type) { _levelShifter = type; }

    /**
     * @brief Get configured level-shifter type
     * @return LevelShifterType (NONE by default)
     */
    LevelShifterType getLevelShifter() const { return _levelShifter; }

  private:
    TimingMode       _timingMode    = TimingMode::AUTO;       // Default: 800kHz standard
    uint16_t         _t0h           = 0;                      // Default: 0 = auto-calculate
    uint16_t         _t0l           = 0;                      // Default: 0 = auto-calculate
    uint16_t         _t1h           = 0;                      // Default: 0 = auto-calculate
    uint16_t         _t1l           = 0;                      // Default: 0 = auto-calculate
    uint32_t         _resetTime     = 0;                      // Default: 0 = auto-calculate (typically 50us)
    LevelShifterType _levelShifter  = kHwDefaultLevelShifter; ///< Default from NEOPIXEL_HW_LEVELSHIFTER build flag (NONE if unset)
};
