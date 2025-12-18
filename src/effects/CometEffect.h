/**
 * @file CometEffect.h
 * @brief Comet/Meteor effect - Moving dot with trailing tail
 *
 * Creates a comet or meteor effect with a bright head and fading tail.
 * Based on common LED effect patterns.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Comet Effect
 *
 * A moving dot with a fading tail, like a comet moving across the sky.
 */
class CometEffect : public Effect
{
  private:
    int16_t _position;
    int8_t _direction;
    uint32_t _lastUpdate;
    uint8_t _hue;
    uint8_t _tailLength;

  public:
    CometEffect() : _position(0), _direction(1), _lastUpdate(0), _hue(0), _tailLength(10)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for tail fade rate (200-250, default 230)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 5) : 230;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        // Use option2 for tail length (5-30, default 10)
        _tailLength = config.option2 > 0 ? config.option2 : 10;
        _tailLength = _tailLength < 5 ? 5 : (_tailLength > 30 ? 30 : _tailLength);

        // Use feature1 for bounce mode (0=restart, 1=bounce)
        bool bounceMode = config.feature1;

        // Use feature2 for rainbow mode (0=set color, 1=rainbow)
        bool rainbowMode = config.feature2;

        // Calculate update interval based on speed
        uint32_t interval = 10 + ((255 - config.speed) * 90) / 255;

        uint32_t now = millis();
        if (now - _lastUpdate < interval)
        {
            return;
        }
        _lastUpdate = now;

        // Fade all pixels to create comet tail
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);

            // Apply configurable fade rate
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);

            segment->setPixel(i, r, g, b);
        }

        // Draw enhanced comet tail
        for (uint8_t t = 1; t <= _tailLength; t++)
        {
            int16_t tailPos = _position - (_direction * t);
            if (tailPos >= 0 && tailPos < length)
            {
                // Calculate tail brightness (exponential decay)
                uint8_t brightness = 255 / (t + 1);

                uint8_t r, g, b;
                if (rainbowMode)
                {
                    // Rainbow tail
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        _hue - (t * 10), 255,
                        FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                {
                    // Default cycling rainbow
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        _hue, 255,
                        FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else
                {
                    // Use configured color
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = FastLEDMath::scale8(config.r(), brightness);
                    g = FastLEDMath::scale8(config.g(), brightness);
                    b = FastLEDMath::scale8(config.b(), brightness);
                }

                segment->setPixel(tailPos, r, g, b);
            }
        }

        // Draw bright comet head if position is valid
        if (_position >= 0 && _position < length)
        {
            uint8_t r, g, b;

            if (rainbowMode || (config.r() == 0 && config.g() == 0 && config.b() == 0))
            {
                // Rainbow or no color configured
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
                _hue += 2; // Slow hue cycling
            }
            else
            {
                // Use configured color
                r = FastLEDMath::scale8(config.r(), config.intensity);
                g = FastLEDMath::scale8(config.g(), config.intensity);
                b = FastLEDMath::scale8(config.b(), config.intensity);
            }

            segment->setPixel(_position, r, g, b);
        }

        // Move comet
        _position += _direction;

        // Handle boundaries
        if (bounceMode)
        {
            // Bounce at ends
            if (_position >= length)
            {
                _position = length - 1;
                _direction = -1;
            }
            else if (_position < 0)
            {
                _position = 0;
                _direction = 1;
            }
        }
        else
        {
            // Restart from beginning
            if (_position >= length)
            {
                _position = 0;
            }
            else if (_position < 0)
            {
                _position = length - 1;
            }
        }
    }

    void reset() override
    {
        _position = 0;
        _direction = 1;
        _lastUpdate = 0;
        _hue = 0;
    }

    const char* getName() override
    {
        return "Comet";
    }
};

/**
 * Meteor Effect
 *
 * Similar to comet but with random size meteors and random decay.
 */
class MeteorEffect : public Effect
{
  private:
    int16_t _position;
    int8_t _direction;
    uint32_t _lastUpdate;
    uint8_t _hue;
    uint8_t _meteorSize;
    uint32_t _nextMeteor;

