#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "ParameterType.h"

/**
 * @brief Lightning effect (FastLED-inspired)
 *
 * Random lightning strikes with short flash + decay.
 *
 * Uses config parameters:
 *  - config.speed     : strike frequency / probability (0 => rare, 255 => frequent)
 *  - config.intensity : peak brightness (0..255)
 *  - config.option1   : strike width (0 => auto)
 *  - config.option2   : decay speed (0 => default)
 *  - config.option3   : hue for colored lightning (only if feature1=1)
 *  - config.feature1  : colored lightning (0 = white, 1 = use hue option3)
 *  - config.reverse   : reverses strike position mapping (mirrors position)
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow) [only used in colored mode]
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow) [only used in colored mode]
 */
class LightningEffect : public Effect
{
  public:
    const char* getName() override { return "Lightning"; }
    const char* getDescription() override { return "Random lightning strikes with flash + decay"; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return "Speed";
            case 1: return "Width";
            case 2: return "Decay";
            case 3: return "Hue";
            default: return nullptr;
        }
    }
    ParameterType getParameterType(uint8_t) const override { return ParameterType::PARAM_UINT8; }
    uint32_t getParameterDefault(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return 32;
            case 1: return 0;  // auto width
            case 2: return 40; // decay
            case 3: return 0;  // hue
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

    void setParameter(Segment* segment, uint8_t idx, uint32_t value) override
    {
        if (!segment) return;
        auto& c = segment->getConfig();
        switch (idx)
        {
            case 0: c.speed = (uint8_t)value; break;
            case 1: c.option1 = (uint8_t)value; break;
            case 2: c.option2 = (uint8_t)value; break;
            case 3: c.option3 = (uint8_t)value; break;
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& st = segment->getState();
        auto& cfg = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // Snapshot config per frame
        const uint8_t speedVal = cfg.speed;         // strike frequency / probability
        const uint8_t peak = cfg.intensity;         // peak brightness
        const uint8_t widthCfg = cfg.option1;       // strike width
        const uint8_t decayCfg = cfg.option2;       // decay speed
        const uint8_t hue = cfg.option3;            // hue for colored lightning
        const bool colored = cfg.feature1;          // colored lightning
        const bool reverseDir = (cfg.reverse != 0); // reverse position mapping
        const bool yellowBoost = cfg.feature2;      // enable yellow brightness compensation
        const bool greenCorr = cfg.feature3;        // enable green correction hooks

        // Clear background (simple + deterministic)
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, 0, 0, 0);

        // phase: 0=idle, 1=flash, 2=decay
        st.counter += (uint16_t)deltaTime;

        if (st.phase == 0)
        {
            // Strike probability check every ~25ms
            if (st.counter >= 25u)
            {
                st.counter = 0;

                // probability: speedVal/255
                if (FastLEDMath::random8() < speedVal)
                {
                    // pick strike location and width
                    uint16_t pos = (length > 1) ? FastLEDMath::random16(length) : 0;
                    if (reverseDir && length > 0) pos = (length - 1u) - pos;

                    uint8_t w = widthCfg;
                    if (w == 0)
                        w = (length >= 8) ? 3 : 1;
                    if (w > length) w = (uint8_t)length;

                    st.aux1 = pos;     // strike center
                    st.aux2 = w;       // strike width
                    st.phase = 1;      // flash
                    st.lastUpdate = 0; // reuse as elapsed within phase
                }
            }
        }
        else
        {
            // elapsed in current phase
            st.lastUpdate += deltaTime;

            const uint16_t center = st.aux1;
            const uint8_t w = (uint8_t)st.aux2;
            const uint16_t half = (uint16_t)(w / 2u);

            uint8_t brightness = peak;

            if (st.phase == 1)
            {
                // short flash duration ~60ms
                if (st.lastUpdate >= 60u)
                {
                    st.phase = 2;
                    st.lastUpdate = 0;
                }
            }
            else if (st.phase == 2)
            {
                // decay duration controlled by option2
                const uint16_t decayMs = (decayCfg == 0) ? 300u : (uint16_t)(50u + (uint16_t)decayCfg * 8u);
                if (st.lastUpdate >= decayMs)
                {
                    st.phase = 0;
                    st.lastUpdate = 0;
                    return;
                }
                // linear fade
                brightness = (uint8_t)(peak - (uint8_t)((uint32_t)peak * st.lastUpdate / decayMs));
            }

            // render strike as a small bar
            const uint16_t start = (center > half) ? (center - half) : 0;
            const uint16_t end = (center + half < (length - 1u)) ? (center + half) : (length - 1u);

            if (!colored)
            {
                for (uint16_t i = start; i <= end; i++)
                    segment->setPixel(i, brightness, brightness, brightness);
            }
            else
            {
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 0, brightness, yellowBoost, greenCorr);
                const uint8_t r = (rgb >> 16) & 0xFF;
                const uint8_t g = (rgb >> 8) & 0xFF;
                const uint8_t b = rgb & 0xFF;
                for (uint16_t i = start; i <= end; i++)
                    segment->setPixel(i, r, g, b);
            }
        }
    }
};
