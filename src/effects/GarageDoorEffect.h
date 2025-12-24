/**
 * @file GarageDoorEffect.h
 * @brief Garage Door Effect - 3-Phase Animation System
 *
 * Professional garage door opening/closing animation with three distinct phases:
 *
 * PHASE 1: Opening Arrow (Cylon-Style)
 *   - Smooth wandering arrow from center to edges
 *   - Separate left/right animation with trail fade
 *   - Perfect for door opening signal
 *
 * PHASE 2: Runway Lights
 *   - Airplane-style runway entrance lights
 *   - Configurable group width (1-10 LEDs)
 *   - Smooth wave from outside to inside
 *
 * PHASE 3: Success Breathing
 *   - Gentle breathing effect for completion
 *   - All LEDs pulsate in sync
 *   - "Safe parking confirmed" signal
 *
 * Optimized for SK6812 RGBW strips with intelligent white channel usage.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

// #########################################################
// ToDo: Complete documentation
// Know Bugs:
// - On Change of phase, a frame delay may occur
// - On phase change, some pixels may briefly flash
// - On Color change the pixel gets the correct color only after phase restart
//

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include <math.h>

/**
 * Garage Door Animation Phases
 */
enum class GaragePhase : uint8_t
{
    OPENING = 0,   // Arrow from center to edges (door opening)
    RUNWAY = 1,    // Runway lights (enter/exit guidance)
    COMPLETED = 2, // Breathing celebration (parking confirmed)
    STOPPED = 3    // Effect finished/idle
};

/**
 * GarageDoorEffect - Multi-phase garage animation
 *
 * Usage:
 *   GarageDoorEffect effect;
 *   effect.setPhase(GaragePhase::OPENING);  // Trigger door opening
 *   segment->setEffect(&effect);
 *   // ... later ...
 *   effect.setPhase(GaragePhase::RUNWAY);   // Switch to runway
 *   effect.setPhase(GaragePhase::SUCCESS);  // Show success
 */
/**
 * Config usage summary (per segment):
 *  - config.speed:     global speed scaling (0=auto/1.0x, otherwise 0.25x..3.0x)
 *  - config.intensity: master brightness scaling for all phase colors (0..255)
 *  - config.option1:   arrow size / trail length (0 = default)
 *  - config.option2:   runway group size (0 = default)
 *  - config.option3:   breathing speed override (0 = default)
 *  - config.reverse:   reverse direction (arrows move inward, runway moves backward)
 *  - config.feature1..3: currently unused (reserved)
 */
class GarageDoorEffect : public Effect
{
  private:
    // =============================================================================
    // Configuration (setters available) - SHARED across all segments
    // =============================================================================

    // Helper: map config.speed (0..255) to a multiplier for pixel-per-frame speeds
    static float speedMultiplierFromConfig(uint8_t speed)
    {
        if (speed == 0) return 1.0f;             // keep existing auto-scaling
        return 0.25f + (speed / 255.0f) * 2.75f; // 0.25x .. 3.0x
    }

