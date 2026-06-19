/**
 * @file GameOfLife2DEffect.h
 * @brief Conway's Game of Life on a 2D LED matrix
 *
 * Cellular automaton (rules B3/S23) on a toroidal (wrap-around) grid.
 * The board is seeded randomly and re-seeds automatically when the
 * population dies out or the pattern stagnates (still life / period-2
 * oscillator), so the matrix never gets stuck on a dead image.
 *
 * Parameters:
 *   0 Speed        — generation speed 1-255 (default: 64)
 *   1 Hue          — base colour hue 0-255 (default: 96 = green)
 *   2 ColorMode    — 0=uniform, 1=age heat, 2=position rainbow (default: 1)
 *   3 SpawnDensity — initial fill density in percent 5-95 (default: 30)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include <string.h>

class GameOfLife2DEffect : public Effect
{
  private:
    uint32_t _lastUpdate = 0; ///< delta-time accumulator (ms)

    static constexpr size_t MAX_GRID_SIZE = 1024; ///< 32x32 max

    uint8_t _cur[MAX_GRID_SIZE] = {0};  ///< current generation (1=alive)
    uint8_t _age[MAX_GRID_SIZE] = {0};  ///< consecutive generations a cell has been alive

    uint8_t  _gridWidth = 0;
    uint8_t  _gridHeight = 0;
    bool     _seeded = false;

    // Stagnation detection: re-seed when the board no longer changes.
    uint32_t _hashPrev1 = 0;
    uint32_t _hashPrev2 = 0;
    uint16_t _stagnantGens = 0;
    uint8_t  _hueShift = 0; ///< slow hue rotation for the colour modes

    static constexpr uint16_t STAGNATION_LIMIT = 30; ///< gens before forced re-seed

    // Supports inverted output ranges (outMax < outMin) — plain unsigned math
    // underflows there and produces garbage step times.
    static uint32_t mapLinear(uint32_t value, uint32_t inMin, uint32_t inMax, uint32_t outMin, uint32_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (value <= inMin) return outMin;
        if (value >= inMax) return outMax;
        const uint32_t span = inMax - inMin;
        if (outMax >= outMin)
            return outMin + ((value - inMin) * (outMax - outMin)) / span;
        return outMin - ((value - inMin) * (outMin - outMax)) / span;
    }

    inline size_t idx(uint8_t x, uint8_t y) const
    {
        return static_cast<size_t>(y) * _gridWidth + x;
    }

    void seed(const EffectConfig& cfg)
    {
        uint8_t density = cfg.option3;
        if (density < 5) density = 5;
        if (density > 95) density = 95;

        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                const bool alive = FastLEDMath::random16(100) < density;
                _cur[idx(x, y)] = alive ? 1 : 0;
                _age[idx(x, y)] = alive ? 1 : 0;
            }
        }

        _stagnantGens = 0;
        _hashPrev1 = 0;
        _hashPrev2 = 0;
        _seeded = true;
    }

    void resetState(Segment* segment)
    {
        const auto& geo = segment->getGeometry();
        _gridWidth = geo.width;
        _gridHeight = geo.height;
        _lastUpdate = 0;

        if (_gridWidth < 2 || _gridHeight < 2 ||
            (static_cast<size_t>(_gridWidth) * _gridHeight) > MAX_GRID_SIZE)
        {
            _seeded = false;
            return;
        }

        seed(segment->getConfig());
    }

    // Count live neighbours with toroidal (wrap-around) edges.
    uint8_t countNeighbours(uint8_t x, uint8_t y) const
    {
        uint8_t n = 0;
        for (int8_t dy = -1; dy <= 1; dy++)
        {
            for (int8_t dx = -1; dx <= 1; dx++)
            {
                if (dx == 0 && dy == 0) continue;
                uint8_t nx = static_cast<uint8_t>((x + dx + _gridWidth) % _gridWidth);
                uint8_t ny = static_cast<uint8_t>((y + dy + _gridHeight) % _gridHeight);
                if (_cur[idx(nx, ny)]) n++;
            }
        }
        return n;
    }

    uint32_t hashGrid() const
    {
        // FNV-1a over the live/dead state.
        uint32_t h = 2166136261u;
        const size_t cells = static_cast<size_t>(_gridWidth) * _gridHeight;
        for (size_t i = 0; i < cells; i++)
        {
            h ^= _cur[i];
            h *= 16777619u;
        }
        return h;
    }

    void stepGeneration(Segment* segment)
    {
        uint8_t next[MAX_GRID_SIZE];
        uint16_t population = 0;

        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                const uint8_t n = countNeighbours(x, y);
                const bool wasAlive = _cur[idx(x, y)] != 0;
                // Conway B3/S23: born on 3, survives on 2 or 3.
                const bool aliveNow = wasAlive ? (n == 2 || n == 3) : (n == 3);
                next[idx(x, y)] = aliveNow ? 1 : 0;
                if (aliveNow) population++;
            }
        }

        for (size_t i = 0; i < static_cast<size_t>(_gridWidth) * _gridHeight; i++)
        {
            if (next[i])
                _age[i] = (_age[i] < 255) ? static_cast<uint8_t>(_age[i] + 1) : 255;
            else
                _age[i] = 0;
            _cur[i] = next[i];
        }

        _hueShift += 2;

        // Stagnation: extinct, a still life (hash repeats), or a period-2
        // oscillator (hash matches two generations back).
        const uint32_t h = hashGrid();
        if (population == 0 || h == _hashPrev1 || h == _hashPrev2)
            _stagnantGens++;
        else
            _stagnantGens = 0;
        _hashPrev2 = _hashPrev1;
        _hashPrev1 = h;

        if (population == 0 || _stagnantGens > STAGNATION_LIMIT)
            seed(segment->getConfig());
    }

    void draw(Segment* segment)
    {
        const auto& cfg = segment->getConfig();
        const uint8_t baseHue = cfg.option1;
        const uint8_t colorMode = cfg.option2;

        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                if (!_cur[idx(x, y)])
                {
                    segment->setPixelXY(x, y, 0, 0, 0);
                    continue;
                }

                uint8_t hue;
                switch (colorMode)
                {
                    case 1: // age heat: hue warms/shifts the longer a cell survives
                        hue = static_cast<uint8_t>(baseHue + (_age[idx(x, y)] > 40 ? 40 : _age[idx(x, y)]) * 3);
                        break;
                    case 2: // position rainbow with slow rotation
                        hue = static_cast<uint8_t>(baseHue + _hueShift +
                              (x * 256 / _gridWidth + y * 256 / _gridHeight) / 2);
                        break;
                    default: // uniform
                        hue = baseHue;
                        break;
                }

                // Fresh cells flash slightly white, then settle to full saturation.
                const uint8_t sat = (_age[idx(x, y)] <= 1) ? 150 : 255;
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, sat, 255);
                segment->setPixelXY(x, y,
                    (rgb >> 16) & 0xFF,
                    (rgb >> 8) & 0xFF,
                    rgb & 0xFF);
            }
        }
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Game of Life 2D", "Game of Life 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Conways \"Spiel des Lebens\" auf einer 2D LED-Matrix. Zellulärer Automat mit "
            "umlaufenden Rändern; startet zufällig neu, wenn die Population ausstirbt oder erstarrt.",
            "Conway's Game of Life on a 2D LED matrix. Cellular automaton with wrap-around edges; "
            "re-seeds automatically when the population dies out or stagnates.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "ColorMode";
            case 3: return "SpawnDensity";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit der Generationen.", "Generation speed.");
            case 1: return PARAM_DESC_DE_EN("Basis-Farbton der lebenden Zellen.", "Base hue of living cells.");
            case 2: return PARAM_DESC_DE_EN("Farbmodus: 0=einfarbig, 1=Alters-Heat, 2=Positions-Rainbow.",
                                            "Colour mode: 0=uniform, 1=age heat, 2=position rainbow.");
            case 3: return PARAM_DESC_DE_EN("Anfangsdichte der zufälligen Startbelegung (Prozent).",
                                            "Initial random fill density (percent).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        if (index == 2) return ParameterType::PARAM_ENUM; // ColorMode
        return ParameterType::PARAM_UINT8;
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 2) return nullptr;
        switch (enumValue)
        {
            case 0: return "Einfarbig";
            case 1: return "Alters-Heat";
            case 2: return "Positions-Regenbogen";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 2) ? 3 : 0;
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed
            case 1: return 0;  // Hue
            case 2: return 0;  // ColorMode
            case 3: return 5;  // SpawnDensity
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Hue
            case 2: return 2;   // ColorMode
            case 3: return 95;  // SpawnDensity
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64;  // Speed
            case 1: return 96;  // Hue (green)
            case 2: return 1;   // ColorMode (age heat)
            case 3: return 30;  // SpawnDensity 30%
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;
            case 1: return cfg.option1;
            case 2: return cfg.option2;
            case 3: return cfg.option3;
        }
        return 0;
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = static_cast<uint8_t>(value); break;
            case 1: cfg.option1 = static_cast<uint8_t>(value); break;
            case 2: cfg.option2 = static_cast<uint8_t>(value); break;
            case 3: cfg.option3 = static_cast<uint8_t>(value); break;
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)segment;
        (void)deltaTime;
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D() || geo.width < 2 || geo.height < 2) return;
        if ((static_cast<size_t>(geo.width) * geo.height) > MAX_GRID_SIZE) return;

        if (_gridWidth != geo.width || _gridHeight != geo.height || !_seeded)
        {
            resetState(segment);
            draw(segment);
            return;
        }

        _lastUpdate += deltaTime;
        const auto& cfg = segment->getConfig();
        uint16_t stepMs = static_cast<uint16_t>(mapLinear(cfg.speed, 1, 255, 800, 60));
        if (stepMs == 0) stepMs = 1;

        while (_lastUpdate >= stepMs)
        {
            _lastUpdate -= stepMs;
            stepGeneration(segment);
        }

        draw(segment);
    }
};