  public:
    MeteorEffect() : _position(-1), _direction(1), _lastUpdate(0), _hue(0), _meteorSize(5), _nextMeteor(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for meteor size range (2-15, default 3-8)
        uint8_t minSize = config.option1 > 0 ? (2 + config.option1 / 20) : 3;
        uint8_t maxSize = minSize + 5;
        maxSize = maxSize > 15 ? 15 : maxSize;

        // Use option2 for meteor frequency (100-5000ms, default based on speed)
        uint32_t baseInterval = config.option2 > 0 ? (100 + config.option2 * 19) : (1000 + ((255 - config.speed) * 4000) / 255);

        // Use feature1 for random colors (0=set color, 1=random)
        bool randomColors = config.feature1;

        // Use feature2 for multiple meteors (0=single, 1=multiple)
        bool multiMeteor = config.feature2;

        uint32_t now = millis();

        // Start new meteor if needed
        if (_position < 0 && now >= _nextMeteor)
        {
            _position = length - 1;                               // Start from top
            _meteorSize = FastLEDMath::random8(minSize, maxSize); // Configurable meteor size

            if (randomColors)
            {
                _hue = FastLEDMath::random8(); // Random color
            }
            else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
            {
                _hue = FastLEDMath::random8(); // Default to random if no color set
            }

            // Schedule next meteor
            uint32_t interval = baseInterval;
            if (multiMeteor)
            {
                interval /= 3; // More frequent meteors
            }
            interval += FastLEDMath::random16(interval); // Add randomness
            _nextMeteor = now + interval;
        }

        // Update meteor position
        uint32_t moveInterval = 20 + ((255 - config.speed) * 80) / 255;
        if (now - _lastUpdate >= moveInterval && _position >= 0)
        {
            _lastUpdate = now;

            // Fade all pixels randomly for meteor rain effect
            for (uint16_t i = 0; i < length; i++)
            {
                if (FastLEDMath::random8() > 160)
                { // 60% chance to fade each pixel
                    uint8_t r, g, b;
                    segment->getPixel(i, r, g, b);

                    // Random fade amount
                    uint8_t fade = FastLEDMath::random8(200, 240);
                    r = FastLEDMath::scale8(r, fade);
                    g = FastLEDMath::scale8(g, fade);
                    b = FastLEDMath::scale8(b, fade);

                    segment->setPixel(i, r, g, b);
                }
            }

            // Draw meteor body
            for (uint8_t i = 0; i < _meteorSize; i++)
            {
                int16_t pos = _position + i;
                if (pos >= 0 && pos < length)
                {
                    uint8_t brightness = 255 - (i * 255 / _meteorSize); // Fade along body

                    uint8_t r, g, b;
                    if (randomColors || (config.r() == 0 && config.g() == 0 && config.b() == 0))
                    {
                        // Use random/rainbow hue
                        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255,
                                                                    FastLEDMath::scale8(brightness, config.intensity));
                        r = (rgb >> 16) & 0xFF;
                        g = (rgb >> 8) & 0xFF;
                        b = rgb & 0xFF;
                    }
                    else
                    {
                        // Use configured color
                        brightness = FastLEDMath::scale8(brightness, config.intensity);
                        r = FastLEDMath::scale8(config.r(), brightness);
                        g = FastLEDMath::scale8(config.g(), brightness);
                        b = FastLEDMath::scale8(config.b(), brightness);
                    }

                    segment->setPixel(pos, r, g, b);
                }
            }

            // Move meteor down
            _position--;

            // Remove meteor when it goes off screen
            if (_position + _meteorSize < 0)
            {
                _position = -1;
            }
        }
    }

    void reset() override
    {
        _position = -1;
        _direction = 1;
        _lastUpdate = 0;
        _hue = 0;
        _nextMeteor = 0;
    }

    const char* getName() override
    {
        return "Meteor";
    }
};