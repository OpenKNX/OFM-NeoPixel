/**
 * @file JuggleEffect.h
 * @brief Juggle effect (FastLED port)
 *
 * Colored dots weaving in and out of sync with each other. Port of FastLED's Juggle pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class JuggleEffect : public Effect
{
  private:
    uint8_t _gHue;

  public:
    JuggleEffect() : _gHue(0) {}

    /**
     * @brief Update the juggle effect
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        uint8_t brightness = config.intensity; // Global brightness control

        // Fade all LEDs by reading, dimming, writing back
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            if (segment->getPixel(i, r, g, b))
            {
                r = FastLEDMath::fadeToBlackBy(r, 20);
                g = FastLEDMath::fadeToBlackBy(g, 20);
                b = FastLEDMath::fadeToBlackBy(b, 20);
                segment->setPixel(i, r, g, b);
            }
        }

        uint8_t dothue = 0;
        for (int i = 0; i < 8; i++)
        {
            int pos = FastLEDMath::beatsin16(i + 7, 0, length - 1);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(dothue, 200, brightness);

            // OR with existing pixel value
            uint8_t r, g, b;
            if (segment->getPixel(pos, r, g, b))
            {
                r |= (rgb >> 16) & 0xFF;
                g |= (rgb >> 8) & 0xFF;
                b |= rgb & 0xFF;
                segment->setPixel(pos, r, g, b);
            }

            dothue += 32;
        }
    }

    /**
     * @brief Reset the effect to its initial state
     */
    void reset() override
    {
        _gHue = 0;
    }

    /**
     * @brief Get effect name
     * @return Human-readable effect name
     */
    const char* getName() override { return "Juggle"; } // Effect name
};
