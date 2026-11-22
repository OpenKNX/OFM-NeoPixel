/**
 * @file TwinkleFoxEffect.h
 * @brief Independent implementation of slowly fading, asynchronous twinkles.
 *
 * Inspired by the appearance of WLED's Twinklefox (effect 80).
 * Timing, pixel hashing, envelope and palettes are implemented here.
 * Licensed under GNU GPL v3.0.
 */
#pragma once

#include "../Segment.h"
#include "../VirtualStrip.h"
#include "Effect.h"

class TwinkleFoxEffect : public Effect
{
    struct State
    {
        uint32_t timeMs;
    };

    struct Color
    {
        uint8_t r, g, b, w;
    };

    static uint32_t hash(uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352d;
        value ^= value >> 15;
        value *= 0x846ca68b;
        return value ^ (value >> 16);
    }

    static uint8_t scale(uint8_t value, uint8_t level)
    {
        return (uint16_t(value) * level + 127) / 255;
    }

    static uint8_t envelope(uint8_t phase)
    {
        constexpr uint8_t crest = 85;
        const uint32_t linear = phase < crest ? phase * 255u / crest : (255u - phase) * 255u / (255 - crest);
        return (linear * linear * (765u - 2u * linear) + 32512) / 65025;
    }

    static Color pixelColor(const EffectConfig& cfg, uint32_t seed, bool hasWhite)
    {
        static constexpr Color fairy[] = {{255, 86, 12, 0}, {255, 150, 52, 0}, {255, 204, 124, 0}, {255, 230, 184, 0}};
        static constexpr Color holiday[] = {{255, 25, 14, 0}, {22, 255, 62, 0}, {42, 88, 255, 0}, {255, 144, 21, 0}};
        switch (cfg.mode)
        {
            case 1: return fairy[(seed >> 24) & 3];
            case 2: return holiday[(seed >> 24) & 3];
            case 3: return hasWhite ? Color{0, 0, 0, 255} : Color{255, 150, 52, 0};
            default: return {cfg.r(), cfg.g(), cfg.b(), 0};
        }
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Twinklefox", "Twinklefox");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Unabhängige Lichtpunkte glimmen langsam auf und ab. Für 1D-Streifen mit wählbarer Palette und Hintergrundhelligkeit.",
            "Independent points of light slowly fade in and out on 1D strips, with a selectable palette and background level.");
    }

    uint8_t getCapabilities() const override { return DIM_1D; }
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Density";
            case 2: return "ColorMode";
            case 3: return "BackgroundLevel";
            default: return nullptr;
        }
    }

    const char* getParameterLabel(uint8_t index, const char* lang = "de") const
    {
        switch (index)
        {
            case 0: return PARAM_LABEL_DE_EN("Geschwindigkeit", "Speed");
            case 1: return PARAM_LABEL_DE_EN("Dichte", "Density");
            case 2: return PARAM_LABEL_DE_EN("Farbpalette", "Palette");
            case 3: return PARAM_LABEL_DE_EN("Hintergrundhelligkeit", "Background level");
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Tempo des Auf- und Abglimmens.", "Speed of fading in and out.");
            case 1: return PARAM_DESC_DE_EN("Anteil aktiver Glimmphasen (0=keine, 255=alle).", "Share of active twinkle cycles (0=none, 255=all).");
            case 2: return PARAM_DESC_DE_EN("Primärfarbe, Lichterkette, Festlich oder Warmweiß. Warmweiß verwendet den Weißkanal, falls vorhanden.", "Primary colour, fairy lights, holiday or warm white. Warm white uses the white channel when present.");
            case 3: return PARAM_DESC_DE_EN("Helligkeit zwischen den Glimmphasen (0=dunkel).", "Brightness between twinkles (0=dark).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return ParameterType::PARAM_ENUM;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint8_t getEnumValueCount(uint8_t index) const override
    {
        if (index == 2) return 4;
        return 0;
    }

    const char* getEnumValueName(uint8_t index, uint8_t value) const override
    {
        if (index == 2)
        {
            switch (value)
            {
                case 0: return "Primärfarbe (RGB)";
                case 1: return "Lichterkette";
                case 2: return "Festlich";
                case 3: return "Warmweiß";
                default: return nullptr;
            }
        }
        return nullptr;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 96;
            case 1: return 96;
            case 2: return 1;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t) const override { return 0; }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 2: return 3;
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
            case 1: return config.option1;
            case 2: return config.mode;
            case 3: return config.option2;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;
            case 1: config.option1 = static_cast<uint8_t>(value); break;
            case 2: config.mode = static_cast<uint8_t>(value); break;
            case 3: config.option2 = static_cast<uint8_t>(value); break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& cfg = segment->getConfig();
        const auto* strip = segment->getVirtualStrip();
        const bool hasWhite = strip && strip->getBytesPerLed() >= 4;
        auto& timeMs = segment->effectScratch<State>().timeMs;
        timeMs += deltaTime;

        const uint16_t length = segment->getLength();
        constexpr uint8_t cycleShift = 18;
        for (uint16_t pixel = 0; pixel < length; ++pixel)
        {
            const uint32_t seed = hash(uint32_t(pixel) + 1);
            const uint32_t rate = (cfg.speed + 24u) * (12u + ((seed >> 21) & 7u)) / 16u;
            const uint32_t clock = timeMs * rate + seed;
            const uint32_t cycle = clock >> cycleShift;
            const uint8_t phase = (clock >> (cycleShift - 8)) & 255u;
            const uint8_t chance = hash(seed + cycle) & 255u;
            const uint8_t pulse = cfg.option1 == 255 || chance < cfg.option1 ? envelope(phase) : 0;
            const uint8_t level = scale(cfg.option2 + scale(255 - cfg.option2, pulse), cfg.intensity);
            const Color color = pixelColor(cfg, seed, hasWhite);
            segment->setPixel(pixel, scale(color.r, level), scale(color.g, level),
                              scale(color.b, level), scale(color.w, level), 0);
        }
    }
};
