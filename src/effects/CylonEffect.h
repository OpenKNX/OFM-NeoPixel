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
        auto& state = segment->getState();
        switch (index)
        {
            case 0: return state.aux1;    // Speed
            case 1: return state.aux2;    // Hue
            case 2: return state.phase;   // EyeSize
            case 3: return state.counter; // FadeAmount
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& state = segment->getState();
        switch (index)
        {
            case 0: state.aux1 = value; break;    // Speed
            case 1: state.aux2 = value; break;    // Hue
            case 2: state.phase = value; break;   // EyeSize
            case 3: state.counter = value; break; // FadeAmount
        }
    }

    // ====================================================================
    // Update
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        // Get parameters from state
        uint8_t speed = state.aux1;         // Speed parameter
        uint8_t hue = state.aux2;           // Hue parameter
        uint8_t eyeSize = state.phase;      // EyeSize parameter
        uint8_t fadeAmount = state.counter; // FadeAmount parameter
        uint8_t brightness = config.intensity;

        // Position stored in position field (float position, range 0.0 to length-1.0)
        // Stored as fixed-point: position * 16
        uint16_t posScaled = state.position & 0x7FFF; // Lower 15 bits = position * 16
        int8_t direction = (state.position & 0x8000) ? -1 : 1;
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

                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, eyeBrightness);
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
        state.position = posScaled | (direction < 0 ? 0x8000 : 0);
    }

    void reset() override {}
};
