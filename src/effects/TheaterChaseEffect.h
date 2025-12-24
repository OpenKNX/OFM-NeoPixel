/**
 * @file TheaterChaseEffect.h
 * @brief Theater Chase effect - Classic chase pattern like old theater marquee
 *
 * Classic theater chase effect with configurable spacing and colors.
 * Based on Adafruit NeoPixel examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Theater Chase Effect
 *
 * Creates a chase pattern where every Nth pixel is lit, creating
 * a "marching ants" effect like old theater marquees.
 */
/**
 * @brief Theater Chase effect
 *
 * Uses config parameters:
 *  - config.speed     : update interval (movement speed)
 *  - config.intensity : master brightness scaling for RGB
 *  - config.option1   : spacing (1..10, 0 => default 3)
 *  - config.option2   : dot size (1..5, 0 => default 1)
 *  - config.reverse   : reverse chase direction
 *  - config.feature1  : trail mode (fade instead of clear)
 */
class TheaterChaseEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing; // Space between lit pixels (default 3)

  public:
    TheaterChaseEffect() : _lastUpdate(0), _position(0), _spacing(3)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        const uint8_t speedVal = config.speed;         // update interval (movement speed)
        const uint8_t intensityVal = config.intensity; // master brightness scaling for RGB
        const uint8_t option1 = config.option1;        // spacing (1..10, 0 => default 3)
        const uint8_t option2 = config.option2;        // dot size (1..5, 0 => default 1)
        const bool reverseDir = (config.reverse != 0); // reverse chase direction
        const bool trailMode = config.feature1;        // trail mode (fade instead of clear)

        // Calculate update interval based on speed (map 0-255 to 20-200ms)
        const uint32_t interval = 20UL + ((uint32_t)(255 - speedVal) * 180UL) / 255UL;

        const uint32_t now = millis();
        if (now - _lastUpdate < interval)
            return; // Not time to update yet
        _lastUpdate = now;

        // Spacing & dot size
        uint8_t spacing = (option1 > 0) ? option1 : 3;
        if (spacing > 10) spacing = 10;
        uint8_t dotSize = (option2 > 0) ? option2 : 1;
        if (dotSize > 5) dotSize = 5;

        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++)
        {
            if (trailMode)
            {
                // Fade existing pixels for trail effect
                uint8_t r, g, b;
                segment->getPixel(i, r, g, b);
                r = (uint8_t)(r * 0.85f); // 15% fade
                g = (uint8_t)(g * 0.85f);
                b = (uint8_t)(b * 0.85f);
                segment->setPixel(i, r, g, b);
            }
            else
            {
                segment->setPixel(i, 0, 0, 0);
            }
        }

        // Light every spacing-th pixel with configured color
        const uint8_t baseR = FastLEDMath::scale8(config.r(), intensityVal);
        const uint8_t baseG = FastLEDMath::scale8(config.g(), intensityVal);
        const uint8_t baseB = FastLEDMath::scale8(config.b(), intensityVal);

        for (uint16_t i = _position; i < length; i += spacing)
        {
            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++)
                segment->setPixel(i + d, baseR, baseG, baseB);
        }

        // Advance position (phase) with optional reverse direction
        if (reverseDir)
            _position = (_position == 0) ? (uint8_t)(spacing - 1) : (uint8_t)(_position - 1);
        else
        {
            _position++;
            if (_position >= spacing) _position = 0;
        }
    }

    void reset() override
    {
        _position = 0;
        _lastUpdate = 0;
    }

    const char* getName() override
    {
        return "Theater Chase";
    }

    const char* getDescription() override
    {
        return "Movie theater chase light effect";
    }
};

/**
 * Theater Chase Rainbow Effect
 *
 * Same as theater chase but cycles through rainbow colors
 */
/**
 * @brief Theater Chase Rainbow effect
 *
 * Uses config parameters:
 *  - config.speed     : update interval (movement speed)
 *  - config.intensity : brightness (HSV V)
 *  - config.option1   : spacing (1..10, 0 => default 3)
 *  - config.option2   : dot size (1..5, 0 => default 1)
 *  - config.option3   : hue advance per step (1..20, 0 => default 5)
 *  - config.reverse   : reverse chase direction
 *  - config.feature1  : trail mode (fade instead of clear)
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
 */
class TheaterChaseRainbowEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing;
    uint8_t _hue;

  public:
    TheaterChaseRainbowEffect() : _lastUpdate(0), _position(0), _spacing(3), _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        const uint8_t speedVal = config.speed;         // update interval (movement speed)
        const uint8_t intensityVal = config.intensity; // master brightness scaling for RGB
        const uint8_t option1 = config.option1;        // spacing (1..10, 0 => default 3)
        const uint8_t option2 = config.option2;        // dot size (1..5, 0 => default 1)
        const uint8_t option3 = config.option3;        // hue advance per step (1..20, 0 => default 5)
        const bool reverseDir = (config.reverse != 0); // reverse chase direction
        const bool trailMode = config.feature1;        // trail mode (fade instead of clear)
        const bool yellowBoost = config.feature2;      // enable HSV rainbow corrections
        const bool greenCorr = config.feature3;        // enable HSV rainbow corrections

        // Calculate update interval based on speed
        const uint32_t interval = 30UL + ((uint32_t)(255 - speedVal) * 170UL) / 255UL;

        const uint32_t now = millis();
        if (now - _lastUpdate < interval)
            return;
        _lastUpdate = now;

        // Spacing & dot size
        uint8_t spacing = (option1 > 0) ? option1 : 3;
        if (spacing > 10) spacing = 10;
        uint8_t dotSize = (option2 > 0) ? option2 : 1;
        if (dotSize > 5) dotSize = 5;

        // Hue change speed
        uint8_t colorSpeed = (option3 > 0) ? option3 : 5;
        if (colorSpeed > 20) colorSpeed = 20;

        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++)
        {
            if (trailMode)
            {
                uint8_t r, g, b;
                segment->getPixel(i, r, g, b);
                r = (uint8_t)(r * 0.85f);
                g = (uint8_t)(g * 0.85f);
                b = (uint8_t)(b * 0.85f);
                segment->setPixel(i, r, g, b);
            }
            else
            {
                segment->setPixel(i, 0, 0, 0);
            }
        }

        // Light every spacing-th pixel with rainbow color
        for (uint16_t i = _position; i < length; i += spacing)
        {
            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, intensityVal, yellowBoost, greenCorr);
            const uint8_t r = (rgb >> 16) & 0xFF;
            const uint8_t g = (rgb >> 8) & 0xFF;
            const uint8_t b = rgb & 0xFF;

            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++)
                segment->setPixel(i + d, r, g, b);
        }

        // Advance position (phase) with optional reverse direction
        if (reverseDir)
            _position = (_position == 0) ? (uint8_t)(spacing - 1) : (uint8_t)(_position - 1);
        else
        {
            _position++;
            if (_position >= spacing) _position = 0;
        }

        _hue += colorSpeed;
    }

    void reset() override
    {
        _position = 0;
        _hue = 0;
        _lastUpdate = 0;
    }

    const char* getName() override
    {
        return "Theater Chase Rainbow";
    }

    const char* getDescription() override
    {
        return "Theater chase with rainbow colors";
    }
};