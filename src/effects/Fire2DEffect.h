/**
 * @file Fire2DEffect.h
 * @brief 2D Fire simulation for LED matrices
 *
 * Each column of the matrix simulates an independent 1D fire strip.
 * Heat rises from the bottom row upward. Falls back to 1D when no
 * matrix geometry is configured.
 *
 * Parameters:
 *   0 Speed    — animation speed (unused, reserved)
 *   1 Cooling  — how fast heat dissipates (20-100, default 90)
 *   2 Sparking — probability of new sparks at bottom (50-200, default 120)
 *   3 BlueFire — 0=normal fire, 1=blue fire mode
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Fire2DEffect : public Effect
{
  private:
    static const uint8_t MAX_DIM = 64;
    uint8_t _heat[MAX_DIM][MAX_DIM];
    uint8_t _lastWidth  = 0;
    uint8_t _lastHeight = 0;

    void resetHeat(uint8_t w, uint8_t h)
    {
        for (uint8_t x = 0; x < w && x < MAX_DIM; x++)
            for (uint8_t y = 0; y < h && y < MAX_DIM; y++)
                _heat[x][y] = 0;
        _lastWidth  = w;
        _lastHeight = h;
    }

    static void heatToRgb(uint8_t t, bool blue, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        uint8_t ramp = (t & 0x3F) << 2;
        if (!blue)
        {
            if      (t & 0x80) { r = 255; g = 255; b = ramp; }
            else if (t & 0x40) { r = 255; g = ramp; b = 0;   }
            else               { r = ramp; g = 0;   b = 0;   }
        }
        else
        {
            if      (t & 0x80) { r = ramp; g = 255; b = 255; }
            else if (t & 0x40) { r = 0;    g = ramp; b = 255; }
            else               { r = 0;    g = 0;    b = ramp; }
        }
    }

    void simulateColumn(uint8_t x, uint8_t h, uint8_t cooling, uint8_t sparking)
    {
        using namespace FastLEDMath;
        // Step 1: cool down
        for (uint8_t y = 0; y < h; y++)
        {
            uint8_t cool = (cooling * 10 / h) + 2;
            _heat[x][y] = qsub8(_heat[x][y], cool);
        }
        // Step 2: heat drifts up
        for (uint8_t y = h - 1; y >= 2; y--)
            _heat[x][y] = (_heat[x][y - 1] + _heat[x][y - 2] + _heat[x][y - 2]) / 3;
        // Step 3: random sparks
        if (random8() < sparking)
        {
            uint8_t y = random8() % (h < 3 ? h : 3);
            _heat[x][y] = qadd8(_heat[x][y], random8(160, 255));
        }
    }

    uint8_t random8() { return (uint8_t)(rand() & 0xFF); }
    uint8_t random8(uint8_t lo, uint8_t hi) { return lo + (rand() % (hi - lo + 1)); }

  public:
    Fire2DEffect()
    {
        for (uint8_t x = 0; x < MAX_DIM; x++)
            for (uint8_t y = 0; y < MAX_DIM; y++)
                _heat[x][y] = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Feuer 2D", "Fire 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "2D Feuer-Simulation für LED-Matrizen. Jede Spalte simuliert einen unabhängigen Feuerverlauf, Hitze steigt von unten auf.",
            "2D fire simulation for LED matrices. Each column simulates an independent fire column, heat rises from bottom.");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    // ====================================================================
    // Parameter API - Self-Describing
    // ====================================================================
    uint8_t getParameterCount() const override { return 3; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Cooling";
            case 1: return "Sparking";
            case 2: return "BlueFire";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Abkühlung: Wie schnell die Hitze in jeder Zelle abnimmt (20=langsames Feuer/hohe Flammen, 100=schnelles Abkühlen/niedrige Flammen).",
                "Cooling: How quickly heat dissipates in each cell (20=slow cooling/tall flames, 100=fast cooling/short flames).");
            case 1: return PARAM_DESC_DE_EN(
                "Funken: Wahrscheinlichkeit neuer Funken an der Basis (50=wenige Funken/ruhiges Feuer, 200=viele Funken/wildes Feuer).",
                "Sparking: Probability of new sparks at the base (50=few sparks/calm fire, 200=many sparks/wild fire).");
            case 2: return PARAM_DESC_DE_EN(
                "Blaues Feuer: 0=normales Feuer (rot/orange/gelb), 1=blaues Feuer (blau/cyan/weiß).",
                "Blue Fire: 0=normal fire (red/orange/yellow), 1=blue fire mode (blue/cyan/white).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 90;  // Cooling
            case 1: return 120; // Sparking
            case 2: return 0;   // BlueFire off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 20;
            case 1: return 50;
            case 2: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 100;
            case 1: return 200;
            case 2: return 1;
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.option1;  // Cooling
            case 1: return cfg.option2;  // Sparking
            case 2: return cfg.feature2; // BlueFire
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.option1  = static_cast<uint8_t>(value); break;
            case 1: cfg.option2  = static_cast<uint8_t>(value); break;
            case 2: cfg.feature2 = static_cast<bool>(value);    break;
        }
    }

    // ====================================================================
    // 1D fallback
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        uint8_t h = (uint8_t)segment->getPhysicalLength();
        if (h > MAX_DIM) h = MAX_DIM;
        auto& cfg = segment->getConfig();
        uint8_t cooling  = cfg.option1 ? cfg.option1  : 90;
        uint8_t sparking = cfg.option2 ? cfg.option2  : 120;
        bool    blue     = cfg.feature2;
        simulateColumn(0, h, cooling, sparking);
        for (uint8_t i = 0; i < h; i++)
        {
            uint8_t r, g, b;
            heatToRgb(_heat[0][i], blue, r, g, b);
            segment->setPixel(i, r, g, b);
        }
    }

    // ====================================================================
    // 2D rendering
    // ====================================================================
    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) { update(segment, deltaTime); return; }

        uint8_t w = geo.width  < MAX_DIM ? geo.width  : MAX_DIM;
        uint8_t h = geo.height < MAX_DIM ? geo.height : MAX_DIM;
        if (w != _lastWidth || h != _lastHeight) resetHeat(w, h);

        auto& cfg = segment->getConfig();
        uint8_t cooling  = cfg.option1 ? cfg.option1  : 90;
        uint8_t sparking = cfg.option2 ? cfg.option2  : 120;
        bool    blue     = cfg.feature2;
        bool    reverse  = (cfg.reverse != 0);

        for (uint8_t x = 0; x < w; x++)
        {
            simulateColumn(x, h, cooling, sparking);
            for (uint8_t y = 0; y < h; y++)
            {
                uint8_t row = reverse ? y : (h - 1 - y);
                uint8_t r, g, b;
                heatToRgb(_heat[x][row], blue, r, g, b);
                segment->setPixelXY(x, y, r, g, b);
            }
        }
    }
};
