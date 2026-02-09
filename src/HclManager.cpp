#include "HclManager.h"
#include <math.h>

// Constructor
HclManager::HclManager(const HclConfig& config)
    : _config(config), _currentKelvin(config.maxKelvin), _appliedKelvin(config.maxKelvin), _lastUpdateMs(0), _latitude(0.0f), _longitude(0.0f)
{
}

// Initialize
void HclManager::begin(float latitude, float longitude)
{
    _latitude = latitude;
    _longitude = longitude;
    _lastUpdateMs = millis();

    // Calculate initial Kelvin value
    _currentKelvin = calculateTargetKelvin();
    _appliedKelvin = _currentKelvin;
}

// Main loop - update HCL calculation
void HclManager::loop(unsigned long currentTimeMs)
{
    if (_config.mode == HclMode::Disabled)
    {
        return; // HCL disabled
    }

    // Calculate new target Kelvin
    uint16_t targetKelvin = calculateTargetKelvin();

    // Calculate time delta in milliseconds AND detect time jumps
    unsigned long deltaTimeMs = currentTimeMs - _lastUpdateMs;

    // Time jump detection: Check if actual time (hour/minute) changed significantly
    // Convert current and last time to minutes since midnight for comparison
    uint16_t currentMinutesSinceMidnight = (_currentHour * 60) + _currentMinute;
    uint16_t lastMinutesSinceMidnight = (_lastHour * 60) + _lastMinute;

    // Calculate absolute time difference (handling day wrap-around)
    int16_t timeDiffMinutes = currentMinutesSinceMidnight - lastMinutesSinceMidnight;
    if (timeDiffMinutes < -720) timeDiffMinutes += 1440; // Wrapped backwards
    if (timeDiffMinutes > 720) timeDiffMinutes -= 1440;  // Wrapped forwards

    // Time jump: If real time changed by >1 minute but millis only advanced <60s
    // This catches manual time changes (e.g., 12:00 → 23:00 via ETS/KNX)
    if (abs(timeDiffMinutes) > 1 && deltaTimeMs < 60000)
    {
        // Large time jump detected - apply target immediately without slew rate
        _appliedKelvin = targetKelvin;
        _currentKelvin = targetKelvin;
        _lastUpdateMs = currentTimeMs;
        _lastHour = _currentHour;
        _lastMinute = _currentMinute;
        return;
    }

    // Normal case: Apply slew rate limiting for gradual transitions
    applySlewRate(targetKelvin, deltaTimeMs);

    _lastUpdateMs = currentTimeMs;
    _lastHour = _currentHour;
    _lastMinute = _currentMinute;
}

// Calculate target Kelvin based on curve type
uint16_t HclManager::calculateTargetKelvin()
{
    switch (_config.curveType)
    {
        case HclCurveType::FixedTime:
            return calculateFixedTimeCurve();

        case HclCurveType::SunPosition:
            return calculateSunPositionCurve();

        case HclCurveType::Manual:
        default:
            return _currentKelvin; // Manual mode - no automatic calculation
    }
}

// Calculate Kelvin for fixed time curve
uint16_t HclManager::calculateFixedTimeCurve()
{
    // Use time set by OAM layer via setCurrentTime()
    uint16_t currentTimeMin = timeToMinutes(_currentHour, _currentMinute);
    uint16_t startTimeMin = timeToMinutes(_config.startHour, _config.startMinute);
    uint16_t endTimeMin = timeToMinutes(_config.endHour, _config.endMinute);

    // Handle wrap-around (e.g., 22:00 to 06:00 next day)
    if (endTimeMin < startTimeMin)
    {
        if (currentTimeMin < startTimeMin && currentTimeMin > endTimeMin)
        {
            // Night time - return min Kelvin
            return _config.minKelvin;
        }
        endTimeMin += 24 * 60; // Add 24 hours
        if (currentTimeMin < startTimeMin)
        {
            currentTimeMin += 24 * 60;
        }
    }

    // Before start time
    if (currentTimeMin < startTimeMin)
    {
        return _config.minKelvin;
    }

    // After end time
    if (currentTimeMin > endTimeMin)
    {
        return _config.minKelvin;
    }

    // During active period - calculate linear interpolation
    uint16_t totalDuration = endTimeMin - startTimeMin;

    // Protection: If duration is zero (start == end), return max Kelvin
    if (totalDuration == 0)
    {
        return _config.maxKelvin;
    }

    uint16_t midpointTime = startTimeMin + (totalDuration / 2);
    uint16_t kelvinRange = _config.maxKelvin - _config.minKelvin;
    uint16_t halfDuration = totalDuration / 2;

    // Protection: If halfDuration is zero, return appropriate boundary
    if (halfDuration == 0)
    {
        return _config.maxKelvin;
    }

    if (currentTimeMin < midpointTime)
    {
        // Rising (morning): min -> max
        uint16_t elapsed = currentTimeMin - startTimeMin;
        uint16_t kelvin = _config.minKelvin + ((kelvinRange * elapsed) / halfDuration);
        return constrain(kelvin, _config.minKelvin, _config.maxKelvin);
    }
    else
    {
        // Falling (evening): max -> min
        uint16_t elapsed = currentTimeMin - midpointTime;
        uint16_t kelvin = _config.maxKelvin - ((kelvinRange * elapsed) / halfDuration);
        return constrain(kelvin, _config.minKelvin, _config.maxKelvin);
    }
}

