/**
 * @file HclManager.h
 * @brief Human Centric Lighting (HCL) Curve Manager Implementation
 *
 * This module calculates and applies color temperature adjustments based on time-of-day or sun position.
 * Each segment can have its own HCL configuration or use global settings.
 * Features:
 * - Fixed time-based curves (configurable start/end times)
 * - Sun position-based curves (sunrise/sunset offsets)
 * - Configurable min/max color temperature (Kelvin)
 * - Slew rate limiting for smooth transitions
 * - Brightness compensation
 * - Per-segment or global application
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * HCL Mode - determines how HCL is applied to a segment
 */
enum class HclMode : uint8_t
{
    Disabled = 0, // HCL disabled for this segment
    Global = 1,   // Use global HCL settings
    Custom = 2    // Use segment-specific HCL settings
};

/**
 * HCL Curve Type - determines how the curve is calculated
 */
enum class HclCurveType : uint8_t
{
    FixedTime = 0,   // Fixed start/end times
    SunPosition = 1, // Based on sunrise/sunset with offsets
    Manual = 2       // Manual Kelvin value (via KO)
};

/**
 * HCL Apply Mode - determines which colors are affected
 */
enum class HclApplyMode : uint8_t
{
    AllColors = 0,     // Apply to all colors
    WhiteOnly = 1,     // Apply only to white LEDs
    HighSaturation = 2 // Apply only to low-saturation (nearly white) colors
};

/**
 * @brief HCL Configuration per Segment
 *
 * Stores all parameters needed for HCL curve calculation
 */
struct HclConfig
{
    // Mode settings
    HclMode mode = HclMode::Global;
    HclCurveType curveType = HclCurveType::FixedTime;
    HclApplyMode applyMode = HclApplyMode::AllColors;

    // Fixed time settings (curveType = FixedTime)
    uint8_t startHour = 6;
    uint8_t startMinute = 0;
    uint8_t endHour = 22;
    uint8_t endMinute = 0;

    // Sun position settings (curveType = SunPosition)
    int16_t sunriseOffsetMin = 0; // Offset in minutes (+/- from calculated sunrise)
    int16_t sunsetOffsetMin = 0;  // Offset in minutes (+/- from calculated sunset)

    // Color temperature range
    uint16_t minKelvin = 2700; // Warmest color temperature
    uint16_t maxKelvin = 6500; // Coldest color temperature

    // Application parameters
    uint8_t strength = 100;               // HCL effect strength (0-100%)
    uint8_t slewRate = 100;               // Kelvin change rate (K/min)
    uint8_t saturationThreshold = 64;     // Saturation threshold for HighSaturation mode
    uint8_t brightnessCompensation = 100; // Brightness compensation (0-100%)
    uint8_t whiteMix = 75;                // White LED mix percentage (0-100%)
    uint8_t preserveCurve = 1;            // Curve preservation mode (0=linear, 1=sigmoid, etc.)
};

/**
 * @brief HCL Manager - Per-Segment HCL Curve Management
 *
 * Calculates and applies color temperature adjustments based on time or sun position.
 * Each segment can have its own HCL configuration or use global settings.
 */
class HclManager
{
  public:
    /**
     * @brief Construct a new HCL Manager
     * @param config HCL configuration for this segment
     */
    HclManager(const HclConfig& config);

    /**
     * @brief Initialize the HCL manager
     * @param latitude Geographical latitude for sun calculations (optional)
     * @param longitude Geographical longitude for sun calculations (optional)
     */
    void begin(float latitude = 0.0f, float longitude = 0.0f);

    /**
     * @brief Update HCL calculation (call periodically)
     * @param currentTimeMs Current time in milliseconds
     */
    void loop(unsigned long currentTimeMs);

    /**
     * @brief Set current time for FixedTime and SunPosition curves
     * @param hour Current hour (0-23)
     * @param minute Current minute (0-59)
     * @note Must be called by OAM layer before loop() to provide time context
     */
    void setCurrentTime(uint8_t hour, uint8_t minute)
    {
        _currentHour = hour;
        _currentMinute = minute;
    }

    /**
     * @brief Get current target Kelvin value
     * @return uint16_t Current target color temperature in Kelvin
     */
    uint16_t getCurrentKelvin() const { return _currentKelvin; }

    /**
     * @brief Get current applied Kelvin value (after slew rate limiting)
     * @return uint16_t Applied color temperature in Kelvin
     */
    uint16_t getAppliedKelvin() const { return _appliedKelvin; }

