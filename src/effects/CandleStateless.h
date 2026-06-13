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
        // O(1) stateless sampling: time is divided into slots; each slot has a
        // deterministic random target brightness, sampled values interpolate
        // linearly between consecutive targets. Slot length follows the speed
        // factor (2..16 frames), matching the original fade-step durations.
        const uint8_t slotShift = speedFactor(speed); // 1..4 -> 2..16 frames per slot
        const uint32_t slotLen = 1UL << slotShift;

        // De-phase pixels so slot boundaries don't align across the strip
        const uint32_t shifted = absoluteFrame + (seed & (slotLen - 1U));
        const uint32_t slot = shifted >> slotShift;
        const uint32_t frac = shifted & (slotLen - 1U);

        const uint32_t phaseA = slot % cycleSegments;
        const uint32_t phaseB = (slot + 1U) % cycleSegments;
        const uint8_t a = nextTarget(seed, phaseA, intensity);
        const uint8_t b = nextTarget(seed, phaseB, intensity);

        const int32_t delta = static_cast<int32_t>(b) - static_cast<int32_t>(a);
        return static_cast<uint8_t>(static_cast<int32_t>(a) + ((delta * static_cast<int32_t>(frac)) >> slotShift));
    }

    inline uint32_t currentFrame()
    {
        return millis() / FRAME_TIME_MS;
    }

} // namespace CandleStateless
