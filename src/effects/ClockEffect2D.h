/**
 * @file ClockEffect.h
 * @brief Digital clock display for LED matrices (2D segments)
 *
 * Renders HH:MM (or HH:MM:SS) using the 5×7 font on a 2D LED matrix.
 * Requires NTP time from the OpenKNX framework (openknx.time).
 *
 * Matrix minimum size: 12×7 for HH:MM (12 wide × 7 high)
 *                      18×7 for HH:MM:SS
 *
 * ETS parameters:
 *   feature1 — format: 0=HH:MM, 1=HH:MM:SS
 *   feature2 — blink colon: 0=steady, 1=blink every second
 *   option1  — colour hue (0-255, used when primary color = auto/black)
 *   primaryRGBW — digit colour (0=auto → hue from option1)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "ScrollTextEffect.h" // reuse font + drawColumn helper via free functions
#include "OpenKNX.h"

// ============================================================================
// Small helpers reusing the ScrollTextEffect font
// ============================================================================
namespace ClockFont
{
    // Draw one character at matrix column xStart (uses same 5×7 font)
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

    // Draw a colon at column xStart (single-pixel wide, two dots)
    inline void drawColon(Segment* seg, uint8_t xStart,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t matH)
    {
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
        // Two dots at row 2 and 4 (0-based) within the 7-row glyph area
        uint8_t dot1 = yOff + 2;
        uint8_t dot2 = yOff + 4;
        if (dot1 < matH) seg->setPixelXY(xStart, dot1, r, g, b);
        if (dot2 < matH) seg->setPixelXY(xStart, dot2, r, g, b);
    }
} // namespace ClockFont

// ============================================================================

class ClockEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Uhr", "Clock");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Digitale Uhr auf LED-Matrix (HH:MM oder HH:MM:SS)",
                                 "Digital clock on LED matrix (HH:MM or HH:MM:SS)");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    void update(Segment* seg, uint32_t dt) override { (void)seg; (void)dt; }

    void update2D(Segment* seg, uint32_t dt) override
    {
        if (!seg) return;
        const auto& geo = seg->getGeometry();
        if (geo.is1D()) return;

        auto& cfg = seg->getConfig();

        // Get time from OpenKNX framework
        if (!openknx.time.isValid()) return;
        auto now = openknx.time.getLocalTime();

        bool showSeconds = cfg.feature1;
        bool blinkColon  = cfg.feature2;
        uint8_t matH     = geo.height;

        // Colour: use primary if non-zero, else generate from hue
        uint8_t r = cfg.r(), g = cfg.g(), b = cfg.b();
        if (r == 0 && g == 0 && b == 0)
        {
            // Auto colour from hue option1
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(cfg.option1, 255, 200);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >>  8) & 0xFF;
            b =  rgb        & 0xFF;
        }

        // Blinking colon: off on odd seconds
        bool colonVisible = !(blinkColon && (now.second & 1));

        seg->clear();

        // Layout (5-wide chars, 1-wide colon, 1-wide gap between fields):
        // HH:MM       → 5+5+1+5+5 + gaps = ~23 px minimum
        // HH:MM:SS    → 5+5+1+5+5+1+5+5 + gaps = ~34 px minimum
        // pixel width: 4 digits × 5 + 1 or 2 colons × 1 + gaps
        uint8_t totalW = showSeconds ? (4 * 5 + 4 + 2) : (4 * 5 + 2 + 1);
        uint8_t xOff = geo.width > totalW ? (geo.width - totalW) / 2 : 0;

        char h1 = '0' + now.hour   / 10;
        char h2 = '0' + now.hour   % 10;
        char m1 = '0' + now.minute / 10;
        char m2 = '0' + now.minute % 10;
        char s1 = '0' + now.second / 10;
        char s2 = '0' + now.second % 10;

        uint8_t cx = xOff;
        auto dc = [&](char ch) {
            if (cx >= geo.width) return;
            ClockFont::drawChar(seg, cx, ch, r, g, b, matH);
            cx += kFontWidth + 1;
        };
        auto dcol = [&]() {
            if (cx >= geo.width) return;
            if (colonVisible) ClockFont::drawColon(seg, cx, r, g, b, matH);
            cx += 2; // colon + gap
        };

        dc(h1); dc(h2); dcol();
        dc(m1); dc(m2);
        if (showSeconds) { dcol(); dc(s1); dc(s2); }
    }
};
