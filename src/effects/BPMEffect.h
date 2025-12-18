/**
 * @file BPMEffect.h
 * @brief BPM (Beats Per Minute) effect (FastLED port) - STATELESS
 *
 * Colored stripes pulsing at a BPM rate. Port of FastLED's BPM pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameters:
 *   [0] BPM (0-255) - Beats per minute
 *   [1] Hue (0-255) - Starting hue offset
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class BPMEffect : public Effect
{
  public:
    BPMEffect() = default;

    const char* getName() override { return "BPM"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "BPM";
            case 1: return "Hue";
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
            case 0: return 62; // BPM default
            case 1: return 0;  // Hue default
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: return state.aux1; // BPM
            case 1: return state.aux2; // Hue
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: state.aux1 = value; break; // BPM
            case 1: state.aux2 = value; break; // Hue
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

        // Get parameters from state
        uint16_t bpm = state.aux1 > 0 ? state.aux1 : config.speed; // Fallback to config.speed
        uint8_t gHue = state.aux2;

        uint16_t length = segment->getLength();
        uint8_t masterBrightness = config.intensity;

        uint8_t beat = FastLEDMath::beatsin8(bpm, 64, 255);

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t hue = gHue + (i * 2);
            uint8_t brightness = beat - gHue + (i * 10);
            brightness = FastLEDMath::scale8(brightness, masterBrightness);

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        state.aux2++; // Increment Hue
    }

    void reset() override {}
};
