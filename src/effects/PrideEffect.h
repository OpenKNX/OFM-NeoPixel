/**
 * @file PrideEffect.h
 * @brief Pride2015 effect (FastLED port)
 *
 * Beautiful moving rainbow with brightness waves. Port of FastLED's famous Pride2015 pattern by Mark Kriegsman.
 *
 * @copyright Original Pride2015 by Mark Kriegsman (FastLED)
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class PrideEffect : public Effect
{
  private:
    uint16_t _pseudotime;
    uint16_t _hue16;
    uint32_t _lastUpdate;

  public:
    PrideEffect() : _pseudotime(0), _hue16(0), _lastUpdate(0) {}

    /**
     * @brief Update the Pride2015 effect
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        uint8_t masterBrightness = config.intensity; // Global brightness control

        // Brightness wave parameters
        uint8_t sat8 = FastLEDMath::beatsin88(87, 220, 250);
        uint8_t brightdepth = FastLEDMath::beatsin88(341, 96, 224);
        uint16_t brightnessthetainc16 = FastLEDMath::beatsin88(203, (25 * 256), (40 * 256));
        uint8_t msmultiplier = FastLEDMath::beatsin88(147, 23, 60);

        uint16_t hue16 = _hue16;
        uint16_t hueinc16 = FastLEDMath::beatsin88(113, 1, 3000);

        uint16_t ms = millis();
        uint16_t deltams = ms - _lastUpdate;
        _lastUpdate = ms;
        _pseudotime += deltams * msmultiplier;
        _hue16 += deltams * FastLEDMath::beatsin88(400, 5, 9);
        uint16_t brightnesstheta16 = _pseudotime;

        for (uint16_t i = 0; i < length; i++)
        {
            hue16 += hueinc16;
            uint8_t hue8 = hue16 / 256;

            brightnesstheta16 += brightnessthetainc16;
            uint16_t b16 = FastLEDMath::sin16(brightnesstheta16) + 32768;

            uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
            uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
            bri8 += (255 - brightdepth);

            // Apply master brightness scaling
            bri8 = FastLEDMath::scale8(bri8, masterBrightness);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue8, sat8, bri8);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }
    }

    /**
     * @brief Reset the effect to its initial state
     */
    void reset() override
    {
        _pseudotime = 0;
        _hue16 = 0;
        _lastUpdate = millis();
    }

    /**
     * @brief Get effect name
     * @return Human-readable effect name
     */
    const char* getName() override { return "Pride2015"; } // Effect name
};
