/**
 * @file PlasmaNebula2DEffect.h
 * @brief Animated plasma nebula for 2D LED matrices
 */
#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class PlasmaNebula2DEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Plasma Nebula", "Plasma Nebula");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Mehrlagige Plasma-Nebel-Animation.", "Multi-layer plasma nebula animation.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Saturation";
            case 2: return "Contrast";
            case 3: return "PaletteShift";
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        return ParameterType::PARAM_UINT8;
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Animationsgeschwindigkeit des Plasma-Nebels (0-255)", "Speed: Animation speed of the plasma nebula (0-255)");
            case 1: return PARAM_DESC_DE_EN("Sättigung: Farbsättigung des Nebels (0=blass, 255=kräftig)", "Saturation: Color saturation of the nebula (0=pale, 255=vivid)");
            case 2: return PARAM_DESC_DE_EN("Kontrast: Helligkeitskontrast zwischen Nebelschichten (0-255)", "Contrast: Brightness contrast between nebula layers (0-255)");
            case 3: return PARAM_DESC_DE_EN("Palettenverschiebung: Verschiebt den Farbton der Palette (0-255)", "PaletteShift: Shifts the hue of the color palette (0-255)");
            default: return "";
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64;
            case 1: return 220;
            case 2: return 170;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        (void)index;
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        (void)index;
        return 255;
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
        uint8_t speed = cfg.speed ? cfg.speed : 64;
        uint8_t sat = cfg.option1 ? cfg.option1 : 220;
        uint8_t contrast = cfg.option2 ? cfg.option2 : 170;
        uint8_t shift = cfg.option3;

        uint8_t timePhase = static_cast<uint8_t>((millis() + deltaTime) / (3 + (255 - speed) / 20));

        for (uint16_t y = 0; y < h; y++)
        {
            for (uint16_t x = 0; x < w; x++)
            {
                uint8_t n1 = FastLEDMath::sin8((uint8_t)((x * 11 + timePhase) & 0xFF));
                uint8_t n2 = FastLEDMath::sin8((uint8_t)((y * 13 - timePhase * 2) & 0xFF));
                uint8_t n3 = FastLEDMath::sin8((uint8_t)(((x + y) * 7 + timePhase * 3) & 0xFF));
                uint8_t v = FastLEDMath::scale8((uint8_t)((n1 + n2 + n3) / 3), contrast);

                uint8_t hue = (uint8_t)(shift + n1 / 2 + n2 / 3 + timePhase);
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, sat, v);
                segment->setPixelGlobalXY(x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
        }
    }
};
