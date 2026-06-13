/**
 * @file Clock2DEffect.h
 * @brief Digital/binary clock display for LED matrices with optional date
 *
 * Uses openknx.time when valid; falls back to uptime (since boot) otherwise.
 * Content wider than the matrix scrolls ping-pong style (start -> end -> start),
 * never wrapping around.
 *
 * Parameters:
 *   0 ViewMode     — 0=Digital, 1=Binaer, 2=Auto-Wechsel (digital/binary alternating)
 *   1 ShowSeconds  — 0=HH:MM, 1=HH:MM:SS (binary: 4 vs 6 BCD columns / 2 vs 3 rows)
 *   2 BlinkColon   — 0=steady colon, 1=blink every second (digital only)
 *   3 ColourHue    — hue override (>0) or auto colour when primary is black
 *   4 DateMode     — 0=off, 1=alternate time/date, 2=date only
 *   5 DateFormat   — 0=TT.MM., 1=TT.MM.JJ, 2=WT TT.MM. (weekday)
 *   6 DateHue      — separate hue for the date (0=same as clock)
 *   7 SwitchSec    — alternation interval in seconds (time/date, digital/binary)
 *   8 ScrollSpeed  — scroll speed when content is wider than matrix (0=static)
 *   9 BinaryStyle  — 0=BCD columns (classic binary clock), 1=binary rows (H/M/S)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "ScrollTextEffect.h" // reuse font constants
#include "FastLEDMath.h"
#include "OpenKNX.h"
#include <stdio.h>

namespace Clock2DFont
{
    inline void drawChar(Segment* seg, int16_t xStart, char ch,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t matW, uint8_t matH)
    {
        if (ch < kFontFirst || ch > 0x7E) ch = ' ';
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
        uint8_t rows = matH < kFontHeight ? matH : kFontHeight;
        for (uint8_t col = 0; col < kFontWidth; col++)
        {
            int16_t px = xStart + col;
            if (px < 0 || px >= matW) continue;
            uint8_t bits = pgm_read_byte(&kFont5x7[ch - kFontFirst][col]);
            for (uint8_t row = 0; row < rows; row++)
            {
                bool on = (bits >> row) & 1;
                seg->setPixelXY((uint8_t)px, yOff + row,
                                on ? r : 0, on ? g : 0, on ? b : 0);
            }
        }
    }

    inline void drawColon(Segment* seg, int16_t xStart,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t matW, uint8_t matH)
    {
        if (xStart < 0 || xStart >= matW) return;
        uint8_t yOff = matH > kFontHeight ? (matH - kFontHeight) / 2 : 0;
        uint8_t dot1 = yOff + 2;
        uint8_t dot2 = yOff + 4;
        if (dot1 < matH) seg->setPixelXY((uint8_t)xStart, dot1, r, g, b);
        if (dot2 < matH) seg->setPixelXY((uint8_t)xStart, dot2, r, g, b);
    }

    // Width of a rendered string: chars are 5+1 px, ':' is 2 px (dots + gap)
    inline uint16_t textWidth(const char* s)
    {
        uint16_t w = 0;
        for (; *s; s++) w += (*s == ':') ? 2 : (uint16_t)(kFontWidth + 1);
        return w > 0 ? (uint16_t)(w - 1) : 0; // drop trailing gap
    }

    inline void drawString(Segment* seg, const char* s, int16_t x,
                           uint8_t r, uint8_t g, uint8_t b, bool colonVisible,
                           uint8_t matW, uint8_t matH)
    {
        for (; *s; s++)
        {
            if (*s == ':')
            {
                if (colonVisible) drawColon(seg, x, r, g, b, matW, matH);
                x += 2;
            }
            else
            {
                drawChar(seg, x, *s, r, g, b, matW, matH);
                x += kFontWidth + 1;
            }
            if (x >= matW) break;
        }
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
            "Uhr auf einer LED-Matrix: digital (HH:MM/HH:MM:SS), binär (BCD-Spalten oder Binärzeilen) oder im Wechsel. Optional mit Datumsanzeige (Wechsel oder dauerhaft, mit Wochentag). Zu breite Anzeigen scrollen automatisch bis zum Ende und zurück. Nutzt die KNX-Uhrzeit, ohne gültige Zeit läuft die Betriebszeit (Uptime).",
            "Clock on an LED matrix: digital (HH:MM/HH:MM:SS), binary (BCD columns or binary rows) or alternating. Optional date display (alternating or permanent, with weekday). Content wider than the matrix auto-scrolls to the end and back. Uses KNX time; falls back to uptime when no valid time is set.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 10; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "ViewMode";
            case 1: return "ShowSeconds";
            case 2: return "BlinkColon";
            case 3: return "ColourHue";
            case 4: return "DateMode";
            case 5: return "DateFormat";
            case 6: return "DateHue";
            case 7: return "SwitchSec";
            case 8: return "ScrollSpeed";
            case 9: return "BinaryStyle";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Anzeigemodus: 0=Digital, 1=Binär, 2=Auto-Wechsel zwischen Digital und Binär (Intervall siehe SwitchSec).",
                "View mode: 0=digital, 1=binary, 2=auto-alternate between digital and binary (interval: SwitchSec).");
            case 1: return PARAM_DESC_DE_EN(
                "Sekunden anzeigen: 0=HH:MM, 1=HH:MM:SS. Bei Binär: 4 statt 6 BCD-Spalten bzw. 2 statt 3 Zeilen.",
                "Show seconds: 0=HH:MM, 1=HH:MM:SS. Binary: 4 vs 6 BCD columns / 2 vs 3 rows.");
            case 2: return PARAM_DESC_DE_EN(
                "Doppelpunkt blinken (nur digital): 0=Doppelpunkt leuchtet dauerhaft, 1=Doppelpunkt blinkt sekündlich.",
                "Blink colon (digital only): 0=colon is always on, 1=colon blinks every second.");
            case 3: return PARAM_DESC_DE_EN(
                "Farb-Farbton der Uhrzeit: HSV-Farbton (0=rot/aus, 85=grün, 170=blau). Wert >0 übersteuert die Primärfarbe; bei 0 gilt die Primärfarbe (schwarz=rot).",
                "Clock colour hue: HSV hue (0=red/off, 85=green, 170=blue). Values >0 override the primary colour; at 0 the primary colour is used (black=red).");
            case 4: return PARAM_DESC_DE_EN(
                "Datumsanzeige: 0=Aus, 1=Wechsel zwischen Uhrzeit und Datum (Intervall siehe SwitchSec), 2=Nur Datum. Ohne gültige KNX-Zeit wird kein Datum angezeigt.",
                "Date display: 0=off, 1=alternate between time and date (interval: SwitchSec), 2=date only. Without valid KNX time no date is shown.");
            case 5: return PARAM_DESC_DE_EN(
                "Datumsformat: 0=TT.MM., 1=TT.MM.JJ, 2=Wochentag + TT.MM. (z.B. Sa 08.02.).",
                "Date format: 0=DD.MM., 1=DD.MM.YY, 2=weekday + DD.MM. (e.g. Sa 08.02.).");
            case 6: return PARAM_DESC_DE_EN(
                "Farb-Farbton des Datums: eigener HSV-Farbton für die Datumsanzeige. 0=gleiche Farbe wie die Uhrzeit.",
                "Date colour hue: separate HSV hue for the date display. 0=same colour as the clock.");
            case 7: return PARAM_DESC_DE_EN(
                "Wechselintervall in Sekunden für Zeit/Datum- und Digital/Binär-Wechsel (2-30 s).",
                "Alternation interval in seconds for time/date and digital/binary switching (2-30 s).");
            case 8: return PARAM_DESC_DE_EN(
                "Scrollgeschwindigkeit, wenn die Anzeige breiter als die Matrix ist: scrollt bis zum Ende und zurück (Ping-Pong). 0=statisch (Anfang sichtbar), 1=langsam bis 255=schnell.",
                "Scroll speed when content is wider than the matrix: scrolls to the end and back (ping-pong). 0=static (start visible), 1=slow to 255=fast.");
            case 9: return PARAM_DESC_DE_EN(
                "Binär-Darstellung: 0=BCD-Spalten (klassische Binäruhr, 1 Spalte je Ziffer), 1=Binär-Zeilen (je eine Zeile für Stunde/Minute/Sekunde).",
                "Binary style: 0=BCD columns (classic binary clock, one column per digit), 1=binary rows (one row each for hour/minute/second).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_ENUM;
            case 1: return ParameterType::PARAM_BOOL;
            case 2: return ParameterType::PARAM_BOOL;
            case 3: return ParameterType::PARAM_HUE;
            case 4: return ParameterType::PARAM_ENUM;
            case 5: return ParameterType::PARAM_ENUM;
            case 6: return ParameterType::PARAM_HUE;
            case 7: return ParameterType::PARAM_UINT8;
            case 8: return ParameterType::PARAM_UINT8;
            case 9: return ParameterType::PARAM_ENUM;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0;   // digital
            case 1: return 0;   // HH:MM
            case 2: return 1;   // blink colon
            case 3: return 0;   // primary colour / red
            case 4: return 0;   // date off
            case 5: return 0;   // TT.MM.
            case 6: return 0;   // date = clock colour
            case 7: return 5;   // 5 s interval
            case 8: return 60;  // moderate scroll
            case 9: return 0;   // BCD columns
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
            case 3: return 0;
            case 4: return 0;
            case 5: return 0;
            case 6: return 0;
            case 7: return 2;
            case 8: return 0;
            case 9: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 2;
            case 1: return 1;
            case 2: return 1;
            case 3: return 255;
            case 4: return 2;
            case 5: return 2;
            case 6: return 255;
            case 7: return 30;
            case 8: return 255;
            case 9: return 1;
            default: return 255;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex == 0)
        {
            switch (enumValue)
            {
                case 0: return "Digital";
                case 1: return "Binaer";
                case 2: return "Auto-Wechsel";
            }
        }
        if (paramIndex == 4)
        {
            switch (enumValue)
            {
                case 0: return "Aus";
                case 1: return "Zeit/Datum Wechsel";
                case 2: return "Nur Datum";
            }
        }
        if (paramIndex == 5)
        {
            switch (enumValue)
            {
                case 0: return "TT.MM.";
                case 1: return "TT.MM.JJ";
                case 2: return "WT TT.MM.";
            }
        }
        if (paramIndex == 9)
        {
            switch (enumValue)
            {
                case 0: return "BCD-Spalten";
                case 1: return "Binaer-Zeilen";
            }
        }
        return nullptr;
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        switch (paramIndex)
        {
            case 0: return 3;
            case 4: return 3;
            case 5: return 3;
            case 9: return 2;
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.option2;                        // ViewMode
            case 1: return cfg.feature1;                       // ShowSeconds
            case 2: return cfg.feature2;                       // BlinkColon
            case 3: return cfg.option1;                        // ColourHue
            case 4: return cfg.option3;                        // DateMode
            case 5: return cfg.legacyOption1 & 0xFF;           // DateFormat
            case 6: return (cfg.legacyOption1 >> 8) & 0xFF;    // DateHue
            case 7: return (cfg.legacyOption1 >> 16) & 0xFF;   // SwitchSec
            case 8: return (cfg.legacyOption1 >> 24) & 0xFF;   // ScrollSpeed
            case 9: return cfg.feature3;                       // BinaryStyle
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.option2  = static_cast<uint8_t>(value); break;
            case 1: cfg.feature1 = static_cast<bool>(value);    break;
            case 2: cfg.feature2 = static_cast<bool>(value);    break;
            case 3: cfg.option1  = static_cast<uint8_t>(value); break;
            case 4: cfg.option3  = static_cast<uint8_t>(value); break;
            case 5: cfg.legacyOption1 = (cfg.legacyOption1 & ~0x000000FFUL) | (value & 0xFF);         break;
            case 6: cfg.legacyOption1 = (cfg.legacyOption1 & ~0x0000FF00UL) | ((value & 0xFF) << 8);  break;
            case 7: cfg.legacyOption1 = (cfg.legacyOption1 & ~0x00FF0000UL) | ((value & 0xFF) << 16); break;
            case 8: cfg.legacyOption1 = (cfg.legacyOption1 & ~0xFF000000UL) | ((value & 0xFF) << 24); break;
            case 9: cfg.feature3 = static_cast<bool>(value);    break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override { (void)segment; (void)deltaTime; }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        (void)deltaTime;
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) return;

        auto& cfg = segment->getConfig();
        uint8_t viewMode    = cfg.option2;                       // 0=digital 1=binary 2=alternate
        bool showSeconds    = cfg.feature1;
        bool blinkColon     = cfg.feature2;
        uint8_t dateMode    = cfg.option3;                       // 0=off 1=alternate 2=only
        uint8_t dateFormat  = cfg.legacyOption1 & 0xFF;
        uint8_t dateHue     = (cfg.legacyOption1 >> 8) & 0xFF;
        uint8_t switchSec   = (cfg.legacyOption1 >> 16) & 0xFF;
        uint8_t scrollSpeed = (cfg.legacyOption1 >> 24) & 0xFF;
        bool binaryRows     = cfg.feature3;
        uint8_t matW = geo.width;
        uint8_t matH = geo.height;
        if (switchSec < 2) switchSec = 5;

        // Time source: KNX time if valid, otherwise fall back to uptime (mod 24 h)
        uint8_t hour, minute, second;
        uint8_t day = 1, month = 1, dow = 0;
        uint16_t year = 2000;
        bool timeValid = openknx.time.isValid();
        if (timeValid)
        {
            auto now = openknx.time.getLocalTime();
            hour = now.hour; minute = now.minute; second = now.second;
            day = now.day; month = now.month; year = now.year; dow = now.dayOfWeek;
        }
        else
        {
            uint32_t upSec = millis() / 1000;
            hour   = (upSec / 3600) % 24;
            minute = (upSec / 60) % 60;
            second = upSec % 60;
        }

        // Phase logic: which content is shown right now?
        uint32_t slot = (millis() / 1000UL) / switchSec;
        bool showDate = (dateMode == 2 && timeValid);
        bool binary   = (viewMode == 1);
        if (dateMode == 1 && timeValid)
        {
            if (viewMode == 2)
            {
                // 4-phase cycle: digital -> date -> binary -> date
                switch (slot & 3)
                {
                    case 1: case 3: showDate = true; break;
                    case 2: binary = true; break;
                    default: break;
                }
            }
            else
            {
                showDate = slot & 1;
            }
        }
        else if (viewMode == 2)
        {
            binary = slot & 1;
        }

        // Colour: hue parameter wins when set (>0); black primary also falls back to hue
        uint8_t r = cfg.r(), g = cfg.g(), b = cfg.b();
        if (cfg.option1 > 0 || (r == 0 && g == 0 && b == 0))
        {
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(cfg.option1, 255, 200);
            r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
        }

        segment->clear();

        if (showDate)
        {
            // Separate date hue (0 = same as clock)
            uint8_t dr = r, dg = g, db = b;
            if (dateHue > 0)
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(dateHue, 255, 200);
                dr = (rgb >> 16) & 0xFF; dg = (rgb >> 8) & 0xFF; db = rgb & 0xFF;
            }
            char buf[14];
            static const char* const kWeekdays[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
            switch (dateFormat)
            {
                case 1:
                    snprintf(buf, sizeof(buf), "%02u.%02u.%02u",
                             (unsigned)day, (unsigned)month, (unsigned)(year % 100));
                    break;
                case 2:
                    snprintf(buf, sizeof(buf), "%s %02u.%02u.",
                             kWeekdays[dow % 7], (unsigned)day, (unsigned)month);
                    break;
                default:
                    snprintf(buf, sizeof(buf), "%02u.%02u.", (unsigned)day, (unsigned)month);
                    break;
            }
            int16_t x = contentX(Clock2DFont::textWidth(buf), matW, scrollSpeed);
            Clock2DFont::drawString(segment, buf, x, dr, dg, db, true, matW, matH);
        }
        else if (binary)
        {
            if (binaryRows)
                renderBinaryRows(segment, hour, minute, second, showSeconds, r, g, b, matW, matH);
            else
                renderBinaryBCD(segment, hour, minute, second, showSeconds, r, g, b, matW, matH);
        }
        else
        {
            bool colonVisible = !(blinkColon && (second & 1));
            char buf[10];
            if (showSeconds)
                snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                         (unsigned)hour, (unsigned)minute, (unsigned)second);
            else
                snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)hour, (unsigned)minute);
            int16_t x = contentX(Clock2DFont::textWidth(buf), matW, scrollSpeed);
            Clock2DFont::drawString(segment, buf, x, r, g, b, colonVisible, matW, matH);
        }
    }

  private:
    /**
     * @brief X offset for text content: centered when it fits, ping-pong scroll otherwise.
     *
     * "Scroll to the end" semantics: content scrolls until its end is visible,
     * holds, then scrolls back — it never wraps around. Stateless (millis-based).
     */
    static int16_t contentX(uint16_t textW, uint8_t matW, uint8_t scrollSpeed)
    {
        if (textW <= matW) return (int16_t)((matW - textW) / 2); // fits: center
        if (scrollSpeed == 0) return 0;                          // static: show start
        uint16_t stepMs = 260 - scrollSpeed;                     // 1 -> 259 ms/px, 255 -> 15 ms/px
        if (stepMs < 15) stepMs = 15;
        uint16_t maxShift = textW - matW;
        const uint32_t holdMs = 1200;
        uint32_t sweep = (uint32_t)maxShift * stepMs;
        uint32_t period = 2 * holdMs + 2 * sweep;
        uint32_t ph = millis() % period;
        uint32_t shift;
        if (ph < holdMs) shift = 0;                              // hold at start
        else if (ph < holdMs + sweep) shift = (ph - holdMs) / stepMs;
        else if (ph < 2 * holdMs + sweep) shift = maxShift;      // hold at end
        else shift = maxShift - (ph - 2 * holdMs - sweep) / stepMs;
        if (shift > maxShift) shift = maxShift;
        return -(int16_t)shift;
    }

    /// Filled cell block with 1 px gap (when the cell is large enough)
    static void fillCell(Segment* seg, uint8_t x0, uint8_t y0, uint8_t cw, uint8_t ch,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t matW, uint8_t matH)
    {
        uint8_t bw = cw > 1 ? cw - 1 : 1;
        uint8_t bh = ch > 1 ? ch - 1 : 1;
        for (uint8_t dx = 0; dx < bw; dx++)
            for (uint8_t dy = 0; dy < bh; dy++)
            {
                uint8_t px = x0 + dx, py = y0 + dy;
                if (px < matW && py < matH) seg->setPixelXY(px, py, r, g, b);
            }
    }

    /**
     * @brief Classic binary clock: one BCD column per digit, LSB at the bottom.
     *
     * Columns: H10 H1 M10 M1 [S10 S1]; impossible bits stay dark,
     * possible-but-off bits are drawn dimmed so the grid stays readable.
     */
    static void renderBinaryBCD(Segment* seg, uint8_t h, uint8_t m, uint8_t s, bool showSec,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t matW, uint8_t matH)
    {
        const uint8_t digits[6]    = {(uint8_t)(h / 10), (uint8_t)(h % 10),
                                      (uint8_t)(m / 10), (uint8_t)(m % 10),
                                      (uint8_t)(s / 10), (uint8_t)(s % 10)};
        const uint8_t digitBits[6] = {2, 4, 3, 4, 3, 4}; // valid bits per digit
        uint8_t nCols = showSec ? 6 : 4;
        uint8_t cw = matW / nCols;
        uint8_t ch = matH / 4;
        if (cw == 0) cw = 1;
        if (ch == 0) ch = 1;
        uint8_t gx = (matW > (uint8_t)(cw * nCols)) ? (matW - cw * nCols) / 2 : 0;
        uint8_t gy = (matH > (uint8_t)(ch * 4)) ? (matH - ch * 4) / 2 : 0;
        uint8_t dimR = r / 10, dimG = g / 10, dimB = b / 10;

        for (uint8_t col = 0; col < nCols; col++)
        {
            for (uint8_t bit = 0; bit < 4; bit++)
            {
                if (bit >= digitBits[col]) continue; // impossible bit: keep dark
                bool on = (digits[col] >> bit) & 1;
                uint8_t x0 = gx + col * cw;
                uint8_t y0 = gy + (uint8_t)(3 - bit) * ch; // LSB at the bottom
                fillCell(seg, x0, y0, cw, ch,
                         on ? r : dimR, on ? g : dimG, on ? b : dimB, matW, matH);
            }
        }
    }

    /**
     * @brief Binary rows: one row each for hour/minute/[second], 6 bits, MSB left.
     */
    static void renderBinaryRows(Segment* seg, uint8_t h, uint8_t m, uint8_t s, bool showSec,
                                 uint8_t r, uint8_t g, uint8_t b, uint8_t matW, uint8_t matH)
    {
        const uint8_t values[3] = {h, m, s};
        uint8_t nRows = showSec ? 3 : 2;
        uint8_t rh = matH / nRows;
        uint8_t bw = matW / 6;
        if (rh == 0) rh = 1;
        if (bw == 0) bw = 1;
        uint8_t gx = (matW > (uint8_t)(bw * 6)) ? (matW - bw * 6) / 2 : 0;
        uint8_t gy = (matH > (uint8_t)(rh * nRows)) ? (matH - rh * nRows) / 2 : 0;
        uint8_t dimR = r / 10, dimG = g / 10, dimB = b / 10;

        for (uint8_t row = 0; row < nRows; row++)
        {
            for (uint8_t bit = 0; bit < 6; bit++)
            {
                bool on = (values[row] >> (5 - bit)) & 1; // MSB left
                uint8_t x0 = gx + bit * bw;
                uint8_t y0 = gy + row * rh;
                fillCell(seg, x0, y0, bw, rh,
                         on ? r : dimR, on ? g : dimG, on ? b : dimB, matW, matH);
            }
        }
    }
};
