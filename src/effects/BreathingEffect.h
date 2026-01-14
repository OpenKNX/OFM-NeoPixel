/**
 * @file BreathingEffect.h
 * @brief Breathing effect - Smooth fade in and out like breathing
 *
 * Creates a breathing effect where the entire strip fades in and out smoothly.
 * Uses sine wave for natural breathing rhythm.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Breathing Effect
 *
 * The entire strip fades in and out smoothly like breathing.
 */
/**
 * Config usage:
 *   - config.speed     : breathing rate (higher=faster, 0-255 maps to 5-60 BPM)
 *   - config.option1   : minimum brightness (0-255, default 0) - breathing depth
 *   - config.option2   : curve adjustment (0-255) - 0=linear, 255=exponential
 *   - config.feature1  : hold at peak (0=smooth, 1=pause at top)
 *   - config.feature2  : rainbow breathing (0=set color, 1=rainbow cycle)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : breathing color (unless rainbow mode)
 */
class BreathingEffect : public Effect
{
  private:
    uint32_t _time;

  public:
    BreathingEffect() : _time(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        _time += deltaTime;

        // Use option1 for breathing depth (50-255, default full range)
        uint8_t minBrightness = config.option1 > 0 ? config.option1 : 0;
        uint8_t maxBrightness = 255;

        // Use option2 for breathing curve (0=linear, 255=exponential)
        uint8_t curve = config.option2;

        // Use feature1 for hold at peak (0=smooth, 1=hold)
        bool holdAtPeak = config.feature1;

        // Use feature2 for rainbow breathing (0=set color, 1=rainbow)
        bool rainbowBreathing = config.feature2;

        // Calculate BPM from speed (map 0-255 to 5-60 breaths per minute)
        uint16_t bpm = 5 + ((config.speed * 55) / 255);

        // Use beatsin8 to get smooth breathing pattern
        uint8_t breath;
        if (holdAtPeak)
        {
            // Modified breathing with pause at peak
            uint8_t rawBreath = FastLEDMath::beatsin8(bpm, 0, 255);
            if (rawBreath > 200)
            {
                breath = 255; // Hold at peak
            }
            else
            {
                breath = FastLEDMath::scale8(rawBreath, 200); // Scale normal breathing
            }
        }
        else
        {
            breath = FastLEDMath::beatsin8(bpm, minBrightness, maxBrightness);
        }

        // Apply curve adjustment
        if (curve > 0)
        {
            // Apply exponential curve for more natural breathing
            uint8_t curved = FastLEDMath::scale8(breath, breath);
            breath = FastLEDMath::lerp8by8(breath, curved, curve);
        }

        // Scale the configured brightness by the breathing pattern
        uint8_t currentBrightness = FastLEDMath::scale8(config.intensity, breath);

        // Apply to all pixels
        uint8_t r, g, b, ww, cw;
        if (rainbowBreathing)
        {
            // Rainbow breathing mode (no white channels)
            uint8_t hue = (_time / 50) % 256; // Slow hue cycle
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
            ww = 0;
            cw = 0;
        }
        else
        {
            // Use configured color with 5-channel support
            r = FastLEDMath::scale8(config.r(), currentBrightness);
            g = FastLEDMath::scale8(config.g(), currentBrightness);
            b = FastLEDMath::scale8(config.b(), currentBrightness);
            ww = FastLEDMath::scale8(config.ww(), currentBrightness);
            cw = FastLEDMath::scale8(config.cw(), currentBrightness);
        }

        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, r, g, b, ww, cw);
        }
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Breathing";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Smooth breathing effect - fade in and out";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "MinBrightness";
            case 2: return "Curve";
            case 3: return "HoldAtPeak";
            case 4: return "RainbowBreathing";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Atemgeschwindigkeit (höher=schneller)", "Speed: Breathing rate (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Minimale Helligkeit beim Ausatmen (0-255)", "Minimum brightness when breathing out (0-255)");
            case 2: return PARAM_DESC_DE_EN("Atemkurve: 0=linear, 255=exponentiell", "Breathing curve: 0=linear, 255=exponential");
            case 3: return PARAM_DESC_DE_EN("Pause am Höhepunkt", "Hold at peak");
            case 4: return PARAM_DESC_DE_EN("Regenbogen-Atmen: Farbe wechselt", "Rainbow breathing: Color cycles");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_BOOL;
            case 3: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 0;   // MinBrightness
            case 2: return 0;   // Curve (linear)
            case 3: return 0;   // HoldAtPeak off
            case 4: return 0;   // RainbowBreathing off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // MinBrightness min
            case 2: return 0; // Curve min
            case 3: return 0; // HoldAtPeak false
            case 4: return 0; // RainbowBreathing false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // MinBrightness max
            case 2: return 255; // Curve max
            case 3: return 1;   // HoldAtPeak true
            case 4: return 1;   // RainbowBreathing true
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;    // Speed
            case 1: return config.option1;  // MinBrightness
            case 2: return config.option2;  // Curve
            case 3: return config.feature1; // HoldAtPeak
            case 4: return config.feature2; // RainbowBreathing
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (breathing rate)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // MinBrightness (0-255)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Curve (0=linear, 1=smooth)
            case 3: config.feature1 = static_cast<bool>(value); break;   // HoldAtPeak
            case 4: config.feature2 = static_cast<bool>(value); break;   // RainbowBreathing
            default: break;
        }
    }
};