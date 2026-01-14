/**
 * @file RGBCCTTestEffect.h
 * @brief RGBCCT 5-Channel Test Effect - Tests all 5 channels independently
 *
 * This effect cycles through RGB, Warm White, and Cool White channels to verify proper
 * RGBCCT operation and identify color mapping issues for 5-channel LED strips.
 *
 * Test Sequence (configurable duration each):
 * 1. Pure Red      (R=255, G=0, B=0, WW=0, CW=0)
 * 2. Pure Green    (R=0, G=255, B=0, WW=0, CW=0)
 * 3. Pure Blue     (R=0, G=0, B=255, WW=0, CW=0)
 * 4. Pure WarmWhite(R=0, G=0, B=0, WW=255, CW=0)
 * 5. Pure CoolWhite(R=0, G=0, B=0, WW=0, CW=255)
 * 6. RGB Mix       (R=255, G=255, B=255, WW=0, CW=0)
 * 7. Whites Mix    (R=0, G=0, B=0, WW=128, CW=128)
 * 8. RGBCCT Full   (R=51, G=51, B=51, WW=51, CW=51)
 * 9. CCT Gradient  (WW fades as CW increases - color temperature sweep)
 * 10. All Fade     (All 5 channels fade in/out together)
 *
 * @copyright Copyright (c) 2025 Erkan Colak - (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "OpenKNX.h"

class RGBCCTTestEffect : public Effect
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
        PHASE_WARM_WHITE,
        PHASE_COOL_WHITE,
        PHASE_RGB_MIX,
        PHASE_WHITES_MIX,
        PHASE_RGBCCT_FULL,
        PHASE_CCT_GRADIENT,
        PHASE_ALL_FADE,
        PHASE_COUNT
    };

  public:
    RGBCCTTestEffect() : _stateTime(0), _testPhase(0), _fadeValue(0), _fadeUp(true) {}

    const char* getName(const char* lang = nullptr) override { return "RGBCCTTest"; }
    const char* getDescription(const char* lang = nullptr) override { return "RGBCCT 5-channel test pattern (10 phases)"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 1; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "PhaseDuration";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Dauer jeder Testphase in Sekunden",
                "Duration of each test phase in seconds");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8; // PhaseDuration
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 5; // PhaseDuration (5 seconds)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // PhaseDuration min (1 second)
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 60; // PhaseDuration max (60 seconds)
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed; // PhaseDuration
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break; // PhaseDuration (1-60 seconds)
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Get phase duration parameter (1-60 seconds, default 5)
        uint8_t phaseDurationSec = config.speed > 0 ? config.speed : 5;
        uint32_t phaseDurationMs = phaseDurationSec * 1000;

        _stateTime += deltaTime;

        // Switch phase when duration expires
        if (_stateTime >= phaseDurationMs)
        {
            _stateTime = 0;
            _testPhase = (_testPhase + 1) % PHASE_COUNT;
            _fadeValue = 0;
            _fadeUp = true;

            openknx.logger.logWithPrefixAndValues("RGBCCT Test", "Phase %u - %s", _testPhase, getPhaseName());
        }

        // Execute current test phase
        switch (_testPhase)
        {
            case PHASE_RED:
                testPureColor(segment, length, 255, 0, 0, 0, 0);
                break;

            case PHASE_GREEN:
                testPureColor(segment, length, 0, 255, 0, 0, 0);
                break;

            case PHASE_BLUE:
                testPureColor(segment, length, 0, 0, 255, 0, 0);
                break;

            case PHASE_WARM_WHITE:
                testPureColor(segment, length, 0, 0, 0, 255, 0);
                break;

            case PHASE_COOL_WHITE:
                testPureColor(segment, length, 0, 0, 0, 0, 255);
                break;

            case PHASE_RGB_MIX:
                testPureColor(segment, length, 255, 255, 255, 0, 0);
                break;

            case PHASE_WHITES_MIX:
                testPureColor(segment, length, 0, 0, 0, 128, 128);
                break;

            case PHASE_RGBCCT_FULL:
                testPureColor(segment, length, 51, 51, 51, 51, 51); // Equal distribution
                break;

            case PHASE_CCT_GRADIENT:
                testCCTGradient(segment, length);
                break;

            case PHASE_ALL_FADE:
                testAllFade(segment, length, deltaTime);
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

  private:
    /**
     * @brief Test pure color (one or more channels at a time)
     */
    void testPureColor(Segment* segment, uint16_t length,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
    {
        auto& config = segment->getConfig();

        // Apply master brightness
        r = FastLEDMath::scale8(r, config.intensity);
        g = FastLEDMath::scale8(g, config.intensity);
        b = FastLEDMath::scale8(b, config.intensity);
        ww = FastLEDMath::scale8(ww, config.intensity);
        cw = FastLEDMath::scale8(cw, config.intensity);

        // Set all LEDs to same color
        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, r, g, b, ww, cw);
        }
    }

    /**
     * @brief Test CCT gradient - Warm to Cool across the strip
     */
    void testCCTGradient(Segment* segment, uint16_t length)
    {
        auto& config = segment->getConfig();

        // Also add time-based animation - shift the gradient
        uint8_t offset = (_stateTime / 50) % 256;

        for (uint16_t i = 0; i < length; i++)
        {
            // Calculate position in gradient (0-255)
            uint8_t pos = ((i * 255) / (length > 1 ? length - 1 : 1) + offset) % 256;

            // Warm white decreases as position increases
            uint8_t ww = 255 - pos;
            // Cool white increases as position increases
            uint8_t cw = pos;

            // Apply brightness
            ww = FastLEDMath::scale8(ww, config.intensity);
            cw = FastLEDMath::scale8(cw, config.intensity);

            segment->setPixel(i, 0, 0, 0, ww, cw);
        }
    }

    /**
     * @brief Test all channels with fade - All 5 channels fade in/out
     */
    void testAllFade(Segment* segment, uint16_t length, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();

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
        uint8_t value = FastLEDMath::scale8(_fadeValue, config.intensity);

        // All LEDs with all channels at same level
        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, value, value, value, value, value);
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
            case PHASE_WARM_WHITE: return "Pure Warm White (WW channel)";
            case PHASE_COOL_WHITE: return "Pure Cool White (CW channel)";
            case PHASE_RGB_MIX: return "RGB Mix (no White)";
            case PHASE_WHITES_MIX: return "Whites Mix (WW+CW)";
            case PHASE_RGBCCT_FULL: return "RGBCCT Full (all channels)";
            case PHASE_CCT_GRADIENT: return "CCT Gradient (WW→CW sweep)";
            case PHASE_ALL_FADE: return "All Fade (5-channel fade test)";
            default: return "Unknown";
        }
    }
};
