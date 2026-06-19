/**
 * @file StarfieldWarp2DEffect.h
 * @brief Starfield warp tunnel for 2D LED matrices
 */
#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class StarfieldWarp2DEffect : public Effect
{
  private:
    static constexpr uint8_t MAX_STARS = 56;
    uint16_t _sx[MAX_STARS] = {0};
    uint16_t _sy[MAX_STARS] = {0};
    uint16_t _sz[MAX_STARS] = {0};
    bool _inited = false;

    void respawn(uint8_t i, uint16_t w, uint16_t h)
    {
        _sx[i] = FastLEDMath::random16(w * 2);
        _sy[i] = FastLEDMath::random16(h * 2);
        _sz[i] = 50 + FastLEDMath::random16(900);
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Starfield Warp", "Starfield Warp");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Sternenfeld mit Warp-Tunnel-Effekt.", "Starfield with warp tunnel effect.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Density";
            case 2: return "ColorMode";
            case 3: return "WarpPulse";
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_ENUM;
            case 3: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Warp-Geschwindigkeit der Sterne (1-255)", "Speed: Warp speed of the stars (1-255)");
            case 1: return PARAM_DESC_DE_EN("Dichte: Anzahl der Sterne in Prozent (10-100)", "Density: Number of stars in percent (10-100)");
            case 2: return PARAM_DESC_DE_EN("Farbmodus: Weiß, Cyan oder Regenbogen", "ColorMode: White, Cyan or Rainbow");
            case 3: return PARAM_DESC_DE_EN("Warp-Puls: 0=Aus, 1=Periodischer Geschwindigkeitsschub", "WarpPulse: 0=Off, 1=Periodic speed boost");
            default: return "";
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 96;
            case 1: return 70;
            case 2: return 0;
            case 3: return 1;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 10;
            case 2: return 0;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 100;
            case 2: return 2;
            case 3: return 1;
            default: return 255;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 2) return nullptr;
        switch (enumValue)
        {
            case 0: return "Weiss";
            case 1: return "Cyan";
            case 2: return "Regenbogen";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 2) ? 3 : 0;
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
            case 3: return cfg.feature1;
            default: return 0;
        }
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
            case 3: cfg.feature1 = static_cast<bool>(value); break;
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
        if (segment->getGeometry().is1D()) return;

        uint16_t w = segment->getRenderWidth();
        uint16_t h = segment->getRenderHeight();
        if (w < 2 || h < 2) return;

        if (!_inited)
        {
            for (uint8_t i = 0; i < MAX_STARS; i++) respawn(i, w, h);
            _inited = true;
        }

        auto& cfg = segment->getConfig();
        uint8_t speed = cfg.speed ? cfg.speed : 96;
        uint8_t density = cfg.option1 ? cfg.option1 : 70;
        uint8_t colorMode = cfg.option2;
        bool warpPulse = cfg.feature1;

        for (uint16_t y = 0; y < h; y++)
            for (uint16_t x = 0; x < w; x++)
                segment->setPixelGlobalXY(x, y, 0, 0, 0);

        uint16_t active = 8 + (uint16_t)density * (MAX_STARS - 8) / 100;
        uint16_t cx = w / 2;
        uint16_t cy = h / 2;

        uint8_t pulse = warpPulse ? FastLEDMath::sin8((millis() / 8) & 0xFF) : 128;
        uint16_t zStep = 3 + (uint16_t)speed / 6 + (warpPulse ? (pulse / 32) : 0);

        for (uint16_t i = 0; i < active; i++)
        {
            if (_sz[i] <= zStep) respawn(i, w, h);
            _sz[i] -= zStep;

            int16_t dx = (int16_t)_sx[i] - (int16_t)w;
            int16_t dy = (int16_t)_sy[i] - (int16_t)h;

            int32_t px = cx + ((int32_t)dx * 512) / (int32_t)_sz[i];
            int32_t py = cy + ((int32_t)dy * 512) / (int32_t)_sz[i];

            if (px < 0 || py < 0 || px >= w || py >= h)
            {
                respawn(i, w, h);
                continue;
            }

            uint8_t v = 255 - FastLEDMath::scale8(static_cast<uint8_t>(_sz[i] & 0xFF), 220);
            uint8_t r = v, g = v, b = v;
            if (colorMode == 1)
            {
                r = FastLEDMath::scale8(v, 80);
                g = v;
                b = v;
            }
            else if (colorMode == 2)
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow((uint8_t)(i * 19 + (millis() >> 4)), 180, v);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }

            segment->setPixelGlobalXY((uint16_t)px, (uint16_t)py, r, g, b);
        }
    }
};
