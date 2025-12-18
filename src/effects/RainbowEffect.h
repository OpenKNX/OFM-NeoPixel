/**
 * @file RainbowEffect.h
 * @brief Classic Rainbow effect - STATELESS
 *
 * Smooth rainbow gradient that cycles across the strip. Based on FastLED's classic rainbow pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameters:
 *   [0] Speed (0-255) - Rotation speed
 *   [1] Delta (1-255) - Hue spacing between LEDs
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class RainbowEffect : public Effect
{
  public:
    RainbowEffect() = default;

    const char* getName() override { return "Rainbow"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Delta";
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
            case 0: return 1; // Speed
            case 1: return 7; // Delta (hue spacing)
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: return state.aux1; // Speed
            case 1: return state.aux2; // Delta
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: state.aux1 = value; break; // Speed
            case 1: state.aux2 = value; break; // Delta
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

        // Get parameters (with defaults)
        uint8_t speed = state.aux1 > 0 ? state.aux1 : 1;
        uint8_t delta = state.aux2 > 0 ? state.aux2 : 7;
        uint8_t brightness = config.intensity;

        // Hue offset stored in position
        uint8_t hueOffset = state.position & 0xFF;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t hue = hueOffset + (i * delta);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        // Rotate hue
        state.position = (state.position + speed) & 0xFF;
    }

    void reset() override {}
};
