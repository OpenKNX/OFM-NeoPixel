/**
 * @file Noise2DEffect.h
 * @brief Smooth XY noise field for LED matrices
 *
 * Parameters:
 *   0 Speed  — animation speed 1-255 (default 64)
 *   1 Scale  — spatial noise scale 1-255 (default 64)
 *   2 Palette— 0=HSV hue field, 1=use primary RGB colour
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Noise2DEffect : public Effect
{
  private:
    uint32_t _timeAcc = 0;

    static uint8_t smoothNoise2D(uint8_t x, uint8_t y, uint16_t tx, uint16_t ty, uint8_t scale)
    {
        uint16_t nx = (uint16_t)x * scale + tx;
        uint16_t ny = (uint16_t)y * scale + ty;
        uint8_t ix = (uint8_t)(nx >> 8), iy = (uint8_t)(ny >> 8);
        uint8_t fx = (uint8_t)(nx & 0xFF), fy = (uint8_t)(ny & 0xFF);
        auto h = [](uint8_t a, uint8_t b) -> uint8_t {
            uint16_t v = (uint16_t)a * 0x9E ^ (uint16_t)b * 0x6C;
            v ^= v >> 5; v *= 0xD3; v ^= v >> 8;
            return (uint8_t)(v & 0xFF);
        };
        uint8_t aa = h(ix, iy), ba = h(ix+1, iy), ab = h(ix, iy+1), bb = h(ix+1, iy+1);
        uint8_t lA = (uint8_t)(((uint16_t)aa*(255-fx) + (uint16_t)ba*fx) >> 8);
        uint8_t lB = (uint8_t)(((uint16_t)ab*(255-fx) + (uint16_t)bb*fx) >> 8);
        return (uint8_t)(((uint16_t)lA*(255-fy) + (uint16_t)lB*fy) >> 8);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Rauschen 2D", "Noise 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Sanftes XY-Rauschfeld für LED-Matrizen mit bilinearer Interpolation. Erzeugt organisch fließende Farbmuster.",
            "Smooth XY noise field for LED matrices with bilinear interpolation. Creates organically flowing colour patterns.");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 3; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Scale";
            case 2: return "Palette";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Geschwindigkeit: Animationsgeschwindigkeit des Rauschfeldes (1=sehr langsam, 255=sehr schnell).",
                "Speed: Animation speed of the noise field (1=very slow, 255=very fast).");
            case 1: return PARAM_DESC_DE_EN(
                "Maßstab: Räumliche Skalierung des Rauschmusters (1=sehr fein, 255=sehr grob). Kleinere Werte erzeugen kleinere Muster.",
                "Scale: Spatial scale of the noise pattern (1=very fine, 255=very coarse). Smaller values create smaller patterns.");
            case 2: return PARAM_DESC_DE_EN(
                "Farbmodus: 0=HSV-Farbrad (automatische Farbgebung), 1=Primärfarbe (Helligkeit aus dem Rauschen, Farbe aus Segment-Einstellung).",
                "Colour mode: 0=HSV colour wheel (automatic colouring), 1=Primary colour (brightness from noise, colour from segment setting).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 64;  // Speed
            case 1: return 64;  // Scale
            case 2: return 0;   // HSV mode
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
            case 0: return 255;
            case 1: return 255;
            case 2: return 1;
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
            case 1: return cfg.option1;  // Scale
            case 2: return cfg.feature1; // Palette mode
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
            case 2: cfg.feature1= static_cast<bool>(value);    break;
        }
    }

    // ====================================================================
    // 1D fallback
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        _timeAcc += deltaTime;
        auto& cfg = segment->getConfig();
        uint8_t scale = cfg.option1 ? cfg.option1 : 64;
        uint16_t t    = (uint16_t)(_timeAcc * cfg.speed / 128);
        uint16_t len  = segment->getLength();
        bool palMode  = cfg.feature1;
        for (uint16_t i = 0; i < len; i++)
        {
            uint8_t n = smoothNoise2D((uint8_t)(i & 0xFF), 0, (uint16_t)(t * scale >> 8), 0, scale);
            if (palMode)
            {
                segment->setPixel(i,
                    FastLEDMath::scale8(cfg.r(), n),
                    FastLEDMath::scale8(cfg.g(), n),
                    FastLEDMath::scale8(cfg.b(), n));
            }
            else
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(n, 255, n);
                segment->setPixel(i, (rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
            }
        }
    }

    // ====================================================================
    // 2D rendering
    // ====================================================================
    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) { update(segment, deltaTime); return; }

        _timeAcc += deltaTime;
        auto& cfg = segment->getConfig();
        uint8_t  scale   = cfg.option1 ? cfg.option1 : 64;
        uint16_t speed   = (uint16_t)cfg.speed;
        uint16_t tx      = (uint16_t)((_timeAcc * speed) >> 7);
        uint16_t ty      = (uint16_t)((_timeAcc * speed) >> 9);
        bool     palMode = cfg.feature1;

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
