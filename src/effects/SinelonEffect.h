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
 *
 * Uses config parameters:
 *  - config.speed     : movement speed mapped to BPM (sine/bounce)
 *  - config.intensity : brightness scaling (HSV V / RGB scale)
 *  - config.option1   : fade rate control (0 => default)
 *  - config.option2   : dot size (0 => 1)
 *  - config.reverse   : reverse direction (mirrors position)
 *  - config.feature1  : rainbow mode (else uses configured RGB color)
 *  - config.feature2  : bounce mode (linear bounce instead of sine)
 *  - config.feature3  : enable HSV rainbow corrections (yellow/green hooks)
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

        // Snapshot config per frame (avoids mixed frames if KNX updates mid-loop)
        const uint8_t speedVal = config.speed;         // movement speed mapped to BPM
        const uint8_t intensityVal = config.intensity; // brightness scaling
        const uint8_t option1 = config.option1;        // fade rate control
        const uint8_t option2 = config.option2;        // dot size
        const bool reverseDir = (config.reverse != 0); // reverse direction
        const bool rainbowMode = config.feature1;      // rainbow mode
        const bool bounceMode = config.feature2;       // bounce mode
        const bool yellowBoost = config.feature3;      // enable HSV rainbow corrections
        const bool greenCorr = config.feature3;        // enable HSV rainbow corrections
        const uint8_t cfgR = config.r();
        const uint8_t cfgG = config.g();
        const uint8_t cfgB = config.b();

        // Use option1 for fade rate (200-250, default 235)
        uint8_t fadeRate = (option1 > 0) ? (uint8_t)(200 + option1 / 5) : 235;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = (option2 > 0) ? option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize;

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
            uint16_t bpm = 5 + ((speedVal * 45) / 255);

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
            uint16_t bpm = 5 + ((speedVal * 45) / 255);
            pos = FastLEDMath::beatsin16(bpm, 0, length - 1);
        }

        if (reverseDir && length > 0)
        {
            pos = (int)((length - 1) - (uint16_t)pos);
        }

        // Determine color
        uint8_t r, g, b;
        if (rainbowMode || (cfgR == 0 && cfgG == 0 && cfgB == 0))
        {
            // Rainbow mode or no color configured
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, intensityVal, yellowBoost, greenCorr);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
            _hue++; // Slowly change hue
        }
        else
        {
            // Use configured color
            r = FastLEDMath::scale8(cfgR, intensityVal);
            g = FastLEDMath::scale8(cfgG, intensityVal);
            b = FastLEDMath::scale8(cfgB, intensityVal);
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