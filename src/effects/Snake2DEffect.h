/**
 * @file Snake2DEffect.h
 * @brief Snake game effect for 2D LED matrices
 *
 * A classic snake game where the snake moves through a 2D grid,
 * eats food to grow, and resets when the grid is full.
 *
 * Parameters:
 *   0 Speed        — movement speed 1-255 (default: 64)
 *   1 HeadColor    — head color hue 0-255 (default: 0 = red)
 *   2 BodyMode     — body color mode: 0=uniform, 1=rainbow, 2=fade (default: 0)
 *   3 BodyColor    — body uniform color hue 0-255 (default: 85 = green)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include <string.h>

class Snake2DEffect : public Effect
{
  private:
    uint32_t _lastUpdate = 0;

    struct SnakeSegment
    {
        uint8_t x;
        uint8_t y;
    };

    static constexpr size_t MAX_SNAKE_SIZE = 255;
    SnakeSegment _snake[MAX_SNAKE_SIZE];
    size_t _snakeLength = 0;
    uint8_t _direction = 0; // 0=right, 1=down, 2=left, 3=up

    bool _foodExists = false;
    uint8_t _foodX = 0;
    uint8_t _foodY = 0;

    static constexpr size_t MAX_GRID_SIZE = 1024;
    bool _gridOccupied[MAX_GRID_SIZE] = {false};
    uint8_t _gridWidth = 0;
    uint8_t _gridHeight = 0;

    // Supports inverted output ranges (outMax < outMin) — plain unsigned math
    // underflows there and produced garbage step times.
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

    void resetState(Segment* segment)
    {
        const auto& geo = segment->getGeometry();
        _gridWidth = geo.width;
        _gridHeight = geo.height;

        if (_gridWidth < 2 || _gridHeight < 2 ||
            (static_cast<size_t>(_gridWidth) * _gridHeight) > MAX_GRID_SIZE)
        {
            _snakeLength = 0;
            _foodExists = false;
            return;
        }

        uint8_t startY = _gridHeight / 2;
        uint8_t startX = (_gridWidth >= 3) ? 2 : 1;

        _snakeLength = (_gridWidth >= 3) ? 3 : 2;
        _snake[0] = {startX, startY};
        _snake[1] = {static_cast<uint8_t>(startX - 1), startY};
        if (_snakeLength > 2)
        {
            _snake[2] = {static_cast<uint8_t>(startX - 2), startY};
        }

        _direction = 0;
        memset(_gridOccupied, false, sizeof(_gridOccupied));
        for (size_t i = 0; i < _snakeLength; i++)
        {
            _gridOccupied[_snake[i].y * _gridWidth + _snake[i].x] = true;
        }

        // _lastUpdate is a delta-time accumulator used by update2D().
        // Using absolute millis() here can stall the while-loop after resets.
        _lastUpdate = 0;
        _foodExists = false;
        spawnFood();
    }

    // The tail cell frees up in the same tick the head moves (unless the snake
    // just ate and the tail is duplicated), so treat it as walkable.
    bool isTailWalkable() const
    {
        if (_snakeLength < 2) return false;
        const auto& tail = _snake[_snakeLength - 1];
        const auto& prev = _snake[_snakeLength - 2];
        return !(tail.x == prev.x && tail.y == prev.y);
    }

    bool isCellBlocked(int16_t x, int16_t y) const
    {
        if (x < 0 || y < 0 || x >= _gridWidth || y >= _gridHeight) return true;
        if (isTailWalkable() &&
            _snake[_snakeLength - 1].x == x && _snake[_snakeLength - 1].y == y)
            return false;
        return _gridOccupied[static_cast<size_t>(y) * _gridWidth + x];
    }

    // Picks the next direction: prefer moving towards the food, but never
    // steer into a wall or the own body if a free neighbour exists.
    // (The old greedy version happily made 180° turns into the snake's neck,
    // causing constant game resets.)
    void chooseDirectionTowardsFood(uint8_t headX, uint8_t headY)
    {
        static constexpr int8_t DX[4] = {1, 0, -1, 0};
        static constexpr int8_t DY[4] = {0, 1, 0, -1};

        uint8_t prefs[4];
        uint8_t count = 0;
        auto addPref = [&](uint8_t dir) {
            if (dir > 3 || count >= 4) return;
            for (uint8_t i = 0; i < count; i++)
                if (prefs[i] == dir) return;
            prefs[count++] = dir;
        };

        if (_foodExists)
        {
            const int16_t dx = static_cast<int16_t>(_foodX) - headX;
            const int16_t dy = static_cast<int16_t>(_foodY) - headY;
            const int16_t adx = (dx < 0) ? -dx : dx;
            const int16_t ady = (dy < 0) ? -dy : dy;
            const uint8_t dirX = (dx > 0) ? 0 : 2;
            const uint8_t dirY = (dy > 0) ? 1 : 3;
            if (dx != 0 && (adx >= ady || dy == 0)) { addPref(dirX); if (dy != 0) addPref(dirY); }
            else if (dy != 0) { addPref(dirY); if (dx != 0) addPref(dirX); }
        }
        addPref(_direction);
        for (uint8_t d = 0; d < 4; d++) addPref(d);

        for (uint8_t i = 0; i < count; i++)
        {
            const uint8_t dir = prefs[i];
            if (!isCellBlocked(headX + DX[dir], headY + DY[dir]))
            {
                _direction = dir;
                return;
            }
        }
        // All neighbours blocked: keep direction — moveSnake() will detect the
        // collision and restart the game.
    }

    void spawnFood()
    {
        if (_gridWidth == 0 || _gridHeight == 0)
        {
            _foodExists = false;
            return;
        }

        uint16_t freeCount = 0;
        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                if (!_gridOccupied[y * _gridWidth + x]) freeCount++;
            }
        }

        if (freeCount == 0)
        {
            _foodExists = false;
            return;
        }

        uint16_t target = FastLEDMath::random16(freeCount);
        uint16_t seen = 0;
        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                if (_gridOccupied[y * _gridWidth + x]) continue;
                if (seen == target)
                {
                    _foodX = x;
                    _foodY = y;
                    _foodExists = true;
                    return;
                }
                seen++;
            }
        }

        _foodExists = false;
    }

    void moveSnake(Segment* segment)
    {
        if (_snakeLength == 0)
        {
            resetState(segment);
            return;
        }

        chooseDirectionTowardsFood(_snake[0].x, _snake[0].y);

        int16_t nx = _snake[0].x;
        int16_t ny = _snake[0].y;
        switch (_direction)
        {
            case 0: nx++; break;
            case 1: ny++; break;
            case 2: nx--; break;
            case 3: ny--; break;
        }

        if (nx < 0 || ny < 0 || nx >= _gridWidth || ny >= _gridHeight)
        {
            resetState(segment);
            return;
        }

        const size_t collisionCheckLen = isTailWalkable() ? _snakeLength - 1 : _snakeLength;
        for (size_t i = 0; i < collisionCheckLen; i++)
        {
            if (_snake[i].x == nx && _snake[i].y == ny)
            {
                resetState(segment);
                return;
            }
        }

        bool ate = (_foodExists && static_cast<uint8_t>(nx) == _foodX && static_cast<uint8_t>(ny) == _foodY);

        if (ate)
        {
            if (_snakeLength < MAX_SNAKE_SIZE)
            {
                _snake[_snakeLength] = _snake[_snakeLength - 1];
                _snakeLength++;
            }
        }

        memmove(&_snake[1], &_snake[0], (_snakeLength - 1) * sizeof(SnakeSegment));
        _snake[0].x = static_cast<uint8_t>(nx);
        _snake[0].y = static_cast<uint8_t>(ny);

        memset(_gridOccupied, false, sizeof(_gridOccupied));
        for (size_t i = 0; i < _snakeLength; i++)
        {
            _gridOccupied[_snake[i].y * _gridWidth + _snake[i].x] = true;
        }

        if (ate)
        {
            if (_snakeLength >= (static_cast<size_t>(_gridWidth) * _gridHeight))
            {
                resetState(segment);
                return;
            }
            spawnFood();
        }
    }

    uint8_t getBodyHue(uint8_t index, const EffectConfig& cfg) const
    {
        uint8_t bodyMode = cfg.option2;
        uint8_t bodyHue = cfg.option3;

        switch (bodyMode)
        {
            case 1:
                return static_cast<uint8_t>(mapLinear(index, 0, _snakeLength > 1 ? _snakeLength - 1 : 1, 0, 255));
            case 2:
                return static_cast<uint8_t>(mapLinear(index, 0, _snakeLength > 1 ? _snakeLength - 1 : 1, cfg.option1, bodyHue));
            default:
                return bodyHue;
        }
    }

    void draw(Segment* segment)
    {
        for (uint8_t y = 0; y < _gridHeight; y++)
        {
            for (uint8_t x = 0; x < _gridWidth; x++)
            {
                segment->setPixelXY(x, y, 0, 0, 0);
            }
        }

        const auto& cfg = segment->getConfig();

        if (_foodExists)
        {
            uint32_t foodRgb = FastLEDMath::hsv2rgb_rainbow(0, 255, 255);
            segment->setPixelXY(_foodX, _foodY,
                (foodRgb >> 16) & 0xFF,
                (foodRgb >> 8) & 0xFF,
                foodRgb & 0xFF);
        }

        if (_snakeLength > 0)
        {
            uint32_t headRgb = FastLEDMath::hsv2rgb_rainbow(cfg.option1, 255, 255);
            segment->setPixelXY(_snake[0].x, _snake[0].y,
                (headRgb >> 16) & 0xFF,
                (headRgb >> 8) & 0xFF,
                headRgb & 0xFF);
        }

        for (size_t i = 1; i < _snakeLength; i++)
        {
            uint8_t hue = getBodyHue(static_cast<uint8_t>(i), cfg);
            uint32_t bodyRgb = FastLEDMath::hsv2rgb_rainbow(hue, 220, 170);
            segment->setPixelXY(_snake[i].x, _snake[i].y,
                (bodyRgb >> 16) & 0xFF,
                (bodyRgb >> 8) & 0xFF,
                bodyRgb & 0xFF);
        }
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Snake 2D", "Snake 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Klassisches Snake auf einer 2D LED-Matrix. Die Schlange frisst zufaelliges Futter und waechst.",
            "Classic snake on a 2D LED matrix. The snake eats random food and grows.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "HeadHue";
            case 2: return "BodyMode";
            case 3: return "BodyHue";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit der Schlange.", "Snake movement speed.");
            case 1: return PARAM_DESC_DE_EN("Farbton des Kopfes.", "Head hue.");
            case 2: return PARAM_DESC_DE_EN("Koerpermodus: 0=fix, 1=rainbow, 2=fade.", "Body mode: 0=fixed, 1=rainbow, 2=fade.");
            case 3: return PARAM_DESC_DE_EN("Koerper-Farbton fuer fix/fade.", "Body hue for fixed/fade.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_UINT8;
            case 0:
            case 1:
            case 3:
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 0;
            case 2: return 0;
            case 3: return 0;
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 255;
            case 2: return 2;
            case 3: return 255;
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64;   // Speed
            case 1: return 0;    // Head color - red
            case 2: return 0;    // Body mode - uniform
            case 3: return 85;   // Body color - green
        }
        return 0;
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

        if (_gridWidth != geo.width || _gridHeight != geo.height || _snakeLength == 0)
        {
            resetState(segment);
            draw(segment);
            return;
        }

        _lastUpdate += deltaTime;
        const auto& cfg = segment->getConfig();
        uint16_t stepMs = static_cast<uint16_t>(mapLinear(cfg.speed, 1, 255, 520, 50));
        if (stepMs == 0) stepMs = 1;

        while (_lastUpdate >= stepMs)
        {
            _lastUpdate -= stepMs;
            moveSnake(segment);
        }

        draw(segment);
    }
};
