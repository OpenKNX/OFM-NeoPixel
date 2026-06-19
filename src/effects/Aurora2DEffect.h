/**
 * @file Aurora2DEffect.h
 * @brief Polar lights (Aurora) on a 2D LED matrix
 *
 * Soft, flowing curtains of light from cross-modulated sine layers — an
 * organic, drifting "northern lights" field. Defaults to green/teal.
 *
 * Parameters:
 *   0 Speed      — drift speed 1-255 (default: 48)
 *   1 Hue        — base hue 0-255 (default: 96 = green) (default: 96)
 *   2 Scale      — spatial detail / wave density 1-64 (default: 18)
 *   3 Intensity  — maximum brightness 0-255 (default: 220)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Aurora2DEffect : public Effect
{
  private:
    uint32_t _timebase = 0;

    static uint32_t mapLinear(uint32_t value, uint32_t inMin, uint32_t inMax, uint32_t outMin, uint32_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (value <= inMin) return outMin;
        if (value >= inMax) return outMax;
        const uint32_t span = inMax - inMin;
        if (outMax >= outMin)
            return outMin + ((value - inMin) * (outMax - outMin)) / span;
        return outMin - ((value - inMin) * (outMin - outMax)) / span;
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Aurora 2D", "Aurora 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Polarlicht auf einer 2D LED-Matrix: weiche, fließende Lichtvorhänge aus überlagerten "
            "Sinus-Schichten.",
            "Polar lights on a 2D LED matrix: soft, flowing curtains from layered sine fields.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "Scale";
            case 3: return "Intensity";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Driftgeschwindigkeit der Vorhänge.", "Drift speed of the curtains.");
            case 1: return PARAM_DESC_DE_EN("Basis-Farbton (96 = grün).", "Base hue (96 = green).");
            case 2: return PARAM_DESC_DE_EN("Räumliche Detailtiefe / Wellendichte.", "Spatial detail / wave density.");
            case 3: return PARAM_DESC_DE_EN("Maximale Helligkeit.", "Maximum brightness.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        (void)index;
        return ParameterType::PARAM_UINT8;
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed
            case 1: return 0;  // Hue
            case 2: return 1;  // Scale
            case 3: return 0;  // Intensity
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Hue
            case 2: return 64;  // Scale
            case 3: return 255; // Intensity
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 48;  // Speed
            case 1: return 96;  // Hue (green)
            case 2: return 18;  // Scale
            case 3: return 220; // Intensity
        }
        return 0;
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;
            case 1: return cfg.option1;
            case 2: return cfg.option2;
            case 3: return cfg.option3;
        }
        return 0;
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = static_cast<uint8_t>(value); break;
            case 1: cfg.option1 = static_cast<uint8_t>(value); break;
            case 2: cfg.option2 = static_cast<uint8_t>(value); break;
            case 3: cfg.option3 = static_cast<uint8_t>(value); break;
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)segment;
        (void)deltaTime;
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D() || geo.width < 2 || geo.height < 2) return;

        _timebase += deltaTime;

        const auto& cfg = segment->getConfig();
        const uint8_t baseHue = cfg.option1;
        const uint8_t scale = cfg.option2 ? cfg.option2 : 18;
        const uint8_t intensity = cfg.option3;
        const uint16_t w = geo.width;
        const uint16_t h = geo.height;

        const uint32_t spd = mapLinear(cfg.speed, 1, 255, 4, 80);
        const uint8_t phase = static_cast<uint8_t>((_timebase * spd) >> 10);
        const uint8_t sx = static_cast<uint8_t>(scale);
        const uint8_t sy = static_cast<uint8_t>((scale >> 1) ? (scale >> 1) : 1);

        for (uint16_t y = 0; y < h; y++)
        {
            for (uint16_t x = 0; x < w; x++)
            {
                const uint8_t a = FastLEDMath::sin8(static_cast<uint8_t>(x * sx + phase));
                const uint8_t b = FastLEDMath::sin8(static_cast<uint8_t>(y * sy - phase + (a >> 1)));
                uint8_t bri = FastLEDMath::scale8(a, b);
                bri = FastLEDMath::scale8(bri, intensity);
                const uint8_t hue = static_cast<uint8_t>(baseHue + (b >> 3));
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, bri);
                segment->setPixelXY(x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
        }
    }
};