    static uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi
                                        : v;
    }

    static float dtFactorFromMs(uint32_t deltaTime)
    {
        // Existing effect speeds were tuned for ~20 FPS (~50ms). Scale to be frame-rate independent.
        if (deltaTime == 0) return 1.0f;
        return (float)deltaTime / 50.0f;
    }

    // Phase 1: Opening Arrow
    uint8_t _arrowSize = 6;                // Arrow eye size (trail length)
    float _arrowSpeed = 0.0f;              // Auto-calculated if 0
    uint32_t _arrowColorRGBW = 0x00000000; // Default: Use segment color (black = fallback)
    uint16_t _openingDuration = 0;         // Duration in ms (0 = infinite loop)

    // Phase 2: Runway
    uint8_t _runwayGroupSize = 1;           // LEDs per group (1-10)
    float _runwaySpeed = 0.0f;              // Auto-calculated if 0
    uint32_t _runwayColorRGBW = 0x00000000; // Default: Use segment color (black = fallback)
    uint16_t _runwayDuration = 0;           // Duration in milliseconds (0 = infinite loop)

    // Phase 3: Success Breathing
    float _breathingSpeed = 0.05f;           // Breathing cycle speed (radians per frame)
    uint32_t _successColorRGBW = 0x00000000; // Default: Use segment color (black = fallback)
    uint16_t _breathingDuration = 0;         // Duration in milliseconds (0 = infinite loop)

    // =============================================================================
    // Runtime State - STORED PER SEGMENT (stateless design!)
    // =============================================================================

    // segment->getState() usage:
    // state.phase    = GaragePhase (OPENING=0, RUNWAY=1, COMPLETED=2, STOPPED=3)
    // state.position = Arrow/runway position
    // state.aux1     = Phase timer (ms elapsed since phase start)
    // state.aux2     = Breathing angle (0-36000, scaled by 100)
    // state.counter  = Initialized flag (0=not init, 1=initialized)

  public:
    /**
     * Constructor
     */
    GarageDoorEffect() {}

    /**
     * Get effect name
     */
    const char* getName() override { return "GarageDoor"; }

    const char* getDescription() override { return "Opening/closing garage door animation"; }
    /**
     * Update effect - called every frame
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();

        // Initialize phase on first run (if never set)
        if (state.lastUpdate == 0)
        {
            state.phase = (uint8_t)GaragePhase::STOPPED;
            state.lastUpdate = 1; // Mark as initialized
        }

        // Get current phase from segment state (stateless design!)
        GaragePhase currentPhase = (GaragePhase)state.phase;

        // Update phase timer
        state.aux1 += deltaTime;

        // Execute current phase
        switch (currentPhase)
        {
            case GaragePhase::OPENING:
                updateOpeningArrow(segment, deltaTime);
                // Auto-transition after duration (if set)
                if (_openingDuration > 0 && state.aux1 >= _openingDuration)
                {
                    setSegmentPhase(segment, GaragePhase::STOPPED);
                }
                break;

            case GaragePhase::RUNWAY:
                updateRunway(segment, deltaTime);
                // Auto-transition to COMPLETED after duration (if not infinite)
                if (_runwayDuration > 0 && state.aux1 >= _runwayDuration)
                {
                    setSegmentPhase(segment, GaragePhase::COMPLETED);
                }
                break;

            case GaragePhase::COMPLETED:
                updateSuccessBreathing(segment, deltaTime);
                // Auto-fade to STOPPED after duration (if not infinite)
                if (_breathingDuration > 0 && state.aux1 >= _breathingDuration)
                {
                    fadeToIdle(segment);
                    setSegmentPhase(segment, GaragePhase::STOPPED);
                }
                break;

            case GaragePhase::STOPPED:
            default:
                // Do nothing - effect idle
                break;
        }
    }

    /**
     * Check if effect is done
     */
    bool isDone(const Segment* segment) const override
    {
        if (!segment) return true;
        GaragePhase currentPhase = (GaragePhase)segment->getState().phase;
        return (currentPhase == GaragePhase::STOPPED);
    }

    // =============================================================================
    // Phase Control - PER SEGMENT (stateless!)
    // =============================================================================

    /**
     * Set animation phase for a specific segment
     * @param segment Target segment
     * @param phase Target phase to activate
     */
    void setSegmentPhase(Segment* segment, GaragePhase phase)
    {
        if (!segment) return;

        auto& state = segment->getState();
        state.phase = (uint8_t)phase;
        state.aux1 = 0;     // Reset timer
        state.position = 0; // Reset position for new phase
        state.counter = 0;  // Reset counter to trigger re-initialization
        state.aux2 = 0;     // Reset breathing angle
    }

    /**
     * Get current phase of a segment
     * @param segment Target segment
     * @return Current phase
     */
    GaragePhase getSegmentPhase(const Segment* segment) const
    {
        if (!segment) return GaragePhase::STOPPED;
        return (GaragePhase)segment->getState().phase;
    }

    /**
     * Legacy: Set phase globally (affects next segment that gets updated)
     * @deprecated Use setSegmentPhase() for per-segment control
     */
    void setPhase(GaragePhase phase)
    {
        _defaultPhase = phase;
    }

    /**
     * Legacy: Get default phase
     * @deprecated Use getSegmentPhase() for per-segment query
     */
    GaragePhase getPhase() const { return _defaultPhase; }

    // =============================================================================
    // Phase 1: Opening Arrow Configuration
    // =============================================================================

    /**
     * Set arrow size (eye + trail)
     * @param size Trail length in LEDs (default: 6)
     */
    void setArrowSize(uint8_t size) { _arrowSize = size; }

    /**
     * Set arrow speed
     * @param speed Pixels per frame (0 = auto-scale based on length)
     */
    void setArrowSpeed(float speed) { _arrowSpeed = speed; }

    /**
     * Set arrow color (RGBW packed)
     * @param rgbw 32-bit RGBW value (0xRRGGBBWW)
     */
    void setArrowColor(uint32_t rgbw) { _arrowColorRGBW = rgbw; }

    /**
     * Set arrow color (separate RGBW)
     */
    void setArrowColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        _arrowColorRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | w;
    }

    /**
     * Set opening duration
     * @param ms Duration in milliseconds (0 = infinite loop, default: 0)
     */
    void setOpeningDuration(uint16_t ms) { _openingDuration = ms; }

    // =============================================================================
    // Phase 2: Runway Configuration
    // =============================================================================

    /**
     * Set runway group size
     * @param size LEDs per group (1-10, default: 1)
     */
    void setRunwayGroupSize(uint8_t size)
    {
        _runwayGroupSize = clampValue(size, (uint8_t)1, (uint8_t)10);
    }

    /**
     * Set runway speed
     * @param speed Pixels per frame (0 = auto-scale)
     */
    void setRunwaySpeed(float speed) { _runwaySpeed = speed; }

    /**
     * Set runway duration
     * @param ms Duration in milliseconds (0 = infinite loop, default: 0)
     */
    void setRunwayDuration(uint16_t ms) { _runwayDuration = ms; }

    /**
     * Set runway color (RGBW packed)
     */
    void setRunwayColor(uint32_t rgbw) { _runwayColorRGBW = rgbw; }

    /**
     * Set runway color (separate RGBW)
     */
    void setRunwayColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        _runwayColorRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | w;
    }

    // =============================================================================
    // Phase 3: Success Breathing Configuration
    // =============================================================================

    /**
     * Set breathing speed
     * @param speed Breathing cycle speed (default: 0.05)
     */
    void setBreathingSpeed(float speed) { _breathingSpeed = speed; }

    /**
     * Set breathing duration
     * @param ms Duration in milliseconds (0 = infinite loop, default: 0)
     */
    void setBreathingDuration(uint16_t ms) { _breathingDuration = ms; }

    /**
     * Set success color (RGBW packed)
     */
    void setSuccessColor(uint32_t rgbw) { _successColorRGBW = rgbw; }

    /**
     * Set success color (separate RGBW)
     */
    void setSuccessColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        _successColorRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | w;
    }

  private:
    GaragePhase _defaultPhase = GaragePhase::STOPPED; // Legacy fallback

    // =============================================================================
    // Phase Implementations
    // =============================================================================

    /**
     * Phase 1: Opening Arrow - Cylon-style from center to edges
     *
     * Animation:
     * - Starts at center (LED count/2)
     * - Two arrows: one moving left, one moving right
     * - Smooth fade trail (Star Trek/Knight Rider style)
     * - Stops when reaching edges
     */
    void updateOpeningArrow(Segment* segment, uint32_t deltaTime)
    {
        auto& state = segment->getState();
        uint16_t length = segment->getLength();
        uint16_t center = length / 2;
        const auto& cfg = segment->getConfig();     // Snapshot config
        const uint8_t cfgSpeed = cfg.speed;         // speed (0=auto)
        const uint8_t cfgIntensity = cfg.intensity; // master brightness scaling (0..255)
        const uint8_t cfgArrowSize = cfg.option1;   // arrow size (0=default)
        const bool reverseDir = (cfg.reverse != 0); // reverse direction

        const uint8_t arrowSize = (cfgArrowSize == 0) ? _arrowSize : clampU8(cfgArrowSize, 1, 50);
        const float speedMul = speedMultiplierFromConfig(cfgSpeed);
        const float dtMul = dtFactorFromMs(deltaTime);

        // Calculate effective speed
        float effectiveSpeed = _arrowSpeed;
        if (effectiveSpeed == 0.0f)
        {
            // Auto-scale: 2-3 seconds from center to edge
            // At ~20 FPS: speed = (length/2) / (2.5 * 20) = length / 100
            effectiveSpeed = (float)length / 100.0f;
            if (effectiveSpeed < 1.0f) effectiveSpeed = 1.0f; // Minimum 1 pixel/frame
        }

        effectiveSpeed *= speedMul;
        effectiveSpeed *= dtMul;

        // Initialize on first run OR when returning to this phase
        if (state.counter == 0)
        {
            state.position = 0; // Start at offset 0 (will add to center)
            state.counter = 1;  // Mark initialized
        }

        // Fade all LEDs
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);

            r = FastLEDMath::fadeToBlackBy(r, 50);
            g = FastLEDMath::fadeToBlackBy(g, 50);
            b = FastLEDMath::fadeToBlackBy(b, 50);

            segment->setPixel(i, r, g, b);
        }

        // Extract RGBW color - use segment color if effect color not set
        uint8_t arrowR = (_arrowColorRGBW >> 24) & 0xFF;
        uint8_t arrowG = (_arrowColorRGBW >> 16) & 0xFF;
        uint8_t arrowB = (_arrowColorRGBW >> 8) & 0xFF;
        uint8_t arrowW = _arrowColorRGBW & 0xFF;

        // Fallback to segment color if effect color is black
        if (arrowR == 0 && arrowG == 0 && arrowB == 0 && arrowW == 0)
        {
            arrowR = segment->getConfig().r();
            arrowG = segment->getConfig().g();
            arrowB = segment->getConfig().b();
            arrowW = segment->getConfig().w();

            // Apply master intensity scaling (0..255)
            arrowR = FastLEDMath::scale8(arrowR, cfgIntensity);
            arrowG = FastLEDMath::scale8(arrowG, cfgIntensity);
            arrowB = FastLEDMath::scale8(arrowB, cfgIntensity);
            arrowW = FastLEDMath::scale8(arrowW, cfgIntensity);
        }

        // Current arrow positions: distance from center (0..maxDist)
        // For even lengths, maxDist is reduced by 1 so the right side stays within [0..length-1].
        float maxDist = (float)center;
        if ((center * 2) == length && maxDist > 0.0f) maxDist -= 1.0f;

        float dist = reverseDir ? (maxDist - (float)state.position) : (float)state.position;
        if (dist < 0.0f) dist = 0.0f;

        float leftPos = (float)center - dist;
        float rightPos = (float)center + dist;

        // Draw left arrow (moving left from center)
        drawArrowEye(segment, leftPos, arrowR, arrowG, arrowB, arrowW, arrowSize);

        // Draw right arrow (moving right from center)
        drawArrowEye(segment, rightPos, arrowR, arrowG, arrowB, arrowW, arrowSize);

        // Move arrows outward
        float newPos = (float)state.position + effectiveSpeed;
        state.position = (uint16_t)newPos;

        // Loop: When arrows reach edges, restart from center
        if (state.position >= (uint16_t)center)
        {
            state.position = 0; // Restart animation (loop)
            state.counter = 0;  // Trigger re-init next frame
        }
    }

    /**
     * Draw arrow eye with trail at given position
     */
    void drawArrowEye(Segment* segment, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t arrowSize)
    {
        uint16_t length = segment->getLength();
        uint16_t pixelPos = (uint16_t)position;

        for (uint8_t i = 0; i < arrowSize; i++)
        {
            int16_t pos = pixelPos + i - (arrowSize / 2);
            if (pos >= 0 && pos < length)
            {
                // Calculate brightness falloff for smooth eye
                uint8_t brightness = 255;
                if (i == 0 || i == arrowSize - 1)
                {
                    brightness = 128; // Dimmer edges
                }
                else if (i == 1 || i == arrowSize - 2)
                {
                    brightness = 200; // Medium edges
                }

                uint8_t scaledR = FastLEDMath::scale8(r, brightness);
                uint8_t scaledG = FastLEDMath::scale8(g, brightness);
                uint8_t scaledB = FastLEDMath::scale8(b, brightness);
                uint8_t scaledW = FastLEDMath::scale8(w, brightness);

                // Use RGBW if white channel provided, otherwise RGB
                if (w > 0)
                {
                    segment->setPixel(pos, scaledR, scaledG, scaledB, scaledW);
                }
                else
                {
                    segment->setPixel(pos, scaledR, scaledG, scaledB);
                }
            }
        }
    }

    /**
     * Phase 2: Runway Lights - Theater chase from outside to inside
     *
     * Animation:
     * - Smooth wave of LEDs moving from edge to center
     * - Configurable group width
     * - Like airplane runway lights
     */
    void updateRunway(Segment* segment, uint32_t deltaTime)
    {
        auto& state = segment->getState();
        uint16_t length = segment->getLength();
        const auto& cfg = segment->getConfig();
        const uint8_t cfgSpeed = cfg.speed;         // speed (0=auto)
        const uint8_t cfgIntensity = cfg.intensity; //  master brightness scaling (0..255)
        const uint8_t cfgGroupSize = cfg.option2;   // runway group size (0=default)
        const bool reverseDir = (cfg.reverse != 0); // reverse direction

        const uint8_t groupSize = (cfgGroupSize == 0) ? _runwayGroupSize : clampU8(cfgGroupSize, 1, 20);
        const float speedMul = speedMultiplierFromConfig(cfgSpeed);
        const float dtMul = dtFactorFromMs(deltaTime);

        // Calculate effective speed
        float effectiveSpeed = _runwaySpeed;
        if (effectiveSpeed == 0.0f)
        {
            if (_runwayDuration > 0)
            {
                // Auto-scale: duration-based speed
                // speed = length / (duration_ms / frame_interval_ms)
                // At 20 FPS (50ms/frame): speed = length / (duration / 50)
                effectiveSpeed = (float)length / ((float)_runwayDuration / 50.0f);
                if (effectiveSpeed < 1.0f) effectiveSpeed = 1.0f;
            }
            else
            {
                // Infinite loop: use fixed speed
                // ~2-3 seconds per cycle at 20 FPS: speed = length / (2.5 * 20) = length / 50
                effectiveSpeed = (float)length / 50.0f;
                if (effectiveSpeed < 1.0f) effectiveSpeed = 1.0f; // Minimum 1 pixel/frame
            }

            effectiveSpeed *= speedMul;
            effectiveSpeed *= dtMul;
        }

        // Initialize on first run OR when returning to this phase
        if (state.counter == 0)
        {
            state.position = 0; // Start from edge
            state.counter = 1;  // Mark initialized
        }

        // Clear all
        segment->clearAll();

        // Extract RGBW color - use segment color if effect color not set
        uint8_t runwayR = (_runwayColorRGBW >> 24) & 0xFF;
        uint8_t runwayG = (_runwayColorRGBW >> 16) & 0xFF;
        uint8_t runwayB = (_runwayColorRGBW >> 8) & 0xFF;
        uint8_t runwayW = _runwayColorRGBW & 0xFF;

        // Fallback to segment color if effect color is black
        if (runwayR == 0 && runwayG == 0 && runwayB == 0 && runwayW == 0)
        {
            runwayR = segment->getConfig().r();
            runwayG = segment->getConfig().g();
            runwayB = segment->getConfig().b();
            runwayW = segment->getConfig().w();

            // Apply master intensity scaling (0..255)
            runwayR = FastLEDMath::scale8(runwayR, cfgIntensity);
            runwayG = FastLEDMath::scale8(runwayG, cfgIntensity);
            runwayB = FastLEDMath::scale8(runwayB, cfgIntensity);
            runwayW = FastLEDMath::scale8(runwayW, cfgIntensity);
        }

        // Draw runway wave
        uint16_t wavePos = state.position;
        for (uint8_t g = 0; g < groupSize; g++)
        {
            uint16_t pos = reverseDir ? (uint16_t)((wavePos + length - (g % length)) % length) : (uint16_t)((wavePos + g) % length);
            {
                // Brightness gradient within group
                uint8_t brightness = 255;
                if (groupSize > 1)
                {
                    // Center of group brightest
                    float centerOffset = abs((float)g - (float)groupSize / 2.0f);
                    brightness = 255 - (uint8_t)(centerOffset * 40);
                }

                uint8_t scaledR = FastLEDMath::scale8(runwayR, brightness);
                uint8_t scaledG = FastLEDMath::scale8(runwayG, brightness);
                uint8_t scaledB = FastLEDMath::scale8(runwayB, brightness);
                uint8_t scaledW = FastLEDMath::scale8(runwayW, brightness);

                // Use RGBW if white channel provided, otherwise RGB
                if (runwayW > 0)
                {
                    segment->setPixel(pos, scaledR, scaledG, scaledB, scaledW);
                }
                else
                {
                    segment->setPixel(pos, scaledR, scaledG, scaledB);
                }
            }
        }

        // Move wave (direction + frame-rate independent)
        float posF = (float)state.position;
        posF += reverseDir ? -effectiveSpeed : effectiveSpeed;

        while (posF < 0.0f)
            posF += (float)length;
        while (posF >= (float)length)
            posF -= (float)length;

        state.position = (uint16_t)posF;
    }

    /**
     * Phase 3: Success Breathing - Gentle pulsating effect
     *
     * Animation:
     * - All LEDs breathe in sync
     * - Smooth sine wave brightness modulation
     * - Calming "success" signal
     */
    void updateSuccessBreathing(Segment* segment, uint32_t deltaTime)
    {
        auto& state = segment->getState();
        uint16_t length = segment->getLength();
        // Snapshot config (stable within this update call)
        const auto& cfg = segment->getConfig();
        const uint8_t cfgSpeed = cfg.speed;         // speed (0=auto)
        const uint8_t cfgIntensity = cfg.intensity; // master brightness scaling (0..255)
        const uint8_t cfgBreathOpt = cfg.option3;   // breathing speed override (0=default)

        const float speedMul = speedMultiplierFromConfig(cfgSpeed);
        const float dtMul = dtFactorFromMs(deltaTime);

        // Breathing speed override via option3 (0=default). Interpreted as degrees per frame / 10.
        float breathingSpeed = _breathingSpeed;
        if (cfgBreathOpt != 0)
            breathingSpeed = (float)cfgBreathOpt / 10.0f;
        breathingSpeed *= speedMul;
        breathingSpeed *= dtMul;

        // Extract RGBW color - use segment color if effect color not set
        uint8_t successR = (_successColorRGBW >> 24) & 0xFF;
        uint8_t successG = (_successColorRGBW >> 16) & 0xFF;
        uint8_t successB = (_successColorRGBW >> 8) & 0xFF;
        uint8_t successW = _successColorRGBW & 0xFF;

        // Fallback to segment color if effect color is black
        if (successR == 0 && successG == 0 && successB == 0 && successW == 0)
        {
            successR = segment->getConfig().r();
            successG = segment->getConfig().g();
            successB = segment->getConfig().b();
            successW = segment->getConfig().w();

            // Apply master intensity scaling (0..255)
            successR = FastLEDMath::scale8(successR, cfgIntensity);
            successG = FastLEDMath::scale8(successG, cfgIntensity);
            successB = FastLEDMath::scale8(successB, cfgIntensity);
            successW = FastLEDMath::scale8(successW, cfgIntensity);
        }

        // Calculate breathing brightness (sine wave)
        // aux2 stores angle (0-360 degrees)
        float angle = (float)state.aux2 * 0.0174533f;   // Convert to radians
        float breathValue = (sin(angle) + 1.0f) / 2.0f; // 0.0 - 1.0
        uint8_t brightness = (uint8_t)(breathValue * 255.0f);

        // Scale color by breathing brightness
        uint8_t scaledR = FastLEDMath::scale8(successR, brightness);
        uint8_t scaledG = FastLEDMath::scale8(successG, brightness);
        uint8_t scaledB = FastLEDMath::scale8(successB, brightness);
        uint8_t scaledW = FastLEDMath::scale8(successW, brightness);

        // Apply to all LEDs
        for (uint16_t i = 0; i < length; i++)
        {
            // Use RGBW if white channel provided, otherwise RGB
            if (successW > 0)
            {
                segment->setPixel(i, scaledR, scaledG, scaledB, scaledW);
            }
            else
            {
                segment->setPixel(i, scaledR, scaledG, scaledB);
            }
        }

        // Advance breathing angle
        state.aux2 += (uint16_t)(breathingSpeed * 100.0f); // Scale for integer storage
        if (state.aux2 >= 36000) state.aux2 = 0;           // Wrap at 360 degrees (scaled by 100)
    }

    /**
     * Fade to black (idle state)
     */
    void fadeToIdle(Segment* segment)
    {
        uint16_t length = segment->getLength();

        // Quick fade to black
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);

            r = FastLEDMath::fadeToBlackBy(r, 200);
            g = FastLEDMath::fadeToBlackBy(g, 200);
            b = FastLEDMath::fadeToBlackBy(b, 200);

            segment->setPixel(i, r, g, b);
        }
    }

    /**
     * Utility: Clamp value to range
     */
    template <typename T>
    T clampValue(T value, T min, T max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
};
