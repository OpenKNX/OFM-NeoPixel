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
 * Creates random sparkles that fade in and out, like twinkling stars.
 */
/**
 * Config usage:
 *   - config.speed     : frequency of new twinkles (higher=more twinkles)
 *   - config.option1   : fade rate (0-240, default 220) - how fast twinkles fade
 *   - config.option2   : density (10-200, default 100) - how many twinkles at once
 *   - config.feature1  : rainbow mode (0=white/set color, 1=rainbow colors)
 *   - config.feature2  : variable brightness (0=fixed, 1=random brightness levels)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : twinkle color (if all 0, uses white; if rainbow mode, ignored)
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

        // Use option1 for fade rate (200-240, default 220)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 6) : 220;
        fadeRate = fadeRate > 240 ? 240 : fadeRate;

        // Use option2 for density/probability (10-200, default 100)
        uint8_t density = config.option2 > 0 ? config.option2 : 100;
        density = density > 200 ? 200 : density;

        // Use feature1 for color mode (0=set color/white, 1=rainbow)
        bool rainbowMode = config.feature1;

        // Use feature2 for variable brightness (0=fixed, 1=variable)
        bool variableBrightness = config.feature2;

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
        uint8_t chanceOfTwinkle = ((config.speed * density) / 255) / 2;

        // Add random twinkles
        if (FastLEDMath::random8() < chanceOfTwinkle)
        {
            int pos = FastLEDMath::random16(length);

            uint8_t r, g, b;
            if (rainbowMode)
            {
                // Rainbow mode
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                    FastLEDMath::random8(), 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
            {
                // No color configured, use white
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);
                r = g = b = brightness;
            }
            else
            {
                // Use configured color
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);

                r = FastLEDMath::scale8(config.r(), brightness);
                g = FastLEDMath::scale8(config.g(), brightness);
                b = FastLEDMath::scale8(config.b(), brightness);
            }

            segment->setPixel(pos, r, g, b);
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Twinkle";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Random LEDs twinkling on and off";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "FadeRate";
            case 2: return "Density";
            case 3: return "RainbowMode";
            case 4: return "VariableBrightness";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Häufigkeit neuer Twinkles (höher=mehr)", "Speed: Frequency of new twinkles (higher=more)");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate: Wie schnell Twinkles verblassen (0-240)", "Fade rate: How fast twinkles fade (0-240)");
            case 2: return PARAM_DESC_DE_EN("Dichte: Wie viele Twinkles gleichzeitig (10-200)", "Density: How many twinkles at once (10-200)");
            case 3: return PARAM_DESC_DE_EN("Regenbogenmodus: Bunte statt weißefarben", "Rainbow mode: Colorful instead of white");
            case 4: return PARAM_DESC_DE_EN("Variable Helligkeit: Zufällige Helligkeitsstufen", "Variable brightness: Random brightness levels");
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
            case 3: return ParameterType::PARAM_BOOL;
            case 4: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 120; // FadeRate (maps to ~220)
            case 2: return 100; // Density
            case 3: return 0;   // RainbowMode off
            case 4: return 0;   // VariableBrightness off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed min
            case 1: return 0;  // FadeRate min
            case 2: return 10; // Density min
            case 3: return 0;  // RainbowMode false
            case 4: return 0;  // VariableBrightness false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 240; // FadeRate max
            case 2: return 200; // Density max
            case 3: return 1;   // RainbowMode true
            case 4: return 1;   // VariableBrightness true
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
            case 2: return config.option2;  // Density
            case 3: return config.feature1; // RainbowMode
            case 4: return config.feature2; // VariableBrightness
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (spawn rate)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // FadeRate (0-240)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Density (10-200)
            case 3: config.feature1 = static_cast<bool>(value); break;   // RainbowMode
            case 4: config.feature2 = static_cast<bool>(value); break;   // VariableBrightness
            default: break;
        }
    }
};