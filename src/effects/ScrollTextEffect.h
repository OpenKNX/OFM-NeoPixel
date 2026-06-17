/**
 * @file ScrollTextEffect.h
 * @brief Horizontal scrolling text for LED matrices (2D segments)
 *
 * Renders text using a selectable built-in pixel font (5×7 / 4×6 / 3×5) and
 * scrolls it from right to left across the matrix. Covers the full printable
 * ASCII set (0x20..0x7E) plus an extended Latin block (German umlauts ä ö ü
 * Ä Ö Ü ß, Ç/ç, common accents, € § ° µ £). Input text may be ISO-8859-1
 * (KNX DPT 16, single byte) or UTF-8 — both are decoded.
 *
 * Font data is auto-generated into ScrollTextFonts.h by
 * scripts/Gen-ScrollTextFonts.ps1 (run that script to add glyphs/sizes).
 *
 * ETS parameters:
 *   Speed    (config.speed)   — scroll speed 1-255 (default 32)
 *   option1                  — gap between characters in pixels (default 1)
 *   feature1                 — wrap: 0=scroll once then stop, 1=loop
 *   option2                  — font: 0=5×7, 1=4×6, 2=3×5 (default 0)
 *   primaryRGBW              — text colour (default white)
 *   Text     (config.effectText) — per-segment text (14 chars), set via ETS/Cue/Scene/KO
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "ScrollTextFonts.h"
#include <Arduino.h>
#include <string.h>

// ============================================================================

class ScrollTextEffect : public Effect
{
  private:
    // Per-render state stored in EffectState:
    //   position = current scroll offset in pixels (0 = start at right edge)
    //   lastUpdate = millis of last pixel advance
    // Text lives per segment in EffectConfig::effectText (no shared singleton).

    // Clamp the configured font index to a valid descriptor
    static const ScrollFontDesc& fontFor(uint8_t fontIdx)
    {
        if (fontIdx >= kScrollFontCount) fontIdx = 0;
        return kScrollFonts[fontIdx];
    }

    // Map a Unicode codepoint to a glyph index (0 = space for unknown)
    static uint8_t glyphIndex(uint16_t cp)
    {
        const uint8_t asciiCount = kScrollFontLastAscii - kScrollFontFirstAscii + 1;
        if (cp >= kScrollFontFirstAscii && cp <= kScrollFontLastAscii)
            return (uint8_t)(cp - kScrollFontFirstAscii);
        for (uint8_t i = 0; i < kScrollFontExtCount; i++)
            if (pgm_read_word(&kScrollFontExtCodepoints[i]) == cp)
                return (uint8_t)(asciiCount + i);
        return 0; // unknown -> space
    }

    // Decode the next codepoint from a byte string. Handles UTF-8 (1-3 byte)
    // and falls back to ISO-8859-1 (single byte) when the bytes are not a valid
    // UTF-8 sequence (KNX DPT 16 strings are Latin-1). Returns bytes consumed.
    static uint8_t decodeCodepoint(const char* s, uint16_t& cp)
    {
        uint8_t b0 = (uint8_t)s[0];
        if (b0 < 0x80) { cp = b0; return 1; }
        // 2-byte UTF-8: 110xxxxx 10xxxxxx
        if ((b0 & 0xE0) == 0xC0)
        {
            uint8_t b1 = (uint8_t)s[1];
            if ((b1 & 0xC0) == 0x80)
            {
                cp = (uint16_t)(((b0 & 0x1F) << 6) | (b1 & 0x3F));
                return 2;
            }
        }
        // 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx (covers € U+20AC)
        else if ((b0 & 0xF0) == 0xE0)
        {
            uint8_t b1 = (uint8_t)s[1];
            uint8_t b2 = b1 ? (uint8_t)s[2] : 0; // avoid reading past terminator
            if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80)
            {
                cp = (uint16_t)(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
                return 3;
            }
        }
        // Not valid UTF-8 -> treat as ISO-8859-1 single byte
        cp = b0;
        return 1;
    }

    // Draw one glyph column of glyph `gi` (col within font width) at matrix column x
    static void drawColumn(Segment* seg, uint8_t x, uint8_t gi, uint8_t col,
                           const ScrollFontDesc& font,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t matH)
    {
        uint8_t colBits = pgm_read_byte(&font.data[(uint16_t)gi * font.width + col]);
        uint8_t rows = matH < font.height ? matH : font.height;
        // Centre the glyph vertically in the matrix
        uint8_t yOff = matH > font.height ? (matH - font.height) / 2 : 0;
        for (uint8_t row = 0; row < rows; row++)
        {
            bool on = (colBits >> row) & 1;
            seg->setPixelXY(x, yOff + row, on ? r : 0, on ? g : 0, on ? b : 0);
        }
    }

  public:
    // ── Effect interface ───────────────────────────────────────────────────────
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Laufschrift", "Scroll Text");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Horizontale Laufschrift auf LED-Matrix (Font 5×7/4×6/3×5, Umlaute & Ç)",
                                 "Horizontal scrolling text on LED matrix (font 5×7/4×6/3×5, umlauts & Ç)");
    }

    uint8_t getCapabilities() const override { return DIM_2D; } // 2D only

    void update(Segment* seg, uint32_t dt) override
    {
        // On 1D segments do nothing
        (void)seg;
        (void)dt;
    }

    void update2D(Segment* seg, uint32_t dt) override
    {
        if (!seg) return;
        const auto& geo = seg->getGeometry();
        if (geo.is1D()) return;

        auto& state = seg->getState();
        auto& cfg = seg->getConfig();

        // Per-segment text (fallback keeps old demo behaviour when unset)
        const char* text = cfg.effectText[0] ? cfg.effectText : "OpenKNX NeoPixel";
        if (!text[0]) return;

        const ScrollFontDesc& font = fontFor(cfg.option2);

        // Decode the byte string into glyph indices (UTF-8 / Latin-1 aware).
        // effectText holds up to 240 bytes; multi-byte chars only shrink the count.
        uint8_t glyphs[sizeof(cfg.effectText)];
        uint8_t glyphCount = 0;
        for (const char* p = text; *p && glyphCount < sizeof(glyphs);)
        {
            uint16_t cp;
            p += decodeCodepoint(p, cp);
            glyphs[glyphCount++] = glyphIndex(cp);
        }
        if (glyphCount == 0) return;

        uint8_t matW = geo.width;
        uint8_t matH = geo.height;
        uint8_t gap = cfg.option1 ? cfg.option1 : 1;
        uint16_t speed = (uint16_t)cfg.speed + 1; // higher = faster
        bool loop = cfg.feature1;

        // Total pixel width of the whole text string
        uint16_t totalW = (uint16_t)glyphCount * (font.width + gap);
        uint16_t maxPos = totalW + matW + gap * 4; // extra gap to scroll fully out

        // Advance scroll position by time
        state.lastUpdate += dt;

        //uint16_t maxPos = totalW + matW;
        uint32_t msPerPixel = 1000 / speed;

        while (state.lastUpdate >= msPerPixel)
        {
            state.lastUpdate -= msPerPixel;
            state.position++;
            if (state.position >= maxPos)
            {
                if (loop)
                    state.position = 0;
                else
                    state.position = maxPos;
            }
        }

        // scroll offset: text starts at matW - position (can be negative = scrolled in)
        int16_t startX = (int16_t)matW - (int16_t)state.position;

        uint8_t r = cfg.r(), g = cfg.g(), b = cfg.b();

        // Clear all columns first
        seg->clear();

        // Render each character column
        for (uint8_t ci = 0; ci < glyphCount; ci++)
        {
            int16_t charX = startX + (int16_t)ci * (font.width + gap);
            for (uint8_t col = 0; col < font.width; col++)
            {
                int16_t px = charX + col;
                if (px < 0 || px >= matW) continue;
                drawColumn(seg, (uint8_t)px, glyphs[ci], col, font, r, g, b, matH);
            }
        }
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Gap";
            case 2: return "Loop";
            case 3: return "Text";
            case 4: return "Font";
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8; // Speed
            case 1: return ParameterType::PARAM_UINT8; // Gap
            case 2: return ParameterType::PARAM_BOOL;  // Loop
            case 3: return ParameterType::PARAM_STRING; // Text
            case 4: return ParameterType::PARAM_ENUM;  // Font size
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 32; // Speed default (32 = halbe Geschwindigkeit)
            case 1: return 1;  // Gap default
            case 2: return 1;  // Loop default (true)
            case 3: return 0;  // Text default (empty)
            case 4: return 0;  // Font default (0 = 5×7)
            default: return 0;
        }
    }

    // Font enum: 0=5×7, 1=4×6, 2=3×5
    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return paramIndex == 4 ? kScrollFontCount : 0;
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 4) return nullptr;
        switch (enumValue)
        {
            case 0: return "5x7";
            case 1: return "4x6";
            case 2: return "3x5";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Animationsgeschwindigkeit (1-255)", "Speed: Animation speed (1-255)");
            case 1: return PARAM_DESC_DE_EN("Lücke: Pixel zwischen Zeichen (Standard 1)", "Gap: Pixels between characters (default 1)");
            case 2: return PARAM_DESC_DE_EN("Schleife: 0=Einmalig, 1=Endlosschleife", "Loop: 0=Once, 1=Loop");
            case 3: return PARAM_DESC_DE_EN("Text: Anzuzeigender Text (ETS max. 14 Zeichen, über KO bis 240)", "Text: Text to display (ETS max. 14 chars, up to 240 via KO)");
            case 4: return PARAM_DESC_DE_EN("Schrift: Zeichengröße 0=5×7, 1=4×6, 2=3×5 (kleinste)", "Font: Glyph size 0=5×7, 1=4×6, 2=3×5 (smallest)");
            default: return "";
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;   // Speed min
            case 1: return 0;   // Gap min
            case 2: return 0;   // Loop min (false)
            case 3: return 0;   // Text min
            case 4: return 0;   // Font min (5×7)
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // Gap max
            case 2: return 1;   // Loop max (true)
            case 3: return 14;  // Text max length (EffectConfig::effectText)
            case 4: return kScrollFontCount - 1; // Font max (3×5)
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;     // Speed
            case 1: return cfg.option1;   // Gap
            case 2: return cfg.feature1;  // Loop
            case 3: return (uint32_t)(uintptr_t)cfg.effectText; // Text pointer (per segment)
            case 4: return cfg.option2;   // Font
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = (uint8_t)value; break;     // Speed
            case 1: cfg.option1 = (uint8_t)value; break;   // Gap
            case 2: cfg.feature1 = (bool)value; break;     // Loop
            case 3: // Text — copy into per-segment buffer
            {
                const char* src = (const char*)(uintptr_t)value;
                strncpy(cfg.effectText, src ? src : "", sizeof(cfg.effectText) - 1);
                cfg.effectText[sizeof(cfg.effectText) - 1] = '\0';
                break;
            }
            case 4: // Font — clamp to valid range
                cfg.option2 = (uint8_t)(value < kScrollFontCount ? value : 0);
                break;
            default: break;
        }
    }
};
