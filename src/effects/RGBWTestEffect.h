/**
 * @file RGBWTestEffect.h
 * @brief Channel Test Effect - Tests RGBW (4ch) or RGB+CCT (5ch) channels
 *
 * Consolidated test effect. Mode selects the channel set:
 *   Mode 0 (RGBW):   8-phase test for 4-channel strips (R,G,B,W)
 *   Mode 1 (RGB+CCT):10-phase test for 5-channel strips (R,G,B,WW,CW)
 *
 * RGBW Sequence (configurable duration each):
 * 1. Pure Red   (R=255, G=0, B=0, W=0)
 * 2. Pure Green (R=0, G=255, B=0, W=0)
 * 3. Pure Blue  (R=0, G=0, B=255, W=0)
 * 4. Pure White (R=0, G=0, B=0, W=255)
 * 5. RGB Mix    (R=255, G=255, B=255, W=0)
 * 6. RGBW Mix   (R=128, G=128, B=128, W=128)
 * 7. White Fade (W=0->255->0)
 * 8. Rainbow + White background
 *
 * RGB+CCT Sequence (configurable duration each):
 * 1. Pure Red, 2. Pure Green, 3. Pure Blue, 4. Pure Warm White,
 * 5. Pure Cool White, 6. RGB Mix, 7. Whites Mix, 8. RGBCCT Full,
 * 9. CCT Gradient, 10. All Fade
 *
 * @copyright Copyright (c) 2025 Erkan Colak - (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "OpenKNX.h"

class RGBWTestEffect : public Effect
{
  private:
    uint32_t _stateTime;
    uint8_t _testPhase;
    uint8_t _fadeValue;
    bool _fadeUp;

    // RGBW (4-channel) phases
    enum TestPhase4 : uint8_t
    {
        P4_RED = 0,
        P4_GREEN,
        P4_BLUE,
        P4_WHITE,
        P4_RGB_MIX,
        P4_RGBW_MIX,
        P4_WHITE_FADE,
        P4_RAINBOW_WHITE,
        P4_COUNT
    };

    // RGBCCT (5-channel) phases
    enum TestPhase5 : uint8_t
    {
        P5_RED = 0,
        P5_GREEN,
        P5_BLUE,
        P5_WARM_WHITE,
        P5_COOL_WHITE,
        P5_RGB_MIX,
        P5_WHITES_MIX,
        P5_RGBCCT_FULL,
        P5_CCT_GRADIENT,
        P5_ALL_FADE,
        P5_COUNT
    };

  public:
    RGBWTestEffect() : _stateTime(0), _testPhase(0), _fadeValue(0), _fadeUp(true) {}

    const char* getName(const char* lang = nullptr) override { return "Test"; }
    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Kanaltest: RGBW (4 Kanäle) oder RGB+CCT (5 Kanäle)",
            "Channel test: RGBW (4 channels) or RGB+CCT (5 channels)");
    }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "PhaseDuration";
            case 1: return "Mode";
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
            case 1: return PARAM_DESC_DE_EN(
                "Modus: 0=RGBW (4 Kanäle), 1=RGB+CCT (5 Kanäle)",
                "Mode: 0=RGBW (4 channels), 1=RGB+CCT (5 channels)");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8; // PhaseDuration
            case 1: return ParameterType::PARAM_ENUM;  // Mode
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 1) return nullptr;
        switch (enumValue)
        {
            case 0: return "RGBW";
            case 1: return "RGB+CCT";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 1) ? 2 : 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 5; // PhaseDuration (5 seconds)
            case 1: return 0; // Mode (RGBW)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // PhaseDuration min (1 second)
            case 1: return 0; // Mode min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 60; // PhaseDuration max (60 seconds)
            case 1: return 1;  // Mode max (0=RGBW, 1=RGBCCT)
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
            case 1: return config.mode;  // Mode
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
            case 1: config.mode = static_cast<uint8_t>(value); break;  // Mode
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        const bool cctMode = (config.mode == 1);
        const uint8_t phaseCount = cctMode ? (uint8_t)P5_COUNT : (uint8_t)P4_COUNT;

        // Get phase duration parameter (1-60 seconds, default 5)
        uint8_t phaseDurationSec = config.speed > 0 ? config.speed : 5;
        uint32_t phaseDurationMs = phaseDurationSec * 1000;

        _stateTime += deltaTime;

        // Switch phase when duration expires
        if (_stateTime >= phaseDurationMs)
        {
            _stateTime = 0;
            _testPhase = (_testPhase + 1) % phaseCount;
            _fadeValue = 0;
            _fadeUp = true;

            openknx.logger.logWithPrefixAndValues(
                "Channel Test", "Phase %u - %s", _testPhase, getPhaseName(cctMode));
        }

        if (_testPhase >= phaseCount) _testPhase = 0;

        if (cctMode)
        {
            updateRGBCCT(segment, length, deltaTime);
        }
        else
        {
            updateRGBW(segment, length, deltaTime);
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
    void updateRGBW(Segment* segment, uint16_t length, uint32_t deltaTime)
    {
        switch (_testPhase)
        {
            case P4_RED:          testPureColor(segment, length, 255, 0, 0, 0); break;
            case P4_GREEN:        testPureColor(segment, length, 0, 255, 0, 0); break;
            case P4_BLUE:         testPureColor(segment, length, 0, 0, 255, 0); break;
            case P4_WHITE:        testPureColor(segment, length, 0, 0, 0, 255); break;
            case P4_RGB_MIX:      testPureColor(segment, length, 255, 255, 255, 0); break;
            case P4_RGBW_MIX:     testPureColor(segment, length, 128, 128, 128, 128); break;
            case P4_WHITE_FADE:   testWhiteFade(segment, length, deltaTime); break;
            case P4_RAINBOW_WHITE:testRainbowWithWhite(segment, length); break;
        }
    }

    void updateRGBCCT(Segment* segment, uint16_t length, uint32_t deltaTime)
    {
        switch (_testPhase)
        {
            case P5_RED:          testPureColor5(segment, length, 255, 0, 0, 0, 0); break;
            case P5_GREEN:        testPureColor5(segment, length, 0, 255, 0, 0, 0); break;
            case P5_BLUE:         testPureColor5(segment, length, 0, 0, 255, 0, 0); break;
            case P5_WARM_WHITE:   testPureColor5(segment, length, 0, 0, 0, 255, 0); break;
            case P5_COOL_WHITE:   testPureColor5(segment, length, 0, 0, 0, 0, 255); break;
            case P5_RGB_MIX:      testPureColor5(segment, length, 255, 255, 255, 0, 0); break;
            case P5_WHITES_MIX:   testPureColor5(segment, length, 0, 0, 0, 128, 128); break;
            case P5_RGBCCT_FULL:  testPureColor5(segment, length, 51, 51, 51, 51, 51); break;
            case P5_CCT_GRADIENT: testCCTGradient(segment, length); break;
            case P5_ALL_FADE:     testAllFade(segment, length, deltaTime); break;
        }
    }

    /**
     * @brief Test pure color (RGBW)
     */
    void testPureColor(Segment* segment, uint16_t length,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        auto& config = segment->getConfig();
        r = FastLEDMath::scale8(r, config.intensity);
        g = FastLEDMath::scale8(g, config.intensity);
        b = FastLEDMath::scale8(b, config.intensity);
        w = FastLEDMath::scale8(w, config.intensity);
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, r, g, b, w);
    }

    /**
     * @brief Test pure color (RGBCCT - 5 channels)
     */
    void testPureColor5(Segment* segment, uint16_t length,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
    {
        auto& config = segment->getConfig();
        r = FastLEDMath::scale8(r, config.intensity);
        g = FastLEDMath::scale8(g, config.intensity);
        b = FastLEDMath::scale8(b, config.intensity);
        ww = FastLEDMath::scale8(ww, config.intensity);
        cw = FastLEDMath::scale8(cw, config.intensity);
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, r, g, b, ww, cw);
    }

    /**
     * @brief Test white channel with fade
     */
    void testWhiteFade(Segment* segment, uint16_t length, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        advanceFade(deltaTime);
        uint8_t white = FastLEDMath::scale8(_fadeValue, config.intensity);
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, 0, 0, 0, white);
    }

    /**
     * @brief Test rainbow with white background
     */
    void testRainbowWithWhite(Segment* segment, uint16_t length)
    {
        auto& config = segment->getConfig();
        uint8_t hueOffset = (_stateTime / 20) % 256;
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t hue = hueOffset + (i * 255 / (length > 0 ? length : 1));
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, config.intensity);
            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;
            uint8_t w = FastLEDMath::scale8(config.intensity, 77); // ~30%
            segment->setPixel(i, r, g, b, w);
        }
    }

    /**
     * @brief Test CCT gradient - Warm to Cool across the strip
     */
    void testCCTGradient(Segment* segment, uint16_t length)
    {
        auto& config = segment->getConfig();
        uint8_t offset = (_stateTime / 50) % 256;
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t pos = ((i * 255) / (length > 1 ? length - 1 : 1) + offset) % 256;
            uint8_t ww = 255 - pos;
            uint8_t cw = pos;
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
        advanceFade(deltaTime);
        uint8_t value = FastLEDMath::scale8(_fadeValue, config.intensity);
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, value, value, value, value, value);
    }

    void advanceFade(uint32_t deltaTime)
    {
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
    }

    /**
     * @brief Get human-readable phase name
     */
    const char* getPhaseName(bool cctMode)
    {
        if (cctMode)
        {
            switch (_testPhase)
            {
                case P5_RED: return "Pure Red (R channel)";
                case P5_GREEN: return "Pure Green (G channel)";
                case P5_BLUE: return "Pure Blue (B channel)";
                case P5_WARM_WHITE: return "Pure Warm White (WW channel)";
                case P5_COOL_WHITE: return "Pure Cool White (CW channel)";
                case P5_RGB_MIX: return "RGB Mix (no White)";
                case P5_WHITES_MIX: return "Whites Mix (WW+CW)";
                case P5_RGBCCT_FULL: return "RGBCCT Full (all channels)";
                case P5_CCT_GRADIENT: return "CCT Gradient (WW->CW sweep)";
                case P5_ALL_FADE: return "All Fade (5-channel fade test)";
                default: return "Unknown";
            }
        }
        switch (_testPhase)
        {
            case P4_RED: return "Pure Red (R channel)";
            case P4_GREEN: return "Pure Green (G channel)";
            case P4_BLUE: return "Pure Blue (B channel)";
            case P4_WHITE: return "Pure White (W channel)";
            case P4_RGB_MIX: return "RGB Mix (no White)";
            case P4_RGBW_MIX: return "RGBW Mix (all channels)";
            case P4_WHITE_FADE: return "White Fade (W channel test)";
            case P4_RAINBOW_WHITE: return "Rainbow + White BG";
            default: return "Unknown";
        }
    }
};
