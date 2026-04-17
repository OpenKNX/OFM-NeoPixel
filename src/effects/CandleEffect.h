/**
 * @file CandleEffect.h
 * @brief Candle flickering effect - single candle sampled statelessly
 *
 * Uses the candle target and fade-step model, sampled deterministically
 * from absolute time so the effect remains singleton-safe and stateless.
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

class CandleEffect : public Effect
{
  public:
    CandleEffect() = default;

    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Kerze", "Candle");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Realistischer Kerzenflacker-Effekt, alle LEDs flackern gemeinsam",
            "Realistic candle flicker effect, all LEDs flicker together");
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
        const uint32_t seed = CandleStateless::segmentSeed(segment);
        const uint8_t brightness = CandleStateless::sampleBrightness(
            seed, frame, config.speed, config.intensity, CandleStateless::SINGLE_CYCLE_SEGMENTS);

        for (uint16_t i = 0; i < length; i++)
        {
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
            case 0: return "speed";
            case 1: return "intensity";
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
