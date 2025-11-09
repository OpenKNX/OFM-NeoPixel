/**
 * @file ConfettiEffect.h
 * @brief Confetti effect (FastLED port)
 *
 * Random colored pixels fade in and out. Port of FastLED's Confetti pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class ConfettiEffect : public Effect
{
  private:
    uint8_t _gHue;
    uint32_t _lastUpdate;

  public:
    ConfettiEffect() : _gHue(0), _lastUpdate(0) {}

    /**
     * @brief Update the confetti effect
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        uint8_t brightness = config.intensity; // Global brightness control

        // Fade all LEDs by reading current, dimming, writing back
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            if (segment->getPixel(i, r, g, b))
            {
                r = FastLEDMath::fadeToBlackBy(r, 10);
                g = FastLEDMath::fadeToBlackBy(g, 10);
                b = FastLEDMath::fadeToBlackBy(b, 10);
                segment->setPixel(i, r, g, b);
            }
        }

        // Add new random confetti (with brightness scaling)
        int pos = FastLEDMath::random8(length);
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_gHue + FastLEDMath::random8(64), 200, brightness);

        // Add to existing pixel (qadd)
        uint8_t r, g, b;
        if (segment->getPixel(pos, r, g, b))
        {
            r = FastLEDMath::qadd8(r, (rgb >> 16) & 0xFF);
            g = FastLEDMath::qadd8(g, (rgb >> 8) & 0xFF);
            b = FastLEDMath::qadd8(b, rgb & 0xFF);
            segment->setPixel(pos, r, g, b);
        }

        // Slowly cycle hue
        uint32_t now = millis();
        if (now - _lastUpdate > 20)
        {
            _gHue++;
            _lastUpdate = now;
        }
    }

    void reset() override { _gHue = 0; _lastUpdate = 0; } // Reset effect state    
    const char* getName() override { return "Confetti"; } // Effect name
}; // class ConfettiEffect
