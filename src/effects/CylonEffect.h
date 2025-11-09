/**
 * @file CylonEffect.h
 * @brief Cylon/Knight Rider effect (FastLED port)
 *
 * A single LED eye bouncing back and forth with a trailing fade.
 * Classic "KITT" effect from Knight Rider / Cylon from Battlestar Galactica.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class CylonEffect : public Effect
{
  private:
    float _position; // Changed to float for smooth sub-pixel movement
    int8_t _direction;
    uint8_t _eyeSize;
    uint8_t _hue;
    float _speed; // Pixels per frame (auto-calculated)

  public:
    /**
     * @brief Constructor
     * @param eyeSize Size of the eye (default: 4)
     * @param hue Color hue (default: 0 = red)
     * @param speed Movement speed in pixels per frame (0 = auto-scale based on length)
     */
    CylonEffect(uint8_t eyeSize = 4, uint8_t hue = 0, float speed = 0.0f)
        : _position(0.0f), _direction(1), _eyeSize(eyeSize), _hue(hue), _speed(speed) {}

    /**
     * @brief Update the Cylon effect
     * @param segment The segment to update
     * @param deltaTime Time since last update in milliseconds
     */
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        uint8_t brightness = config.intensity; // Global brightness control

        // Auto-calculate speed based on segment length if not manually set
        // Target: ~2 seconds per direction (4 seconds full cycle)
        // At 62.5 FPS: speed = length / (2 * 62.5) = length / 125
        float effectiveSpeed = _speed;
        if (effectiveSpeed == 0.0f)
        {
            // Scale: 8 LEDs = 0.5 px/frame, 200 LEDs = 1.6 px/frame
            effectiveSpeed = (float)length / 125.0f;
            if (effectiveSpeed < 0.5f) effectiveSpeed = 0.5f; // Minimum speed for small segments
        }

        // Fade all LEDs
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            if (segment->getPixel(i, r, g, b))
            {
                r = FastLEDMath::fadeToBlackBy(r, 40);
                g = FastLEDMath::fadeToBlackBy(g, 40);
                b = FastLEDMath::fadeToBlackBy(b, 40);
                segment->setPixel(i, r, g, b);
            }
        }

        // Draw the eye at current position (rounded)
        uint16_t pixelPos = (uint16_t)_position;
        for (uint8_t i = 0; i < _eyeSize; i++)
        {
            int16_t pos = pixelPos + i - (_eyeSize / 2);
            if (pos >= 0 && pos < length)
            {
                // Calculate brightness falloff for smoother eye
                uint8_t eyeBrightness = brightness;
                if (i == 0 || i == _eyeSize - 1)
                {
                    eyeBrightness = brightness / 2; // Dimmer edges
                }

                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, eyeBrightness);
                segment->setPixel(pos,
                                  (rgb >> 16) & 0xFF,
                                  (rgb >> 8) & 0xFF,
                                  rgb & 0xFF);
            }
        }

        // Move the eye
        _position += effectiveSpeed * _direction;

        // Bounce at edges
        if (_position >= length - 1)
        {
            _position = length - 1;
            _direction = -1;
        }
        else if (_position <= 0)
        {
            _position = 0;
            _direction = 1;
        }
    }

    /**
     * @brief Reset the effect to its initial state
     */
    void reset() override
    {
        _position = 0.0f;
        _direction = 1;
    }

    /**
     * @brief Set the eye size
     * @param size New eye size
     */
    void setEyeSize(uint8_t size)
    {
        _eyeSize = size;
    }

    void setHue(uint8_t hue) { _hue = hue; } // Set new hue value
    void setSpeed(float speed) { _speed = speed; } // 0 = auto-scale, > 0 = manual pixels per frame
    const char* getName() override { return "Cylon"; } // Effect name
};
