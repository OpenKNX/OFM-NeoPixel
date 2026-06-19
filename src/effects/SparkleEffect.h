/**
 * @file SparkleEffect.h
 * @brief Sparkle effect - random sparkles with selectable rendering mode
 *
 * Consolidated effect. Mode selects the rendering style:
 *   Mode 0 (Sparkle):  fast random sparkles with party vibe
 *   Mode 1 (Twinkle):  random sparkles fading in/out like twinkling stars
 *   Mode 2 (Confetti): random colored pixels fading over time (FastLED Confetti)
 *
 * Based on FastLED examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Sparkle Effect (Sparkle / Twinkle / Confetti)
 *
 * Config usage:
 *   - config.speed     : base speed / spawn rate
 *   - config.option1   : Sparkle/Twinkle fade rate, Confetti saturation
 *   - config.option2   : Sparkle sparkle count, Twinkle density
 *   - config.option3   : Sparkle probability
 *   - config.feature1  : Sparkle whiteOnly, Twinkle rainbow mode
 *   - config.feature2  : Sparkle burst, Twinkle variable brightness
 *   - config.mode      : 0=Sparkle, 1=Twinkle, 2=Confetti
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : color (if all 0, uses random rainbow / white)
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
        if (segment->getLength() == 0) return;

        switch (segment->getConfig().mode)
        {
            case 1: updateTwinkle(segment, deltaTime); break;
            case 2: updateConfetti(segment, deltaTime); break;
            default: updateSparkle(segment, deltaTime); break;
        }
    }

    // ── Mode 0: Sparkle ──────────────────────────────────────────────────
    void updateSparkle(Segment* segment, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // option1: fade rate (180-220, default 200)
        uint8_t fadeRate = config.option1 > 0 ? (180 + config.option1 / 6) : 200;
        fadeRate = fadeRate > 220 ? 220 : fadeRate;

        // option2: sparkle count (1-8)
        uint8_t sparkleCount = config.option2 > 0 ? (1 + config.option2 / 32) : (1 + config.speed / 64);
        sparkleCount = sparkleCount > 8 ? 8 : sparkleCount;

        // option3: probability (50-200, default 100)
        uint8_t probability = config.option3 > 0 ? config.option3 : 100;
        probability = probability > 200 ? 200 : probability;

        bool whiteOnly = config.feature1;
        bool burstMode = config.feature2;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            segment->setPixel(i, r, g, b);
        }

        uint8_t actualSparkles = burstMode ? (FastLEDMath::random8() < 50 ? sparkleCount * 2 : 0) : sparkleCount;

        for (uint8_t i = 0; i < actualSparkles; i++)
        {
            if (FastLEDMath::random8() < probability)
            {
                int pos = FastLEDMath::random16(length);
                uint8_t r, g, b;
                if (whiteOnly)
                {
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = g = b = brightness;
                }
                else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                {
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(FastLEDMath::random8(), 255, config.intensity);
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                }
                else
                {
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

    // ── Mode 1: Twinkle ──────────────────────────────────────────────────
    void updateTwinkle(Segment* segment, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // option1: fade rate (200-240, default 220)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 6) : 220;
        fadeRate = fadeRate > 240 ? 240 : fadeRate;

        // option2: density (10-200, default 100)
        uint8_t density = config.option2 > 0 ? config.option2 : 100;
        density = density > 200 ? 200 : density;

        bool rainbowMode = config.feature1;
        bool variableBrightness = config.feature2;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            segment->setPixel(i, r, g, b);
        }

        uint8_t chanceOfTwinkle = ((config.speed * density) / 255) / 2;
        if (FastLEDMath::random8() < chanceOfTwinkle)
        {
            int pos = FastLEDMath::random16(length);
            uint8_t r, g, b;
            if (rainbowMode)
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(FastLEDMath::random8(), 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
            {
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);
                r = g = b = brightness;
            }
            else
            {
                uint8_t brightness = variableBrightness ? FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);
                r = FastLEDMath::scale8(config.r(), brightness);
                g = FastLEDMath::scale8(config.g(), brightness);
                b = FastLEDMath::scale8(config.b(), brightness);
            }
            segment->setPixel(pos, r, g, b);
        }
    }

    // ── Mode 2: Confetti ─────────────────────────────────────────────────
    void updateConfetti(Segment* segment, uint32_t deltaTime)
    {
        auto& state = segment->getState();
        const auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        const uint8_t speed = config.speed;         // FadeSpeed
        const uint8_t intensity = config.intensity; // Brightness/Value
        const uint8_t saturation = config.option1;  // Saturation
        const bool yellowBoost = config.feature2;
        const bool greenCorr = config.feature3;

        uint8_t fadeSpeed = speed;
        if (fadeSpeed == 0) fadeSpeed = 10;
        if (fadeSpeed > 50) fadeSpeed = 50;

        const uint8_t sat = saturation == 0 ? 200 : saturation;
        uint8_t gHue = state.position & 0xFF;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            if (segment->getPixel(i, r, g, b))
            {
                r = FastLEDMath::fadeToBlackBy(r, fadeSpeed);
                g = FastLEDMath::fadeToBlackBy(g, fadeSpeed);
                b = FastLEDMath::fadeToBlackBy(b, fadeSpeed);
                segment->setPixel(i, r, g, b);
            }
        }

        uint16_t pos = FastLEDMath::random16(length);
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(gHue + FastLEDMath::random8(64), sat, intensity, yellowBoost, greenCorr);
        uint8_t r, g, b;
        if (segment->getPixel(pos, r, g, b))
        {
            r = FastLEDMath::qadd8(r, (rgb >> 16) & 0xFF);
            g = FastLEDMath::qadd8(g, (rgb >> 8) & 0xFF);
            b = FastLEDMath::qadd8(b, rgb & 0xFF);
            segment->setPixel(pos, r, g, b);
        }

        if (deltaTime > 0) gHue++;
        state.position = (state.position & 0xFF00) | gHue;
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
        return EFFECT_DESC_DE_EN(
            "Zufällige Funkeln: Sparkle, Twinkle oder Confetti je nach Modus",
            "Random sparkles: Sparkle, Twinkle or Confetti depending on mode");
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 7; }

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
            case 6: return "Mode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Basis-Geschwindigkeit / Spawnrate", "Speed: Base speed / spawn rate");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate / Confetti-Sättigung", "Fade rate / Confetti saturation");
            case 2: return PARAM_DESC_DE_EN("Anzahl (Sparkle) / Dichte (Twinkle)", "Count (Sparkle) / Density (Twinkle)");
            case 3: return PARAM_DESC_DE_EN("Wahrscheinlichkeit: Sparkle-Chance (50-200)", "Probability: Sparkle chance (50-200)");
            case 4: return PARAM_DESC_DE_EN("Nur Weiß (Sparkle) / Regenbogen (Twinkle)", "White only (Sparkle) / Rainbow (Twinkle)");
            case 5: return PARAM_DESC_DE_EN("Burst (Sparkle) / Variable Helligkeit (Twinkle)", "Burst (Sparkle) / Variable brightness (Twinkle)");
            case 6: return PARAM_DESC_DE_EN("Modus: 0=Sparkle, 1=Twinkle, 2=Confetti", "Mode: 0=Sparkle, 1=Twinkle, 2=Confetti");
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
            case 6: return ParameterType::PARAM_ENUM; // Mode
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 6) return nullptr;
        switch (enumValue)
        {
            case 0: return "Sparkle";
            case 1: return "Twinkle";
            case 2: return "Konfetti";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 6) ? 3 : 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 120; // FadeRate
            case 2: return 64;  // SparkleCount
            case 3: return 100; // Probability
            case 4: return 0;   // WhiteOnly off
            case 5: return 0;   // BurstMode off
            case 6: return 0;   // Mode (Sparkle)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 0;
            case 2: return 0;
            case 3: return 50;
            case 4: return 0;
            case 5: return 0;
            case 6: return 0; // Mode min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 220;
            case 2: return 255;
            case 3: return 200;
            case 4: return 1;
            case 5: return 1;
            case 6: return 2; // Mode max (0..2)
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
            case 1: return config.option1;  // FadeRate / Saturation
            case 2: return config.option2;  // SparkleCount / Density
            case 3: return config.option3;  // Probability
            case 4: return config.feature1; // WhiteOnly / Rainbow
            case 5: return config.feature2; // BurstMode / VariableBrightness
            case 6: return config.mode;     // Mode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;
            case 1: config.option1 = static_cast<uint8_t>(value); break;
            case 2: config.option2 = static_cast<uint8_t>(value); break;
            case 3: config.option3 = static_cast<uint8_t>(value); break;
            case 4: config.feature1 = static_cast<bool>(value); break;
            case 5: config.feature2 = static_cast<bool>(value); break;
            case 6: config.mode = static_cast<uint8_t>(value); break;
            default: break;
        }
    }
};
