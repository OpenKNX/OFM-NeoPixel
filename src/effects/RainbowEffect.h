/**
 * @file RainbowEffect.h
 * @brief Classic Rainbow effect (FastLED port)
 *
 * Smooth rainbow gradient that cycles across the strip. Based on FastLED's classic rainbow pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class RainbowEffect : public Effect
{
  private:
    uint8_t _hueOffset;

  public:
    RainbowEffect() : _hueOffset(0) {}

    /**
     * @brief Update the rainbow effect
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Rainbow across segment (use intensity as brightness)
        uint8_t brightness = config.intensity;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t hue = _hueOffset + (i * 255 / length);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        // Rotate hue (speed control)
        _hueOffset += (config.speed > 0) ? config.speed : 1;
    }

    /**
     * @brief Reset the effect to its initial state
     */
    void reset() override
    {
        _hueOffset = 0;
    }

    /**
     * @brief Get effect name
     * @return Human-readable effect name
     */
    const char* getName() override { return "Rainbow"; } // Effect name
};
