/**
 * @file SinelonEffect.h
 * @brief Sinelon effect - A colored dot sweeping back and forth with fading trails
 *
 * Port of FastLED's Sinelon effect with smooth sine-wave movement.
 * Based on FastLED DemoReel100 examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Sinelon Effect
 *
 * A colored dot sweeping back and forth, with fading trails.
 * Uses beatsin16 for smooth sine wave movement.
 */
class SinelonEffect : public Effect
{
  private:
    uint8_t _hue;
    int16_t _position;
    bool _direction; // For bounce mode

  public:
    SinelonEffect() : _hue(0), _position(0), _direction(true)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for fade rate (200-250, default 235)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 5) : 235;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize;

        // Use feature1 for rainbow mode (0=use set color, 1=rainbow)
        bool rainbowMode = config.feature1;

        // Use feature2 for bounce mode (0=sine wave, 1=linear bounce)
        bool bounceMode = config.feature2;

        // Fade all pixels to create trailing effect
        for (uint16_t i = 0; i < length; i++)
        {
            // Get current pixel color and fade it
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);

            // Apply configurable fade rate
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);

            segment->setPixel(i, r, g, b);
        }

        // Calculate position
        int pos;
        if (bounceMode)
        {
            // Linear bounce mode
            // Calculate BPM from speed (map 0-255 to 5-50 BPM)
            uint16_t bpm = 5 + ((config.speed * 45) / 255);

            // Update position based on direction
            _position += _direction ? bpm / 10 : -(bpm / 10);

            // Bounce at edges
            if (_position >= (length - 1) * 16)
            {
                _position = (length - 1) * 16;
                _direction = false;
            }
            else if (_position <= 0)
            {
                _position = 0;
                _direction = true;
            }

            pos = _position / 16; // Scale down for pixel position
        }
        else
        {
            // Sine wave mode (original behavior)
            uint16_t bpm = 5 + ((config.speed * 45) / 255);
            pos = FastLEDMath::beatsin16(bpm, 0, length - 1);
        }

        // Determine color
        uint8_t r, g, b;
        if (rainbowMode || (config.r() == 0 && config.g() == 0 && config.b() == 0))
        {
            // Rainbow mode or no color configured
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
            _hue++; // Slowly change hue
        }
        else
        {
            // Use configured color
            r = FastLEDMath::scale8(config.r(), config.intensity);
            g = FastLEDMath::scale8(config.g(), config.intensity);
            b = FastLEDMath::scale8(config.b(), config.intensity);
        }

        // Add bright pixel(s) at current position
        for (uint8_t d = 0; d < dotSize && (pos + d) < length; d++)
        {
            segment->setPixel(pos + d, r, g, b);
        }
    }

    void reset() override
    {
        _hue = 0;
        _position = 0;
        _direction = true;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Sinelon";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Single LED moving with sine wave motion";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "FadeRate";
            case 2: return "DotSize";
            case 3: return "RainbowMode";
            case 4: return "BounceMode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Bewegungsgeschwindigkeit (höher=schneller)", "Speed: Movement speed (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate: Wie schnell der Schweif verblasst (0-250)", "Fade rate: How fast the trail fades (0-250)");
            case 2: return PARAM_DESC_DE_EN("Punktgröße: Anzahl LEDs im Punkt (1-5)", "Dot size: Number of LEDs in dot (1-5)");
            case 3: return PARAM_DESC_DE_EN("Regenbogenmodus: Farbwechsel statt fester Farbe", "Rainbow mode: Color cycling instead of fixed color");
            case 4: return PARAM_DESC_DE_EN("Sprungmodus: Linear springen statt Sinuswelle", "Bounce mode: Linear bounce instead of sine wave");
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
            case 1: return 175; // FadeRate
            case 2: return 1;   // DotSize
            case 3: return 0;   // RainbowMode
            case 4: return 0;   // BounceMode
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // FadeRate min
            case 2: return 1; // DotSize min
            case 3: return 0; // RainbowMode false
            case 4: return 0; // BounceMode false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 250; // FadeRate max
            case 2: return 5;   // DotSize max
            case 3: return 1;   // RainbowMode true
            case 4: return 1;   // BounceMode true
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
            case 2: return config.option2;  // DotSize
            case 3: return config.feature1; // RainbowMode
            case 4: return config.feature2; // BounceMode
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
            case 2: config.option2 = static_cast<uint8_t>(value); break; // DotSize (1-5 LEDs)
            case 3: config.feature1 = static_cast<bool>(value); break;   // RainbowMode
            case 4: config.feature2 = static_cast<bool>(value); break;   // BounceMode
            default: break;
        }
    }
};