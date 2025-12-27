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

/**
 * @brief Rainbow effect
 *
 * Uses config parameters:
 *  - config.speed     : animation speed (time-based)
 *  - config.intensity : brightness (HSV V)
 *  - config.option1   : hue spacing / wavelength (0 => auto)
 *  - config.option2   : saturation (0 => 255)
 *  - config.option3   : phase / start hue offset
 *  - config.reverse   : reverse direction
 *  - config.feature1  : mirror
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
 */
class RainbowEffect : public Effect
{
  public:
    RainbowEffect() = default;

    const char* getName(const char* lang = nullptr) override { return "Rainbow"; }
    const char* getDescription(const char* lang = nullptr) override { return "Full spectrum rainbow cycle"; }

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
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;   // Speed
            case 1: return cfg.option1; // Delta
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (animation speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Delta (color spread)
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
        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // UI mapping
        const uint8_t speed = config.speed;   // Effect Speed
        const uint8_t v = config.intensity;   // Effect Intensity -> HSV V
        uint8_t delta = config.option1;       // Option1: hue spacing (0=auto)
        uint8_t s = config.option2;           // Option2: saturation (0=255)
        const uint8_t phase = config.option3; // Option3: start hue offset

        const bool reverse = (config.reverse != 0);
        const bool mirror = config.feature1;      // Feature1: Mirror
        const bool yellowBoost = config.feature2; // Feature2: Yellow brightness comp
        const bool greenCorr = config.feature3;   // Feature3: Green correction

        if (s == 0) s = 255;

        // auto delta: one full rainbow across segment
        if (delta == 0)
        {
            uint16_t d = 256 / length;
            delta = (d == 0) ? 1 : (uint8_t)d;
        }

        // time-based speed (frame-rate independent)
        const uint16_t intervalMs = (uint16_t)(1 + (255 - speed));
        state.counter += (uint16_t)deltaTime;
        while (state.counter >= intervalMs)
        {
            state.counter -= intervalMs;
            state.position = (state.position + 1) & 0xFF;
        }

        const uint8_t baseHue = (uint8_t)(state.position & 0xFF) + phase;

        for (uint16_t i = 0; i < length; i++)
        {
            uint16_t j = i;

            if (mirror)
            {
                uint16_t last = length - 1;
                if (j > last - j) j = last - j;
            }

            const uint8_t step = (uint8_t)((uint16_t)j * (uint16_t)delta);
            const uint8_t hue = reverse ? (uint8_t)(baseHue - step)
                                        : (uint8_t)(baseHue + step);

            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, s, v, yellowBoost, greenCorr);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }
    }

    void reset() override {}
};
