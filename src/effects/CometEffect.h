/**
 * @file CometEffect.h
 * @brief Comet/Meteor effect - Moving dot with trailing tail
 *
 * Creates a comet or meteor effect with a bright head and fading tail.
 * Based on common LED effect patterns.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Comet Effect
 *
 * A moving dot with a fading tail, like a comet moving across the sky.
 */
/**
 * Config usage:
 *   - config.speed     : movement speed (higher=faster)
 *   - config.option1   : tail fade rate (0-250, default 230) - how fast tail fades
 *   - config.option2   : tail length (5-30 LEDs, default 10)
 *   - config.feature1  : bounce mode (0=restart from start, 1=bounce at ends)
 *   - config.feature2  : rainbow mode (0=set color, 1=rainbow gradient in tail)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : comet color (if all 0, uses cycling rainbow)
 */
class CometEffect : public Effect
{
  private:
    int16_t _position;
    int8_t _direction;
    uint32_t _lastUpdate;
    uint8_t _hue;
    uint8_t _tailLength;

  public:
    CometEffect() : _position(0), _direction(1), _lastUpdate(0), _hue(0), _tailLength(10)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for tail fade rate (200-250, default 230)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 5) : 230;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        // Use option2 for tail length (5-30, default 10)
        _tailLength = config.option2 > 0 ? config.option2 : 10;
        _tailLength = _tailLength < 5 ? 5 : (_tailLength > 30 ? 30 : _tailLength);

        // Use feature1 for bounce mode (0=restart, 1=bounce)
        bool bounceMode = config.feature1;

        // Use feature2 for rainbow mode (0=set color, 1=rainbow)
        bool rainbowMode = config.feature2;

        // Calculate update interval based on speed
        uint32_t interval = 10 + ((255 - config.speed) * 90) / 255;

        uint32_t now = millis();
        if (now - _lastUpdate < interval)
        {
            return;
        }
        _lastUpdate = now;

        // Fade all pixels to create comet tail
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);

            // Apply configurable fade rate
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);

            segment->setPixel(i, r, g, b);
        }

        // Draw enhanced comet tail
        for (uint8_t t = 1; t <= _tailLength; t++)
        {
            int16_t tailPos = _position - (_direction * t);
            if (tailPos >= 0 && tailPos < length)
            {
                // Calculate tail brightness (exponential decay)
                uint8_t brightness = 255 / (t + 1);

                uint8_t r, g, b;
                if (rainbowMode)
                {
                    // Rainbow tail
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        _hue - (t * 10), 255,
                        FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                {
                    // Default cycling rainbow
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        _hue, 255,
                        FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else
                {
                    // Use configured color
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = FastLEDMath::scale8(config.r(), brightness);
                    g = FastLEDMath::scale8(config.g(), brightness);
                    b = FastLEDMath::scale8(config.b(), brightness);
                }

                segment->setPixel(tailPos, r, g, b);
            }
        }

        // Draw bright comet head if position is valid
        if (_position >= 0 && _position < length)
        {
            uint8_t r, g, b;

            if (rainbowMode || (config.r() == 0 && config.g() == 0 && config.b() == 0))
            {
                // Rainbow or no color configured
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
                _hue += 2; // Slow hue cycling
            }
            else
            {
                // Use configured color
                r = FastLEDMath::scale8(config.r(), config.intensity);
                g = FastLEDMath::scale8(config.g(), config.intensity);
                b = FastLEDMath::scale8(config.b(), config.intensity);
            }

            segment->setPixel(_position, r, g, b);
        }

        // Move comet
        _position += _direction;

        // Handle boundaries
        if (bounceMode)
        {
            // Bounce at ends
            if (_position >= length)
            {
                _position = length - 1;
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
            // Restart from beginning
            if (_position >= length)
            {
                _position = 0;
            }
            else if (_position < 0)
            {
                _position = length - 1;
            }
        }
    }

    void reset() override
    {
        _position = 0;
        _direction = 1;
        _lastUpdate = 0;
        _hue = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Comet";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Comet flying through space with tail";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "FadeRate";
            case 2: return "TailLength";
            case 3: return "BounceMode";
            case 4: return "RainbowMode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Bewegungsgeschwindigkeit (höher=schneller)", "Speed: Movement speed (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate: Wie schnell der Schweif verblasst (0-250)", "Fade rate: How fast the tail fades (0-250)");
            case 2: return PARAM_DESC_DE_EN("Schweiflänge in LEDs (5-30)", "Tail length in LEDs (5-30)");
            case 3: return PARAM_DESC_DE_EN("Sprungmodus: Hin und her springen", "Bounce mode: Bounce back and forth");
            case 4: return PARAM_DESC_DE_EN("Regenbogenmodus: Farbverlauf im Schweif", "Rainbow mode: Color gradient in tail");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_UINT8; // TailLength (5-30)
            case 3: return ParameterType::PARAM_BOOL;  // BounceMode
            case 4: return ParameterType::PARAM_BOOL;  // RainbowMode
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 150; // FadeRate (maps to ~230)
            case 2: return 10;  // TailLength
            case 3: return 0;   // BounceMode off
            case 4: return 0;   // RainbowMode off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // FadeRate min
            case 2: return 5; // TailLength min
            case 3: return 0; // BounceMode false
            case 4: return 0; // RainbowMode false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 250; // FadeRate max
            case 2: return 30;  // TailLength max
            case 3: return 1;   // BounceMode true
            case 4: return 1;   // RainbowMode true
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
            case 1: return config.option1;  // FadeRate
            case 2: return config.option2;  // TailLength
            case 3: return config.feature1; // BounceMode
            case 4: return config.feature2; // RainbowMode
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
            case 1: config.option1 = static_cast<uint8_t>(value); break; // FadeRate (0-250)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // TailLength (5-30 LEDs)
            case 3: config.feature1 = static_cast<bool>(value); break;   // BounceMode
            case 4: config.feature2 = static_cast<bool>(value); break;   // RainbowMode
            default: break;
        }
    }
};