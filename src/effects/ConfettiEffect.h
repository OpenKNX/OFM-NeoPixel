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
        auto& state = segment->getState();
        switch (index)
        {
            case 0: return state.aux1; // FadeSpeed
            case 1: return state.aux2; // Saturation
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: state.aux1 = value; break;
            case 1: state.aux2 = value; break;
        }
    }

    // ====================================================================
    // Update
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Get parameters
        uint8_t fadeSpeed = state.aux1 > 0 ? state.aux1 : 10;
        uint8_t saturation = state.aux2 > 0 ? state.aux2 : 200;
        uint8_t brightness = config.intensity;

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
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(gHue + FastLEDMath::random8(64), saturation, brightness);

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
