/**
 * @file Lissajous2DEffect.h
 * @brief Animated Lissajous figure on a 2D LED matrix
 *
 * Traces a Lissajous curve x=sin(a·t), y=sin(b·t+φ); the relative phase φ
 * drifts over time so the figure continuously morphs. A fading trail buffer
 * gives the curve a glowing afterimage.
 *
 * Parameters:
 *   0 Speed  — phase drift speed 1-255 (default: 56)
 *   1 Hue    — base hue 0-255 (default: 160)
 *   2 FreqA  — horizontal frequency 1-16 (default: 3)
 *   3 FreqB  — vertical frequency 1-16 (default: 2)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include <string.h>

class Lissajous2DEffect : public Effect
{
  private:
    uint32_t _timebase = 0;

    static constexpr size_t MAX_GRID_SIZE = 1024; // 32x32 max
    uint8_t _trail[MAX_GRID_SIZE] = {0};
    uint8_t _gridWidth = 0;
    uint8_t _gridHeight = 0;

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
        return EFFECT_NAME_DE_EN("Lissajous 2D", "Lissajous 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Animierte Lissajous-Figur auf einer 2D LED-Matrix mit driftender Phase und nachglühendem "
            "Schweif.",
            "Animated Lissajous figure on a 2D LED matrix with drifting phase and a glowing trail.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "FreqA";
            case 3: return "FreqB";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Driftgeschwindigkeit der Phase.", "Phase drift speed.");
            case 1: return PARAM_DESC_DE_EN("Basis-Farbton der Kurve.", "Base hue of the curve.");
            case 2: return PARAM_DESC_DE_EN("Horizontale Frequenz.", "Horizontal frequency.");
            case 3: return PARAM_DESC_DE_EN("Vertikale Frequenz.", "Vertical frequency.");
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
            case 2: return 1;  // FreqA
            case 3: return 1;  // FreqB
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Hue
            case 2: return 16;  // FreqA
            case 3: return 16;  // FreqB
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 56;  // Speed
            case 1: return 160; // Hue
            case 2: return 3;   // FreqA
            case 3: return 2;   // FreqB
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
        if ((static_cast<size_t>(geo.width) * geo.height) > MAX_GRID_SIZE) return;

        if (_gridWidth != geo.width || _gridHeight != geo.height)
        {
            _gridWidth = geo.width;
            _gridHeight = geo.height;
            memset(_trail, 0, sizeof(_trail));
        }

        _timebase += deltaTime;

        const auto& cfg = segment->getConfig();
        const uint8_t baseHue = cfg.option1;
        const uint8_t freqA = cfg.option2 ? cfg.option2 : 3;
        const uint8_t freqB = cfg.option3 ? cfg.option3 : 2;
        const uint16_t w = _gridWidth;
        const uint16_t h = _gridHeight;
        const size_t cells = static_cast<size_t>(w) * h;

        // Fade the existing trail.
        for (size_t i = 0; i < cells; i++)
            _trail[i] = FastLEDMath::fadeToBlackBy(_trail[i], 40);

        // Re-draw the full figure; the drifting phase morphs it each frame.
        const uint32_t spd = mapLinear(cfg.speed, 1, 255, 1, 24);
        const uint8_t phase = static_cast<uint8_t>((_timebase * spd) >> 10);

        for (uint16_t t = 0; t < 256; t += 2)
        {
            const uint8_t tt = static_cast<uint8_t>(t);
            const uint8_t px = FastLEDMath::sin8(static_cast<uint8_t>(freqA * tt));
            const uint8_t py = FastLEDMath::sin8(static_cast<uint8_t>(freqB * tt + phase));
            const uint8_t x = static_cast<uint8_t>((static_cast<uint16_t>(px) * (w - 1)) / 255);
            const uint8_t y = static_cast<uint8_t>((static_cast<uint16_t>(py) * (h - 1)) / 255);
            _trail[static_cast<size_t>(y) * w + x] = 255;
        }

        // Render the trail buffer.
        for (uint16_t y = 0; y < h; y++)
        {
            for (uint16_t x = 0; x < w; x++)
            {
                const uint8_t v = _trail[static_cast<size_t>(y) * w + x];
                if (!v)
                {
                    segment->setPixelXY(x, y, 0, 0, 0);
                    continue;
                }
                const uint8_t hue = static_cast<uint8_t>(baseHue + (255 - v) / 4);
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, v);
                segment->setPixelXY(x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
        }
    }
};
