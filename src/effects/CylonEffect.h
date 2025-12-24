/**
 * @file CylonEffect.h
 * @brief Cylon/Knight Rider effect - STATELESS
 *
 * A single LED eye bouncing back and forth with a trailing fade.
 * Classic "KITT" effect from Knight Rider / Cylon from Battlestar Galactica.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Parameters:
 *   [0] Speed (0-255) - Movement speed (0 = auto-scale)
 *   [1] Hue (0-255) - Eye color
 *   [2] EyeSize (1-10) - Size of the eye
 *   [3] FadeAmount (1-255) - Trail fade speed
 *
 * Config usage (ETS mapping):
 *   - config.speed      -> Speed (movement speed)
 *   - config.option1    -> Hue (eye color)
 *   - config.option2    -> EyeSize (1-10, 0 => default)
 *   - config.option3    -> FadeAmount (trail fade, 0 => default)
 *   - config.intensity  -> Brightness (0-255)
 *   - config.reverse    -> Invert initial direction while keeping bounce logic
 *   - config.feature2   -> hsv2rgb_rainbow yellow brightness compensation
 *   - config.feature3   -> hsv2rgb_rainbow green correction hooks
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class CylonEffect : public Effect
{
  public:
    CylonEffect() = default;

    const char* getName() override { return "Cylon"; }
    const char* getDescription() override { return "Bouncing eye with trailing fade (KITT/BSG)"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "EyeSize";
            case 3: return "FadeAmount";
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
            case 0: return 0;  // Speed (0 = auto)
            case 1: return 0;  // Hue (red)
            case 2: return 4;  // EyeSize
            case 3: return 40; // FadeAmount
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;   // Speed
            case 1: return cfg.option1; // Hue
            case 2: return cfg.option2; // EyeSize
            case 3: return cfg.option3; // FadeAmount
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = static_cast<uint8_t>(value); break;   // Speed
            case 1: cfg.option1 = static_cast<uint8_t>(value); break; // Hue
            case 2: cfg.option2 = static_cast<uint8_t>(value); break; // EyeSize
            case 3: cfg.option3 = static_cast<uint8_t>(value); break; // FadeAmount
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        const uint8_t speedIn = config.speed;        // Speed (0=auto)
        const uint8_t hueIn = config.option1;        // Hue
        const uint8_t eyeSizeIn = config.option2;    // EyeSize (0 => default)
        const uint8_t fadeIn = config.option3;       // FadeAmount (0 => default)
        const uint8_t brightness = config.intensity; // Brightness (0..255)

        const bool reverse = (config.reverse != 0);
        const bool yellowBoost = config.feature2;
        const bool greenCorr = config.feature3;

        // Apply sane defaults/clamps
        const uint8_t eyeSize = (eyeSizeIn == 0) ? 4 : ((eyeSizeIn < 1) ? 1 : (eyeSizeIn > 10 ? 10 : eyeSizeIn));
        const uint8_t fadeAmount = (fadeIn == 0) ? 40 : fadeIn;
        const uint8_t speed = speedIn;
        const uint8_t hue = hueIn;

        // Position stored in position field (float position, range 0.0 to length-1.0)
        // Stored as fixed-point: position * 16
        uint16_t posScaled = state.position & 0x7FFF; // Lower 15 bits = position * 16
        int8_t dirStored = (state.position & 0x8000) ? -1 : 1;
        int8_t direction = reverse ? (int8_t)-dirStored : dirStored;
        float position = (float)posScaled / 16.0f;

        // Calculate effective speed
        float effectiveSpeed = (speed > 0) ? (speed / 10.0f) : (length / 125.0f);
        if (effectiveSpeed < 0.5f) effectiveSpeed = 0.5f;

        // Fade all LEDs
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            if (segment->getPixel(i, r, g, b))
            {
                r = FastLEDMath::fadeToBlackBy(r, fadeAmount);
                g = FastLEDMath::fadeToBlackBy(g, fadeAmount);
                b = FastLEDMath::fadeToBlackBy(b, fadeAmount);
                segment->setPixel(i, r, g, b);
            }
        }

        // Draw the eye at current position
        uint16_t pixelPos = (uint16_t)position;
        for (uint8_t i = 0; i < eyeSize; i++)
        {
            int16_t pos = pixelPos + i - (eyeSize / 2);
            if (pos >= 0 && pos < length)
            {
                uint8_t eyeBrightness = brightness;
                if (i == 0 || i == eyeSize - 1)
                {
                    eyeBrightness = brightness / 2; // Dimmer edges
                }

                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, eyeBrightness, yellowBoost, greenCorr);
                segment->setPixel(pos,
                                  (rgb >> 16) & 0xFF,
                                  (rgb >> 8) & 0xFF,
                                  rgb & 0xFF);
            }
        }

        // Move the eye
        position += effectiveSpeed * direction;

        // Bounce at edges
        if (position >= length - 1)
        {
            position = length - 1;
            direction = -1;
        }
        else if (position <= 0)
        {
            position = 0;
            direction = 1;
        }

        // Store position (scaled by 16, max 32767/16 = 2047 LEDs)
        posScaled = (uint16_t)(position * 16.0f) & 0x7FFF;
        int8_t dirToStore = reverse ? (int8_t)-direction : direction;
        state.position = posScaled | (dirToStore < 0 ? 0x8000 : 0);
    }

    void reset() override {}
};
