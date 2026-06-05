/**
 * @file Clock2DEffect.h
 * @brief Digital clock display for LED matrices (HH:MM or HH:MM:SS)
 *
 * Requires openknx.time to be valid (time set via KNX bus or other source).
 * Minimum matrix width: 12 px (HH:MM), 18 px (HH:MM:SS).
 *
 * Parameters:
 *   0 ShowSeconds  — 0=HH:MM, 1=HH:MM:SS
 *   1 BlinkColon   — 0=steady colon, 1=blink every second
 *   2 ColourHue    — hue when primary colour is black (auto colour)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "ScrollTextEffect.h" // reuse font constants
#include "FastLEDMath.h"
#include "OpenKNX.h"

namespace Clock2DFont
{
    inline void drawChar(Segment* seg, uint8_t xStart, char ch,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t matH)
    {
        if (ch < kFontFirst || ch > 0x7E) ch = ' ';
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
        uint8_t rows = matH < kFontHeight ? matH : kFontHeight;
        for (uint8_t col = 0; col < kFontWidth; col++)
        {
            uint8_t bits = pgm_read_byte(&kFont5x7[ch - kFontFirst][col]);
            for (uint8_t row = 0; row < rows; row++)
            {
                bool on = (bits >> row) & 1;
                seg->setPixelXY(xStart + col, yOff + row,
                                on ? r : 0, on ? g : 0, on ? b : 0);
            }
        }
    }

    inline void drawColon(Segment* seg, uint8_t xStart,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t matH)
    {
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
        uint8_t dot1 = yOff + 2;
        uint8_t dot2 = yOff + 4;
        if (dot1 < matH) seg->setPixelXY(xStart, dot1, r, g, b);
        if (dot2 < matH) seg->setPixelXY(xStart, dot2, r, g, b);
    }
}

class Clock2DEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Uhr 2D", "Clock 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Digitale Uhr (HH:MM oder HH:MM:SS) auf einer LED-Matrix. Erfordert gültige KNX-Uhrzeit. Mindestbreite: 12 Pixel (HH:MM), 18 Pixel (HH:MM:SS).",
            "Digital clock (HH:MM or HH:MM:SS) on an LED matrix. Requires valid KNX time. Minimum width: 12 px (HH:MM), 18 px (HH:MM:SS).");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 3; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "ShowSeconds";
            case 1: return "BlinkColon";
            case 2: return "ColourHue";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Sekunden anzeigen: 0=HH:MM (12 Zeichen breit), 1=HH:MM:SS (18 Zeichen breit).",
                "Show seconds: 0=HH:MM (12 chars wide), 1=HH:MM:SS (18 chars wide).");
            case 1: return PARAM_DESC_DE_EN(
                "Doppelpunkt blinken: 0=Doppelpunkt leuchtet dauerhaft, 1=Doppelpunkt blinkt sekündlich.",
                "Blink colon: 0=colon is always on, 1=colon blinks every second.");
            case 2: return PARAM_DESC_DE_EN(
                "Farb-Farbton: HSV-Farbton wenn Primärfarbe schwarz ist (0=rot, 85=grün, 170=blau). Nur aktiv wenn Primärfarbe = schwarz.",
                "Colour hue: HSV hue used when primary colour is black (0=red, 85=green, 170=blue). Only active when primary colour = black.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_BOOL;
            case 1: return ParameterType::PARAM_BOOL;
            case 2: return ParameterType::PARAM_HUE;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0;   // HH:MM
            case 1: return 1;   // blink colon
            case 2: return 0;   // red hue
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0;
            case 1: return 0;
            case 2: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 1;
            case 2: return 255;
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.feature1; // ShowSeconds
            case 1: return cfg.feature2; // BlinkColon
            case 2: return cfg.option1;  // ColourHue
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.feature1 = static_cast<bool>(value);    break;
            case 1: cfg.feature2 = static_cast<bool>(value);    break;
            case 2: cfg.option1  = static_cast<uint8_t>(value); break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override { (void)segment; (void)deltaTime; }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) return;
        if (!openknx.time.isValid()) return;

        auto now = openknx.time.getLocalTime();
        auto& cfg = segment->getConfig();
        bool showSeconds = cfg.feature1;
        bool blinkColon  = cfg.feature2;
        uint8_t matH     = geo.height;
        bool colonVisible = !(blinkColon && (now.second & 1));

        uint8_t r = cfg.r(), g = cfg.g(), b = cfg.b();
        if (r == 0 && g == 0 && b == 0)
        {
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(cfg.option1, 255, 200);
            r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
        }

        uint8_t totalW = showSeconds ? (4 * 5 + 4 + 2) : (4 * 5 + 2 + 1);
        uint8_t xOff   = geo.width > totalW ? (geo.width - totalW) / 2 : 0;

        seg->clear();

        uint8_t cx = xOff;
        auto dc = [&](char ch) {
            if (cx + kFontWidth > geo.width) return;
            Clock2DFont::drawChar(segment, cx, ch, r, g, b, matH);
            cx += kFontWidth + 1;
        };
        auto dcol = [&]() {
            if (cx >= geo.width) return;
            if (colonVisible) Clock2DFont::drawColon(segment, cx, r, g, b, matH);
            cx += 2;
        };

        dc('0' + now.hour   / 10); dc('0' + now.hour   % 10); dcol();
        dc('0' + now.minute / 10); dc('0' + now.minute % 10);
        if (showSeconds) { dcol(); dc('0' + now.second / 10); dc('0' + now.second % 10); }
    }

  private:
    Segment* seg = nullptr; // helper alias set in update2D
};
