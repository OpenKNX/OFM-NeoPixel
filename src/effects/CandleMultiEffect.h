/**
 * @file CandleMultiEffect.h
 * @brief Multi-candle flickering effect - independent flames sampled statelessly
 *
 * Each LED uses the same candle target and fade-step model as the single
 * candle, but with a unique deterministic seed so every pixel behaves like
 * its own flame without per-pixel runtime state.
 *
 * Inspired by:
 *   - https://github.com/avanhanegem/ArduinoCandleEffectNeoPixel
 *   - https://cpldcpu.wordpress.com/2016/01/05/reverse-engineering-a-real-candle/
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "CandleStateless.h"
#include "Effect.h"

class CandleMultiEffect : public Effect
{
  public:
    CandleMultiEffect() = default;

    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Kerzen Multi", "Candle Multi");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Jede LED flackert unabhängig wie eine eigene Kerze",
            "Each LED flickers independently like its own candle");
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)deltaTime;
        if (!segment) return;

        const uint16_t length = segment->getLength();
        if (length == 0) return;

        const auto& config = segment->getConfig();
        const auto colors = CandleStateless::resolveColors(config);
        const uint32_t frame = CandleStateless::currentFrame();
        const uint32_t baseSeed = CandleStateless::segmentSeed(segment);

        for (uint16_t i = 0; i < length; i++)
        {
            const uint32_t seed = CandleStateless::mix32(baseSeed ^ (0x9E3779B9u * static_cast<uint32_t>(i + 1U)));
            const uint8_t brightness = CandleStateless::sampleBrightness(
                seed, frame, config.speed, config.intensity, CandleStateless::MULTI_CYCLE_SEGMENTS);

            segment->setPixel(
                i,
                CandleStateless::blendChannel(colors.br, colors.pr, brightness),
                CandleStateless::blendChannel(colors.bg, colors.pg, brightness),
                CandleStateless::blendChannel(colors.bb, colors.pb, brightness),
                CandleStateless::blendChannel(colors.bww, colors.pww, brightness),
                CandleStateless::blendChannel(colors.bcw, colors.pcw, brightness));
        }
    }

    uint8_t getParameterCount() const override { return 2; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Intensity";
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        return ParameterType::PARAM_UINT8;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 96;
            case 1: return 224;
            default: return 0;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0:
                return PARAM_DESC_DE_EN(
                    "Flackergeschwindigkeit (0=langsam, 255=schnell)",
                    "Flicker speed (0=slow, 255=fast)");
            case 1:
                return PARAM_DESC_DE_EN(
                    "Flackerintensität (0=ruhig, 255=wild)",
                    "Flicker intensity (0=calm, 255=wild)");
            default:
                return nullptr;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override { return 0; }
    uint32_t getParameterMax(uint8_t index) const override { return 255; }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;
            case 1: return config.intensity;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = value & 0xFF; break;
            case 1: config.intensity = value & 0xFF; break;
        }
    }
};
