/**
 * @file PowerManager.h
 * @brief Power/Current Management for NeoPixel LEDs
 *
 * Calculates power consumption and limits brightness to prevent
 * exceeding available current from power supply.
 *
 * Example:
 *   PowerManager pm(5000);  // 5A (5000mA) limit
 *   pm.setLedCurrent(60, 40, 40, 0);  // WS2812B: 60mA R, 40mA G, 40mA B per LED
 *   
 *   // In update loop:
 *   float scale = pm.calculateBrightnessScale(strips, numStrips);
 *   // Apply scale to all brightness values
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include <stdint.h>

/**
 * LED Current Profile - per-color current consumption
 */
struct LedCurrentProfile
{
    uint16_t redMA;    // Red channel max current (mA) at full brightness
    uint16_t greenMA;  // Green channel max current (mA)
    uint16_t blueMA;   // Blue channel max current (mA)
    uint16_t whiteMA;  // White channel max current (mA, 0 for RGB-only)

    // Default: WS2812B typical values
    LedCurrentProfile()
        : redMA(60), greenMA(40), blueMA(40), whiteMA(0) {}

    LedCurrentProfile(uint16_t r, uint16_t g, uint16_t b, uint16_t w = 0)
        : redMA(r), greenMA(g), blueMA(b), whiteMA(w) {}

    // Equality operator for profile comparison
    bool operator==(const LedCurrentProfile& other) const
    {
        return redMA == other.redMA && greenMA == other.greenMA &&
               blueMA == other.blueMA && whiteMA == other.whiteMA;
    }
};

/**
 * Common LED Current Profiles
 */
namespace LedProfiles
{
    // WS2812B: 60mA max (20mA per channel, all on = white)
    static const LedCurrentProfile WS2812B(20, 20, 20, 0);

    // SK6812 RGBW: Similar to WS2812B + white channel
    static const LedCurrentProfile SK6812_RGBW(20, 20, 20, 20);

    // APA102: Lower current per LED (configurable via global brightness)
    static const LedCurrentProfile APA102(15, 15, 15, 0);

    // Conservative estimate (max current)
    static const LedCurrentProfile CONSERVATIVE(20, 20, 20, 20);
}

/**
 * Power Manager - Current Limiting for LED Strips
 */
class PowerManager
{
  public:
    /**
     * @brief Constructor
     * @param maxCurrentMA Maximum allowed current in milliamps (mA)
     */
    PowerManager(uint32_t maxCurrentMA = 5000)
        : _maxCurrentMA(maxCurrentMA), _enabled(true), _profile(LedProfiles::WS2812B), _lastCalculatedCurrent(0), _lastActualCurrent(0)
    {
    }

    /**
     * @brief Set maximum allowed current
     * @param maxCurrentMA Maximum current in mA
     */
    void setMaxCurrent(uint32_t maxCurrentMA)
    {
        _maxCurrentMA = maxCurrentMA;
    }

    /**
     * @brief Get maximum allowed current
     * @return Maximum current in mA
     */
    uint32_t getMaxCurrent() const
    {
        return _maxCurrentMA;
    }

    /**
     * @brief Set LED current profile
     * @param profile Current profile for LEDs
     */
    void setLedProfile(const LedCurrentProfile& profile)
    {
        _profile = profile;
    }

    /**
     * @brief Get LED current profile
     * @return Current profile
     */
    LedCurrentProfile getLedProfile() const
    {
        return _profile;
    }

    /**
     * @brief Enable/disable power management
     * @param enabled true = limit current, false = no limiting
     */
    void setEnabled(bool enabled)
    {
        _enabled = enabled;
    }

    /**
     * @brief Check if power management is enabled
     * @return true if enabled
     */
    bool isEnabled() const
    {
        return _enabled;
    }

    /**
     * @brief Calculate current consumption for a single pixel
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param w White value (0-255)
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     *                           For APA102/SK9822: 5-bit global brightness (0-31 mapped from 0-255)
     *                           For WS2812B/SK6812: ignored (always 255)
     * @return Current in mA
     */
    uint32_t calculatePixelCurrent(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0, uint8_t hardwareBrightness = 255) const
    {
        // For APA102/SK9822: Hardware brightness is a 5-bit global multiplier (0-31)
        // Convert 0-255 -> 0-31 scale, then apply as additional multiplier
        // Current = (RGB_Value / 255) × (HW_Brightness / 255) × MaxCurrent
        
        // Optimize: Combine both divisions into one
        // Instead of (r/255) * (hwb/255), we do r*hwb / (255*255)
        uint32_t hwScale = hardwareBrightness; // 0-255
        
        uint32_t current = 0;
        current += (r * _profile.redMA * hwScale) / 65025;   // 65025 = 255*255
        current += (g * _profile.greenMA * hwScale) / 65025;
        current += (b * _profile.blueMA * hwScale) / 65025;
        current += (w * _profile.whiteMA * hwScale) / 65025;
        return current;
    }

    /**
     * @brief Calculate total current consumption
     * @param pixels Pointer to pixel buffer (RGB or RGBW)
     * @param numPixels Number of pixels
     * @param bytesPerPixel 3 for RGB, 4 for RGBW
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @return Total current in mA
     */
    uint32_t calculateTotalCurrent(const uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255) const
    {
        if (!pixels || numPixels == 0)
            return 0;

        uint32_t totalCurrent = 0;

        for (uint16_t i = 0; i < numPixels; i++)
        {
            uint16_t offset = i * bytesPerPixel;
            uint8_t r = pixels[offset];
            uint8_t g = pixels[offset + 1];
            uint8_t b = pixels[offset + 2];
            uint8_t w = (bytesPerPixel == 4) ? pixels[offset + 3] : 0;

            totalCurrent += calculatePixelCurrent(r, g, b, w, hardwareBrightness);
        }

        return totalCurrent;
    }

