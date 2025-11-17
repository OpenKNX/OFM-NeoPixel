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

    const char* getName() override { return "Pride2015"; }

    // ====================================================================
    // Parameter API (Pride has no adjustable params - it's a fixed pattern)
    // ====================================================================
    uint8_t getParameterCount() const override { return 0; }

    const char* getParameterName(uint8_t index) const override { return nullptr; }

    ParameterType getParameterType(uint8_t index) const override { return ParameterType::PARAM_UINT8; }

    uint32_t getParameterDefault(uint8_t index) const override { return 0; }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override { return 0; }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override {}

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

        // State stored in EffectState
        uint16_t pseudotime = state.position;
        uint16_t hue16 = state.counter;

        uint8_t sat8 = FastLEDMath::beatsin88(87, 220, 250);
        uint8_t brightdepth = FastLEDMath::beatsin88(341, 96, 224);
        uint16_t brightnessthetainc16 = FastLEDMath::beatsin88(203, (25 * 256), (40 * 256));
        uint8_t msmultiplier = FastLEDMath::beatsin88(147, 23, 60);

        uint16_t hueinc16 = FastLEDMath::beatsin88(113, 1, 3000);

        pseudotime += deltaTime * msmultiplier;
        hue16 += deltaTime * FastLEDMath::beatsin88(400, 5, 9);
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

