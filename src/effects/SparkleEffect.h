/**
 * @file SparkleEffect.h
 * @brief Sparkle effect - Fast random sparkles with party vibe
 *
 * Similar to twinkle but with more random, party-like sparkles.
 * Based on FastLED examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Sparkle Effect
 *
 * Similar to twinkle but with more random, party-like sparkles.
 */
/**
 * Config usage:
 *   - config.speed     : base speed of sparkles (higher=more frequent)
 *   - config.option1   : fade rate (0-220, default 200) - how fast sparkles fade
 *   - config.option2   : sparkle count (0-255, maps to 1-8) - sparkles per frame
 *   - config.option3   : probability (50-200, default 100) - sparkle chance
 *   - config.feature1  : white only mode (0=color/rainbow, 1=white sparkles only)
 *   - config.feature2  : burst mode (0=continuous, 1=random bursts)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : sparkle color (if all 0, uses random rainbow)
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

        // Use option1 for fade rate (180-220, default 200)
        uint8_t fadeRate = config.option1 > 0 ? (180 + config.option1 / 6) : 200;
        fadeRate = fadeRate > 220 ? 220 : fadeRate;

        // Use option2 for sparkle count (1-8, default based on speed)
        uint8_t sparkleCount = config.option2 > 0 ? (1 + config.option2 / 32) : (1 + config.speed / 64);
        sparkleCount = sparkleCount > 8 ? 8 : sparkleCount;

        // Use option3 for probability (50-200, default 100)
        uint8_t probability = config.option3 > 0 ? config.option3 : 100;
        probability = probability > 200 ? 200 : probability;

        // Use feature1 for color mode (0=set color/rainbow, 1=white only)
        bool whiteOnly = config.feature1;

        // Use feature2 for burst mode (0=continuous, 1=burst)
        bool burstMode = config.feature2;

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
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = g = b = brightness;
                }
                else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                {
                    // No color configured, use random rainbow colors
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        FastLEDMath::random8(), 255, config.intensity);
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else
                {
                    // Use configured color with random brightness
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, config.intensity);

                    r = FastLEDMath::scale8(config.r(), brightness);
                    g = FastLEDMath::scale8(config.g(), brightness);
                    b = FastLEDMath::scale8(config.b(), brightness);
                }

                segment->setPixel(pos, r, g, b);
            }
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Sparkle";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Fast random sparkles with party vibe";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 6; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "FadeRate";
            case 2: return "SparkleCount";
            case 3: return "Probability";
            case 4: return "WhiteOnly";
            case 5: return "BurstMode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Basis-Geschwindigkeit der Sparkles", "Speed: Base speed of sparkles");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate: Wie schnell Sparkles verblassen (0-220)", "Fade rate: How fast sparkles fade (0-220)");
            case 2: return PARAM_DESC_DE_EN("Anzahl: Wie viele Sparkles pro Frame (1-8)", "Count: How many sparkles per frame (1-8)");
            case 3: return PARAM_DESC_DE_EN("Wahrscheinlichkeit: Sparkle-Chance (50-200)", "Probability: Sparkle chance (50-200)");
            case 4: return PARAM_DESC_DE_EN("Nur Weiß: Nur weiße Sparkles", "White only: Only white sparkles");
            case 5: return PARAM_DESC_DE_EN("Burst-Modus: Zufällige Explosionen", "Burst mode: Random bursts");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_UINT8;
            case 3: return ParameterType::PARAM_UINT8;
            case 4: return ParameterType::PARAM_BOOL;
            case 5: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 120; // FadeRate (maps to ~200)
            case 2: return 64;  // SparkleCount (maps to ~3)
            case 3: return 100; // Probability
            case 4: return 0;   // WhiteOnly off
            case 5: return 0;   // BurstMode off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed min
            case 1: return 0;  // FadeRate min
            case 2: return 0;  // SparkleCount min (maps to 1)
            case 3: return 50; // Probability min
            case 4: return 0;  // WhiteOnly false
            case 5: return 0;  // BurstMode false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 220; // FadeRate max
            case 2: return 255; // SparkleCount max (maps to 8)
            case 3: return 200; // Probability max
            case 4: return 1;   // WhiteOnly true
            case 5: return 1;   // BurstMode true
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;    // Speed
            case 1: return config.option1;  // FadeRate
            case 2: return config.option2;  // SparkleCount
            case 3: return config.option3;  // Probability
            case 4: return config.feature1; // WhiteOnly
            case 5: return config.feature2; // BurstMode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (base speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // FadeRate (0-220)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // SparkleCount (density)
            case 3: config.option3 = static_cast<uint8_t>(value); break; // Probability (50-200)
            case 4: config.feature1 = static_cast<bool>(value); break;   // WhiteOnly
            case 5: config.feature2 = static_cast<bool>(value); break;   // BurstMode
            default: break;
        }
    }
};
