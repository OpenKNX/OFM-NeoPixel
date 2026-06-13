/**
 * @file Matrix2DEffect.h
 * @brief Matrix-style digital rain for 2D LED matrices
 */
#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Matrix2DEffect : public Effect
{
  private:
    static constexpr uint16_t MAX_COLS = 255;
    uint8_t _headY[MAX_COLS] = {0};
    uint8_t _trailLen[MAX_COLS] = {0};
    uint8_t _colActive[MAX_COLS] = {0};
    uint8_t _colGold[MAX_COLS] = {0};
    uint32_t _accumMs = 0;
    uint16_t _lastW = 0;
    uint16_t _lastH = 0;

    void resetColumns(uint16_t w, uint16_t h, uint8_t palette)
    {
        for (uint16_t x = 0; x < MAX_COLS; x++)
        {
            _headY[x] = 0;
            _trailLen[x] = 0;
            _colActive[x] = 0;
            _colGold[x] = (palette == 1) ? 1 : ((palette == 2) ? (FastLEDMath::random8() & 0x01) : 0);
        }
        _lastW = w;
        _lastH = h;
    }

    static uint16_t mapU16(uint16_t value, uint16_t inMin, uint16_t inMax, uint16_t outMin, uint16_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (value <= inMin) return outMin;
        if (value >= inMax) return outMax;
        // Supports inverted output ranges (outMax < outMin), e.g. speed -> step time.
        if (outMax >= outMin)
            return outMin + (uint32_t)(value - inMin) * (outMax - outMin) / (inMax - inMin);
        return outMin - (uint32_t)(value - inMin) * (outMin - outMax) / (inMax - inMin);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Matrix 2D", "Matrix 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Digitaler Matrix-Regen in 2D. Varianten: Gruen, Gold oder Gemischt mit optionalen Glitch-Blitzen.",
            "Digital Matrix rain in 2D. Variants: green, gold, or mixed with optional glitch flashes.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Density";
            case 2: return "Palette";
            case 3: return "Glitch";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit des Regens.", "Rain speed.");
            case 1: return PARAM_DESC_DE_EN("Dichte der Spalten in Prozent.", "Column density in percent.");
            case 2: return PARAM_DESC_DE_EN("Palette: 0=Gruen, 1=Gold, 2=Gemischt.", "Palette: 0=Green, 1=Gold, 2=Mixed.");
            case 3: return PARAM_DESC_DE_EN("Glitch-Blitze aktivieren.", "Enable glitch flashes.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_ENUM;
            case 3: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 80;
            case 1: return 35;
            case 2: return 0;
            case 3: return 1;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 5;
            case 2: return 0;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 100;
            case 2: return 2;
            case 3: return 1;
            default: return 255;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 2) return nullptr;
        switch (enumValue)
        {
            case 0: return "Green";
            case 1: return "Gold";
            case 2: return "Mixed";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 2) ? 3 : 0;
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
            case 3: return cfg.feature1;
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
            case 3: cfg.feature1 = static_cast<bool>(value); break;
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
        if (geo.is1D()) return;

        uint16_t renderW = segment->getRenderWidth();
        uint16_t renderH = segment->getRenderHeight();
        if (renderW == 0 || renderH < 2) return;
        if (renderW > MAX_COLS) renderW = MAX_COLS;

        auto& cfg = segment->getConfig();
        uint8_t speed = cfg.speed ? cfg.speed : 80;
        uint8_t density = cfg.option1 ? cfg.option1 : 35;
        uint8_t palette = cfg.option2;
        bool glitch = cfg.feature1;

        if (_lastW != renderW || _lastH != renderH)
        {
            resetColumns(renderW, renderH, palette);
        }

        uint16_t stepMs = mapU16(speed, 1, 255, 200, 24);
        _accumMs += deltaTime;

        while (_accumMs >= stepMs)
        {
            _accumMs -= stepMs;
            for (uint16_t x = 0; x < renderW; x++)
            {
                if (!_colActive[x])
                {
                    if (FastLEDMath::random8(100) < density)
                    {
                        _colActive[x] = 1;
                        _headY[x] = 0;
                        _trailLen[x] = 4 + FastLEDMath::random8((renderH > 12) ? 12 : renderH);
                        if (palette == 2) _colGold[x] = FastLEDMath::random8() & 0x01;
                    }
                    continue;
                }

                _headY[x] = static_cast<uint8_t>(_headY[x] + 1);
                if (_headY[x] > (renderH + _trailLen[x]))
                {
                    _colActive[x] = 0;
                }
            }
        }

        for (uint16_t y = 0; y < renderH; y++)
            for (uint16_t x = 0; x < renderW; x++)
                segment->setPixelGlobalXY(x, y, 0, 0, 0);

        for (uint16_t x = 0; x < renderW; x++)
        {
            if (!_colActive[x]) continue;

            bool gold = (palette == 1) || (palette == 2 && _colGold[x] != 0);
            uint8_t headR = gold ? 255 : 180;
            uint8_t headG = gold ? 210 : 255;
            uint8_t headB = gold ? 90  : 160;

            int16_t head = _headY[x];
            for (uint8_t t = 0; t < _trailLen[x]; t++)
            {
                int16_t yy = head - t;
                if (yy < 0 || yy >= (int16_t)renderH) continue;
                uint8_t fade = FastLEDMath::scale8(255, 255 - (uint8_t)((t * 220) / (_trailLen[x] ? _trailLen[x] : 1)));
                uint8_t r = FastLEDMath::scale8(headR, fade);
                uint8_t g = FastLEDMath::scale8(headG, fade);
                uint8_t b = FastLEDMath::scale8(headB, fade);
                if (t == 0)
                {
                    r = 255;
                    g = 255;
                    b = gold ? 180 : 210;
                }
                segment->setPixelGlobalXY(x, static_cast<uint16_t>(yy), r, g, b);
            }
        }

        if (glitch && FastLEDMath::random8(100) < 6)
        {
            uint8_t flashes = 1 + FastLEDMath::random8(3);
            for (uint8_t i = 0; i < flashes; i++)
            {
                uint16_t x = FastLEDMath::random16(renderW);
                uint16_t y = FastLEDMath::random16(renderH);
                segment->setPixelGlobalXY(x, y, 255, 255, 255);
            }
        }
    }
};
