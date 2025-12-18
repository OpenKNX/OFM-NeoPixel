/**
 * @file JuggleEffect.h
 * @brief Juggle effect - STATELESS
 *
 * Colored dots weaving in and out of sync with each other. Port of FastLED's Juggle pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameters:
 *   [0] NumDots (1-16) - Number of dots
 *   [1] FadeSpeed (1-50) - Fade speed
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

    const char* getName() override { return "Juggle"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "NumDots";
            case 1: return "FadeSpeed";
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
            case 0: return 8;  // NumDots
            case 1: return 20; // FadeSpeed
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: return state.aux1; // NumDots
            case 1: return state.aux2; // FadeSpeed
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

        uint8_t numDots = state.aux1 > 0 ? state.aux1 : 8;
        uint8_t fadeSpeed = state.aux2 > 0 ? state.aux2 : 20;
        uint8_t brightness = config.intensity;

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

        uint8_t dothue = 0;
        for (int i = 0; i < numDots; i++)
        {
            int pos = FastLEDMath::beatsin16(i + 7, 0, length - 1);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(dothue, 200, brightness);

            uint8_t r, g, b;
            if (segment->getPixel(pos, r, g, b))
            {
                r |= (rgb >> 16) & 0xFF;
                g |= (rgb >> 8) & 0xFF;
                b |= rgb & 0xFF;
                segment->setPixel(pos, r, g, b);
            }

            dothue += 32;
        }
    }

    void reset() override {}
}; // End of JuggleEffect class
