#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "ParameterType.h"

/**
 * @brief Noise effect (inoise8-inspired, lightweight)
 *
 * This is a small, self-contained value-noise implementation (no FastLED inoise dependency).
 * Produces smooth-ish noise along the strip with time evolution and maps it to color.
 *
 * Uses config parameters:
 *  - config.speed     : time evolution speed
 *  - config.intensity : master brightness (HSV V)
 *  - config.option1   : spatial scale (0 => default)
 *  - config.option2   : saturation (0 => 255)
 *  - config.option3   : hue offset
 *  - config.reverse   : flips spatial direction
 *  - config.feature1  : palette mode (0=HSV, 1=use fixed RGB config color modulated by noise)
 *  - config.feature2  : value-only mode (0=hue+value, 1=value only)
 *  - config.feature3  : enable yellow brightness compensation (hsv2rgb_rainbow)
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow)
 */
class NoiseEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override { return "Noise"; }
    const char* getDescription(const char* lang = nullptr) override { return "Smooth noise along strip (inoise8-inspired)"; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return "Speed";
            case 1: return "Scale";
            case 2: return "Saturation";
            case 3: return "HueOffset";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Zeitliche Entwicklung (0-255)", "Speed: Time evolution speed (0-255)");
            case 1: return PARAM_DESC_DE_EN("Maßstab: Räumliche Rauschskalierung (0-255)", "Scale: Spatial noise scaling (0-255)");
            case 2: return PARAM_DESC_DE_EN("Sättigung: Farbsättigung (0-255)", "Saturation: Color saturation (0-255)");
            case 3: return PARAM_DESC_DE_EN("Farbton-Versatz: Basis-Farbverschiebung (0-255)", "Hue offset: Base color shift (0-255)");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t) const override { return ParameterType::PARAM_UINT8; }
    uint32_t getParameterDefault(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return 32;
            case 1: return 48;
            case 2: return 255;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t idx) const override
    {
        if (!segment) return 0;
        const auto& c = segment->getConfig();
        switch (idx)
        {
            case 0: return c.speed;
            case 1: return c.option1;
            case 2: return c.option2;
            case 3: return c.option3;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (animation speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Scale (noise scale)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Saturation
            case 3: config.option3 = static_cast<uint8_t>(value); break; // HueOffset (color shift)
            default: break;
        }
    }

  private:
    static uint16_t hash16(uint16_t x)
    {
        // simple integer hash
        x ^= x << 7;
        x ^= x >> 9;
        x *= 0x9E37u;
        x ^= x >> 8;
        return x;
    }

    static uint8_t smooth8(uint8_t t)
    {
        // smoothstep: 3t^2 - 2t^3 in 8-bit
        uint16_t tt = (uint16_t)t * (uint16_t)t; // 0..65025
        uint16_t ttt = (uint16_t)(tt * t) >> 8;  // keep in range
        uint16_t res = (3u * tt - 2u * ttt) >> 8;
        return (uint8_t)res;
    }

    static uint8_t noise8(uint16_t x, uint16_t t)
    {
        // 1D value noise with interpolation
        const uint16_t x0 = x & 0xFF00u;
        const uint16_t x1 = x0 + 0x0100u;

        const uint8_t frac = (uint8_t)(x & 0x00FFu);
        const uint8_t f = smooth8(frac);

        const uint8_t v0 = (uint8_t)(hash16((uint16_t)(x0 + t)) & 0xFFu);
        const uint8_t v1 = (uint8_t)(hash16((uint16_t)(x1 + t)) & 0xFFu);

        return FastLEDMath::lerp8by8(v0, v1, f);
    }

  public:
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& st = segment->getState();
        auto& cfg = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // Snapshot config per frame
        const uint8_t speedVal = cfg.speed;
        const uint8_t intensityVal = cfg.intensity;
        uint8_t scaleVal = cfg.option1;
        uint8_t s = cfg.option2;
        const uint8_t hueOffset = cfg.option3;
        const bool reverseDir = (cfg.reverse != 0);
        const bool fixedColorMode = cfg.feature1;
        const bool valueOnlyMode = cfg.feature2;

        const bool yellowBoost = cfg.feature3;
        const bool greenCorr = cfg.feature3;

        const uint8_t cfgR = cfg.r();
        const uint8_t cfgG = cfg.g();
        const uint8_t cfgB = cfg.b();

        if (scaleVal == 0) scaleVal = 48;
        if (s == 0) s = 255;

        // Advance time
        st.counter += (uint16_t)deltaTime;
        const uint16_t intervalMs = (uint16_t)(1 + (255 - speedVal));
        while (st.counter >= intervalMs)
        {
            st.counter -= intervalMs;
            st.position = (uint16_t)((st.position + 1) & 0xFFFF);
        }
        const uint16_t t = st.position;

        for (uint16_t i = 0; i < length; i++)
        {
            const uint16_t j = reverseDir ? (uint16_t)((length - 1u) - i) : i;
            const uint16_t x = (uint16_t)(j * (uint16_t)scaleVal);

            const uint8_t n = noise8(x, t); // 0..255
            const uint8_t v = FastLEDMath::scale8(n, intensityVal);

            if (!fixedColorMode)
            {
                const uint8_t hue = valueOnlyMode ? hueOffset : (uint8_t)(hueOffset + n);
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, s, v, yellowBoost, greenCorr);
                segment->setPixel(i, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
            else
            {
                // Fixed RGB color modulated by noise brightness
                const uint8_t r = FastLEDMath::scale8(cfgR, v);
                const uint8_t g = FastLEDMath::scale8(cfgG, v);
                const uint8_t b = FastLEDMath::scale8(cfgB, v);
                segment->setPixel(i, r, g, b);
            }
        }
    }
};
