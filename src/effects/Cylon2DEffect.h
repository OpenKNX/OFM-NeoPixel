/**
 * @file Cylon2DEffect.h
 * @brief Cylon eye sweeping as full row or column across a 2D LED matrix
 *
 * Parameters:
 *   0 Speed     — sweep speed 1-255 (default 64)
 *   1 Hue       — eye colour hue 0-255 (default 0 = red)
 *   2 EyeSize   — width of the eye band in LEDs (default 1)
 *   3 FadeAmount— trail fade per frame 0-255 (default 50)
 *   4 Direction — 0=horizontal (column sweeps left-right), 1=vertical (row sweeps top-bottom)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Cylon2DEffect : public Effect
{
  private:
    uint32_t _timeAcc = 0;

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Cylon 2D", "Cylon 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Cylon-Auge das als leuchtende Zeile oder Spalte über eine LED-Matrix springt. Richtung, Farbe und Schweif konfigurierbar.",
            "Cylon eye sweeping as a glowing row or column across a 2D LED matrix. Direction, colour and trail configurable.");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "EyeSize";
            case 3: return "FadeAmount";
            case 4: return "Direction";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Geschwindigkeit: Bewegungsgeschwindigkeit des Auges (1=sehr langsam, 255=sehr schnell).",
                "Speed: Movement speed of the eye (1=very slow, 255=very fast).");
            case 1: return PARAM_DESC_DE_EN(
                "Farbton: HSV-Farbwert des Auges (0=rot, 85=grün, 170=blau, 255=rot).",
                "Hue: HSV colour value of the eye (0=red, 85=green, 170=blue, 255=red).");
            case 2: return PARAM_DESC_DE_EN(
                "Augengröße: Breite des leuchtenden Bandes in LEDs (1-20). Größere Werte erzeugen ein breiteres Auge.",
                "Eye Size: Width of the glowing band in LEDs (1-20). Larger values create a wider eye.");
            case 3: return PARAM_DESC_DE_EN(
                "Schweif: Wie schnell der Schweif hinter dem Auge verblasst (1=langer Schweif, 255=kurzer Schweif).",
                "Fade: How quickly the trail fades (1=long trail, 255=short trail).");
            case 4: return PARAM_DESC_DE_EN(
                "Richtung: 0=horizontal (Spalte wandert links-rechts), 1=vertikal (Zeile wandert oben-unten).",
                "Direction: 0=horizontal (column sweeps left-right), 1=vertical (row sweeps top-bottom).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_HUE;
            case 2: return ParameterType::PARAM_UINT8;
            case 3: return ParameterType::PARAM_UINT8;
            case 4: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64;  // Speed
            case 1: return 0;   // Hue (red)
            case 2: return 1;   // EyeSize
            case 3: return 50;  // FadeAmount
            case 4: return 0;   // Direction horizontal
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0;
            case 1: return 0;
            case 2: return 1;
            case 3: return 1;
            case 4: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 255;
            case 2: return 20;
            case 3: return 255;
            case 4: return 1;
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;    // Speed
            case 1: return cfg.option1;  // Hue
            case 2: return cfg.option2;  // EyeSize
            case 3: return cfg.fade;     // FadeAmount
            case 4: return cfg.feature1; // Direction
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed   = static_cast<uint8_t>(value); break;
            case 1: cfg.option1 = static_cast<uint8_t>(value); break;
            case 2: cfg.option2 = static_cast<uint8_t>(value); break;
            case 3: cfg.fade    = static_cast<uint8_t>(value); break;
            case 4: cfg.feature1= static_cast<bool>(value);    break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)segment; (void)deltaTime; // 1D not supported
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) return;

        _timeAcc += deltaTime;
        auto& cfg  = segment->getConfig();
        uint8_t hue      = cfg.option1;
        uint8_t eyeSize  = cfg.option2 ? cfg.option2 : 1;
        uint8_t fadeAmt  = cfg.fade    ? cfg.fade    : 50;
        uint16_t speed   = (uint16_t)cfg.speed + 1;
        bool vertical    = cfg.feature1;

        uint16_t dim    = vertical ? geo.height : geo.width;
        uint32_t period = (uint32_t)(dim * 2 - 2) * (256 - speed + 1) * 4;
        if (period == 0) period = 1;
        uint32_t phase  = _timeAcc % period;
        uint16_t pos;
        if (phase < period / 2)
            pos = (uint16_t)((phase * (dim - 1)) / (period / 2));
        else
            pos = (uint16_t)(dim - 1 - ((phase - period / 2) * (dim - 1)) / (period / 2));

        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, 255);
        uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;

        // Fade all pixels
        for (uint8_t y = 0; y < geo.height; y++)
            for (uint8_t x = 0; x < geo.width; x++)
            {
                uint16_t idx = segment->xyToIndex(x, y);
                if (idx == 0xFFFF) continue;
                uint8_t pr, pg, pb;
                segment->getPixel(idx, pr, pg, pb);
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
