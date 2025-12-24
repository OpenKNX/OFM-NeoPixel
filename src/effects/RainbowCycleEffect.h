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
 *
 * Uses config parameters:
 *  - config.speed     : cycle speed (0 => minimal)
 *  - config.intensity : brightness (HSV V)
 *  - config.reverse   : reverse cycling direction
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
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
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        const uint8_t speedVal = (config.speed == 0) ? 1 : config.speed; // Cycle speed (0 => minimal)
        const uint8_t intensityVal = config.intensity;                   // Brightness (HSV V)
        const bool reverseDir = (config.reverse != 0);                   // Reverse cycling direction
        const bool yellowBoost = config.feature2;                        // Yellow brightness compensation
        const bool greenCorr = config.feature3;                          // Green correction hooks

        // Calculate how much to advance the color cycle each frame
        uint8_t inc = speedVal / 16; // Slower than regular rainbow
        if (inc == 0) inc = 1;
        _colorIndex = reverseDir ? (uint8_t)(_colorIndex - inc) : (uint8_t)(_colorIndex + inc);
        // Draw rainbow cycle across the strip
        for (uint16_t i = 0; i < length; i++)
        {
            // Each pixel gets a different hue based on position
            // This creates a rainbow that moves around the strip
            uint8_t hue = _colorIndex + (i * 255 / length);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, intensityVal, yellowBoost, greenCorr);

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