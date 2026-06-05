/**
 * @file CylonEffect2D.h
 * @brief 2D Cylon effect for LED matrices
 *
 * In 2D mode the Cylon eye sweeps as a full column (vertical bar) or
 * full row (horizontal bar) across the matrix, depending on direction.
 *
 * ETS parameters (shared with CylonEffect):
 *   Speed      (config.speed)   — sweep speed 1-255
 *   Hue        (config.option1) — eye color hue 0-255
 *   EyeSize    (config.option2) — width of the eye in LEDs (default 1)
 *   FadeAmount (config.fade)    — trail fade 0-255
 *   feature1                   — direction: 0=horizontal (column sweeps), 1=vertical (row sweeps)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class CylonEffect2D : public Effect
{
  private:
    uint32_t _timeAcc  = 0;

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Cylon 2D", "Cylon 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Cylon-Auge als Zeile/Spalte über eine LED-Matrix",
                                 "Cylon eye sweeping as row or column across a matrix");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        // 1D fallback: do nothing (CylonEffect handles 1D already)
        if (!segment) return;
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) return;

        _timeAcc += deltaTime;
        auto& cfg    = segment->getConfig();

        uint8_t hue       = cfg.option1;
        uint8_t eyeSize   = cfg.option2 ? cfg.option2 : 1;
        uint8_t fadeAmt   = cfg.fade    ? cfg.fade    : 50;
        uint16_t speed    = (uint16_t)cfg.speed + 1;
        bool vertical     = cfg.feature1; // true = row sweeps, false = column sweeps

        uint16_t dim = vertical ? geo.height : geo.width; // number of positions
        // Bounce back & forth using a triangle wave based on time
        uint32_t period = (uint32_t)(dim * 2 - 2) * (256 - speed + 1) * 4;
        uint32_t phase  = _timeAcc % period;
        uint16_t pos;
        if (phase < period / 2)
            pos = (uint16_t)((phase * (dim - 1)) / (period / 2));
        else
            pos = (uint16_t)(dim - 1 - ((phase - period / 2) * (dim - 1)) / (period / 2));

        // Eye color
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, 255);
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >>  8) & 0xFF;
        uint8_t b =  rgb        & 0xFF;

        // Fade all pixels
        for (uint8_t y = 0; y < geo.height; y++)
            for (uint8_t x = 0; x < geo.width; x++)
            {
                uint8_t pr, pg, pb;
                segment->getPixel(segment->xyToIndex(x, y), pr, pg, pb);
                pr = FastLEDMath::scale8(pr, 255 - fadeAmt);
                pg = FastLEDMath::scale8(pg, 255 - fadeAmt);
                pb = FastLEDMath::scale8(pb, 255 - fadeAmt);
                segment->setPixelXY(x, y, pr, pg, pb);
            }

        // Draw eye
        for (uint8_t e = 0; e < eyeSize; e++)
        {
            uint16_t epos = pos + e;
            if (epos >= dim) break;
            if (vertical)
                for (uint8_t x = 0; x < geo.width; x++)
                    segment->setPixelXY(x, (uint8_t)epos, r, g, b);
            else
                for (uint8_t y = 0; y < geo.height; y++)
                    segment->setPixelXY((uint8_t)epos, y, r, g, b);
        }
    }
};
