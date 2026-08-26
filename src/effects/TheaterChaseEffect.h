/**
 * @file TheaterChaseEffect.h
 * @brief Theater Chase effect - Classic chase pattern like old theater marquee
 *
 * Classic theater chase effect with configurable spacing and colors.
 * Based on Adafruit NeoPixel examples.
 *
 * Consolidated effect: the former "Theater Chase Rainbow" effect is now
 * the ColorMode=1 (Rainbow) variant of this effect, and a Bounce mode adds
 * a runway-style back-and-forth sweep.
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
 *
 * Config usage:
 *   - config.speed     : movement speed (higher=faster)
 *   - config.option1   : spacing between lit pixels (1-10)
 *   - config.option2   : dot size (1-5)
 *   - config.option3   : color change speed for rainbow mode (1-20)
 *   - config.feature1  : trail mode (0=off, 1=fading tail)
 *   - config.mode      : color mode (0=single color, 1=rainbow)
 *   - config.feature2  : bounce mode (0=wrap, 1=runway back-and-forth)
 */
class TheaterChaseEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    int16_t _position;
    int8_t _direction;
    uint8_t _hue;

  public:
    TheaterChaseEffect() : _lastUpdate(0), _position(0), _direction(1), _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        if (length == 0) return;

        // Use mode for color mode (0=single color, 1=rainbow)
        bool rainbow = (config.mode == 1);

        // Calculate update interval based on speed
        // Rainbow variant historically used a slightly different range.
        uint32_t interval = rainbow ? (30 + ((255 - config.speed) * 170) / 255)
                                    : (20 + ((255 - config.speed) * 180) / 255);

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

        // Use option3 for color change speed in rainbow mode (1-20, default 5)
        uint8_t colorSpeed = config.option3 > 0 ? config.option3 : 5;
        colorSpeed = colorSpeed > 20 ? 20 : colorSpeed;

        // Use feature1 for trail mode (0=off, 1=on)
        bool trailMode = config.feature1;

        // Use feature2 for bounce mode (0=wrap, 1=runway back-and-forth)
        bool bounceMode = config.feature2;

        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++)
        {
            if (trailMode)
            {
                // Fade existing pixels for trail effect (5-channel support)
                uint8_t r, g, b, ww, cw;
                segment->getPixel(i, r, g, b, ww, cw);
                r = r * 0.85f; // 15% fade
                g = g * 0.85f;
                b = b * 0.85f;
                ww = ww * 0.85f;
                cw = cw * 0.85f;
                segment->setPixel(i, r, g, b, ww, cw);
            }
            else
            {
                segment->setPixel(i, 0, 0, 0, 0, 0);
            }
        }

        // Compute the single (non-rainbow) color once
        uint8_t cr = FastLEDMath::scale8(config.r(), config.intensity);
        uint8_t cg = FastLEDMath::scale8(config.g(), config.intensity);
        uint8_t cb = FastLEDMath::scale8(config.b(), config.intensity);
        uint8_t cww = FastLEDMath::scale8(config.ww(), config.intensity);
        uint8_t ccw = FastLEDMath::scale8(config.cw(), config.intensity);

        // Light pixels starting from current position with dot size
        for (int16_t i = _position; i < (int16_t)length; i += spacing)
        {
            if (i < 0) continue;

            uint8_t r, g, b, ww = 0, cw = 0;
            if (rainbow)
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else
            {
                r = cr; g = cg; b = cb; ww = cww; cw = ccw;
            }

            // Set multiple pixels for larger dot size
            for (uint8_t d = 0; d < dotSize && (i + d) < (int16_t)length; d++)
            {
                segment->setPixel(i + d, r, g, b, ww, cw);
            }
        }

        // Advance position
        _position += _direction;
        if (bounceMode)
        {
            // Runway: bounce the chase offset between 0 and spacing-1
            if (_position >= (int16_t)spacing)
            {
                _position = (int16_t)spacing - 1;
                _direction = -1;
            }
            else if (_position < 0)
            {
                _position = 0;
                _direction = 1;
            }
        }
        else
        {
            // Wrap around within one spacing period
            if (_position >= (int16_t)spacing)
            {
                _position = 0;
            }
            else if (_position < 0)
            {
                _position = (int16_t)spacing - 1;
            }
        }

        // Advance hue for rainbow mode
        if (rainbow)
        {
            _hue += colorSpeed;
        }
    }

    void reset() override
    {
        _position = 0;
        _direction = 1;
        _hue = 0;
        _lastUpdate = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Theater Chase";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
                "Laufende Punktmuster wie bei einer Marquee-Lichterkette",
                "Movie theater chase light effect");
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 6; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Spacing";
            case 2: return "DotSize";
            case 3: return "ColorMode";
            case 4: return "ColorSpeed";
            case 5: return "Bounce";
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
            case 3: return PARAM_DESC_DE_EN("Farbmodus: 0=Einfarbig, 1=Regenbogen", "Color mode: 0=single color, 1=rainbow");
            case 4: return PARAM_DESC_DE_EN("Geschwindigkeit des Farbwechsels im Regenbogen-Modus (1-20)", "Speed of color change in rainbow mode (1-20)");
            case 5: return PARAM_DESC_DE_EN("Bounce: 0=Umlauf, 1=Hin-und-her (Runway)", "Bounce: 0=wrap, 1=back-and-forth (runway)");
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
            case 3: return ParameterType::PARAM_ENUM;  // ColorMode
            case 4: return ParameterType::PARAM_UINT8; // ColorSpeed
            case 5: return ParameterType::PARAM_ENUM;  // Bounce
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex == 3)
        {
            switch (enumValue)
            {
                case 0: return "Einfarbig";
                case 1: return "Regenbogen";
                default: return nullptr;
            }
        }
        if (paramIndex == 5)
        {
            switch (enumValue)
            {
                case 0: return "Umlauf";
                case 1: return "Hin und her";
                default: return nullptr;
            }
        }
        return nullptr;
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        if (paramIndex == 3) return 2;
        if (paramIndex == 5) return 2;
        return 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 3;   // Spacing
            case 2: return 1;   // DotSize
            case 3: return 0;   // ColorMode (single color)
            case 4: return 5;   // ColorSpeed
            case 5: return 0;   // Bounce off
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
            case 3: return 0; // ColorMode min
            case 4: return 1; // ColorSpeed min
            case 5: return 0; // Bounce false
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
            case 3: return 1;   // ColorMode max (0=single,1=rainbow)
            case 4: return 20;  // ColorSpeed max
            case 5: return 1;   // Bounce true
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
            case 3: return config.mode;     // ColorMode
            case 4: return config.option3;  // ColorSpeed
            case 5: return config.feature2; // Bounce
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Spacing (1-10)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // DotSize (1-5)
            case 3: config.mode = static_cast<uint8_t>(value); break;    // ColorMode
            case 4: config.option3 = static_cast<uint8_t>(value); break; // ColorSpeed (1-20)
            case 5: config.feature2 = static_cast<bool>(value); break;   // Bounce
            default: break;
        }
    }
};
