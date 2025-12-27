/**
 * @file RainbowCycleEffect.h
 * @brief Rainbow Cycle effect - Rainbow that cycles around the entire strip
 *
 * Similar to Rainbow but with different cycling behavior.
 * Based on Adafruit NeoPixel examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Rainbow Cycle Effect
 *
 * Like rainbow, but distributes colors more evenly across the strip.
 */
class RainbowCycleEffect : public Effect
{
  private:
    uint8_t _colorIndex;

  public:
    RainbowCycleEffect() : _colorIndex(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for saturation (0-255, default 255=full color)
        uint8_t saturation = config.option1 > 0 ? config.option1 : 255;

        // Use option2 for density (1-10, default 1=one full rainbow)
        uint8_t density = config.option2 > 0 ? config.option2 : 1;
        density = density > 10 ? 10 : density;

        // Use feature1 for reverse direction
        bool reverse = config.feature1;

        // Calculate how much to advance the color cycle each frame
        uint8_t speed = config.speed > 0 ? config.speed : 1;
        if (reverse)
        {
            _colorIndex -= speed / 16; // Reverse direction
        }
        else
        {
            _colorIndex += speed / 16; // Forward direction
        }

        // Draw rainbow cycle across the strip
        for (uint16_t i = 0; i < length; i++)
        {
            // Each pixel gets a different hue based on position and density
            // Density controls how many rainbow cycles fit on the strip
            uint8_t hue = _colorIndex + ((i * 255 * density) / length);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, saturation, config.intensity);

            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;

            segment->setPixel(i, r, g, b);
        }
    }

    void reset() override
    {
        _colorIndex = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Rainbow Cycle";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Saturation";
            case 2: return "Density";
            case 3: return "Reverse";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Rotationsgeschwindigkeit (1-255)", "Speed: Rotation speed (1-255)");
            case 1: return PARAM_DESC_DE_EN("Sättigung: Farbintensität (0=weiß, 255=volle Farbe)", "Saturation: Color intensity (0=white, 255=full color)");
            case 2: return PARAM_DESC_DE_EN("Dichte: Anzahl Regenbogenzyklen (1-10)", "Density: Number of rainbow cycles (1-10)");
            case 3: return PARAM_DESC_DE_EN("Rückwärts: Richtung umkehren", "Reverse: Reverse direction");
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
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128;
            case 1: return 255;
            case 2: return 1;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 0;
            case 2: return 1;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 255;
            case 2: return 10;
            case 3: return 1;
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;
            case 1: return config.option1;
            case 2: return config.option2;
            case 3: return config.feature1;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (rotation speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Saturation (0=white, 255=full color)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Density (color cycles per segment)
            case 3: config.feature1 = static_cast<bool>(value); break;   // Reverse direction
            default: break;
        }
    }
};