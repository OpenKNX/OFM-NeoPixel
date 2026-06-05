/**
 * @file NoiseEffect2D.h
 * @brief 2D smooth noise effect for LED matrices
 *
 * Produces a smoothly animated XY noise field on a 2D LED matrix.
 * Uses a lightweight value-noise function (no FastLED inoise dependency).
 *
 * ETS parameters (shared with NoiseEffect):
 *   Speed   (config.speed)   — animation speed 1-255
 *   option1                  — spatial scale 0-255 (0=default=64)
 *   feature1                 — palette mode: 0=HSV hue field, 1=use primary color
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class NoiseEffect2D : public Effect
{
  private:
    uint32_t _timeAcc = 0; // accumulated time for smooth animation

    // Lightweight 2D value noise: integer hash of (x, y, t)
    static uint8_t noise2D(uint8_t x, uint8_t y, uint16_t t)
    {
        // Mix x, y, t with simple hash
        uint16_t h = (uint16_t)x * 73 ^ (uint16_t)y * 137 ^ t;
        h ^= h >> 7;
        h *= 0xD5A3;
        h ^= h >> 5;
        uint16_t h2 = ((uint16_t)(x + 1) * 73 ^ (uint16_t)y * 137 ^ t);
        h2 ^= h2 >> 7; h2 *= 0xD5A3; h2 ^= h2 >> 5;
        // Interpolate along x using fractional part (simple lerp)
        return (uint8_t)((h >> 8));
    }

    // Smoother 2D noise with bilinear interpolation
    static uint8_t smoothNoise2D(uint8_t x, uint8_t y, uint16_t tx, uint16_t ty, uint8_t scale)
    {
        // Scale x,y into noise space
        uint16_t nx = (uint16_t)x * scale + tx;
        uint16_t ny = (uint16_t)y * scale + ty;

        uint8_t ix = (uint8_t)(nx >> 8);
        uint8_t iy = (uint8_t)(ny >> 8);
        uint8_t fx = (uint8_t)(nx & 0xFF);
        uint8_t fy = (uint8_t)(ny & 0xFF);

        // Four corner hashes
        auto h = [](uint8_t a, uint8_t b) -> uint8_t {
            uint16_t v = (uint16_t)a * 0x9E ^ (uint16_t)b * 0x6C;
            v ^= v >> 5; v *= 0xD3; v ^= v >> 8;
            return (uint8_t)(v & 0xFF);
        };

        uint8_t aa = h(ix,     iy);
        uint8_t ba = h(ix + 1, iy);
        uint8_t ab = h(ix,     iy + 1);
        uint8_t bb = h(ix + 1, iy + 1);

        // Bilinear interpolation
        using namespace FastLEDMath;
        uint8_t lerpA = (uint8_t)(((uint16_t)aa * (255 - fx) + (uint16_t)ba * fx) >> 8);
        uint8_t lerpB = (uint8_t)(((uint16_t)ab * (255 - fx) + (uint16_t)bb * fx) >> 8);
        return (uint8_t)(((uint16_t)lerpA * (255 - fy) + (uint16_t)lerpB * fy) >> 8);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Rauschen 2D", "Noise 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Smooth XY-Rauschen für LED-Matrizen",
                                 "Smooth XY noise field for LED matrices");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        // 1D fallback: animate noise along the strip
        if (!segment) return;
        _timeAcc += deltaTime;
        uint16_t t = (uint16_t)(_timeAcc * segment->getConfig().speed / 128);
        uint16_t len = segment->getLength();
        uint8_t scale = segment->getConfig().option1 ? segment->getConfig().option1 : 64;
        const bool palMode = segment->getConfig().feature1;

        for (uint16_t i = 0; i < len; i++)
        {
            uint8_t n = smoothNoise2D((uint8_t)(i & 0xFF), 0, (uint16_t)(t * scale >> 8), 0, scale);
            if (palMode)
            {
                uint8_t r = FastLEDMath::scale8(segment->getConfig().r(), n);
                uint8_t g = FastLEDMath::scale8(segment->getConfig().g(), n);
                uint8_t b = FastLEDMath::scale8(segment->getConfig().b(), n);
                segment->setPixel(i, r, g, b);
            }
            else
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(n, 255, n);
                segment->setPixel(i, (rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
            }
        }
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) { update(segment, deltaTime); return; }

        _timeAcc += deltaTime;
        auto& cfg = segment->getConfig();
        uint8_t scale  = cfg.option1 ? cfg.option1 : 64;
        uint16_t speed = (uint16_t)cfg.speed;
        // Two independent time axes for richer look
        uint16_t tx = (uint16_t)((_timeAcc * speed) >> 7);
        uint16_t ty = (uint16_t)((_timeAcc * speed) >> 9); // slower Y drift
        const bool palMode = cfg.feature1;

        for (uint8_t y = 0; y < geo.height; y++)
        {
            for (uint8_t x = 0; x < geo.width; x++)
            {
                uint8_t n = smoothNoise2D(x, y, tx, ty, scale);
                uint8_t r, g, b;
                if (palMode)
                {
                    r = FastLEDMath::scale8(cfg.r(), n);
                    g = FastLEDMath::scale8(cfg.g(), n);
                    b = FastLEDMath::scale8(cfg.b(), n);
                }
                else
                {
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(n, 240, n);
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >>  8) & 0xFF;
                    b =  rgb        & 0xFF;
                }
                segment->setPixelXY(x, y, r, g, b);
            }
        }
    }
};
