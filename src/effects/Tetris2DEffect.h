/**
 * @file Tetris2DEffect.h
 * @brief Tetris-style falling blocks effect for 2D LED matrices
 *
 * Classic Tetris: all 7 Tetrominoes with SRS wall-kick rotation, line clearing
 * with flash animation, level speed-up, ghost piece, and game-over sweep reset.
 *
 * Parameters:
 *   0 Speed         — drop speed 1-255 (default: 128; higher = faster)
 *   1 BgBrightness  — background grid dot brightness 0-64 (default: 8; 0=off)
 *   2 GhostPiece    — show ghost piece: 0=off, 1=on (default: 1)
 *   3 ColorMode     — piece colors: 0=per-shape, 1=random, 2=rainbow (default: 0)
 *   4 FlashLines    — flash cleared lines: 0=off, 1=on (default: 1)
 *   5 AutoPlay      — self-playing demo AI: 0=off (manual), 1=beginner, 2=normal, 3=expert (default: 3)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include <string.h>

class Tetris2DEffect : public Effect
{
  private:
    // Grid storage maximum. The active playfield (_pw/_ph) is clamped to the
    // actual matrix size at runtime so the effect uses the full matrix width
    // (blocks stay 1 pixel each — never stretched) and also works on smaller matrices.
    static constexpr uint8_t MAXW = 32;
    static constexpr uint8_t MAXH = 32;

    // Grid: 0=empty, 1-7=locked piece (hue-index+1), 255=flash
    uint8_t _grid[MAXW][MAXH];

    // 7 pieces × 4 rotations × 4 blocks × (dx,dy)
    static constexpr int8_t PIECES[7][4][4][2] = {
        // I
        {{{-1,0},{0,0},{1,0},{2,0}},{{1,-1},{1,0},{1,1},{1,2}},
         {{-1,1},{0,1},{1,1},{2,1}},{{0,-1},{0,0},{0,1},{0,2}}},
        // O
        {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},
         {{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}},
        // T
        {{{-1,0},{0,0},{1,0},{0,1}},{{0,-1},{0,0},{1,0},{0,1}},
         {{-1,0},{0,0},{1,0},{0,-1}},{{0,-1},{0,0},{-1,0},{0,1}}},
        // S
        {{{0,0},{1,0},{-1,1},{0,1}},{{0,-1},{0,0},{1,0},{1,1}},
         {{0,0},{1,0},{-1,1},{0,1}},{{0,-1},{0,0},{1,0},{1,1}}},
        // Z
        {{{-1,0},{0,0},{0,1},{1,1}},{{1,-1},{0,0},{1,0},{0,1}},
         {{-1,0},{0,0},{0,1},{1,1}},{{1,-1},{0,0},{1,0},{0,1}}},
        // J
        {{{-1,0},{0,0},{1,0},{-1,1}},{{0,-1},{0,0},{0,1},{1,-1}},
         {{-1,0},{0,0},{1,0},{1,-1}},{{0,-1},{0,0},{0,1},{-1,1}}},
        // L
        {{{-1,0},{0,0},{1,0},{1,1}},{{0,-1},{0,0},{0,1},{1,1}},
         {{-1,0},{0,0},{1,0},{-1,-1}},{{0,-1},{0,0},{0,1},{-1,-1}}}
    };

    // Classic piece hues: I=cyan, O=yellow, T=purple, S=green, Z=red, J=blue, L=orange
    static constexpr uint8_t PIECE_HUES[7] = {128, 43, 192, 96, 0, 160, 21};

    uint8_t  _type;
    uint8_t  _rot;
    int8_t   _px, _py;
    uint32_t _elapsed;
    uint32_t _flashTimer;
    uint8_t  _flashMask[MAXH];
    bool     _flashing;
    bool     _gameOver;
    uint32_t _gameOverTimer;
    uint16_t _linesCleared;
    uint8_t  _rainbowHue;
    uint8_t  _pw, _ph; ///< active playfield size (clamped to matrix, max MAXW×MAXH)
    uint8_t  _ox, _oy; ///< render offset to center the playfield on the matrix

    // Auto-play AI state
    uint8_t  _aiTargetRot; ///< target rotation the AI steers towards
    int8_t   _aiTargetX;   ///< target x column the AI steers towards
    bool     _aiPlanned;   ///< whether a target has been computed for the current piece
    uint32_t _aiMoveTimer; ///< accumulates time between visible AI moves
    uint8_t  _aiLevel;     ///< AI strength 0=beginner, 1=normal, 2=expert
    static constexpr uint32_t AI_STEP_MS = 60; ///< one visible AI action every 60 ms

    static uint32_t mapLinear(uint32_t v, uint32_t inMin, uint32_t inMax,
                               uint32_t outMin, uint32_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (v <= inMin) return outMin;
        if (v >= inMax) return outMax;
        const uint32_t span = inMax - inMin;
        if (outMax >= outMin)
            return outMin + ((v - inMin) * (outMax - outMin) + span / 2) / span;
        return outMin - ((v - inMin) * (outMin - outMax) + span / 2) / span;
    }

    bool cellFree(int8_t x, int8_t y) const
    {
        if (x < 0 || x >= _pw || y >= _ph) return false;
        if (y < 0) return true;
        return _grid[x][y] == 0;
    }

    bool pieceCanFit(uint8_t type, uint8_t rot, int8_t px, int8_t py) const
    {
        for (uint8_t b = 0; b < 4; b++)
        {
            int8_t bx = px + PIECES[type][rot][b][0];
            int8_t by = py + PIECES[type][rot][b][1];
            if (!cellFree(bx, by)) return false;
        }
        return true;
    }

    void lockPiece()
    {
        uint8_t hue = static_cast<uint8_t>(_type + 1);
        for (uint8_t b = 0; b < 4; b++)
        {
            int8_t bx = _px + PIECES[_type][_rot][b][0];
            int8_t by = _py + PIECES[_type][_rot][b][1];
            if (bx >= 0 && bx < _pw && by >= 0 && by < _ph)
                _grid[bx][by] = hue;
        }
    }

    uint8_t findFullLines()
    {
        uint8_t count = 0;
        memset(_flashMask, 0, sizeof(_flashMask));
        for (uint8_t y = 0; y < _ph; y++)
        {
            bool full = true;
            for (uint8_t x = 0; x < _pw; x++)
                if (_grid[x][y] == 0) { full = false; break; }
            if (full) { _flashMask[y] = 1; count++; }
        }
        return count;
    }

    void clearFullLines()
    {
        // Compact the board downward: copy every NON-cleared row to the bottom, then
        // blank the rows left at the top. Single bottom-up pass, guaranteed to
        // terminate.
        //
        // NOTE: the previous in-place version shifted rows with a `y++` re-check but
        // never cleared/shifted _flashMask[y] — so a flagged row kept matching and the
        // loop span forever. On a wide (e.g. 32-col) board a full line is rare, so the
        // hang only surfaced after minutes of play → looked like a random watchdog reboot
        // mid-Tetris. (See clearFullLines infinite-loop bug.)
        int8_t dst = _ph - 1;
        for (int8_t src = _ph - 1; src >= 0; src--)
        {
            if (_flashMask[src]) continue; // drop cleared rows
            if (dst != src)
                for (uint8_t x = 0; x < _pw; x++) _grid[x][dst] = _grid[x][src];
            dst--;
        }
        for (; dst >= 0; dst--)
            for (uint8_t x = 0; x < _pw; x++) _grid[x][dst] = 0;
        memset(_flashMask, 0, sizeof(_flashMask));
    }

    void spawnPiece()
    {
        _type = FastLEDMath::random8(7);
        _rot  = 0;
        _px   = _pw / 2 - 1;
        _py   = 0;
        _aiPlanned = false; // force the AI to plan a fresh target for this piece
        if (!pieceCanFit(_type, _rot, _px, _py))
        {
            _gameOver = true;
            _gameOverTimer = 0;
        }
    }

    void tryRotate()
    {
        uint8_t newRot = (_rot + 1) & 3;
        static constexpr int8_t kicks[] = {0, -1, 1, -2, 2};
        for (uint8_t k = 0; k < 5; k++)
        {
            if (pieceCanFit(_type, newRot, _px + kicks[k], _py))
            {
                _rot = newRot;
                _px += kicks[k];
                return;
            }
        }
    }

    int8_t ghostDropY() const
    {
        int8_t gy = _py;
        while (pieceCanFit(_type, _rot, _px, gy + 1)) gy++;
        return gy;
    }

    // ── Auto-play AI ──────────────────────────────────────────────────────────
    // Drop the given piece/rotation/column onto the CURRENT locked grid and rate
    // the resulting board. Higher score = better. Classic El-Tetris heuristic
    // (scaled to integers): reward cleared lines, punish height, holes, bumpiness.
    int32_t scorePlacement(uint8_t type, uint8_t rot, int8_t px) const
    {
        // Resting Y: drop until the next row would collide.
        int8_t gy = -4;
        while (pieceCanFit(type, rot, px, gy + 1)) gy++;
        if (!pieceCanFit(type, rot, px, gy)) return INT32_MIN; // column not reachable

        // Build a scratch board = current grid + this piece placed at (px, gy).
        static uint8_t tmp[MAXW][MAXH];
        for (uint8_t x = 0; x < _pw; x++)
            for (uint8_t y = 0; y < _ph; y++)
                tmp[x][y] = _grid[x][y];
        for (uint8_t b = 0; b < 4; b++)
        {
            int8_t bx = px + PIECES[type][rot][b][0];
            int8_t by = gy + PIECES[type][rot][b][1];
            if (bx >= 0 && bx < _pw && by >= 0 && by < _ph) tmp[bx][by] = 1;
            else if (by < 0) return INT32_MIN; // sticks out the top → game over move
        }

        // Column heights + holes.
        int32_t aggHeight = 0, holes = 0;
        uint8_t colTop[MAXW];
        for (uint8_t x = 0; x < _pw; x++)
        {
            int8_t top = -1;
            for (uint8_t y = 0; y < _ph; y++)
                if (tmp[x][y]) { top = y; break; }
            uint8_t h = (top < 0) ? 0 : static_cast<uint8_t>(_ph - top);
            colTop[x] = h;
            aggHeight += h;
            if (top >= 0)
                for (uint8_t y = top + 1; y < _ph; y++)
                    if (!tmp[x][y]) holes++;
        }

        // Bumpiness (sum of |height diffs| between neighbours).
        int32_t bumpiness = 0;
        for (uint8_t x = 0; x + 1 < _pw; x++)
        {
            int32_t d = static_cast<int32_t>(colTop[x]) - static_cast<int32_t>(colTop[x + 1]);
            bumpiness += (d < 0) ? -d : d;
        }

        // Completed lines.
        int32_t lines = 0;
        for (uint8_t y = 0; y < _ph; y++)
        {
            bool full = true;
            for (uint8_t x = 0; x < _pw; x++)
                if (!tmp[x][y]) { full = false; break; }
            if (full) lines++;
        }

        return 76 * lines - 51 * aggHeight - 36 * holes - 18 * bumpiness;
    }

    void computeBestMove()
    {
        int32_t best = INT32_MIN;
        _aiTargetRot = _rot;
        _aiTargetX   = _px;
        for (uint8_t rot = 0; rot < 4; rot++)
        {
            for (int8_t px = -1; px <= static_cast<int8_t>(_pw); px++)
            {
                int32_t s = scorePlacement(_type, rot, px);
                if (s > best)
                {
                    best = s;
                    _aiTargetRot = rot;
                    _aiTargetX   = px;
                }
            }
        }
        // Lower AI levels deliberately make mistakes: with some probability,
        // discard the optimal target and pick a random reachable placement.
        uint8_t mistakePct = (_aiLevel >= 2) ? 0 : (_aiLevel == 1 ? 12 : 35);
        if (mistakePct > 0 && FastLEDMath::random8(100) < mistakePct)
        {
            for (uint8_t tries = 0; tries < 16; tries++)
            {
                uint8_t rot = FastLEDMath::random8(4);
                int8_t  px  = static_cast<int8_t>(FastLEDMath::random8(_pw));
                if (scorePlacement(_type, rot, px) > INT32_MIN)
                {
                    _aiTargetRot = rot;
                    _aiTargetX   = px;
                    break;
                }
            }
        }
        _aiPlanned = true;
    }

    // Apply one visible AI action: rotate towards target, then shift, then soft-drop.
    void aiStep()
    {
        if (_rot != _aiTargetRot)        { tryRotate(); return; }
        if (_px < _aiTargetX)            { if (pieceCanFit(_type, _rot, _px + 1, _py)) { _px++; return; } }
        else if (_px > _aiTargetX)       { if (pieceCanFit(_type, _rot, _px - 1, _py)) { _px--; return; } }
        // Aligned (or blocked): soft-drop to keep the demo snappy.
        if (pieceCanFit(_type, _rot, _px, _py + 1)) _py++;
    }

    uint8_t getPieceHue(uint8_t type, uint8_t colorMode) const
    {
        if (colorMode == 1) return FastLEDMath::random8();
        if (colorMode == 2) return _rainbowHue;
        return PIECE_HUES[type];
    }

    void drawXY(Segment* seg, int8_t x, int8_t y, uint8_t hue, uint8_t val, uint8_t sat = 255) const
    {
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, sat, val);
        seg->setPixelXY(x + _ox, y + _oy, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    void renderGrid(Segment* seg, uint8_t bgBright, uint8_t colorMode) const
    {
        for (uint8_t y = 0; y < _ph; y++)
        {
            for (uint8_t x = 0; x < _pw; x++)
            {
                uint8_t cell = _grid[x][y];
                if (cell == 0)
                {
                    if (bgBright > 0 && (x % 2 == 0) && (y % 2 == 0))
                        drawXY(seg, x, y, 0, bgBright, 0);
                    else
                        seg->setPixelXY(x + _ox, y + _oy, 0, 0, 0);
                }
                else
                {
                    uint8_t idx = cell - 1;
                    uint8_t hue;
                    if (colorMode == 2)      hue = static_cast<uint8_t>(x * 25 + y * 13);
                    else if (colorMode == 1) hue = static_cast<uint8_t>(idx * 36 + y * 7);
                    else                     hue = PIECE_HUES[idx < 7 ? idx : 0];
                    drawXY(seg, x, y, hue, 220);
                }
            }
        }
    }

  public:
    Tetris2DEffect()
        : _type(0), _rot(0), _px(0), _py(0),
          _elapsed(0), _flashTimer(0), _flashing(false),
          _gameOver(false), _gameOverTimer(0),
          _linesCleared(0), _rainbowHue(0),
          _pw(MAXW), _ph(MAXH), _ox(0), _oy(0),
          _aiTargetRot(0), _aiTargetX(0), _aiPlanned(false), _aiMoveTimer(0),
          _aiLevel(2)
    {
        memset(_grid, 0, sizeof(_grid));
        memset(_flashMask, 0, sizeof(_flashMask));
        spawnPiece();
    }

    void resetGame(Segment* seg = nullptr)
    {
        (void)seg;
        memset(_grid, 0, sizeof(_grid));
        memset(_flashMask, 0, sizeof(_flashMask));
        _gameOver = _flashing = false;
        _gameOverTimer = _flashTimer = _elapsed = _linesCleared = 0;
        _rainbowHue = 0;
        _aiPlanned = false;
        _aiMoveTimer = 0;
        spawnPiece();
    }

    // ── Effect interface ──────────────────────────────────────────────────────
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Tetris 2D", "Tetris 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Klassisches Tetris auf einer 2D LED-Matrix. Alle 7 Tetrominoes, SRS-Rotation, Geist-Stein, Linien-Flash.",
            "Classic Tetris on a 2D LED matrix. All 7 Tetrominoes, SRS rotation, ghost piece, line-clear flash.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 6; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "BgBrightness";
            case 2: return "GhostPiece";
            case 3: return "ColorMode";
            case 4: return "FlashLines";
            case 5: return "AutoPlay";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Fallgeschwindigkeit (1=langsam, 255=schnell).", "Drop speed (1=slow, 255=fast).");
            case 1: return PARAM_DESC_DE_EN("Hintergrundgitter-Helligkeit (0=aus, max 64).", "Background grid brightness (0=off, max 64).");
            case 2: return PARAM_DESC_DE_EN("Geist-Stein: 0=aus, 1=an.", "Ghost piece: 0=off, 1=on.");
            case 3: return PARAM_DESC_DE_EN("Farbmodus: 0=Form-Farbe, 1=Zufall, 2=Regenbogen.", "Color mode: 0=per shape, 1=random, 2=rainbow.");
            case 4: return PARAM_DESC_DE_EN("Volle Reihen aufblitzen: 0=aus, 1=an.", "Flash cleared lines: 0=off, 1=on.");
            case 5: return PARAM_DESC_DE_EN("Selbstspielende KI: 0=aus (manuell), 1=Anfänger, 2=Normal, 3=Profi.", "Self-playing AI: 0=off (manual), 1=beginner, 2=normal, 3=expert.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_BOOL;
            case 3: return ParameterType::PARAM_ENUM; // ColorMode
            case 4: return ParameterType::PARAM_BOOL;
            case 5: return ParameterType::PARAM_ENUM; // AutoPlay
            default: return ParameterType::PARAM_UINT8; // 0: Speed, 1: BgBrightness are uint8
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex == 3)
        {
            switch (enumValue)
            {
                case 0: return "Form-Farbe";
                case 1: return "Zufall";
                case 2: return "Regenbogen";
                default: return nullptr;
            }
        }
        if (paramIndex == 5)
        {
            switch (enumValue)
            {
                case 0: return "Aus (manuell)";
                case 1: return "Anfaenger";
                case 2: return "Normal";
                case 3: return "Profi";
                default: return nullptr;
            }
        }
        return nullptr;
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        if (paramIndex == 3) return 3;
        if (paramIndex == 5) return 4;
        return 0;
    }

    uint32_t getParameterMin(uint8_t index) const override {
        switch (index)
        {
            case 0: return 1;   // Speed
            case 1: return 0;   // BgBrightness
            case 2: return 0;   // GhostPiece
            case 3: return 0;   // ColorMode
            case 4: return 0;   // FlashLines
            case 5: return 0;   // AutoPlay
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 64;
            case 2: return 1;
            case 3: return 2;
            case 4: return 1;
            case 5: return 3;
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128;
            case 1: return 8;
            case 2: return 1;
            case 3: return 0;
            case 4: return 1;
            case 5: return 3;
        }
        return 0;
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return getParameterDefault(index);
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;
            case 1: return cfg.option1;
            case 2: return cfg.option2;
            case 3: return cfg.option3;
            case 4: return cfg.feature1;
            case 5: return cfg.legacyOption1 & 0xFF;
        }
        return 0;
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
            case 3: cfg.option3 = static_cast<uint8_t>(value); break;
            case 4: cfg.feature1 = static_cast<bool>(value); break;
            case 5: cfg.legacyOption1 = (cfg.legacyOption1 & ~0xFFu) | (value & 0xFF); break;
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override { (void)segment; (void)deltaTime; }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        if (segment->getGeometry().is1D()) return;

        // Adapt the playfield to the actual matrix (clamped to the grid storage maximum),
        // so Tetris also runs on matrices smaller than 10×20 instead of staying black.
        uint16_t mw = segment->getRenderWidth();
        uint16_t mh = segment->getRenderHeight();
        if (mw == 0 || mh == 0) return;
        uint8_t pw = static_cast<uint8_t>(mw < MAXW ? mw : MAXW);
        uint8_t ph = static_cast<uint8_t>(mh < MAXH ? mh : MAXH);
        if (pw < 4 || ph < 4) return; // too small for a playable board
        if (pw != _pw || ph != _ph)
        {
            _pw = pw;
            _ph = ph;
            resetGame();
        }
        // Center the playfield horizontally/vertically on the matrix.
        _ox = static_cast<uint8_t>((mw - _pw) / 2);
        _oy = static_cast<uint8_t>((mh - _ph) / 2);

        // Blank the whole matrix so the centered margins stay black.
        for (uint16_t yy = 0; yy < mh; yy++)
            for (uint16_t xx = 0; xx < mw; xx++)
                segment->setPixelXY(xx, yy, 0, 0, 0);

        const auto& cfg   = segment->getConfig();
        const uint8_t spd       = cfg.speed   ? cfg.speed   : 128;
        const uint8_t bgBright  = cfg.option1;
        const bool    showGhost = cfg.option2 != 0;
        const uint8_t colorMode = cfg.option3 < 3 ? cfg.option3 : 0;
        const bool    flashLines = cfg.feature1;
        // AutoPlay parameter doubles as AI mode: 0=off (manual), 1=beginner, 2=normal, 3=expert.
        const uint8_t autoMode   = static_cast<uint8_t>(cfg.legacyOption1 & 0xFF);
        const bool    autoPlay   = autoMode != 0;

        // Drop interval with level speed-up
        uint32_t stepMs = mapLinear(spd, 1, 255, 2000, 50);
        uint16_t level  = _linesCleared / 10;
        if (stepMs > 30u * level) stepMs -= 30u * level; else stepMs = 50;

        // ── Flash animation ───────────────────────────────────────────────────
        if (_flashing)
        {
            _flashTimer += deltaTime;
            // Draw the static board ONCE, then overlay only the flashing rows.
            // (The old code called renderGrid() per non-flashing cell → O((pw*ph)^2)
            // ≈ 262k setPixel/frame on a 32×16 board → loop stall / watchdog reboot
            // that looked like the game "restarting" on every line clear.)
            renderGrid(segment, bgBright, colorMode);
            uint8_t v = (_flashTimer % 120 < 60) ? 255 : 0;
            for (uint8_t y = 0; y < _ph; y++)
                if (_flashMask[y])
                    for (uint8_t x = 0; x < _pw; x++)
                        segment->setPixelXY(x + _ox, y + _oy, v, v, v);
            if (_flashTimer >= 360)
            {
                _flashing = false;
                clearFullLines();
                if (!_gameOver) spawnPiece();
            }
            return;
        }

        // ── Game over sweep ───────────────────────────────────────────────────
        if (_gameOver)
        {
            _gameOverTimer += deltaTime;
            uint8_t sweepY = static_cast<uint8_t>((_gameOverTimer / 35) % (_ph + 8));
            for (uint8_t y = 0; y < _ph; y++)
                for (uint8_t x = 0; x < _pw; x++)
                {
                    if (y == sweepY % _ph)
                        segment->setPixelXY(x + _ox, y + _oy, 255, 0, 0);
                    else if (_grid[x][y])
                    {
                        uint8_t hue = PIECE_HUES[(_grid[x][y] - 1) < 7 ? (_grid[x][y] - 1) : 0];
                        drawXY(segment, x, y, hue, 100);
                    }
                    else
                        segment->setPixelXY(x + _ox, y + _oy, 0, 0, 0);
                }
            if (_gameOverTimer >= static_cast<uint32_t>((_ph + 8) * 35 + 400))
                resetGame();
            return;
        }

        // ── Auto-play AI ──────────────────────────────────────────────────────
        // Plan the best landing for the current piece and steer it there with
        // visible, timed moves so it looks like a real player.
        if (autoPlay)
        {
            // Map AutoPlay mode 1/2/3 → internal AI level 0/1/2 (beginner/normal/expert).
            _aiLevel = static_cast<uint8_t>(autoMode - 1);
            if (!_aiPlanned) computeBestMove();
            _aiMoveTimer += deltaTime;
            while (_aiMoveTimer >= AI_STEP_MS)
            {
                _aiMoveTimer -= AI_STEP_MS;
                aiStep();
            }
        }

        // ── Gravity ───────────────────────────────────────────────────────────
        _elapsed += deltaTime;
        while (_elapsed >= stepMs)
        {
            _elapsed -= stepMs;
            if (pieceCanFit(_type, _rot, _px, _py + 1))
            {
                _py++;
            }
            else
            {
                lockPiece();
                uint8_t lines = findFullLines();
                _linesCleared += lines;
                if (lines > 0 && flashLines)
                {
                    _flashing   = true;
                    _flashTimer = 0;
                }
                else
                {
                    if (lines > 0) clearFullLines();
                    if (!_gameOver) spawnPiece();
                }
            }
        }

        // ── Render ────────────────────────────────────────────────────────────
        renderGrid(segment, bgBright, colorMode);

        if (showGhost)
        {
            int8_t gy = ghostDropY();
            if (gy != _py)
            {
                uint8_t hue = getPieceHue(_type, colorMode);
                for (uint8_t b = 0; b < 4; b++)
                {
                    int8_t bx = _px + PIECES[_type][_rot][b][0];
                    int8_t by = gy  + PIECES[_type][_rot][b][1];
                    if (bx >= 0 && bx < _pw && by >= 0 && by < _ph)
                        drawXY(segment, bx, by, hue, 55, 180);
                }
            }
        }

        {
            uint8_t hue = getPieceHue(_type, colorMode);
            for (uint8_t b = 0; b < 4; b++)
            {
                int8_t bx = _px + PIECES[_type][_rot][b][0];
                int8_t by = _py + PIECES[_type][_rot][b][1];
                if (bx >= 0 && bx < _pw && by >= 0 && by < _ph)
                    drawXY(segment, bx, by, hue, 255);
            }
        }

        _rainbowHue++;
    }

    // Console helpers
    void cmdLeft()   { if (!_gameOver && !_flashing && pieceCanFit(_type, _rot, _px - 1, _py)) _px--; }
    void cmdRight()  { if (!_gameOver && !_flashing && pieceCanFit(_type, _rot, _px + 1, _py)) _px++; }
    void cmdDown()   { if (!_gameOver && !_flashing && pieceCanFit(_type, _rot, _px, _py + 1)) _py++; }
    void cmdRotate() { if (!_gameOver && !_flashing) tryRotate(); }
    void cmdReset()  { resetGame(); }
};

