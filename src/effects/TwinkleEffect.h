/**
 * @file TwinkleEffect.h
 * @brief Twinkle/Sparkle effects - Random sparkles and twinkling stars
 *
 * Creates random sparkles and twinkles like a starry night sky.
 * Based on FastLED's addGlitter function and twinkle examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Twinkle Effect
 *
 * Random LEDs twinkling on and off (starry night).
 *
 * Uses config parameters:
 *  - config.speed     : twinkle chance / activity level
 *  - intensityVal : master brightness
 *  - config.option1   : fade rate control (0 => default)
 *  - config.option2   : density / probability scaling (0 => default)
 *  - config.feature1  : color mode (0 = configured color/white, 1 = rainbow)
 *  - config.feature2  : variable brightness (0 = fixed, 1 = variable)
 *  - config.feature3  : enable HSV rainbow corrections (yellow/green hooks)
 */
class TwinkleEffect : public Effect
{
  private:
    uint8_t _hue;

  public:
    TwinkleEffect() : _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Snapshot config per frame (avoids mixed frames if KNX updates mid-loop)
        const uint8_t speedVal = config.speed;           // twinkle chance / activity level
        const uint8_t intensityVal = intensityVal;       // master brightness
        const uint8_t option1 = config.option1;          // fade rate control
        const uint8_t option2 = config.option2;          // density / probability scaling
        const bool rainbowMode = config.feature1;        // color mode
        const bool variableBrightness = config.feature2; // variable brightness
        const bool yellowBoost = config.feature3;        // enable HSV rainbow corrections
        const bool greenCorr = config.feature3;          // enable HSV rainbow corrections
        const uint8_t cfgR = config.r();
        const uint8_t cfgG = config.g();
        const uint8_t cfgB = config.b();

        // Use option1 for fade rate (200-240, default 220)
        uint8_t fadeRate = option1 > 0 ? (200 + option1 / 6) : 220;
        fadeRate = fadeRate > 240 ? 240 : fadeRate;

        // Use option2 for density/probability (10-200, default 100)
        uint8_t density = option2 > 0 ? option2 : 100;
        density = density > 200 ? 200 : density;

        // Fade all pixels gradually
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

        // Calculate sparkle probability based on speed and density
        uint8_t chanceOfTwinkle = ((speedVal * density) / 255) / 2;

        // Add random twinkles
        if (FastLEDMath::random8() < chanceOfTwinkle)
        {
            int pos = FastLEDMath::random16(length);

            uint8_t r, g, b;
            if (rainbowMode)
            {
                // Rainbow mode
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                    FastLEDMath::random8(), 255, intensityVal, yellowBoost, greenCorr);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else if (cfgR == 0 && cfgG == 0 && cfgB == 0)
            {
                // No color configured, use white
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, intensityVal);
                r = g = b = brightness;
            }
            else
            {
                // Use configured color
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, intensityVal);

                r = FastLEDMath::scale8(cfgR, brightness);
                g = FastLEDMath::scale8(cfgG, brightness);
                b = FastLEDMath::scale8(cfgB, brightness);
            }

            segment->setPixel(pos, r, g, b);
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName() override
    {
        return "Twinkle";
    }

    const char* getDescription() override
    {
        return "Random LEDs twinkling on and off";
    }
};

/**
 * Sparkle Effect
 *
 * Faster, party-like sparkles.
 *
 * Uses config parameters:
 *  - config.speed     : affects default sparkle count (when option2=0)
 *  - intensityVal : master brightness
 *  - config.option1   : fade rate control (0 => default)
 *  - config.option2   : sparkle count control (0 => derived from speed)
 *  - config.option3   : sparkle probability (0 => default)
 *  - config.feature1  : white-only mode (1 = white only, 0 = color)
 *  - config.feature2  : burst mode
 *  - config.feature3  : enable HSV rainbow corrections (yellow/green hooks)
 */
class SparkleEffect : public Effect
{
  private:
    uint8_t _hue;

  public:
    SparkleEffect() : _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Snapshot config per frame (avoids mixed frames if KNX updates mid-loop)
        const uint8_t speedVal = config.speed;         // affects default sparkle count
        const uint8_t intensityVal = config.intensity; // master brightness
        const uint8_t option1 = config.option1;        // fade rate control
        const uint8_t option2 = config.option2;        // sparkle count control
        const uint8_t option3 = config.option3;        // sparkle probability
        const bool whiteOnly = config.feature1;        // white-only mode
        const bool burstMode = config.feature2;        // burst mode
        const bool yellowBoost = config.feature3;      // enable HSV rainbow corrections
        const bool greenCorr = config.feature3;        // enable HSV rainbow corrections
        const uint8_t cfgR = config.r();
        const uint8_t cfgG = config.g();
        const uint8_t cfgB = config.b();

        // Use option1 for fade rate (180-220, default 200)
        uint8_t fadeRate = option1 > 0 ? (180 + option1 / 6) : 200;
        fadeRate = fadeRate > 220 ? 220 : fadeRate;

        // Use option2 for sparkle count (1-8, default based on speed)
        uint8_t sparkleCount = option2 > 0 ? (1 + option2 / 32) : (1 + speedVal / 64);
        sparkleCount = sparkleCount > 8 ? 8 : sparkleCount;

        // Use option3 for probability (50-200, default 100)
        uint8_t probability = option3 > 0 ? option3 : 100;
        probability = probability > 200 ? 200 : probability;

        // Fade all pixels more aggressively than twinkle
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

        // Add multiple sparkles
        uint8_t actualSparkles = burstMode ? (FastLEDMath::random8() < 50 ? sparkleCount * 2 : 0) : sparkleCount;

        for (uint8_t i = 0; i < actualSparkles; i++)
        {
            if (FastLEDMath::random8() < probability)
            {
                int pos = FastLEDMath::random16(length);

                uint8_t r, g, b;
                if (whiteOnly)
                {
                    // White sparkles only
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, intensityVal);
                    r = g = b = brightness;
                }
                else if (cfgR == 0 && cfgG == 0 && cfgB == 0)
                {
                    // No color configured, use random rainbow colors
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        FastLEDMath::random8(), 255, intensityVal, yellowBoost, greenCorr);
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else
                {
                    // Use configured color with random brightness
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, intensityVal);

                    r = FastLEDMath::scale8(cfgR, brightness);
                    g = FastLEDMath::scale8(cfgG, brightness);
                    b = FastLEDMath::scale8(cfgB, brightness);
                }

                segment->setPixel(pos, r, g, b);
            }
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName() override
    {
        return "Sparkle";
    }

    const char* getDescription() override
    {
        return "Fast random sparkles with party vibe";
    }
};