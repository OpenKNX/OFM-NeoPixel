#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "ParameterType.h"

/**
 * @brief Gradient effect (FastLED-style fill_gradient)
 *
 * Generates a linear HSV gradient across the segment.
 *
 * Uses config parameters:
 *  - config.speed     : animation speed (phase shift over time; 0 => very slow)
 *  - config.intensity : brightness (HSV V)
 *  - config.option1   : start hue (0-255)
 *  - config.option2   : end hue (0-255)
 *  - config.option3   : saturation (0 => 255)
 *  - config.reverse   : swaps gradient direction
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
 */
class GradientEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override { return "Gradient"; }
    const char* getDescription(const char* lang = nullptr) override { return EFFECT_DESC_DE_EN(
                "Linearer HSV-Farbverlauf über den Streifen",
                "Linear HSV gradient across the strip"); }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return "Speed";
            case 1: return "StartHue";
            case 2: return "EndHue";
            case 3: return "Saturation";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Animationsgeschwindigkeit (0-255)", "Speed: Animation speed (0-255)");
            case 1: return PARAM_DESC_DE_EN("Start-Farbton: Anfangsfarbe des Gradienten (0-255)", "Start hue: Gradient start color (0-255)");
            case 2: return PARAM_DESC_DE_EN("End-Farbton: Endfarbe des Gradienten (0-255)", "End hue: Gradient end color (0-255)");
            case 3: return PARAM_DESC_DE_EN("Sättigung: Farbsättigung (0-255)", "Saturation: Color saturation (0-255)");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t) const override { return ParameterType::PARAM_UINT8; }
    uint32_t getParameterDefault(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return 32;  // speed
            case 1: return 0;   // start hue
            case 2: return 160; // end hue
            case 3: return 255; // saturation
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t idx) const override
    {
        if (!segment) return 0;
        const auto& c = segment->getConfig();
        switch (idx)
        {
            case 0: return c.speed;
            case 1: return c.option1;
            case 2: return c.option2;
            case 3: return c.option3;
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
            case 1: config.option1 = static_cast<uint8_t>(value); break; // StartHue (start color)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // EndHue (end color)
            case 3: config.option3 = static_cast<uint8_t>(value); break; // Saturation
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // Snapshot config per frame
        const uint8_t speedVal = config.speed;      // animation speed
        const uint8_t v = config.intensity;         // brightness (HSV V)
        const uint8_t startHueCfg = config.option1; // start hue
        const uint8_t endHueCfg = config.option2;   // end hue
        uint8_t s = config.option3;                 // saturation
        if (s == 0) s = 255;
        const bool reverseDir = (config.reverse != 0); // reverse direction
        const bool yellowBoost = config.feature2;      // enable yellow brightness compensation
        const bool greenCorr = config.feature3;        // enable green correction hooks

        // time-based phase advance (frame-rate independent)
        const uint16_t intervalMs = (uint16_t)(1 + (255 - speedVal));
        state.counter += (uint16_t)deltaTime;
        while (state.counter >= intervalMs)
        {
            state.counter -= intervalMs;
            state.position = (uint16_t)((state.position + 1) & 0xFF); // 0..255
        }
        const uint8_t phase = (uint8_t)(state.position & 0xFF);

        uint8_t startHue = startHueCfg;
        uint8_t endHue = endHueCfg;
        if (reverseDir)
        {
            uint8_t tmp = startHue;
            startHue = endHue;
            endHue = tmp;
        }

        for (uint16_t i = 0; i < length; i++)
        {
            const uint8_t t = (uint8_t)((uint32_t)i * 255u / (length - 1u ? (length - 1u) : 1u));
            // linear interpolation in hue space with wrap handled by uint8
            const uint8_t hue = (uint8_t)(startHue + (uint8_t)(((int16_t)(endHue - startHue) * (int16_t)t) >> 8) + phase);

            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, s, v, yellowBoost, greenCorr);
            segment->setPixel(i, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
    }
};
