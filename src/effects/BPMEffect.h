/**
 * @file BPMEffect.h
 * @brief BPM (Beats Per Minute) effect (FastLED port) - STATELESS
 *
 * Colored stripes pulsing at a BPM rate. Port of FastLED's BPM pattern.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameter convention in this project:
 *   - ETS "Effekt Speed"      -> config.speed      (used here as BPM)
 *   - ETS "Effekt Option 1"   -> config.option1    (used here as base Hue)
 *   - ETS "Effekt Intensity"  -> config.intensity  (used here as brightness scaling)
 *
 * Effect parameters (API) exposed for compatibility:
 *   [0] BPM (0-255)
 *   [1] Hue (0-255)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class BPMEffect : public Effect
{
  public:
    BPMEffect() = default;

    const char* getName(const char* lang = nullptr) override { return "BPM"; }
    const char* getDescription(const char* lang = nullptr) override { return EFFECT_DESC_DE_EN(
                "Pulsierende Farbwellen im eingestellten Takt (BPM)",
                "Beats per minute - pulsing colored waves"); }

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

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Schläge pro Minute (0-255)", "Speed: Beats per minute (0-255)");
            case 1: return PARAM_DESC_DE_EN("Farbton: Basis-Farbton der Welle (0-255)", "Hue: Base hue of the wave (0-255)");
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
            case 0: return 62; // BPM default
            case 1: return 0;  // Hue default
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;   // BPM
            case 1: return cfg.option1; // Base Hue
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        auto& state = segment->getState();

        switch (index)
        {
            case 0:
                config.speed = static_cast<uint8_t>(value); // BPM (beats per minute)
                break;

            case 1:
                config.option1 = static_cast<uint8_t>(value); // Base Hue
                state.position = 0;                           // restart hue drift so parameter stays the base
                break;

            default:
                break;
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

        // BPM from config.speed (0 is valid in general, but beatsin8 with 0 is not useful)
        uint16_t bpm = config.speed;
        if (bpm == 0) bpm = static_cast<uint16_t>(getParameterDefault(0)); // fallback 62

        // Base hue from config.option1, animated hue drift stored in state.position
        const uint8_t baseHue = config.option1;
        const uint8_t gHue = static_cast<uint8_t>(baseHue + (uint8_t)state.position);

        // Brightness scaling from config.intensity (0 means "off" / black)
        const uint8_t masterBrightness = config.intensity;
        const bool yellowBoost = config.feature2;
        const bool greenCorr = config.feature3;

        const uint8_t beat = FastLEDMath::beatsin8(bpm, 64, 255);

        for (uint16_t i = 0; i < length; i++)
        {
            const uint8_t hue = static_cast<uint8_t>(gHue + (i * 2));

            uint8_t brightness = static_cast<uint8_t>(beat - gHue + (i * 10));
            brightness = FastLEDMath::scale8(brightness, masterBrightness);

            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness, yellowBoost, greenCorr);

            segment->setPixel(i,
                              (rgb >> 16) & 0xFF,
                              (rgb >> 8) & 0xFF,
                              rgb & 0xFF);
        }

        // keep hue drifting (so baseHue stays fixed in config.option1)
        state.position = (state.position + 1) & 0xFF;
    }

    void reset() override {}
};
