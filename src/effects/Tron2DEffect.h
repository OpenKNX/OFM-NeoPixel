/**
 * @file Tron2DEffect.h
 * @brief TRON-style scanner grid for 2D LED matrices
 */
#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Tron2DEffect : public Effect
{
  private:
    static uint16_t mapU16(uint16_t value, uint16_t inMin, uint16_t inMax, uint16_t outMin, uint16_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (value <= inMin) return outMin;
        if (value >= inMax) return outMax;
        // Supports inverted output ranges (outMax < outMin), e.g. speed -> travel time.
        if (outMax >= outMin)
            return outMin + (uint32_t)(value - inMin) * (outMax - outMin) / (inMax - inMin);
        return outMin - (uint32_t)(value - inMin) * (outMin - outMax) / (inMax - inMin);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("TRON", "TRON");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Neon-Grid mit bewegten Scanlinien im TRON-Stil.",
            "Neon grid with moving scan lines in TRON style.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "GridSpacing";
            case 3: return "Glow";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit der Scanner.", "Scanner speed.");
            case 1: return PARAM_DESC_DE_EN("Neon-Farbton.", "Neon hue.");
            case 2: return PARAM_DESC_DE_EN("Abstand der Grid-Linien.", "Grid line spacing.");
            case 3: return PARAM_DESC_DE_EN("Glow-Intensität.", "Glow intensity.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        if (index == 1) return ParameterType::PARAM_HUE;
        return ParameterType::PARAM_UINT8;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 90;
            case 1: return 150;
            case 2: return 4;
            case 3: return 170;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 0;
            case 2: return 2;
            case 3: return 20;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 255;
            case 2: return 12;
            case 3: return 255;
            default: return 255;
        }
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
            default: return 0;
        }
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
        if (segment->getGeometry().is1D()) return;

        uint16_t w = segment->getRenderWidth();
        uint16_t h = segment->getRenderHeight();
        if (w == 0 || h == 0) return;

        auto& cfg = segment->getConfig();
        uint8_t speed = cfg.speed ? cfg.speed : 90;
        uint8_t hue = cfg.option1;
        uint8_t spacing = cfg.option2 ? cfg.option2 : 4;
        uint8_t glow = cfg.option3 ? cfg.option3 : 170;

        uint16_t travelX = mapU16(speed, 1, 255, 420, 70);
        uint16_t travelY = mapU16(speed, 1, 255, 520, 85);

        uint32_t t = millis() + deltaTime;
        uint16_t scanX = (travelX > 0) ? ((t / travelX) % w) : 0;
        uint16_t scanY = (travelY > 0) ? ((t / travelY) % h) : 0;

        uint32_t baseRgb = FastLEDMath::hsv2rgb_rainbow(hue, 220, glow);
        uint8_t br = (baseRgb >> 16) & 0xFF;
        uint8_t bg = (baseRgb >> 8) & 0xFF;
        uint8_t bb = baseRgb & 0xFF;

        for (uint16_t y = 0; y < h; y++)
        {
            for (uint16_t x = 0; x < w; x++)
            {
                uint8_t r = 0, g = 0, b = 0;

                bool onGrid = ((x % spacing) == 0) || ((y % spacing) == 0);
                if (onGrid)
                {
                    r = FastLEDMath::scale8(br, 70);
                    g = FastLEDMath::scale8(bg, 70);
                    b = FastLEDMath::scale8(bb, 70);
                }

                uint16_t dx = (x > scanX) ? (x - scanX) : (scanX - x);
                uint16_t dy = (y > scanY) ? (y - scanY) : (scanY - y);

                if (dx <= 1)
                {
                    uint8_t beam = (dx == 0) ? 255 : 140;
                    r = FastLEDMath::qadd8(r, FastLEDMath::scale8(br, beam));
                    g = FastLEDMath::qadd8(g, FastLEDMath::scale8(bg, beam));
                    b = FastLEDMath::qadd8(b, FastLEDMath::scale8(bb, beam));
                }

                if (dy <= 1)
                {
                    uint8_t beam = (dy == 0) ? 255 : 140;
                    r = FastLEDMath::qadd8(r, FastLEDMath::scale8(br, beam));
                    g = FastLEDMath::qadd8(g, FastLEDMath::scale8(bg, beam));
                    b = FastLEDMath::qadd8(b, FastLEDMath::scale8(bb, beam));
                }

                segment->setPixelGlobalXY(x, y, r, g, b);
            }
        }
    }
};
