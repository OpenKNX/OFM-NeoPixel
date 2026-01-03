/**
 * @file PulseEffect.h
 * @brief Pulse effect - Dramatic pulse with adjustable width
 *
 * Similar to breathing but with adjustable pulse width and more dramatic effect.
 * Uses configurable gamma correction for natural looking pulses.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Pulse Effect
 *
 * Similar to breathing but with adjustable pulse width and more dramatic effect.
 */
/**
 * Config usage:
 *   - config.speed     : pulse rate (higher=faster, 0-255 maps to 10-120 BPM)
 *   - config.option1   : pulse width (10-200, default 100) - duration of pulse
 *   - config.option2   : gamma correction (0-255, default 128) - for natural pulse curve
 *   - config.feature1  : sharp pulse (0=smooth, 1=sharp/dramatic start)
 *   - config.feature2  : rainbow pulse (0=set color, 1=rainbow cycle)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : pulse color (unless rainbow mode)
 */
class PulseEffect : public Effect
{
  private:
    uint32_t _time;

  public:
    PulseEffect() : _time(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        _time += deltaTime;

        // Use option1 for pulse width (10-200, default 100)
        uint8_t pulseWidth = config.option1 > 0 ? config.option1 : 100;
        pulseWidth = pulseWidth < 10 ? 10 : (pulseWidth > 200 ? 200 : pulseWidth);

        // Use option2 for gamma correction strength (0-255, default 128)
        uint8_t gamma = config.option2 > 0 ? config.option2 : 128;

        // Use feature1 for sharp pulse (0=smooth, 1=sharp)
        bool sharpPulse = config.feature1;

        // Use feature2 for rainbow pulse (0=set color, 1=rainbow)
        bool rainbowPulse = config.feature2;

        // Calculate pulse rate from speed (map 0-255 to 10-120 BPM)
        uint16_t bpm = 10 + ((config.speed * 110) / 255);

        // Calculate pulse range based on width
        uint8_t minVal = sharpPulse ? 0 : (255 - pulseWidth) / 2;
        uint8_t maxVal = sharpPulse ? pulseWidth : 255;

        // Use beatsin8 with configurable range for pulse
        uint8_t pulse = FastLEDMath::beatsin8(bpm, minVal, maxVal);

        // Apply gamma correction for more natural looking pulse
        if (gamma > 0)
        {
            uint8_t squared = FastLEDMath::scale8(pulse, pulse); // Square for gamma curve
            pulse = FastLEDMath::lerp8by8(pulse, squared, gamma);
        }

        // Scale the configured brightness by the pulse pattern
        uint8_t currentBrightness = FastLEDMath::scale8(config.intensity, pulse);

        // Apply to all pixels
        uint8_t r, g, b, ww, cw;
        if (rainbowPulse)
        {
            // Rainbow pulse mode (no white channels)
            uint8_t hue = (_time / 30) % 256; // Medium speed hue cycle
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
            ww = 0;
            cw = 0;
        }
        else
        {
            // Use configured color with 5-channel support
            r = FastLEDMath::scale8(config.r(), currentBrightness);
            g = FastLEDMath::scale8(config.g(), currentBrightness);
            b = FastLEDMath::scale8(config.b(), currentBrightness);
            ww = FastLEDMath::scale8(config.ww(), currentBrightness);
            cw = FastLEDMath::scale8(config.cw(), currentBrightness);
        }

        for (uint16_t i = 0; i < length; i++)
        {
            segment->setPixel(i, r, g, b, ww, cw);
        }
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Pulse";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Dramatic pulse with adjustable width";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "PulseWidth";
            case 2: return "Gamma";
            case 3: return "SharpPulse";
            case 4: return "RainbowPulse";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Puls-Geschwindigkeit (höher=schneller)", "Speed: Pulse rate (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Pulsbreite: Wie lang der Puls dauert (10-200)", "Pulse width: How long the pulse lasts (10-200)");
            case 2: return PARAM_DESC_DE_EN("Gamma-Korrektur für natürlicheren Puls (0-255)", "Gamma correction for more natural pulse (0-255)");
            case 3: return PARAM_DESC_DE_EN("Scharfer Puls: Dramatischer Start", "Sharp pulse: Dramatic start");
            case 4: return PARAM_DESC_DE_EN("Regenbogen-Puls: Farbe wechselt", "Rainbow pulse: Color cycles");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8;
            case 1: return ParameterType::PARAM_UINT8;
            case 2: return ParameterType::PARAM_UINT8;
            case 3: return ParameterType::PARAM_BOOL;
            case 4: return ParameterType::PARAM_BOOL;
            default: return ParameterType::PARAM_UINT8;
        }
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 100; // PulseWidth
            case 2: return 128; // Gamma
            case 3: return 0;   // SharpPulse off
            case 4: return 0;   // RainbowPulse off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed min
            case 1: return 10; // PulseWidth min
            case 2: return 0;  // Gamma min
            case 3: return 0;  // SharpPulse false
            case 4: return 0;  // RainbowPulse false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 200; // PulseWidth max
            case 2: return 255; // Gamma max
            case 3: return 1;   // SharpPulse true
            case 4: return 1;   // RainbowPulse true
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;    // Speed
            case 1: return config.option1;  // PulseWidth
            case 2: return config.option2;  // Gamma
            case 3: return config.feature1; // SharpPulse
            case 4: return config.feature2; // RainbowPulse
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (pulse rate)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // PulseWidth (10-200)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Gamma (curve intensity)
            case 3: config.feature1 = static_cast<bool>(value); break;   // SharpPulse
            case 4: config.feature2 = static_cast<bool>(value); break;   // RainbowPulse
            default: break;
        }
    }
};
