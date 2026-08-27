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
    uint16_t redMA;       // Red channel max current (mA) at full brightness
    uint16_t greenMA;     // Green channel max current (mA)
    uint16_t blueMA;      // Blue channel max current (mA)
    uint16_t warmWhiteMA; // Warm white channel max current (mA), was 'whiteMA'
    uint16_t coolWhiteMA; // Cool white channel max current (mA), 0 for RGB/RGBW

    // Default: WS2812B typical values (RGB only)
    LedCurrentProfile()
        : redMA(20), greenMA(20), blueMA(20), warmWhiteMA(0), coolWhiteMA(0) {}

    // RGB-only constructor (backward compatible)
    LedCurrentProfile(uint16_t r, uint16_t g, uint16_t b)
        : redMA(r), greenMA(g), blueMA(b), warmWhiteMA(0), coolWhiteMA(0) {}

    // RGBW constructor (backward compatible - maps w to warmWhiteMA)
    LedCurrentProfile(uint16_t r, uint16_t g, uint16_t b, uint16_t w)
        : redMA(r), greenMA(g), blueMA(b), warmWhiteMA(w), coolWhiteMA(0) {}

    // RGBCCT constructor (5-channel)
    LedCurrentProfile(uint16_t r, uint16_t g, uint16_t b, uint16_t ww, uint16_t cw)
        : redMA(r), greenMA(g), blueMA(b), warmWhiteMA(ww), coolWhiteMA(cw) {}

    // Total max current per LED
    uint16_t totalMA() const
    {
        return redMA + greenMA + blueMA + warmWhiteMA + coolWhiteMA;
    }

    // Equality operator for profile comparison
    bool operator==(const LedCurrentProfile& other) const
    {
        return redMA == other.redMA && greenMA == other.greenMA &&
               blueMA == other.blueMA && warmWhiteMA == other.warmWhiteMA &&
               coolWhiteMA == other.coolWhiteMA;
    }
};

/**
 * Common LED Current Profiles
 */
namespace LedProfiles
{
    // WS2812B: 60mA max (20mA per channel, all on = white)
    static const LedCurrentProfile WS2812B(20, 20, 20);

    // SK6812 RGBW: Similar to WS2812B + white channel
    static const LedCurrentProfile SK6812_RGBW(20, 20, 20, 20);

    // APA102: Lower current per LED (configurable via global brightness)
    static const LedCurrentProfile APA102(15, 15, 15);

    // Conservative estimate (max current for 4-channel)
    static const LedCurrentProfile CONSERVATIVE(20, 20, 20, 20);

    // 5-Channel RGBCCT (RGB + Warm White + Cool White)
    static const LedCurrentProfile SK6812_RGBCCT(20, 20, 20, 20, 20);
    static const LedCurrentProfile WS2814_RGBCCT(20, 20, 20, 20, 20);
    static const LedCurrentProfile WS2805_RGBCCT(20, 20, 20, 20, 20);

    // Conservative estimate for 5-channel
    static const LedCurrentProfile CONSERVATIVE_5CH(20, 20, 20, 20, 20);

    /**
     * @brief Current profile for a protocol
     * @note A single global profile counted the white channels of an RGBW strip as 0 mA,
     *       so a mixed installation under-reported by a quarter on those strips.
     */
    inline LedCurrentProfile forProtocol(LedProtocol protocol)
    {
        if (ProtocolHelper::isSPI(protocol)) return APA102;
        switch (ProtocolHelper::getChannelCount(protocol))
        {
            case 6: // FW1906: RGB + CW + WW + unused
            case 5: return CONSERVATIVE_5CH;
            case 4: return SK6812_RGBW;
            default: return WS2812B;
        }
    }
} // namespace LedProfiles

/**
 * Power Limit Mode - how current limiting is applied
 */
