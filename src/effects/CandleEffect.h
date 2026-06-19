/**
 * @file CandleEffect.h
 * @brief Candle flickering effect - single or multi-zone candles sampled statelessly
 *
 * Uses the candle target and fade-step model, sampled deterministically
 * from absolute time so the effect remains singleton-safe and stateless.
 *
 * Consolidated effect: the former "Candle Multi" effect is now the Count>1
 * variant. Count=1 makes all LEDs flicker together as a single flame; higher
 * values split the strip into independent flickering zones (Count>=length
 * behaves like the old per-pixel multi-candle).
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
            "Realistischer Kerzenflacker-Effekt. Bei Zonen=1 flackern alle LEDs gemeinsam, höhere Werte erzeugen mehrere unabhängige Flammen.",
            "Realistic candle flicker effect. With Zones=1 all LEDs flicker together, higher values create multiple independent flames.");
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

        // Count/Zones: 1 = single shared flame, >1 = independent zones.
        uint16_t zones = config.count > 0 ? config.count : 1;
        if (zones > length) zones = length;

        if (zones <= 1)
        {
            // Single shared flame (classic candle)
            const uint8_t brightness = CandleStateless::sampleBrightness(
                baseSeed, frame, config.speed, config.intensity, CandleStateless::SINGLE_CYCLE_SEGMENTS);

            for (uint16_t i = 0; i < length; i++)
            {
                setCandlePixel(segment, i, colors, brightness);
            }
            return;
        }

        // Multiple independent zones; each LED belongs to a zone and uses a
        // zone-specific seed so every zone behaves like its own flame.
        for (uint16_t i = 0; i < length; i++)
        {
            const uint16_t zone = (uint16_t)(((uint32_t)i * zones) / length);
            const uint32_t seed = CandleStateless::mix32(baseSeed ^ (0x9E3779B9u * static_cast<uint32_t>(zone + 1U)));
            const uint8_t brightness = CandleStateless::sampleBrightness(
                seed, frame, config.speed, config.intensity, CandleStateless::MULTI_CYCLE_SEGMENTS);

            setCandlePixel(segment, i, colors, brightness);
        }
    }

    uint8_t getParameterCount() const override { return 3; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Intensity";
            case 2: return "Zones";
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
            case 2: return 1; // Zones (single flame)
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
            case 2:
                return PARAM_DESC_DE_EN(
                    "Zonen: Anzahl unabhängiger Flammen (1=gemeinsames Flackern, höher=mehrere Kerzen)",
                    "Zones: Number of independent flames (1=common flicker, higher=multiple candles)");
            default:
                return nullptr;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0;
            case 1: return 0;
            case 2: return 1; // Zones min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 255;
            case 2: return 255; // Zones max (clamped to strip length at runtime)
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;
            case 1: return config.intensity;
            case 2: return config.count;
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
            case 2: config.count = value & 0xFF; break;
        }
    }

  private:
    static void setCandlePixel(Segment* segment, uint16_t i,
                               const CandleStateless::Colors& colors, uint8_t brightness)
    {
        segment->setPixel(
            i,
            CandleStateless::blendChannel(colors.br, colors.pr, brightness),
            CandleStateless::blendChannel(colors.bg, colors.pg, brightness),
            CandleStateless::blendChannel(colors.bb, colors.pb, brightness),
            CandleStateless::blendChannel(colors.bww, colors.pww, brightness),
            CandleStateless::blendChannel(colors.bcw, colors.pcw, brightness));
    }
};
