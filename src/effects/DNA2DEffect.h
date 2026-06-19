/**
 * @file DNA2DEffect.h
 * @brief Rotating DNA double-helix on a 2D LED matrix
 *
 * Two sine strands wind across the matrix in opposite phase, joined by
 * periodic "rungs", and rotate over time.
 *
 * Parameters:
 *   0 Speed        — rotation speed 1-255 (default: 64)
 *   1 Hue          — strand A base hue 0-255 (strand B is complementary) (default: 0)
 *   2 Twist        — helix turns across the width 1-32 (default: 8)
 *   3 RungSpacing  — draw a rung every N columns, 0 = none (default: 4)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class DNA2DEffect : public Effect
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

    static inline uint8_t yFromSine(uint8_t s, uint16_t h)
    {
        return static_cast<uint8_t>((static_cast<uint16_t>(s) * (h - 1)) / 255);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("DNA 2D", "DNA 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Rotierende DNA-Doppelhelix auf einer 2D LED-Matrix: zwei gegenphasige Sinus-Stränge "
            "mit Sprossen.",
            "Rotating DNA double helix on a 2D LED matrix: two counter-phase sine strands with rungs.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "Twist";
            case 3: return "RungSpacing";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Rotationsgeschwindigkeit der Helix.", "Helix rotation speed.");
            case 1: return PARAM_DESC_DE_EN("Basis-Farbton von Strang A (B ist komplementär).",
                                            "Base hue of strand A (B is complementary).");
            case 2: return PARAM_DESC_DE_EN("Anzahl der Helix-Windungen über die Breite.",
                                            "Number of helix turns across the width.");
            case 3: return PARAM_DESC_DE_EN("Sprosse alle N Spalten (0 = keine).",
                                            "Draw a rung every N columns (0 = none).");
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
            case 2: return 1;  // Twist
            case 3: return 0;  // RungSpacing
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Hue
            case 2: return 32;  // Twist
            case 3: return 16;  // RungSpacing
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64; // Speed
            case 1: return 0;  // Hue
            case 2: return 8;  // Twist
            case 3: return 4;  // RungSpacing
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
        const uint8_t hueA = cfg.option1;
        const uint8_t hueB = static_cast<uint8_t>(hueA + 128);
        const uint8_t twist = cfg.option2 ? cfg.option2 : 8;
        const uint8_t rung = cfg.option3;
        const uint16_t w = geo.width;
        const uint16_t h = geo.height;

        const uint32_t spd = mapLinear(cfg.speed, 1, 255, 4, 90);
        const uint8_t phase = static_cast<uint8_t>((_timebase * spd) >> 11);
        const uint8_t anglePerCol = static_cast<uint8_t>((256u * twist) / w);

        for (uint16_t y = 0; y < h; y++)
            for (uint16_t x = 0; x < w; x++)
                segment->setPixelXY(x, y, 0, 0, 0);

        for (uint16_t x = 0; x < w; x++)
        {
            const uint8_t theta = static_cast<uint8_t>(x * anglePerCol + phase);
            const uint8_t yA = yFromSine(FastLEDMath::sin8(theta), h);
            const uint8_t yB = yFromSine(FastLEDMath::sin8(static_cast<uint8_t>(theta + 128)), h);

            if (rung > 0 && (x % rung) == 0)
            {
                const uint8_t y0 = (yA < yB) ? yA : yB;
                const uint8_t y1 = (yA < yB) ? yB : yA;
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(static_cast<uint8_t>((hueA + hueB) / 2), 200, 70);
                for (uint8_t y = y0; y <= y1; y++)
                    segment->setPixelXY(x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }

            const uint32_t cA = FastLEDMath::hsv2rgb_rainbow(hueA, 255, 255);
            const uint32_t cB = FastLEDMath::hsv2rgb_rainbow(hueB, 255, 255);
            segment->setPixelXY(x, yA, (cA >> 16) & 0xFF, (cA >> 8) & 0xFF, cA & 0xFF);
            segment->setPixelXY(x, yB, (cB >> 16) & 0xFF, (cB >> 8) & 0xFF, cB & 0xFF);
        }
    }
};
