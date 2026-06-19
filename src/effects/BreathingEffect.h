/**
 * @file BreathingEffect.h
 * @brief Breathing effect - smooth fade in/out with selectable waveform
 *
 * Creates a breathing effect where the entire strip fades in and out.
 *
 * Consolidated effect: the former "Pulse" effect is now the Waveform=2/3
 * (Pulse / Sharp Pulse) variant of this effect.
 *   Waveform 0 (Soft):       smooth sine breathing
 *   Waveform 1 (HoldAtPeak): breathing with a pause at the peak
 *   Waveform 2 (Pulse):      dramatic pulse with adjustable width
 *   Waveform 3 (SharpPulse): pulse with a sharp/dramatic start
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Breathing Effect (Breathing / Pulse)
 *
 * Config usage:
 *   - config.speed     : breathing/pulse rate
 *   - config.option1   : breathing min brightness / pulse width
 *   - config.option2   : curve (breathing) / gamma (pulse)
 *   - config.feature2  : rainbow color cycle
 *   - config.mode      : waveform 0=Soft, 1=HoldAtPeak, 2=Pulse, 3=SharpPulse
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : color (unless rainbow mode)
 */
class BreathingEffect : public Effect
{
  private:
    uint32_t _time;

  public:
    BreathingEffect() : _time(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        _time += deltaTime;

        const uint8_t waveform = config.mode; // 0=Soft,1=HoldAtPeak,2=Pulse,3=SharpPulse
        const bool pulseStyle = (waveform >= 2);
        const bool rainbowMode = config.feature2;

        uint8_t value; // 0..255 brightness envelope

        if (!pulseStyle)
        {
            // ── Breathing waveforms ──
            uint8_t minBrightness = config.option1 > 0 ? config.option1 : 0;
            uint8_t maxBrightness = 255;
            uint8_t curve = config.option2;
            bool holdAtPeak = (waveform == 1);

            uint16_t bpm = 5 + ((config.speed * 55) / 255);

            uint8_t breath;
            if (holdAtPeak)
            {
                uint8_t rawBreath = FastLEDMath::beatsin8(bpm, 0, 255);
                if (rawBreath > 200)
                    breath = 255;
                else
                    breath = FastLEDMath::scale8(rawBreath, 200);
            }
            else
            {
                breath = FastLEDMath::beatsin8(bpm, minBrightness, maxBrightness);
            }

            if (curve > 0)
            {
                uint8_t curved = FastLEDMath::scale8(breath, breath);
                breath = FastLEDMath::lerp8by8(breath, curved, curve);
            }
            value = breath;
        }
        else
        {
            // ── Pulse waveforms ──
            uint8_t pulseWidth = config.option1 > 0 ? config.option1 : 100;
            pulseWidth = pulseWidth < 10 ? 10 : (pulseWidth > 200 ? 200 : pulseWidth);
            uint8_t gamma = config.option2 > 0 ? config.option2 : 128;
            bool sharpPulse = (waveform == 3);

            uint16_t bpm = 10 + ((config.speed * 110) / 255);

            uint8_t minVal = sharpPulse ? 0 : (255 - pulseWidth) / 2;
            uint8_t maxVal = sharpPulse ? pulseWidth : 255;

            uint8_t pulse = FastLEDMath::beatsin8(bpm, minVal, maxVal);

            if (gamma > 0)
            {
                uint8_t squared = FastLEDMath::scale8(pulse, pulse);
                pulse = FastLEDMath::lerp8by8(pulse, squared, gamma);
            }
            value = pulse;
        }

        // Scale the configured brightness by the envelope
        uint8_t currentBrightness = FastLEDMath::scale8(config.intensity, value);

        uint8_t r, g, b, ww, cw;
        if (rainbowMode)
        {
            uint8_t hueDiv = pulseStyle ? 30 : 50;
            uint8_t hue = (_time / hueDiv) % 256;
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
            ww = 0;
            cw = 0;
        }
        else
        {
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
        return "Breathing";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Sanftes Ein-/Ausatmen mit wählbarer Wellenform (weich, Halt, Puls, scharfer Puls)",
            "Smooth fade in/out with selectable waveform (soft, hold, pulse, sharp pulse)");
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "MinBrightness";
            case 2: return "Curve";
            case 3: return "RainbowMode";
            case 4: return "Waveform";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Atem-/Pulsgeschwindigkeit (höher=schneller)", "Speed: Breathing/pulse rate (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Min. Helligkeit (Atmen) / Pulsbreite (Puls)", "Min brightness (breathing) / pulse width (pulse)");
            case 2: return PARAM_DESC_DE_EN("Kurve (Atmen) / Gamma (Puls): 0=linear, 255=stark", "Curve (breathing) / gamma (pulse): 0=linear, 255=strong");
            case 3: return PARAM_DESC_DE_EN("Regenbogen: Farbe wechselt", "Rainbow: color cycles");
            case 4: return PARAM_DESC_DE_EN("Wellenform: 0=Weich, 1=Halt am Höhepunkt, 2=Puls, 3=Scharfer Puls", "Waveform: 0=Soft, 1=Hold at peak, 2=Pulse, 3=Sharp pulse");
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
            case 3: return ParameterType::PARAM_BOOL;  // RainbowMode
            case 4: return ParameterType::PARAM_ENUM;  // Waveform
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 4) return nullptr;
        switch (enumValue)
        {
            case 0: return "Weich";
            case 1: return "Halt am Hoehepunkt";
            case 2: return "Puls";
            case 3: return "Scharfer Puls";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 4) ? 4 : 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 0;   // MinBrightness
            case 2: return 0;   // Curve (linear)
            case 3: return 0;   // RainbowMode off
            case 4: return 0;   // Waveform (Soft)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // MinBrightness min
            case 2: return 0; // Curve min
            case 3: return 0; // RainbowMode false
            case 4: return 0; // Waveform min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // MinBrightness max
            case 2: return 255; // Curve max
            case 3: return 1;   // RainbowMode true
            case 4: return 3;   // Waveform max (0..3)
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
            case 1: return config.option1;  // MinBrightness / PulseWidth
            case 2: return config.option2;  // Curve / Gamma
            case 3: return config.feature2; // RainbowMode
            case 4: return config.mode;     // Waveform
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
            case 2: config.option2 = static_cast<uint8_t>(value); break;
            case 3: config.feature2 = static_cast<bool>(value); break;
            case 4: config.mode = static_cast<uint8_t>(value); break;
            default: break;
        }
    }
};
