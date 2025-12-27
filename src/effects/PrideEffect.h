/**
 * @file PrideEffect.h
 * @brief Pride2015 effect - STATELESS
 *
 * Beautiful moving rainbow with brightness waves. Port of FastLED's famous Pride2015 pattern by Mark Kriegsman.
 *
 * Note: This effect has minimal configurable parameters as it's a carefully tuned pattern.
 *
 * @copyright Original Pride2015 by Mark Kriegsman (FastLED)
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class PrideEffect : public Effect
{
  public:
    PrideEffect() = default;

    const char* getName(const char* lang = nullptr) override { return "Pride2015"; }
    const char* getDescription(const char* lang = nullptr) override { return "Rainbow colors with dynamic brightness waves"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 1; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Animationsgeschwindigkeit (höher=schneller)",
                "Animation speed (higher=faster)");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8; // Speed
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed (normal)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min (must be > 0)
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed; // Speed
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break; // Speed (animation speed)
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
        uint16_t length = segment->getLength();
        uint8_t masterBrightness = config.intensity;

        // Get speed parameter (1-255, default 128)
        uint8_t speed = config.speed > 0 ? config.speed : 128;
        uint32_t scaledDeltaTime = (deltaTime * speed) / 128; // Scale deltaTime by speed

        // State stored in EffectState
        uint16_t pseudotime = state.position;
        uint16_t hue16 = state.counter;

        uint8_t sat8 = FastLEDMath::beatsin88(87, 220, 250);
        uint8_t brightdepth = FastLEDMath::beatsin88(341, 96, 224);
        uint16_t brightnessthetainc16 = FastLEDMath::beatsin88(203, (25 * 256), (40 * 256));
        uint8_t msmultiplier = FastLEDMath::beatsin88(147, 23, 60);

        uint16_t hueinc16 = FastLEDMath::beatsin88(113, 1, 3000);

        pseudotime += scaledDeltaTime * msmultiplier;
        hue16 += scaledDeltaTime * FastLEDMath::beatsin88(400, 5, 9);
        uint16_t brightnesstheta16 = pseudotime;

        for (uint16_t i = 0; i < length; i++)
        {
            hue16 += hueinc16;
            uint8_t hue8 = hue16 / 256;

            brightnesstheta16 += brightnessthetainc16;
            uint16_t b16 = FastLEDMath::sin16(brightnesstheta16) + 32768;

            uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
            uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
            bri8 += (255 - brightdepth);

            bri8 = FastLEDMath::scale8(bri8, masterBrightness);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue8, sat8, bri8);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        // Store state
        state.position = pseudotime;
        state.counter = hue16;
    }

    void reset() override {}
};
