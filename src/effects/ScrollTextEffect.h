/**
 * @file ScrollTextEffect.h
 * @brief Horizontal scrolling text for LED matrices (2D segments)
 *
 * Renders ASCII text using a built-in 5×7 pixel font and scrolls it
 * from right to left across the matrix.
 *
 * ETS parameters:
 *   Speed    (config.speed)   — scroll speed 1-255 (default 64)
 *   option1                  — gap between characters in pixels (default 1)
 *   feature1                 — wrap: 0=scroll once then stop, 1=loop
 *   primaryRGBW              — text colour (default white)
 *   Text     (config.effectText) — per-segment text (14 chars), set via ETS/Cue/Scene/KO
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include <Arduino.h>
#include <string.h>

// ============================================================================
// Minimal 5×7 ASCII font (printable chars 0x20..0x7E)
// Each character: 5 bytes, one byte per column (bit 0 = top row)
// ============================================================================
static const uint8_t kFont5x7[][5] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 0x20 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x40, 0x3C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x0C, 0x02, 0x0C, 0x00, 0x00}, // ~
};

static const uint8_t kFontWidth = 5;
static const uint8_t kFontHeight = 7;
static const uint8_t kFontFirst = 0x20; // first printable ASCII

// ============================================================================

class ScrollTextEffect : public Effect
{
  private:
    // Per-render state stored in EffectState:
    //   position = current scroll offset in pixels (0 = start at right edge)
    //   lastUpdate = millis of last pixel advance
    // Text lives per segment in EffectConfig::effectText (no shared singleton).

    // Draw one glyph column (0-4) of char c at matrix column x
    void drawColumn(Segment* seg, uint8_t x, uint8_t ch, uint8_t col,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t matH) const
    {
        if (ch < kFontFirst || ch > 0x7E) ch = 0x20; // replace unknown with space
        uint8_t colBits = pgm_read_byte(&kFont5x7[ch - kFontFirst][col]);
        uint8_t rows = matH < kFontHeight ? matH : kFontHeight;
        // Centre the 7-row glyph vertically in the matrix
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
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
        return EFFECT_DESC_DE_EN("Horizontale Laufschrift auf LED-Matrix (5×7 Font)",
                                 "Horizontal scrolling text on LED matrix (5×7 font)");
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
        uint8_t textLen = (uint8_t)strlen(text);
        if (textLen == 0) return;

        uint8_t matW = geo.width;
        uint8_t matH = geo.height;
        uint8_t gap = cfg.option1 ? cfg.option1 : 1;
        uint16_t speed = (uint16_t)cfg.speed + 1; // higher = faster
        bool loop = cfg.feature1;

        // Total pixel width of the whole text string
        uint16_t totalW = (uint16_t)textLen * (kFontWidth + gap);
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
        for (uint8_t ci = 0; ci < textLen; ci++)
        {
            int16_t charX = startX + (int16_t)ci * (kFontWidth + gap);
            for (uint8_t col = 0; col < kFontWidth; col++)
            {
                int16_t px = charX + col;
                if (px < 0 || px >= matW) continue;
                drawColumn(seg, (uint8_t)px, (uint8_t)text[ci], col, r, g, b, matH);
            }
        }
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Gap";
            case 2: return "Loop";
            case 3: return "Text";
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
            default: return 0;
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
            default: break;
        }
    }
};