enum class PowerLimitMode : uint8_t
{
    GLOBAL = 0,      // Limit total current across all strips (default)
    PER_CHANNEL = 1, // Limit current per physical output channel
    PER_LED = 2      // Limit current per individual LED
};

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
        : _maxCurrentMA(maxCurrentMA),
          _enabled(true),
          _profile(LedProfiles::WS2812B),
          _lastCalculatedCurrent(0),
          _lastActualCurrent(0),
          _mode(PowerLimitMode::GLOBAL),
          _maxCurrentPerChannel(0),
          _maxCurrentPerLed(0),
          _thresholdPercent(100),
          _maxBrightnessPercent(100),
          _slewRatePercentPerSec(0),
          _currentBrightnessPercent(100.0f),
          _targetBrightnessPercent(100.0f)
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
    /**
     * @brief Current per LED as configured in ETS (0 = not configured)
     *
     * profileForProtocol() splits this across the channels the protocol actually has,
     * so a 4-channel strip and a 5-channel strip on the same device each get it right.
     */
    void setCurrentPerLedMa(uint16_t currentPerLedMa) { _userCurrentPerLedMa = currentPerLedMa; }

    /**
     * @brief Current profile to measure a strip of this protocol with
     *
     * The ETS value wins when the user configured one; the per-protocol table is the
     * fallback. Measuring with the table while ETS says otherwise silently changed the
     * measured current, which moved the limiter threshold.
     */
    LedCurrentProfile profileForProtocol(LedProtocol protocol) const
    {
        if (_userCurrentPerLedMa == 0) return LedProfiles::forProtocol(protocol);

        const uint8_t channels = ProtocolHelper::getChannelCount(protocol);
        const uint16_t perChannel = (channels > 0) ? (uint16_t)(_userCurrentPerLedMa / channels) : 0;
        switch (channels)
        {
            case 5:
            case 6: return LedCurrentProfile(perChannel, perChannel, perChannel, perChannel, perChannel);
            case 4: return LedCurrentProfile(perChannel, perChannel, perChannel, perChannel);
            default: return LedCurrentProfile(perChannel, perChannel, perChannel, 0);
        }
    }

    bool isEnabled() const
    {
        return _enabled;
    }

    // ====================================================================
    // Power Limit Mode (Phase 1: Core Features)
    // ====================================================================

    /**
     * @brief Set power limiting mode
     * @param mode Power limit mode (GLOBAL, PER_CHANNEL, PER_LED)
     */
    void setPowerLimitMode(PowerLimitMode mode)
    {
        _mode = mode;
    }

    /**
     * @brief Get current power limiting mode
     * @return Power limit mode
     */
    PowerLimitMode getPowerLimitMode() const
    {
        return _mode;
    }

    /**
     * @brief Set maximum current per channel (for PER_CHANNEL mode)
     * @param mA Maximum current per physical output channel
     */
    void setMaxCurrentPerChannel(uint16_t mA)
    {
        _maxCurrentPerChannel = mA;
    }

    /**
     * @brief Get maximum current per channel
     * @return Maximum current per channel in mA
     */
    uint16_t getMaxCurrentPerChannel() const
    {
        return _maxCurrentPerChannel;
    }

    /**
     * @brief Set maximum current per LED (for PER_LED mode)
     * @param mA Maximum current per individual LED (typically 0-70mA)
     */
    void setMaxCurrentPerLed(uint8_t mA)
    {
        _maxCurrentPerLed = mA;
    }

    /**
     * @brief Get maximum current per LED
     * @return Maximum current per LED in mA
     */
    uint8_t getMaxCurrentPerLed() const
    {
        return _maxCurrentPerLed;
    }

    // ====================================================================
    // Soft Limiting (Phase 2: Threshold)
    // ====================================================================

    /**
     * @brief Set soft-limiting threshold percentage
     * @param percent Threshold (0-100%, default 100 = no soft limit)
     *
     * When current exceeds this percentage of the limit, start reducing
     * brightness gradually instead of hard-limiting.
     *
     * Example: With 1000mA limit and 80% threshold:
     *  - 0-800mA: No limiting
     *  - 800-1000mA: Gradual brightness reduction
     *  - >1000mA: Hard limit to 1000mA
     */
    void setThresholdPercent(uint8_t percent)
    {
        if (percent > 100) percent = 100;
        _thresholdPercent = percent;
    }

    /**
     * @brief Get soft-limiting threshold
     * @return Threshold percentage (0-100)
     */
    uint8_t getThresholdPercent() const
    {
        return _thresholdPercent;
    }

    // ====================================================================
    // Auto Brightness Limit (Phase 2: Brightness Cap)
    // ====================================================================

    /**
     * @brief Set maximum brightness cap
     * @param percent Maximum brightness (0-100%, default 100)
     *
     * Caps brightness regardless of power consumption. Useful for
     * preventing strips from being too bright in specific installations.
     */
    void setMaxBrightnessPercent(uint8_t percent)
    {
        if (percent > 100) percent = 100;
        _maxBrightnessPercent = percent;
    }

    /**
     * @brief Get maximum brightness cap
     * @return Maximum brightness percentage (0-100)
     */
    uint8_t getMaxBrightnessPercent() const
    {
        return _maxBrightnessPercent;
    }

    // ====================================================================
    // Slew Rate Control (Phase 3: Smooth Transitions)
    // ====================================================================

    /**
     * @brief Set brightness slew rate
     * @param percentPerSecond Rate of change (0-100%/s, 0 = instant)
     *
     * Controls how fast brightness changes when power limiting kicks in.
     * 0 = instant change, 100 = full range in 1 second.
     */
    void setBrightnessSlewRate(uint8_t percentPerSecond)
    {
        if (percentPerSecond > 100) percentPerSecond = 100;
        _slewRatePercentPerSec = percentPerSecond;
    }

    /**
     * @brief Get brightness slew rate
     * @return Slew rate in percent per second
     */
    uint8_t getBrightnessSlewRate() const
    {
        return _slewRatePercentPerSec;
    }

    /**
     * @brief Update slew rate controller
     * @param deltaTimeMs Time since last update in milliseconds
     *
     * Call this periodically to update the slew rate controller.
     * Updates _currentBrightnessPercent towards _targetBrightnessPercent.
     */
    void updateSlewRate(uint32_t deltaTimeMs)
    {
        if (_slewRatePercentPerSec == 0 || _currentBrightnessPercent == _targetBrightnessPercent)
        {
            return; // Instant or already at target
        }

        // Calculate maximum change allowed in this time step
        float maxChange = (_slewRatePercentPerSec * deltaTimeMs) / 1000.0f;

        // Move towards target, but not past it
        float diff = _targetBrightnessPercent - _currentBrightnessPercent;
        if (diff > maxChange)
        {
            _currentBrightnessPercent += maxChange;
        }
        else if (diff < -maxChange)
        {
            _currentBrightnessPercent -= maxChange;
        }
        else
        {
            _currentBrightnessPercent = _targetBrightnessPercent;
        }
    }

    /**
     * @brief Get current brightness after slew rate limiting
     * @return Current brightness (0.0-100.0)
     */
    float getCurrentBrightnessPercent() const
    {
        return _currentBrightnessPercent;
    }

    // ====================================================================
    // Current Calculation
    // ====================================================================

    /**
     * @brief Calculate current consumption for a single pixel (5-channel)
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param ww Warm white value (0-255)
     * @param cw Cool white value (0-255)
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     *                           For APA102/SK9822: 5-bit global brightness (0-31 mapped from 0-255)
     *                           For WS2812B/SK6812: ignored (always 255)
     * @return Current in mA
     */
    uint32_t calculatePixelCurrent(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw, uint8_t hardwareBrightness = 255) const
    {
        // For APA102/SK9822: Hardware brightness is a 5-bit global multiplier (0-31)
        // Convert 0-255 -> 0-31 scale, then apply as additional multiplier
        // Current = (RGB_Value / 255) × (HW_Brightness / 255) × MaxCurrent

        // Optimize: Combine both divisions into one
        // Instead of (r/255) * (hwb/255), we do r*hwb / (255*255)
        uint32_t hwScale = hardwareBrightness; // 0-255

        uint32_t current = 0;
        current += (r * _profile.redMA * hwScale) / 65025; // 65025 = 255*255
        current += (g * _profile.greenMA * hwScale) / 65025;
        current += (b * _profile.blueMA * hwScale) / 65025;
        current += (ww * _profile.warmWhiteMA * hwScale) / 65025;
        current += (cw * _profile.coolWhiteMA * hwScale) / 65025;
        return current;
    }

    /**
     * @brief Calculate current consumption for a single pixel (4-channel backward compat)
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @param w White value (0-255) - mapped to warm white
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @return Current in mA
     */
    uint32_t calculatePixelCurrent(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t hardwareBrightness) const
    {
        return calculatePixelCurrent(r, g, b, w, 0, hardwareBrightness);
    }

    /**
     * @brief Calculate current consumption for a single pixel (RGB only)
     */
    uint32_t calculatePixelCurrentRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t hardwareBrightness = 255) const
    {
        return calculatePixelCurrent(r, g, b, 0, 0, hardwareBrightness);
    }

    /**
     * @brief Calculate total current consumption for any LED protocol (universal function)
     * @param buffer Pointer to LED buffer (format depends on protocol)
     * @param numPixels Number of actual LEDs
     * @param protocol LED protocol (WS2812B, APA102, SK6812, etc.)
     * @param order Color order (for determining bytes per pixel)
     * @param hasDummyLed True if first LED frame should be skipped (APA102 dummy LED mode)
     * @return Total current in mA
     *
     * This function automatically handles different buffer formats:
     * - APA102/SK9822: Special format with start frame, brightness byte, BGR order
     * - WS2812B/SK6812/etc.: Standard RGB/RGBW/RGBCCT format
     */
    uint32_t calculateStripCurrent(const uint8_t* buffer, uint16_t numPixels, LedProtocol protocol,
                                   ColorOrder order, bool hasDummyLed = false) const
    {
        if (!buffer || numPixels == 0)
            return 0;

        // APA102/SK9822: Special buffer format
        if (protocol == LedProtocol::APA102 || protocol == LedProtocol::APA102_CLONE || protocol == LedProtocol::SK9822)
        {
            return calculateAPA102Current(buffer, numPixels, hasDummyLed);
        }

        // All other protocols: Standard RGB/RGBW/RGBCCT format
        // Determine bytes per pixel from ColorOrder
        uint8_t bytesPerPixel = 3; // Default RGB
        if (order == ColorOrder::RGBCCT || order == ColorOrder::GRBCCT ||
            order == ColorOrder::RGBCTW || order == ColorOrder::GRBCTW)
        {
            bytesPerPixel = 5; // 5-channel
        }
        else if (order == ColorOrder::RGBW || order == ColorOrder::GRBW)
        {
            bytesPerPixel = 4; // 4-channel
        }

        return calculateTotalCurrent(buffer, numPixels, bytesPerPixel, 255);
    }

    /**
     * @brief Calculate total current consumption for APA102/SK9822 strips
     * @param buffer Pointer to APA102 buffer (includes start frame, LED frames with brightness byte, end frame)
     * @param numPixels Number of actual LEDs (excludes dummy LED)
     * @param hasDummyLed True if first LED frame is dummy (skip it)
     * @return Total current in mA
     *
     * APA102/SK9822 Buffer Format:
     * - Start Frame: 4 bytes (0x00 0x00 0x00 0x00)
     * - LED Frames: Each LED = 4 bytes [0xE0|brightness, Blue, Green, Red]
     *   - Byte 0: 0xE0 + 5-bit brightness (0-31)
     *   - Byte 1: Blue
     *   - Byte 2: Green
     *   - Byte 3: Red
     * - End Frame: 4+ bytes
     */
    uint32_t calculateAPA102Current(const uint8_t* buffer, uint16_t numPixels, bool hasDummyLed = true) const
    {
        if (!buffer || numPixels == 0)
            return 0;

        uint32_t totalCurrent = 0;

        // Skip start frame (4 bytes)
        size_t offset = 4;

        // Skip dummy LED if present (sacrifice LED #0)
        if (hasDummyLed)
        {
            offset += 4;
        }

        // Process each LED frame
        for (uint16_t i = 0; i < numPixels; i++)
        {
            // APA102 frame: [0xE0|brightness, B, G, R]
            uint8_t brightnessByte = buffer[offset];
            uint8_t b = buffer[offset + 1];
            uint8_t g = buffer[offset + 2];
            uint8_t r = buffer[offset + 3];

            // Extract 5-bit brightness (0-31) from brightness byte
            uint8_t brightness5bit = brightnessByte & 0x1F; // Mask lower 5 bits

            // Scale to 0-255 for calculatePixelCurrent
            // brightness5bit ranges 0-31, we scale to 0-255
            uint8_t hardwareBrightness = (brightness5bit * 255) / 31;

            // Calculate current for this pixel (APA102 is RGB, no white channels)
            totalCurrent += calculatePixelCurrent(r, g, b, 0, 0, hardwareBrightness);

            offset += 4; // Move to next LED frame
        }

        return totalCurrent;
    }

    /**
     * @brief Calculate total current consumption
     * @param pixels Pointer to pixel buffer (RGB, RGBW, or RGBCCT)
     * @param numPixels Number of pixels
     * @param bytesPerPixel 3 for RGB, 4 for RGBW, 5 for RGBCCT
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @return Total current in mA
     */
    /**
     * @brief Total current using an explicit profile instead of the configured one
     */
    uint32_t calculateTotalCurrent(const uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel,
                                   uint8_t hardwareBrightness, const LedCurrentProfile& profile) const
    {
        if (!pixels || numPixels == 0) return 0;

        uint32_t totalCurrent = 0;
        for (uint16_t i = 0; i < numPixels; i++)
        {
            const uint16_t offset = i * bytesPerPixel;
            const uint8_t ww = (bytesPerPixel >= 4) ? pixels[offset + 3] : 0;
            const uint8_t cw = (bytesPerPixel >= 5) ? pixels[offset + 4] : 0;
            const uint32_t hw = hardwareBrightness;
            totalCurrent += (pixels[offset] * profile.redMA * hw) / 65025;
            totalCurrent += (pixels[offset + 1] * profile.greenMA * hw) / 65025;
            totalCurrent += (pixels[offset + 2] * profile.blueMA * hw) / 65025;
            totalCurrent += (ww * profile.warmWhiteMA * hw) / 65025;
            totalCurrent += (cw * profile.coolWhiteMA * hw) / 65025;
        }
        return totalCurrent;
    }

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
            uint8_t ww = (bytesPerPixel >= 4) ? pixels[offset + 3] : 0;
            uint8_t cw = (bytesPerPixel >= 5) ? pixels[offset + 4] : 0;

            totalCurrent += calculatePixelCurrent(r, g, b, ww, cw, hardwareBrightness);
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
     *
     * Enhanced with soft-limiting threshold and auto brightness cap.
     */
    float calculateBrightnessScale(const uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255) const
    {
        if (!_enabled)
            return 1.0f;

        // Apply auto brightness limit cap first
        float brightnessCapScale = _maxBrightnessPercent / 100.0f;

        // Calculate total current based on mode
        uint32_t totalCurrent = calculateTotalCurrent(pixels, numPixels, bytesPerPixel, hardwareBrightness);

        // Determine effective limit based on mode
        uint32_t effectiveLimit = _maxCurrentMA;
        if (_mode == PowerLimitMode::PER_CHANNEL && _maxCurrentPerChannel > 0)
        {
            // For per-channel mode, limit is per channel (simplified: assume 1 channel here)
            effectiveLimit = _maxCurrentPerChannel;
        }
        else if (_mode == PowerLimitMode::PER_LED && _maxCurrentPerLed > 0)
        {
            // For per-LED mode, limit is per LED × number of LEDs
            effectiveLimit = (uint32_t)_maxCurrentPerLed * numPixels;
        }

        // Calculate threshold (where soft-limiting starts)
        uint32_t thresholdCurrent = (effectiveLimit * _thresholdPercent) / 100;

        // No limiting needed if below threshold
        if (totalCurrent <= thresholdCurrent)
        {
            return brightnessCapScale; // Only apply brightness cap
        }

        // Calculate power scale factor
        float powerScale = 1.0f;

        if (totalCurrent <= effectiveLimit)
        {
            // Soft-limiting region (threshold to limit)
            // Apply gradual reduction using smooth curve
            // Scale linearly from 1.0 at threshold to (threshold/limit) at limit
            float excess = (float)(totalCurrent - thresholdCurrent);
            float range = (float)(effectiveLimit - thresholdCurrent);
            if (range > 0)
            {
                float t = excess / range; // 0.0 at threshold, 1.0 at limit
                // Smooth curve: start at 1.0, end at threshold/limit ratio
                float targetScale = (float)thresholdCurrent / (float)totalCurrent;
                powerScale = 1.0f - t * (1.0f - targetScale);
            }
        }
        else
        {
            // Hard limit (above limit)
            powerScale = (float)effectiveLimit / (float)totalCurrent;
        }

        // Combine power scale with brightness cap (take minimum)
        return (powerScale < brightnessCapScale) ? powerScale : brightnessCapScale;
    }

    /**
     * @brief Apply brightness scaling to pixel buffer (modifies in-place)
     * @param pixels Pointer to pixel buffer (will be modified)
     * @param numPixels Number of pixels
     * @param bytesPerPixel 3 for RGB, 4 for RGBW, 5 for RGBCCT
     * @param hardwareBrightness Hardware brightness (0-255, default 255 = full)
     * @param deltaTimeMs Time since last update (for slew rate, 0 = no slew)
     * @return true if scaling was applied, false if not needed
     *
     * Enhanced with slew rate control for smooth transitions.
     */
    bool applyCurrentLimit(uint8_t* pixels, uint16_t numPixels, uint8_t bytesPerPixel, uint8_t hardwareBrightness = 255, uint32_t deltaTimeMs = 0)
    {
        // Measure first, regardless of _enabled. Reporting the consumption and limiting it
        // are separate jobs; returning zero when limiting is off left the console showing
        // 0 mA on a running installation.
        uint32_t totalCurrent = calculateTotalCurrent(pixels, numPixels, bytesPerPixel, hardwareBrightness);
        _lastCalculatedCurrent = totalCurrent;
        _lastActualCurrent = totalCurrent;

        if (!_enabled) return false;

        // Calculate target scale factor
        float targetScale = calculateBrightnessScale(pixels, numPixels, bytesPerPixel, hardwareBrightness);
        _targetBrightnessPercent = targetScale * 100.0f;

        // Apply slew rate if enabled
        if (_slewRatePercentPerSec > 0 && deltaTimeMs > 0)
        {
            updateSlewRate(deltaTimeMs);
            targetScale = _currentBrightnessPercent / 100.0f;
        }
        else
        {
            // Instant change - update current to match target
            _currentBrightnessPercent = _targetBrightnessPercent;
        }

        if (targetScale >= 1.0f)
        {
            // No limiting needed - actual = requested
            _lastActualCurrent = totalCurrent;
            return false;
        }

        // Scale all pixel values
        for (uint16_t i = 0; i < numPixels; i++)
        {
            uint16_t offset = i * bytesPerPixel;
            pixels[offset] = (uint8_t)(pixels[offset] * targetScale);         // R
            pixels[offset + 1] = (uint8_t)(pixels[offset + 1] * targetScale); // G
            pixels[offset + 2] = (uint8_t)(pixels[offset + 2] * targetScale); // B
            if (bytesPerPixel >= 4)
            {
                pixels[offset + 3] = (uint8_t)(pixels[offset + 3] * targetScale); // WW
            }
            if (bytesPerPixel >= 5)
            {
                pixels[offset + 4] = (uint8_t)(pixels[offset + 4] * targetScale); // CW
            }
        }

        // Actual current after limiting = requested * scale
        _lastActualCurrent = (uint32_t)(totalCurrent * targetScale);

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

    // ====================================================================
    // Helper Methods
    // ====================================================================

    /**
     * @brief Get default LED current profile for a protocol
     * @param protocol LED protocol
     * @return Default current profile for the protocol
     *
     * Auto-detects appropriate current profile based on LED strip type.
     */
    static LedCurrentProfile getDefaultProfile(LedProtocol protocol)
    {
        switch (protocol)
        {
            // 4-channel RGBW
            case LedProtocol::SK6812:
            case LedProtocol::SK6805:
            case LedProtocol::WS2814:
            case LedProtocol::TM1814:
                return LedProfiles::SK6812_RGBW;

            // 5-channel RGBCCT (RGB + Warm White + Cool White)
            case LedProtocol::SK6812_RGBCCT:
                return LedProfiles::SK6812_RGBCCT;
            case LedProtocol::WS2814_RGBCCT:
                return LedProfiles::WS2814_RGBCCT;
            case LedProtocol::WS2805_RGBCCT:
                return LedProfiles::WS2805_RGBCCT;

            // SPI protocols
            case LedProtocol::APA102:
            case LedProtocol::SK9822:
                return LedProfiles::APA102;

            // Default: 3-channel RGB
            case LedProtocol::WS2812B:
            default:
                return LedProfiles::WS2812B;
        }
    }

  private:
    // Core settings
    uint32_t _maxCurrentMA;     // Maximum allowed current (mA) - GLOBAL mode
    bool _enabled;              // Enable/disable power management
    uint16_t _userCurrentPerLedMa = 0; // ETS current per LED, 0 = use the per-protocol table
    LedCurrentProfile _profile; // Current profile for LEDs

    // Statistics
    mutable uint32_t _lastCalculatedCurrent; // Cached current from last calculation (BEFORE limiting)
    mutable uint32_t _lastActualCurrent;     // Cached current AFTER limiting was applied

    // Phase 1: Power Limit Modes
    PowerLimitMode _mode;           // Power limiting mode
    uint16_t _maxCurrentPerChannel; // Maximum current per channel (PER_CHANNEL mode)
    uint8_t _maxCurrentPerLed;      // Maximum current per LED (PER_LED mode)

    // Phase 2: Soft Limiting & Brightness Cap
    uint8_t _thresholdPercent;     // Soft-limiting threshold (0-100%, default 100)
    uint8_t _maxBrightnessPercent; // Maximum brightness cap (0-100%, default 100)

    // Phase 3: Slew Rate Control
    uint8_t _slewRatePercentPerSec;  // Brightness change rate (%/s, 0 = instant)
    float _currentBrightnessPercent; // Current brightness (slew-rate limited)
    float _targetBrightnessPercent;  // Target brightness (from power calc)
};
