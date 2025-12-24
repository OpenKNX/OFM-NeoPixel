/**
 * @file RGBWTestEffect.h
 * @brief SK6812 RGBW Test Effect - Tests all 4 channels independently
 *
 * This effect cycles through RGB and White channels to verify proper
 * RGBW operation and identify color mapping issues.
 *
 * Test Sequence (5 seconds each):
 * 1. Pure Red   (R=255, G=0, B=0, W=0)
 * 2. Pure Green (R=0, G=255, B=0, W=0)
 * 3. Pure Blue  (R=0, G=0, B=255, W=0)
 * 4. Pure White (R=0, G=0, B=0, W=255)
 * 5. RGB Mix    (R=255, G=255, B=255, W=0)
 * 6. RGBW Mix   (R=128, G=128, B=128, W=128)
 * 7. White Fade (W=0→255→0)
 * 8. Rainbow + White background
 *
 * @copyright Copyright (c) 2025 Erkan Colak - (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "OpenKNX.h"

/**
 * @brief RGBW test pattern for SK6812 (and similar)
 *
 * Uses config parameters (mainly for the rainbow+white phase):
 *  - config.intensity : overall brightness reference (used in rainbow/white)
 *  - config.speed     : hue rotation speed in PHASE_RAINBOW_WHITE (0 => default)
 *  - config.option1   : phase duration override in ms steps (0 => 5000ms, else option1*100ms)
 *  - config.reverse   : reverse rainbow direction in PHASE_RAINBOW_WHITE
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
 */
class RGBWTestEffect : public Effect
{
  private:
    uint32_t _stateTime;
    uint8_t _testPhase;
    uint8_t _fadeValue;
    bool _fadeUp;

    enum TestPhase : uint8_t
    {
        PHASE_RED = 0,
        PHASE_GREEN,
        PHASE_BLUE,
        PHASE_WHITE,
        PHASE_RGB_MIX,
        PHASE_RGBW_MIX,
        PHASE_WHITE_FADE,
        PHASE_RAINBOW_WHITE,
        PHASE_COUNT
    };

  public:
    RGBWTestEffect() : _stateTime(0), _testPhase(0), _fadeValue(0), _fadeUp(true) {}

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        const uint8_t intensityVal = config.intensity; // overall brightness reference
        const uint8_t speedVal = config.speed;         // hue rotation speed in PHASE_RAINBOW_WHITE
        const uint8_t opt1 = config.option1;           // phase duration override
        const bool reverseDir = (config.reverse != 0); // reverse rainbow direction
        const bool yellowBoost = config.feature2;      // enable yellow brightness compensation
        const bool greenCorr = config.feature3;        // enable green correction hooks

        const uint32_t phaseDurationMs = (opt1 == 0) ? 5000UL : (uint32_t)opt1 * 100UL;

        _stateTime += deltaTime;

        // Switch phase every x seconds
        if (_stateTime >= phaseDurationMs)
        {
            _stateTime = 0;
            _testPhase = (_testPhase + 1) % PHASE_COUNT;
            _fadeValue = 0;
            _fadeUp = true;

            openknx.logger.logWithPrefixAndValues("RGBW Test", "Phase %u - %s", _testPhase, getPhaseName());
        }

