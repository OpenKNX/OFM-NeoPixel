/**
 * @file RainbowEffect.h
 * @brief Classic Rainbow effect - STATELESS (Rainbow / Rainbow Cycle)
 *
 * Smooth rainbow gradient that cycles across the strip. Based on FastLED's
 * classic rainbow pattern.
 *
 * Consolidated effect: the former "Rainbow Cycle" effect is now the Mode=1
 * variant. Mode 0 spaces hues by Delta (hue step per LED); Mode 1 distributes
 * Density full rainbow cycles evenly across the strip.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * @brief Rainbow effect (Rainbow / Rainbow Cycle)
 *
 * Uses config parameters:
 *  - config.speed     : animation speed (time-based)
 *  - config.intensity : brightness (HSV V)
 *  - config.option1   : hue spacing / Delta (0 => auto)
 *  - config.option2   : saturation (0 => 255)
 *  - config.option3   : phase / start hue offset
 *  - config.count     : density (number of rainbow cycles) for Mode 1
 *  - config.reverse   : reverse direction
 *  - config.mode      : 0=Rainbow (Delta), 1=Cycle (Density)
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
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Delta";
            case 2: return "Saturation";
            case 3: return "Density";
            case 4: return "Mode";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Rotationsgeschwindigkeit (0-255)", "Speed: Rotation speed (0-255)");
            case 1: return PARAM_DESC_DE_EN("Delta: Farbabstand zwischen LEDs (Modus 0, 1-255)", "Delta: Hue spacing between LEDs (Mode 0, 1-255)");
            case 2: return PARAM_DESC_DE_EN("Sättigung: Farbintensität (0=weiß, 255=volle Farbe)", "Saturation: Color intensity (0=white, 255=full color)");
            case 3: return PARAM_DESC_DE_EN("Dichte: Anzahl Regenbogenzyklen (Modus 1, 1-10)", "Density: Number of rainbow cycles (Mode 1, 1-10)");
            case 4: return PARAM_DESC_DE_EN("Modus: 0=Rainbow (Delta), 1=Cycle (Dichte)", "Mode: 0=Rainbow (Delta), 1=Cycle (Density)");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        if (index == 4) return ParameterType::PARAM_ENUM; // Mode
        return ParameterType::PARAM_UINT8;
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 4) return nullptr;
        switch (enumValue)
        {
            case 0: return "Rainbow (Delta)";
            case 1: return "Cycle (Dichte)";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 4) ? 2 : 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed
            case 1: return 7; // Delta (hue spacing)
            case 2: return 255; // Saturation
            case 3: return 1; // Density
            case 4: return 0; // Mode (Rainbow)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0; // Speed
            case 1: return 0; // Delta (0=auto)
            case 2: return 0; // Saturation
            case 3: return 1; // Density min
            case 4: return 0; // Mode min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Delta
            case 2: return 255; // Saturation
            case 3: return 10;  // Density max
            case 4: return 1;   // Mode max (0=Rainbow,1=Cycle)
            default: return 255;
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
            case 2: return cfg.option2; // Saturation
            case 3: return cfg.count;   // Density
            case 4: return cfg.mode;    // Mode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Delta
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Saturation
            case 3: config.count = static_cast<uint8_t>(value); break;   // Density
            case 4: config.mode = static_cast<uint8_t>(value); break;    // Mode
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

        const bool cycleMode = (config.mode == 1);
        const bool reverse = (config.reverse != 0);
        const bool mirror = config.feature1;      // Feature1: Mirror
        const bool yellowBoost = config.feature2; // Feature2: Yellow brightness comp
        const bool greenCorr = config.feature3;   // Feature3: Green correction

        if (s == 0) s = 255;

        if (cycleMode)
        {
            // Density: number of full rainbow cycles across the strip
            uint8_t density = config.count > 0 ? config.count : 1;
            if (density > 10) density = 10;
            // delta = (256 * density) / length, ensuring 'density' cycles fit
            uint16_t d = (uint16_t)((256u * density) / length);
            delta = (d == 0) ? 1 : (uint8_t)d;
        }
        else if (delta == 0)
        {
            // auto delta: one full rainbow across segment
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
