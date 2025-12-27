/**
 * @file JuggleEffect.h
 * @brief Juggle effect - STATELESS
 *
 * Multiple colored dots weaving in and out of sync with each other.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Config usage summary:
 *   - config.speed     : movement speed (mapped to BPM base for beatsin)
 *   - config.intensity : master brightness (HSV V)
 *   - config.option1   : number of dots (0=default, 1..16)
 *   - config.option2   : fade speed (0=default, 1..50) higher = faster fade
 *   - config.option3   : hue offset / start hue
 *   - config.feature2  : yellow brightness compensation (hsv2rgb_rainbow)
 *   - config.feature3  : green correction hooks (hsv2rgb_rainbow)
 *
 * ETS mapping (standard):
 *   Speed   -> config.speed
 *   Option1 -> config.option1
 *   Option2 -> config.option2
 *   Option3 -> config.option3
 *   Intensity -> config.intensity
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class JuggleEffect : public Effect
{
  public:
    JuggleEffect() = default;

    const char* getName(const char* lang = nullptr) override { return "Juggle"; }
    const char* getDescription(const char* lang = nullptr) override { return "Multiple colored dots bouncing and fading"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "NumDots";
            case 2: return "FadeSpeed";
            case 3: return "HueOffset";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Bewegungsgeschwindigkeit (BPM Basis)", "Speed: Movement speed (BPM base)");
            case 1: return PARAM_DESC_DE_EN("Anzahl Punkte: Anzahl der Jonglier-Punkte (1-16)", "Num dots: Number of juggling dots (1-16)");
            case 2: return PARAM_DESC_DE_EN("Ausblendrate: Wie schnell Punkte verblassen (1-50)", "Fade speed: How fast dots fade (1-50)");
            case 3: return PARAM_DESC_DE_EN("Farbton-Versatz: Farbverschiebung (0-255)", "Hue offset: Color shift (0-255)");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        return ParameterType::PARAM_UINT8;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 80; // Speed (mapped to BPM base)
            case 1: return 8;  // NumDots
            case 2: return 20; // FadeSpeed
            case 3: return 0;  // HueOffset
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;
            case 1: return cfg.option1;
            case 2: return cfg.option2;
            case 3: return cfg.option3;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();

        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (juggle rate)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // NumDots (number of dots)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // FadeSpeed
            case 3: config.option3 = static_cast<uint8_t>(value); break; // HueOffset (color shift)
            default: break;
        }
    }

    // ====================================================================
    // Update
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        const uint8_t speed = config.speed;          //  Speed
        const uint8_t brightness = config.intensity; // Master brightness (HSV V)
        const uint8_t optNumDots = config.option1;   // Number of dots
        const uint8_t optFade = config.option2;      // Fade speed
        const uint8_t hueOffset = config.option3;    // Hue offset / start hue

        const bool yellowBoost = config.feature2; // Yellow brightness compensation
        const bool greenCorr = config.feature3;   // Green correction hooks

        // NumDots: 0 => default
        uint8_t numDots = (optNumDots == 0) ? 8 : optNumDots;
        if (numDots < 1) numDots = 1;
        if (numDots > 16) numDots = 16;

        // FadeSpeed: 0 => default
        uint8_t fadeSpeed = (optFade == 0) ? 20 : optFade;
        if (fadeSpeed < 1) fadeSpeed = 1;
        if (fadeSpeed > 50) fadeSpeed = 50;

        // Speed mapped to BPM base (1..180); 0 => default
        uint8_t bpmBase;
        if (speed == 0) bpmBase = 60;
        else
            bpmBase = (uint8_t)(1 + ((uint16_t)speed * 179u) / 255u);

        // Fade all LEDs
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

        // Draw dots
        uint8_t dothue = hueOffset;
        for (uint8_t i = 0; i < numDots; i++)
        {
            // each dot has a slightly different BPM so they weave
            const uint8_t bpm = (uint8_t)(bpmBase + i * 7u);
            const int pos = FastLEDMath::beatsin16(bpm, 0, (int)(length - 1));

            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(dothue, 200, brightness, yellowBoost, greenCorr);

            uint8_t r, g, b;
            if (segment->getPixel((uint16_t)pos, r, g, b))
            {
                r |= (rgb >> 16) & 0xFF;
                g |= (rgb >> 8) & 0xFF;
                b |= rgb & 0xFF;
                segment->setPixel((uint16_t)pos, r, g, b);
            }

            dothue += 32;
        }
    }

    void reset() override {}
}; // End of JuggleEffect class