// Calculate Kelvin for sun position curve
uint16_t HclManager::calculateSunPositionCurve()
{
    // Calculate sunrise/sunset times
    uint16_t sunriseMin = calculateSunrise();
    uint16_t sunsetMin = calculateSunset();

    // Apply offsets
    sunriseMin += _config.sunriseOffsetMin;
    sunsetMin += _config.sunsetOffsetMin;

    // Use time set by OAM layer via setCurrentTime()
    uint16_t currentTimeMin = timeToMinutes(_currentHour, _currentMinute);

    // Before sunrise - min Kelvin
    if (currentTimeMin < sunriseMin)
    {
        return _config.minKelvin;
    }

    // After sunset - min Kelvin
    if (currentTimeMin > sunsetMin)
    {
        return _config.minKelvin;
    }

    // During day - calculate curve
    uint16_t totalDuration = sunsetMin - sunriseMin;

    // Protection: If duration is zero (sunrise == sunset), return max Kelvin
    if (totalDuration == 0)
    {
        return _config.maxKelvin;
    }

    uint16_t midpointTime = sunriseMin + (totalDuration / 2);
    uint16_t kelvinRange = _config.maxKelvin - _config.minKelvin;
    uint16_t halfDuration = totalDuration / 2;

    // Protection: If halfDuration is zero, return appropriate boundary
    if (halfDuration == 0)
    {
        return _config.maxKelvin;
    }

    if (currentTimeMin < midpointTime)
    {
        // Rising (morning): min -> max
        uint16_t elapsed = currentTimeMin - sunriseMin;
        uint16_t kelvin = _config.minKelvin + ((kelvinRange * elapsed) / halfDuration);
        return constrain(kelvin, _config.minKelvin, _config.maxKelvin);
    }
    else
    {
        // Falling (evening): max -> min
        uint16_t elapsed = currentTimeMin - midpointTime;
        uint16_t kelvin = _config.maxKelvin - ((kelvinRange * elapsed) / halfDuration);
        return constrain(kelvin, _config.minKelvin, _config.maxKelvin);
    }
}

// Apply slew rate limiting
void HclManager::applySlewRate(uint16_t targetKelvin, unsigned long deltaTimeMs)
{
    if (_config.slewRate == 0)
    {
        // No slew rate limiting - instant change
        _appliedKelvin = targetKelvin;
        _currentKelvin = targetKelvin;
        return;
    }

    // Calculate maximum change allowed in this time step
    // slewRate is in K/min
    float deltaTimeMin = deltaTimeMs / 60000.0f;
    int16_t maxChange = (int16_t)(_config.slewRate * deltaTimeMin);

    // Calculate difference
    int16_t diff = targetKelvin - _appliedKelvin;

    if (abs(diff) <= maxChange)
    {
        // Within slew rate - apply full change
        _appliedKelvin = targetKelvin;
    }
    else
    {
        // Limit change to slew rate
        if (diff > 0)
        {
            _appliedKelvin += maxChange;
        }
        else
        {
            _appliedKelvin -= maxChange;
        }
    }

    _currentKelvin = targetKelvin;
}

// Set manual Kelvin value
void HclManager::setManualKelvin(uint16_t kelvin)
{
    _currentKelvin = constrain(kelvin, _config.minKelvin, _config.maxKelvin);
}

// Update configuration
void HclManager::updateConfig(const HclConfig& config)
{
    _config = config;

    // Recalculate current Kelvin
    _currentKelvin = calculateTargetKelvin();
}

// Calculate time in minutes from midnight
uint16_t HclManager::timeToMinutes(uint8_t hour, uint8_t minute)
{
    return (uint16_t)hour * 60 + (uint16_t)minute;
}

// Calculate sunrise time - returns value set by OAM layer via setSunTimes()
uint16_t HclManager::calculateSunrise()
{
    if (_sunTimesValid)
    {
        return _sunriseMin;
    }
    else
    {
        // Fallback if sun times not set (e.g., no Timer module or time not synced)
        return timeToMinutes(6, 30); // 6:30 AM default
    }
}

// Calculate sunset time - returns value set by OAM layer via setSunTimes()
uint16_t HclManager::calculateSunset()
{
    if (_sunTimesValid)
    {
        return _sunsetMin;
    }
    else
    {
        // Fallback if sun times not set (e.g., no Timer module or time not synced)
        return timeToMinutes(18, 30); // 6:30 PM default
    }
}