/**
 * @file TheaterChaseRainbowEffect.h
 * @brief Theater Chase Rainbow effect - Chase pattern with rainbow colors
 *
 * Theater chase effect that cycles through rainbow colors.
 * Based on Adafruit NeoPixel examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Theater Chase Rainbow Effect
 *
 * Same as theater chase but cycles through rainbow colors
 */
class TheaterChaseRainbowEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing;
    uint8_t _hue;

  public:
    TheaterChaseRainbowEffect() : _lastUpdate(0), _position(0), _spacing(3), _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Calculate update interval based on speed
        uint32_t interval = 30 + ((255 - config.speed) * 170) / 255;

        uint32_t now = millis();
        if (now - _lastUpdate < interval)
        {
            return;
        }
        _lastUpdate = now;

        // Use option1 for spacing (1-10, default 3)
        uint8_t spacing = config.option1 > 0 ? config.option1 : 3;
        spacing = spacing > 10 ? 10 : spacing;

        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize;

        // Use option3 for color change speed (1-20, default 5)
        uint8_t colorSpeed = config.option3 > 0 ? config.option3 : 5;
        colorSpeed = colorSpeed > 20 ? 20 : colorSpeed;

        // Use feature1 for trail mode (0=off, 1=on)
        bool trailMode = config.feature1;

        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++)
        {
            if (trailMode)
            {
                // Fade existing pixels for trail effect
                uint8_t r, g, b;
                segment->getPixel(i, r, g, b);
                r = r * 0.85; // 15% fade
                g = g * 0.85;
                b = b * 0.85;
                segment->setPixel(i, r, g, b);
            }
            else
            {
                segment->setPixel(i, 0, 0, 0);
            }
        }

        // Light every spacing-th pixel with rainbow color
        for (uint16_t i = _position; i < length; i += spacing)
        {
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);

            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;

            // Set multiple pixels for larger dot size
            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++)
            {
                segment->setPixel(i + d, r, g, b);
            }
        }

        // Advance position and hue
        _position++;
        if (_position >= spacing)
        {
            _position = 0;
        }
        _hue += colorSpeed; // Configurable color change speed
    }

    void reset() override
    {
        _position = 0;
        _hue = 0;
        _lastUpdate = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Theater Chase Rainbow";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Theater chase with rainbow colors";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Spacing";
            case 2: return "DotSize";
            case 3: return "ColorSpeed";
            case 4: return "TrailMode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Bewegungsgeschwindigkeit (höher=schneller)", "Speed: Movement speed (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Abstand zwischen leuchtenden Pixeln (1-10)", "Space between lit pixels (1-10)");
            case 2: return PARAM_DESC_DE_EN("Größe der Leuchtpunkte (1-5)", "Size of light dots (1-5)");
            case 3: return PARAM_DESC_DE_EN("Geschwindigkeit des Farbwechsels (1-20)", "Speed of color change (1-20)");
            case 4: return PARAM_DESC_DE_EN("Schweifmodus: Nachleuchteffekt", "Trail mode: Fading tail effect");
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
            case 3: return ParameterType::PARAM_UINT8;
            case 4: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 3;   // Spacing
            case 2: return 1;   // DotSize
            case 3: return 5;   // ColorSpeed
            case 4: return 0;   // TrailMode off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 1; // Spacing min
            case 2: return 1; // DotSize min
            case 3: return 1; // ColorSpeed min
            case 4: return 0; // TrailMode false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 10;  // Spacing max
            case 2: return 5;   // DotSize max
            case 3: return 20;  // ColorSpeed max
            case 4: return 1;   // TrailMode true
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
            case 1: return config.option1;  // Spacing
            case 2: return config.option2;  // DotSize
            case 3: return config.option3;  // ColorSpeed
            case 4: return config.feature1; // TrailMode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = value; break;    // Speed
            case 1: config.option1 = value; break;  // Spacing
            case 2: config.option2 = value; break;  // DotSize
            case 3: config.option3 = value; break;  // ColorSpeed
            case 4: config.feature1 = value; break; // TrailMode
            default: break;
        }
    }
};
