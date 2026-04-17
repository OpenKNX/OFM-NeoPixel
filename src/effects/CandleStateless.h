#pragma once

#include "../Segment.h"
#include <Arduino.h>
#include <stdint.h>

namespace CandleStateless
{

    constexpr uint8_t CANDLE_SAMPLE_FPS = 42;
    constexpr uint32_t FRAME_TIME_MS = 1000U / CANDLE_SAMPLE_FPS;
    constexpr uint16_t SINGLE_CYCLE_SEGMENTS = 1024;
    constexpr uint16_t MULTI_CYCLE_SEGMENTS = 256;

    struct Colors
    {
        uint8_t pr;
        uint8_t pg;
        uint8_t pb;
        uint8_t pww;
        uint8_t pcw;
        uint8_t br;
        uint8_t bg;
        uint8_t bb;
        uint8_t bww;
        uint8_t bcw;
    };

    inline uint32_t mix32(uint32_t value)
    {
        value += 0x9E3779B9u;
        value = (value ^ (value >> 16)) * 0x7FEB352Du;
        value = (value ^ (value >> 15)) * 0x846CA68Bu;
        return value ^ (value >> 16);
    }

    inline uint32_t segmentSeed(const Segment* segment)
    {
        uint32_t seed = static_cast<uint32_t>(segment->getStartLed()) << 16;
        seed ^= static_cast<uint32_t>(segment->getEndLed());
        seed ^= static_cast<uint32_t>(segment->getLength()) << 8;
        seed ^= static_cast<uint32_t>(reinterpret_cast<uintptr_t>(segment->getVirtualStrip()));
        return mix32(seed);
    }

    inline uint8_t randomByte(uint32_t seed, uint32_t streamIndex)
    {
        return static_cast<uint8_t>(mix32(seed ^ (streamIndex * 0x9E3779B9u)) >> 24);
    }

    inline uint8_t randomRange(uint32_t seed, uint32_t streamIndex, uint8_t limit)
    {
        if (limit == 0) return 0;
        return static_cast<uint8_t>((static_cast<uint16_t>(randomByte(seed, streamIndex)) * limit) >> 8);
    }

    inline uint8_t speedFactor(uint8_t speed)
    {
        if (speed > 252) return 1;
        if (speed > 99) return 2;
        if (speed > 49) return 3;
        return 4;
    }

    inline uint8_t nextTarget(uint32_t seed, uint32_t phaseIndex, uint8_t intensity)
    {
        const uint8_t rndval = intensity >> 1;
        uint8_t target = randomRange(seed, 1U + phaseIndex * 3U, rndval) + randomRange(seed, 2U + phaseIndex * 3U, rndval);

        if (target < (rndval >> 1))
        {
            target = static_cast<uint8_t>((rndval >> 1) + randomRange(seed, 3U + phaseIndex * 3U, rndval));
        }

        return static_cast<uint8_t>(target + (255U - intensity));
    }

    inline uint8_t initialTarget(uint32_t seed)
    {
        return static_cast<uint8_t>(130U + randomRange(seed, 0, 4));
    }

    inline uint8_t computeFadeStep(uint8_t current, uint8_t target, uint8_t speedFactorValue)
    {
        const uint8_t diff = (target > current) ? (target - current) : (current - target);
        uint8_t step = diff >> speedFactorValue;
        if (step == 0) step = 1;
        return step;
    }

    inline uint16_t computeDuration(uint8_t current, uint8_t target, uint8_t fadeStep)
    {
        const uint8_t diff = (target > current) ? (target - current) : (current - target);
        if (diff == 0) return 1;
        return static_cast<uint16_t>((diff + fadeStep - 1U) / fadeStep);
    }

    inline uint8_t advanceBrightness(uint8_t current, uint8_t target, uint8_t fadeStep, uint16_t frameOffset)
    {
        const uint16_t delta = static_cast<uint16_t>(fadeStep) * static_cast<uint16_t>(frameOffset + 1U);

        if (target > current)
        {
            const uint16_t value = static_cast<uint16_t>(current) + delta;
            return static_cast<uint8_t>(value > 255U ? 255U : value);
        }

        return (delta >= current) ? 0U : static_cast<uint8_t>(current - delta);
    }

    inline uint8_t blendChannel(uint8_t background, uint8_t foreground, uint8_t amount)
    {
        const int16_t delta = static_cast<int16_t>(foreground) - static_cast<int16_t>(background);
        return static_cast<uint8_t>(static_cast<int16_t>(background) + ((delta * amount) >> 8));
    }

    inline Colors resolveColors(const EffectConfig& config)
    {
        Colors colors = {
            config.r(), config.g(), config.b(), config.ww(), config.cw(),
            config.r2(), config.g2(), config.b2(), config.ww2(), config.cw2()};

        if (colors.pr == 0 && colors.pg == 0 && colors.pb == 0 && colors.pww == 0 && colors.pcw == 0)
        {
            colors.pr = 255;
            colors.pg = 147;
            colors.pb = 41;
        }

        return colors;
    }

    inline uint8_t sampleBrightness(uint32_t seed, uint32_t absoluteFrame, uint8_t speed, uint8_t intensity,
                                    uint16_t cycleSegments)
    {
        const uint8_t speedFactorValue = speedFactor(speed);

        uint32_t totalFrames = 0;
        uint8_t current = 128;
        uint8_t target = initialTarget(seed);
        uint8_t fadeStep = 1;

        for (uint16_t phase = 0; phase < cycleSegments; phase++)
        {
            const uint16_t duration = computeDuration(current, target, fadeStep);
            totalFrames += duration;

            current = advanceBrightness(current, target, fadeStep, duration - 1U);
            target = nextTarget(seed, phase, intensity);
            fadeStep = computeFadeStep(current, target, speedFactorValue);
        }

        if (totalFrames == 0) return 128;

        uint32_t frameInCycle = absoluteFrame % totalFrames;
        current = 128;
        target = initialTarget(seed);
        fadeStep = 1;

        for (uint16_t phase = 0; phase < cycleSegments; phase++)
        {
            const uint16_t duration = computeDuration(current, target, fadeStep);
            if (frameInCycle < duration)
            {
                return advanceBrightness(current, target, fadeStep, static_cast<uint16_t>(frameInCycle));
            }

            frameInCycle -= duration;
            current = advanceBrightness(current, target, fadeStep, duration - 1U);
            target = nextTarget(seed, phase, intensity);
            fadeStep = computeFadeStep(current, target, speedFactorValue);
        }

        return current;
    }

    inline uint32_t currentFrame()
    {
        return millis() / FRAME_TIME_MS;
    }

} // namespace CandleStateless
