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

    const char* getName() override
    {
        return "Sinelon";
    }

    const char* getDescription() override
    {
        return "Single LED moving with sine wave motion";
    }
};