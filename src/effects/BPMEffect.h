/**
 * @file BPMEffect.h
 * @brief BPM (Beats Per Minute) effect (FastLED port)
 *
 * Colored stripes pulsing at a BPM rate. Port of FastLED's BPM pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class BPMEffect : public Effect
{
  private:
    uint8_t _gHue;
    uint16_t _beatsPerMinute;

  public:
    BPMEffect(uint16_t bpm = 62) : _gHue(0), _beatsPerMinute(bpm) {}

    /**
     * @brief Get the current BPM (Beats Per Minute)
     *
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        uint16_t length = segment->getLength();
        auto& config = segment->getConfig();
        uint8_t masterBrightness = config.intensity; // Global brightness control

        // Use speed config if available
        if (config.speed > 0)
        {
            _beatsPerMinute = config.speed;
        }

        uint8_t beat = FastLEDMath::beatsin8(_beatsPerMinute, 64, 255);

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t hue = _gHue + (i * 2);
            uint8_t brightness = beat - _gHue + (i * 10);

            // Apply master brightness scaling
            brightness = FastLEDMath::scale8(brightness, masterBrightness);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        _gHue++;
    }

    void reset() override { _gHue = 0; } // Reset hue effect
    void setBPM(uint16_t bpm) { _beatsPerMinute = bpm; } // Set BPM
    const char* getName() override { return "BPM"; } // Effect name
}; // class BPMEffect