    /**
     * @brief Check if HCL is currently active
     * @return true if HCL is enabled and applying
     */
    bool isActive() const { return _config.mode != HclMode::Disabled; }

    /**
     * @brief Set manual Kelvin value (for Manual curve type)
     * @param kelvin Color temperature in Kelvin
     */
    void setManualKelvin(uint16_t kelvin);

    /**
     * @brief Update configuration (from ETS parameter reload)
     * @param config New HCL configuration
     */
    void updateConfig(const HclConfig& config);

    /**
     * @brief Get HCL configuration (for console/debugging)
     * @return const reference to HclConfig
     */
    const HclConfig& getConfig() const { return _config; }

    /**
     * @brief Set sunrise/sunset times from external source (e.g., Timer module)
     * @param sunriseMin Sunrise time in minutes from midnight
     * @param sunsetMin Sunset time in minutes from midnight
     */
    void setSunTimes(uint16_t sunriseMin, uint16_t sunsetMin)
    {
        _sunriseMin = sunriseMin;
        _sunsetMin = sunsetMin;
        _sunTimesValid = true;
    }

    /**
     * @brief Get calculated sunrise time
     * @return uint16_t Sunrise in minutes from midnight (0 if not set)
     */
    uint16_t getSunriseMin() const { return _sunTimesValid ? _sunriseMin : 0; }

    /**
     * @brief Get calculated sunset time
     * @return uint16_t Sunset in minutes from midnight (0 if not set)
     */
    uint16_t getSunsetMin() const { return _sunTimesValid ? _sunsetMin : 0; }

    /**
     * @brief Check if sun times are valid (set by OAM layer)
     * @return true if sun times have been set
     */
    bool areSunTimesValid() const { return _sunTimesValid; }

    /**
     * @brief Calculate current time in minutes from midnight
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @return uint16_t Minutes from midnight
     */
    static uint16_t timeToMinutes(uint8_t hour, uint8_t minute);

    /**
     * @brief Apply HCL transformation to a single pixel
     * @param kelvin Target color temperature in Kelvin
     * @param config HCL configuration (apply mode, thresholds, etc.)
     * @param r Red component (in/out)
     * @param g Green component (in/out)
     * @param b Blue component (in/out)
     * @param ww Warm White component (in/out, nullptr for RGB strips)
     * @param cw Cool White component (in/out, nullptr for RGB/RGBW strips)
     */

  private:
    HclConfig _config;           // HCL configuration
    uint16_t _currentKelvin;     // Current target Kelvin
    uint16_t _appliedKelvin;     // Applied Kelvin (after slew rate)
    unsigned long _lastUpdateMs; // Last update timestamp

    float _latitude;  // Geographical latitude
    float _longitude; // Geographical longitude

    uint8_t _currentHour = 12;  // Current hour set by OAM layer (0-23)
    uint8_t _currentMinute = 0; // Current minute set by OAM layer (0-59)
    uint8_t _lastHour = 12;     // Last hour for time jump detection
    uint8_t _lastMinute = 0;    // Last minute for time jump detection

    uint16_t _sunriseMin = 0;    // Calculated sunrise in minutes from midnight
    uint16_t _sunsetMin = 0;     // Calculated sunset in minutes from midnight
    bool _sunTimesValid = false; // True if sun times have been set by OAM layer

    /**
     * @brief Calculate target Kelvin based on current time
     * @return uint16_t Target color temperature
     */
    uint16_t calculateTargetKelvin();

    /**
     * @brief Calculate Kelvin for fixed time curve
     * @return uint16_t Target color temperature
     */
    uint16_t calculateFixedTimeCurve();

    /**
     * @brief Calculate Kelvin for sun position curve
     * @return uint16_t Target color temperature
     */
    uint16_t calculateSunPositionCurve();

    /**
     * @brief Apply slew rate limiting to Kelvin transitions
     * @param targetKelvin Target Kelvin value
     * @param deltaTimeMs Time elapsed since last update
     */
    void applySlewRate(uint16_t targetKelvin, unsigned long deltaTimeMs);

    /**
     * @brief Calculate sunrise time in minutes from midnight
     * @return uint16_t Sunrise time
     */
    uint16_t calculateSunrise();

    /**
     * @brief Calculate sunset time in minutes from midnight
     * @return uint16_t Sunset time
     */
    uint16_t calculateSunset();
};