    /**
     * @brief Calculate brightness scale factor to stay within current limit
     * @param pixels Pointer to pixel buffer
     * @param numPixels Number of pixels
     * @param bytesPerPixel 3 for RGB, 4 for RGBW
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @return Scale factor (0.0 to 1.0)
     *         1.0 = no limiting needed
     *         <1.0 = reduce brightness by this factor
     */
    float calculateBrightnessScale(const uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255) const
    {
        if (!_enabled)
            return 1.0f;

        uint32_t totalCurrent = calculateTotalCurrent(pixels, numPixels, bytesPerPixel, hardwareBrightness);

        if (totalCurrent <= _maxCurrentMA)
            return 1.0f;

        // Scale down to fit within limit
        return (float)_maxCurrentMA / (float)totalCurrent;
    }

    /**
     * @brief Apply brightness scaling to pixel buffer (modifies in-place)
     * @param pixels Pointer to pixel buffer (will be modified)
     * @param numPixels Number of pixels
     * @param bytesPerPixel 3 for RGB, 4 for RGBW
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @return true if scaling was applied, false if not needed
     */
    bool applyCurrentLimit(uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255)
    {
        if (!_enabled)
        {
            _lastCalculatedCurrent = 0;
            _lastActualCurrent = 0;
            return false;
        }

        // Calculate BEFORE limiting (cache for getRequestedPower)
        uint32_t totalCurrent = calculateTotalCurrent(pixels, numPixels, bytesPerPixel, hardwareBrightness);
        _lastCalculatedCurrent = totalCurrent;

        if (totalCurrent <= _maxCurrentMA)
        {
            // No limiting needed - actual = requested
            _lastActualCurrent = totalCurrent;
            return false;
        }

        // Calculate scale factor
        float scale = (float)_maxCurrentMA / (float)totalCurrent;

        // Scale all pixel values
        for (uint16_t i = 0; i < numPixels; i++)
        {
            uint16_t offset = i * bytesPerPixel;
            pixels[offset] = (uint8_t)(pixels[offset] * scale);         // R
            pixels[offset + 1] = (uint8_t)(pixels[offset + 1] * scale); // G
            pixels[offset + 2] = (uint8_t)(pixels[offset + 2] * scale); // B
            if (bytesPerPixel == 4)
            {
                pixels[offset + 3] = (uint8_t)(pixels[offset + 3] * scale); // W
            }
        }

        // Actual current after limiting = requested * scale
        // Or simply: capped at max current
        _lastActualCurrent = _maxCurrentMA;

        return true; // Scaling applied
    }

    /**
     * @brief Get estimated power consumption in Watts
     * @param pixels Pointer to pixel buffer (NOT used - we use cached value)
     * @param numPixels Number of pixels (NOT used - we use cached value)
     * @param bytesPerPixel 3 for RGB, 4 for RGBW (NOT used)
     * @param hardwareBrightness Hardware brightness (NOT used)
     * @param voltage Supply voltage (default 5V)
     * @return Power in Watts (based on LAST calculated current before limiting)
     * 
     * NOTE: This returns the power based on the REQUESTED brightness (before limiting),
     *       not the actual scaled brightness. This shows what the LEDs WANT to draw,
     *       not what they actually draw after current limiting.
     *       Use getActualPowerWatts() to get real consumption after limiting.
     */
    float calculatePowerWatts(const uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255, float voltage = 5.0f) const
    {
        // Use cached value from last applyCurrentLimit() call
        // This gives us the REQUESTED power before limiting was applied
        return (_lastCalculatedCurrent / 1000.0f) * voltage;
    }

    /**
     * @brief Get ACTUAL power consumption after current limiting
     * @param voltage Supply voltage (default 5V)
     * @return Power in Watts (based on ACTUAL current after limiting)
     * 
     * This returns what the LEDs are ACTUALLY drawing after current limiting
     * has been applied. This is always <= calculatePowerWatts().
     */
    float getActualPowerWatts(float voltage = 5.0f) const
    {
        return (_lastActualCurrent / 1000.0f) * voltage;
    }

    /**
     * @brief Get last calculated current (before limiting)
     * @return Current in mA (requested)
     */
    uint32_t getLastCalculatedCurrent() const
    {
        return _lastCalculatedCurrent;
    }

    /**
     * @brief Get actual current (after limiting)
     * @return Current in mA (actual)
     */
    uint32_t getActualCurrent() const
    {
        return _lastActualCurrent;
    }

    /**
     * @brief Manually set cached current values (for global limiting)
     * @param requestedCurrent Requested current before limiting (mA)
     * @param actualCurrent Actual current after limiting (mA)
     * 
     * Used by NeoPixelManager when doing global current limiting across
     * multiple VirtualStrips. Allows updating statistics after manual scaling.
     */
    void setCachedCurrentValues(uint32_t requestedCurrent, uint32_t actualCurrent)
    {
        _lastCalculatedCurrent = requestedCurrent;
        _lastActualCurrent = actualCurrent;
    }

  private:
    uint32_t _maxCurrentMA;    // Maximum allowed current (mA)
    bool _enabled;             // Enable/disable power management
    LedCurrentProfile _profile; // Current profile for LEDs
    mutable uint32_t _lastCalculatedCurrent; // Cached current from last calculation (BEFORE limiting)
    mutable uint32_t _lastActualCurrent;     // Cached current AFTER limiting was applied
};
