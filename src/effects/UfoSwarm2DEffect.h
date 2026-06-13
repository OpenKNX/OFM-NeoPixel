/**
 * @file UfoSwarm2DEffect.h
 * @brief Small UFO swarm with optional tractor beams
 */
#pragma once

#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class UfoSwarm2DEffect : public Effect
{
  private:
    static constexpr uint8_t MAX_UFO = 8;
    uint16_t _x[MAX_UFO] = {0};
    uint8_t _yBase[MAX_UFO] = {0};
    bool _init = false;

    void initSwarm(uint16_t w, uint16_t h, uint8_t count)
    {
        if (count > MAX_UFO) count = MAX_UFO;
        for (uint8_t i = 0; i < count; i++)
        {
            _x[i] = FastLEDMath::random16(w);
            _yBase[i] = 1 + FastLEDMath::random8((h > 4) ? (h / 2) : 2);
        }
        _init = true;
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("UFO Swarm", "UFO Swarm");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN("Mehrere UFOs mit optionalem Traktorstrahl.", "Multiple UFOs with optional tractor beam.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Count";
            case 2: return "Hue";
            case 3: return "Beam";
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_HUE;
            case 3: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Fluggeschwindigkeit der UFOs (1-255)", "Speed: Flight speed of the UFOs (1-255)");
            case 1: return PARAM_DESC_DE_EN("Anzahl: Anzahl der UFOs im Schwarm (1-8)", "Count: Number of UFOs in the swarm (1-8)");
            case 2: return PARAM_DESC_DE_EN("Farbton: Farbton der UFO-Lichter (0-255)", "Hue: Hue of the UFO lights (0-255)");
            case 3: return PARAM_DESC_DE_EN("Strahl: 0=Aus, 1=Traktorstrahl anzeigen", "Beam: 0=Off, 1=Show tractor beam");
            default: return "";
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 90;
            case 1: return 4;
            case 2: return 150;
            case 3: return 1;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 1;
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
            case 1: return 8;
            case 2: return 255;
            case 3: return 1;
            default: return 255;
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
        if (w < 4 || h < 3) return;

        auto& cfg = segment->getConfig();
        uint8_t speed = cfg.speed ? cfg.speed : 90;
        uint8_t count = cfg.option1 ? cfg.option1 : 4;
        if (count > MAX_UFO) count = MAX_UFO;
        uint8_t hue = cfg.option2;
        bool beam = cfg.feature1;

        if (!_init) initSwarm(w, h, count);

        for (uint16_t y = 0; y < h; y++)
            for (uint16_t x = 0; x < w; x++)
                segment->setPixelGlobalXY(x, y, 0, 0, 0);

        uint16_t step = 1 + speed / 48;
        uint8_t bobPhase = static_cast<uint8_t>((millis() + deltaTime) >> 4);

        for (uint8_t i = 0; i < count; i++)
        {
            _x[i] = (_x[i] + step + i) % w;
            uint8_t bob = FastLEDMath::sin8((uint8_t)(bobPhase + i * 37));
            int16_t y = _yBase[i] + (int16_t)(bob / 96) - 1;
            if (y < 1) y = 1;
            if (y >= (int16_t)h - 1) y = h - 2;

            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue + i * 12, 220, 230);
            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;

            uint16_t x = _x[i];
            segment->setPixelGlobalXY((x + w - 1) % w, (uint16_t)y, FastLEDMath::scale8(r, 120), FastLEDMath::scale8(g, 120), FastLEDMath::scale8(b, 120));
            segment->setPixelGlobalXY(x, (uint16_t)y, r, g, b);
            segment->setPixelGlobalXY((x + 1) % w, (uint16_t)y, FastLEDMath::scale8(r, 120), FastLEDMath::scale8(g, 120), FastLEDMath::scale8(b, 120));

            if (beam)
            {
                uint8_t beamLen = 1 + FastLEDMath::random8((h > 6) ? (h / 2) : 3);
                for (uint8_t by = 1; by <= beamLen && (uint16_t)(y + by) < h; by++)
                {
                    uint8_t fade = FastLEDMath::scale8(180, 255 - (by * 220 / beamLen));
                    segment->setPixelGlobalXY(x, (uint16_t)(y + by), FastLEDMath::scale8(r, fade), FastLEDMath::scale8(g, fade), FastLEDMath::scale8(b, fade));
                }
            }
        }
    }
};
