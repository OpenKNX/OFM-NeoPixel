/**
 * @file ConfettiEffect.h
 * @brief Confetti effect - STATELESS
 *
 * Random colored pixels fade in and out. Port of FastLED's Confetti pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameters:
 *   [0] FadeSpeed (1-50) - How fast pixels fade
 *   [1] Saturation (0-255) - Color saturation
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

// ====================================================================
// Config usage summary (ETS mapping)
//   config.speed     -> FadeSpeed (amount per update, clamped to 1..50; 0 => default)
//   config.option1   -> Saturation (0..255)
//   config.intensity -> Brightness/Value (0..255)
//   config.feature2  -> Yellow brightness compensation (hsv2rgb_rainbow)
//   config.feature3  -> Green correction hooks (hsv2rgb_rainbow)
// Notes:
//   - Hue is animated internally via state.position (low 8 bits).
// ====================================================================

class ConfettiEffect : public Effect
{
  public:
    ConfettiEffect() = default;

    const char* getName() override { return "Confetti"; }
    const char* getDescription() override { return "Random colored pixels fading over time"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "FadeSpeed";
            case 1: return "Saturation";
            default: return nullptr;
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
            case 0: return 10;  // FadeSpeed
            case 1: return 200; // Saturation
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;   // FadeSpeed
            case 1: return cfg.option1; // Saturation
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = static_cast<uint8_t>(value); break;   // FadeSpeed
            case 1: cfg.option1 = static_cast<uint8_t>(value); break; // Saturation
            default: break;
        }
    }

    // ====================================================================
    // Update
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        const auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Snapshot config parameters (consistent for this update call)
        const uint8_t speed = config.speed;         // FadeSpeed
        const uint8_t intensity = config.intensity; // Brightness/Value
        const uint8_t saturation = config.option1;  // Saturation

        const bool yellowBoost = config.feature2; // Yellow boost
        const bool greenCorr = config.feature3;   // Green correction hooks

        // FadeSpeed: clamp to 1..50; treat 0 as default
        uint8_t fadeSpeed = speed;
        if (fadeSpeed == 0) fadeSpeed = 10;
        if (fadeSpeed > 50) fadeSpeed = 50;

        const uint8_t brightness = intensity;
        // Hue stored in position (lower 8 bits)
        uint8_t gHue = state.position & 0xFF;

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

        // Add new random confetti
        int pos = FastLEDMath::random8(length);
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(gHue + FastLEDMath::random8(64), saturation, brightness, yellowBoost, greenCorr);

        uint8_t r, g, b;
        if (segment->getPixel(pos, r, g, b))
        {
            r = FastLEDMath::qadd8(r, (rgb >> 16) & 0xFF);
            g = FastLEDMath::qadd8(g, (rgb >> 8) & 0xFF);
            b = FastLEDMath::qadd8(b, rgb & 0xFF);
            segment->setPixel(pos, r, g, b);
        }

        // Cycle hue (every ~20ms)
        if (deltaTime > 0) gHue++;
        state.position = (state.position & 0xFF00) | gHue;
    }

    void reset() override {}
};
