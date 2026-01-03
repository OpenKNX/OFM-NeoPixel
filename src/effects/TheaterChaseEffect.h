/**
 * @file TheaterChaseEffect.h
 * @brief Theater Chase effect - Classic chase pattern like old theater marquee
 *
 * Classic theater chase effect with configurable spacing and colors.
 * Based on Adafruit NeoPixel examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Theater Chase Effect
 *
 * Creates a chase pattern where every Nth pixel is lit, creating
 * a "marching ants" effect like old theater marquees.
 */
class TheaterChaseEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing; // Space between lit pixels (default 3)

  public:
    TheaterChaseEffect() : _lastUpdate(0), _position(0), _spacing(3)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Calculate update interval based on speed (map 0-255 to 20-200ms)
        uint32_t interval = 20 + ((255 - config.speed) * 180) / 255;

        uint32_t now = millis();
        if (now - _lastUpdate < interval)
        {
            return; // Not time to update yet
        }
        _lastUpdate = now;

        // Use option1 for spacing (1-10, default 3)
        uint8_t spacing = config.option1 > 0 ? config.option1 : 3;
        spacing = spacing > 10 ? 10 : spacing; // Clamp to reasonable max

        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize; // Clamp to reasonable max

        // Use feature1 for trail mode (0=off, 1=on)
        bool trailMode = config.feature1;

        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++)
        {
            if (trailMode)
            {
                // Fade existing pixels for trail effect (5-channel support)
                uint8_t r, g, b, ww, cw;
                segment->getPixel(i, r, g, b, ww, cw);
                r = r * 0.85; // 15% fade
                g = g * 0.85;
                b = b * 0.85;
                ww = ww * 0.85;
                cw = cw * 0.85;
                segment->setPixel(i, r, g, b, ww, cw);
            }
            else
            {
                segment->setPixel(i, 0, 0, 0, 0, 0);
            }
        }

        // Light pixels starting from current position with dot size
        for (uint16_t i = _position; i < length; i += spacing)
        {
            // Apply master brightness to configured color (5-channel support)
            uint8_t r = FastLEDMath::scale8(config.r(), config.intensity);
            uint8_t g = FastLEDMath::scale8(config.g(), config.intensity);
            uint8_t b = FastLEDMath::scale8(config.b(), config.intensity);
            uint8_t ww = FastLEDMath::scale8(config.ww(), config.intensity);
            uint8_t cw = FastLEDMath::scale8(config.cw(), config.intensity);

            // Set multiple pixels for larger dot size
            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++)
            {
                segment->setPixel(i + d, r, g, b, ww, cw);
            }
        }

        // Advance position
        _position++;
        if (_position >= spacing)
        {
            _position = 0;
        }
    }

    void reset() override
    {
        _position = 0;
        _lastUpdate = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Theater Chase";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Movie theater chase light effect";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Spacing";
            case 2: return "DotSize";
            case 3: return "TrailMode";
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
            case 3: return PARAM_DESC_DE_EN("Schweifmodus: Nachleuchteffekt", "Trail mode: Fading tail effect");
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
            case 0: return 128; // Speed
            case 1: return 3;   // Spacing
            case 2: return 1;   // DotSize
            case 3: return 0;   // TrailMode off
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
            case 3: return 0; // TrailMode false
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
            case 3: return 1;   // TrailMode true
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
            case 3: return config.feature1; // TrailMode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (movement speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Spacing (1-10)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // DotSize (1-5)
            case 3: config.feature1 = static_cast<bool>(value); break;   // TrailMode
            default: break;
        }
    }
};