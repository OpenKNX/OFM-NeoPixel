/**
 * @file StrobeEffect.h
 * @brief Strobe effect - Fast on/off flashing like a strobe light
 *
 * Creates a strobe light effect with configurable timing and colors.
 * Based on classic strobe light patterns.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Strobe Effect
 *
 * Fast on/off flashing like a strobe light.
 */
/**
 * Config usage:
 *   - config.speed     : strobe speed (higher=faster flashing, 0-255 maps to 10-500ms)
 *   - config.option1   : on ratio (10-200, default 100) - percentage of on-time
 *   - config.option2   : minimum brightness when "off" (0-100, default 0)
 *   - config.feature1  : random timing (0=regular, 1=random intervals)
 *   - config.feature2  : rainbow strobe (0=set color, 1=rainbow cycle)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : strobe color (unless rainbow mode)
 */
class StrobeEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    bool _isOn;

  public:
    StrobeEffect() : _lastUpdate(0), _isOn(false)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Use option1 for on duration ratio (10-200, default 50/50)
        uint8_t onRatio = config.option1 > 0 ? config.option1 : 100; // Percentage on time
        onRatio = onRatio > 200 ? 200 : onRatio;

        // Use option2 for minimum brightness when "off" (0-100, default 0)
        uint8_t minBrightness = config.option2 > 0 ? config.option2 : 0;
        minBrightness = minBrightness > 100 ? 100 : minBrightness;

        // Use feature1 for random timing (0=regular, 1=random)
        bool randomTiming = config.feature1;

        // Use feature2 for rainbow strobe (0=set color, 1=rainbow)
        bool rainbowStrobe = config.feature2;

        // Calculate strobe interval based on speed (map 0-255 to 10-500ms)
        uint32_t baseInterval = 10 + ((255 - config.speed) * 490) / 255;
        uint32_t interval = randomTiming ? (baseInterval / 2 + FastLEDMath::random16(baseInterval)) : baseInterval;

        uint32_t now = millis();
        if (now - _lastUpdate >= interval)
        {
            _lastUpdate = now;
            _isOn = !_isOn;
        }

        // Calculate on/off durations based on ratio
        uint32_t currentPhase = (now - _lastUpdate);
        uint32_t onDuration = (interval * onRatio) / 200; // Convert percentage to duration
        bool shouldBeOn = currentPhase < onDuration;

        uint8_t r, g, b, ww, cw;
        if (_isOn && shouldBeOn)
        {
            // Full brightness when on
            if (rainbowStrobe)
            {
                uint8_t hue = (now / 100) % 256; // Fast hue cycling
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
                ww = 0;
                cw = 0;
            }
            else
            {
                r = FastLEDMath::scale8(config.r(), config.intensity);
                g = FastLEDMath::scale8(config.g(), config.intensity);
                b = FastLEDMath::scale8(config.b(), config.intensity);
                ww = FastLEDMath::scale8(config.ww(), config.intensity);
                cw = FastLEDMath::scale8(config.cw(), config.intensity);
            }
        }
        else
        {
            // Off or dim
            uint8_t dimBrightness = FastLEDMath::scale8(config.intensity, minBrightness);
            if (rainbowStrobe)
            {
                uint8_t hue = (now / 100) % 256;
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, dimBrightness);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
                ww = 0;
                cw = 0;
            }
            else
            {
                r = FastLEDMath::scale8(config.r(), dimBrightness);
                g = FastLEDMath::scale8(config.g(), dimBrightness);
                b = FastLEDMath::scale8(config.b(), dimBrightness);
                ww = FastLEDMath::scale8(config.ww(), dimBrightness);
                cw = FastLEDMath::scale8(config.cw(), dimBrightness);
            }
        }

        // Apply to all pixels
        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, r, g, b, ww, cw);
        }
    }

    void reset() override
    {
        _lastUpdate = 0;
        _isOn = false;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Strobe";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Fast on/off flashing strobe light";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "OnRatio";
            case 2: return "MinBrightness";
            case 3: return "RandomTiming";
            case 4: return "RainbowStrobe";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Strobe-Geschwindigkeit (höher=schneller)", "Speed: Strobe speed (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("An-Zeit Verhältnis in % (10-200)", "On-time ratio in % (10-200)");
            case 2: return PARAM_DESC_DE_EN("Minimale Helligkeit wenn aus (0-100)", "Minimum brightness when off (0-100)");
            case 3: return PARAM_DESC_DE_EN("Zufälliges Timing statt regelmäßig", "Random timing instead of regular");
            case 4: return PARAM_DESC_DE_EN("Regenbogen-Strobe: Farbe wechselt", "Rainbow strobe: Color cycles");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_UINT8;
            case 3: return ParameterType::PARAM_BOOL;
            case 4: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 100; // OnRatio (50/50)
            case 2: return 0;   // MinBrightness
            case 3: return 0;   // RandomTiming off
            case 4: return 0;   // RainbowStrobe off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed min
            case 1: return 10; // OnRatio min
            case 2: return 0;  // MinBrightness min
            case 3: return 0;  // RandomTiming false
            case 4: return 0;  // RainbowStrobe false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 200; // OnRatio max
            case 2: return 100; // MinBrightness max
            case 3: return 1;   // RandomTiming true
            case 4: return 1;   // RainbowStrobe true
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
            case 1: return config.option1;  // OnRatio
            case 2: return config.option2;  // MinBrightness
            case 3: return config.feature1; // RandomTiming
            case 4: return config.feature2; // RainbowStrobe
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (strobe rate)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // OnRatio (10-200%)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // MinBrightness (0-100)
            case 3: config.feature1 = static_cast<bool>(value); break;   // RandomTiming
            case 4: config.feature2 = static_cast<bool>(value); break;   // RainbowStrobe
            default: break;
        }
    }
};