        // Execute current test phase
        switch (_testPhase)
        {
            case PHASE_RED:
                testPureColor(segment, length, intensityVal, 255, 0, 0, 0);
                break;

            case PHASE_GREEN:
                testPureColor(segment, length, intensityVal, 0, 255, 0, 0);
                break;

            case PHASE_BLUE:
                testPureColor(segment, length, intensityVal, 0, 0, 255, 0);
                break;

            case PHASE_WHITE:
                testPureColor(segment, length, intensityVal, 0, 0, 0, 255);
                break;

            case PHASE_RGB_MIX:
                testPureColor(segment, length, intensityVal, 255, 255, 255, 0);
                break;

            case PHASE_RGBW_MIX:
                testPureColor(segment, length, intensityVal, 128, 128, 128, 128);
                break;

            case PHASE_WHITE_FADE:
                testWhiteFade(segment, length, intensityVal, deltaTime);
                break;

            case PHASE_RAINBOW_WHITE:
                testRainbowWithWhite(segment, length, intensityVal, speedVal, reverseDir, yellowBoost, greenCorr);
                break;
        }
    }

    void reset() override
    {
        _stateTime = 0;
        _testPhase = 0;
        _fadeValue = 0;
        _fadeUp = true;
    }

    const char* getName() override
    {
        return "RGBW_Test";
    }

    const char* getDescription() override
    {
        return "Test pattern for RGBW LED strips";
    }

  private:
    /**
     * @brief Test pure color (one channel at a time)
     */
    void testPureColor(Segment* segment, uint16_t length, uint8_t intensityVal,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        // Apply master brightness
        r = FastLEDMath::scale8(r, intensityVal);
        g = FastLEDMath::scale8(g, intensityVal);
        b = FastLEDMath::scale8(b, intensityVal);
        w = FastLEDMath::scale8(w, intensityVal);
        // Set all LEDs to same color
        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, r, g, b, w);
        }
    }

    /**
     * @brief Test white channel with fade
     */
    void testWhiteFade(Segment* segment, uint16_t length, uint8_t intensityVal, uint32_t deltaTime)
    {
        // Update fade value (2 steps per ms = 2 seconds full cycle)
        if (_fadeUp)
        {
            _fadeValue += (deltaTime * 2 > 255) ? 255 : deltaTime * 2;
            if (_fadeValue >= 250)
            {
                _fadeValue = 255;
                _fadeUp = false;
            }
        }
        else
        {
            if (_fadeValue < deltaTime * 2)
            {
                _fadeValue = 0;
                _fadeUp = true;
            }
            else
            {
                _fadeValue -= deltaTime * 2;
            }
        }

        // Apply brightness
        uint8_t white = FastLEDMath::scale8(_fadeValue, intensityVal);

        // All LEDs white with fade
        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, 0, 0, 0, white);
        }
    }

    /**
     * @brief Test rainbow with white background
     */
    void testRainbowWithWhite(Segment* segment,
                              uint16_t length,
                              uint8_t intensityVal,
                              uint8_t speedVal,
                              bool reverseDir,
                              bool yellowBoost,
                              bool greenCorr)
    {
        // Hue step period in ms: 0 => legacy default (20ms). Higher speed => faster.
        const uint16_t hueStepMs = (speedVal == 0) ? 20u : (uint16_t)(1u + (255u - speedVal));
        const uint8_t hueOffset = (uint8_t)((_stateTime / hueStepMs) & 0xFF);
        for (uint16_t i = 0; i < length; i++)
        {
            // Rainbow color
            const uint8_t posHue = (uint8_t)((uint32_t)i * 255u / length);
            uint8_t hue = reverseDir ? (uint8_t)(hueOffset - posHue) : (uint8_t)(hueOffset + posHue);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, intensityVal, yellowBoost, greenCorr);

            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;

            // Add white background (30% brightness)
            uint8_t w = FastLEDMath::scale8(intensityVal, 77); // 77/255 ≈ 30%

            segment->setPixel(i, r, g, b, w);
        }
    }

    /**
     * @brief Get human-readable phase name
     */
    const char* getPhaseName()
    {
        switch (_testPhase)
        {
            case PHASE_RED: return "Pure Red (R channel)";
            case PHASE_GREEN: return "Pure Green (G channel)";
            case PHASE_BLUE: return "Pure Blue (B channel)";
            case PHASE_WHITE: return "Pure White (W channel)";
            case PHASE_RGB_MIX: return "RGB Mix (no White)";
            case PHASE_RGBW_MIX: return "RGBW Mix (all channels)";
            case PHASE_WHITE_FADE: return "White Fade (W channel test)";
            case PHASE_RAINBOW_WHITE: return "Rainbow + White BG";
            default: return "Unknown";
        }
    }
};