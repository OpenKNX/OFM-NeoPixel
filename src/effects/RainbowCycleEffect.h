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

        // Calculate how much to advance the color cycle each frame
        uint8_t speed = config.speed > 0 ? config.speed : 1;
        _colorIndex += speed / 16; // Slower than regular rainbow

        // Draw rainbow cycle across the strip
        for (uint16_t i = 0; i < length; i++)
        {
            // Each pixel gets a different hue based on position
            // This creates a rainbow that moves around the strip
            uint8_t hue = _colorIndex + (i * 255 / length);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, config.intensity);

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

    const char* getName() override
    {
        return "Rainbow Cycle";
    }
};