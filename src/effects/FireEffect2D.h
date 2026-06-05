/**
 * @file FireEffect2D.h
 * @brief 2D Fire simulation for LED matrices
 *
 * Extends FireEffect with a true 2D heat grid. Each column simulates an
 * independent fire column; heat spreads upward across rows.
 * Reuses the same ETS parameters as FireEffect (Speed, Cooling, Sparking,
 * ReverseDirection, BlueFireMode).
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * FireEffect2D — column-based 2D fire for LED matrices.
 *
 * Each column of the matrix is treated as an independent 1D fire strip.
 * Heat rises from the bottom row upward.
 *
 * Segment geometry must be configured (setGeometry) for this effect to
 * render in 2D — otherwise it falls back to 1D via update().
 */
class FireEffect2D : public Effect
{
  private:
    // Heat grid: max 64×64 = 4096 cells (fits in RP2040/ESP32 RAM easily)
    static const uint8_t MAX_DIM = 64;
    uint8_t _heat[MAX_DIM][MAX_DIM]; // [col][row]
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

  public:
    FireEffect2D()
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
        return EFFECT_DESC_DE_EN("2D Feuer-Simulation für LED-Matrizen",
                                 "2D fire simulation for LED matrices");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    // ── 1D fallback: delegate to nothing (FireEffect handles 1D, we just render empty) ──
    void update(Segment* segment, uint32_t deltaTime) override
    {
        // 1D fallback: animate as single fire column
        if (!segment) return;
        uint8_t h = (uint8_t)segment->getPhysicalLength();
        if (h > MAX_DIM) h = MAX_DIM;
        simulateColumn(0, h, segment->getConfig());
        auto& cfg = segment->getConfig();
        const bool blueMode = cfg.feature2;
        for (uint8_t i = 0; i < h; i++)
            renderCell(segment, i, _heat[0][i], blueMode);
    }

    // ── 2D update: one independent fire column per matrix column ──
    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        uint8_t w = geo.width  < MAX_DIM ? geo.width  : MAX_DIM;
        uint8_t h = geo.height < MAX_DIM ? geo.height : MAX_DIM;

        if (w != _lastWidth || h != _lastHeight) resetHeat(w, h);

        auto& cfg = segment->getConfig();
        const bool blueMode = cfg.feature2;
        const bool reverse  = (cfg.reverse != 0);

        for (uint8_t x = 0; x < w; x++)
        {
            simulateColumn(x, h, cfg);
            for (uint8_t y = 0; y < h; y++)
            {
                uint8_t row = reverse ? y : (h - 1 - y); // fire rises from bottom
                renderCellXY(segment, x, y, _heat[x][row], blueMode);
            }
        }
    }

  private:
    void simulateColumn(uint8_t x, uint8_t h, const EffectConfig& cfg)
    {
        using namespace FastLEDMath;
        uint8_t cooling  = cfg.option1 ? cfg.option1 : 90;
        uint8_t sparking = cfg.option2 ? cfg.option2 : 120;

        // Step 1: cool down every cell
        for (uint8_t y = 0; y < h; y++)
        {
            uint8_t cool = (cooling * 10 / h) + 2;
            _heat[x][y] = qsub8(_heat[x][y], cool);
        }

        // Step 2: heat drifts up, diffuses slightly
        for (uint8_t y = h - 1; y >= 2; y--)
            _heat[x][y] = (_heat[x][y - 1] + _heat[x][y - 2] + _heat[x][y - 2]) / 3;

        // Step 3: random sparks at bottom
        if (random8() < sparking)
        {
            uint8_t y = random8() % (h < 3 ? h : 3);
            _heat[x][y] = qadd8(_heat[x][y], random8(160, 255));
        }
    }

    void renderCell(Segment* segment, uint16_t index, uint8_t heat, bool blueMode)
    {
        uint8_t r, g, b;
        heatToRgb(heat, blueMode, r, g, b);
        segment->setPixel(index, r, g, b);
    }

    void renderCellXY(Segment* segment, uint8_t x, uint8_t y, uint8_t heat, bool blueMode)
    {
        uint8_t r, g, b;
        heatToRgb(heat, blueMode, r, g, b);
        segment->setPixelXY(x, y, r, g, b);
    }

    static void heatToRgb(uint8_t t, bool blue, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        uint8_t ramp = (t & 0x3F) << 2;
        if (!blue)
        {
            if      (t & 0x80) { r = 255; g = 255; b = ramp; }
            else if (t & 0x40) { r = 255; g = ramp; b = 0;    }
            else               { r = ramp; g = 0;   b = 0;    }
        }
        else
        {
            if      (t & 0x80) { r = ramp; g = 255; b = 255; }
            else if (t & 0x40) { r = 0;    g = ramp; b = 255; }
            else               { r = 0;    g = 0;    b = ramp; }
        }
    }

    uint8_t random8() { return (uint8_t)(rand() & 0xFF); }
    uint8_t random8(uint8_t lo, uint8_t hi) { return lo + (rand() % (hi - lo + 1)); }
};
