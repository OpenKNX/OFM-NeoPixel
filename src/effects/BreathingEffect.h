/**
 * @file BreathingEffect.h
 * @brief Breathing effect - Smooth fade in and out like breathing
 *
 * Creates a breathing effect where the entire strip fades in and out smoothly.
 * Uses sine wave for natural breathing rhythm.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Breathing Effect
 *
 * Config usage:
 *  - intensity: max brightness (0 = off)
 *  - speed: breathing speed (mapped to BPM)
 *  - option1: minimum brightness floor
 *  - option2: curve shaping (0 = linear, 255 = more exponential)
 *  - feature1: hold at peak (pause at max)
 *  - feature2: rainbow mode (else fixed RGB color)
 *  - feature3: enables HSV->RGB correction flags (yellow + green)
 *
 * The entire strip fades in and out smoothly like breathing.
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

        const auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();

        // Snapshot config parameters for a consistent frame (values may change via KNX during runtime)
        const uint8_t speed = config.speed;            // breathing speed (mapped to BPM)
        const uint8_t intensity = config.intensity;    // max brightness (0=off)
        const uint8_t option1 = config.option1;        // minimum brightness floor
        const uint8_t option2 = config.option2;        // curve shaping (0 = linear, 255 = more exponential)
        const bool holdAtPeak = config.feature1;       // hold at peak (pause at max)
        const bool rainbowBreathing = config.feature2; // rainbow mode (else fixed RGB color)
        const bool corrFlags = config.feature3;        // enables HSV->RGB correction flags (yellow+green)
        const uint8_t baseR = config.r();
        const uint8_t baseG = config.g();
        const uint8_t baseB = config.b();

        _time += deltaTime;

        // If intensity is 0, treat as OFF
        if (intensity == 0)
        {
            for (uint16_t i = 0; i < length; i++)
                segment->setPixel(i, 0, 0, 0);
            return;
        }

        const uint8_t minBrightness = option1; // 0 is valid
        const uint8_t maxBrightness = intensity;
        const uint8_t curve = option2; // 0=linear

        // Calculate BPM from speed (map 0-255 to 5-60 breaths per minute)
        const uint16_t bpm = 5 + ((uint16_t)speed * 55u) / 255u;

        // Use beatsin8 to get smooth breathing pattern
        uint8_t breath;
        if (holdAtPeak)
        {
            // Modified breathing with pause at peak
            const uint8_t rawBreath = FastLEDMath::beatsin8(bpm, 0, 255);
            if (rawBreath > 200)
                breath = 255; // Hold at peak
            else
                breath = FastLEDMath::scale8(rawBreath, 200); // Scale normal breathing
        }
        else
        {
            breath = FastLEDMath::beatsin8(bpm, minBrightness, maxBrightness);
        }

        // Apply curve adjustment
        if (curve > 0)
        {
            const uint8_t curved = FastLEDMath::scale8(breath, breath); // square curve
            breath = FastLEDMath::lerp8by8(breath, curved, curve);
        }

        // Scale the configured brightness by the breathing pattern
        const uint8_t currentBrightness = FastLEDMath::scale8(intensity, breath);

        // Compute one color for the whole segment
        uint8_t r, g, b;
        if (rainbowBreathing)
        {
            const uint8_t hue = (uint8_t)((_time / 50u) & 0xFF); // slow hue cycle
            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness, corrFlags, corrFlags);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
        }
        else
        {
            r = FastLEDMath::scale8(baseR, currentBrightness);
            g = FastLEDMath::scale8(baseG, currentBrightness);
            b = FastLEDMath::scale8(baseB, currentBrightness);
        }

        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, r, g, b);
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName() override
    {
        return "Breathing";
    }

    const char* getDescription() override
    {
        return "Smooth breathing effect - fade in and out";
    }
};

/**
 * Strobe Effect
 *
 * Config usage:
 *  - intensity: ON brightness
 *  - speed: strobe period (higher = faster)
 *  - option1: ON ratio in percent (duty cycle)
 *  - option2: minimum brightness for OFF phase (0..255)
 *  - feature1: random timing jitter
 *  - feature2: rainbow strobe (else fixed RGB color)
 *  - feature3: enables HSV->RGB correction flags (yellow + green)
 *
 * Fast on/off flashing like a strobe light.
 */
class StrobeEffect : public Effect
{
  private:
    uint32_t _lastUpdate;
    bool _isOn;

  public:
    StrobeEffect() : _lastUpdate(0), _isOn(false)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)deltaTime;
        if (!segment) return;

        const auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();

        // Snapshot config parameters for a consistent frame
        const uint8_t speed = config.speed;         // strobe period (higher = faster)
        const uint8_t intensity = config.intensity; // ON brightness
        const uint8_t option1 = config.option1;     // ON ratio in percent (duty cycle)
        const uint8_t option2 = config.option2;     // minimum brightness for OFF phase (0..255)
        const bool randomTiming = config.feature1;  // random timing jitter
        const bool rainbowStrobe = config.feature2; // rainbow strobe (else fixed RGB color)
        const bool corrFlags = config.feature3;     // enables HSV->RGB correction flags (yellow + green)
        const uint8_t baseR = config.r();
        const uint8_t baseG = config.g();
        const uint8_t baseB = config.b();

        // Use option1 for on duration ratio (0..200, default 100)
        uint8_t onRatio = option1 ? option1 : 100;
        if (onRatio > 200) onRatio = 200;

        // Use option2 for minimum brightness when "off" (0..100)
        uint8_t minBrightness = option2;
        if (minBrightness > 100) minBrightness = 100;

        // Calculate strobe interval based on speed (map 0-255 to 10-500ms)
        const uint32_t baseInterval = 10u + ((uint32_t)(255u - speed) * 490u) / 255u;
        const uint32_t interval = randomTiming ? (baseInterval / 2u + FastLEDMath::random16(baseInterval)) : baseInterval;

        const uint32_t now = millis();
        if (now - _lastUpdate >= interval)
        {
            _lastUpdate = now;
            _isOn = !_isOn;
        }

        // Calculate on/off durations based on ratio
        const uint32_t currentPhase = (now - _lastUpdate);
        const uint32_t onDuration = (interval * onRatio) / 200u; // Convert percentage to duration
        const bool shouldBeOn = currentPhase < onDuration;

        uint8_t r, g, b;
        if (_isOn && shouldBeOn)
        {
            // Full brightness when on
            if (rainbowStrobe)
            {
                const uint8_t hue = (uint8_t)((now / 100u) & 0xFF);
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, intensity, corrFlags, corrFlags);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else
            {
                r = FastLEDMath::scale8(baseR, intensity);
                g = FastLEDMath::scale8(baseG, intensity);
                b = FastLEDMath::scale8(baseB, intensity);
            }
        }
        else
        {
            // Off or dim
            const uint8_t dimBrightness = FastLEDMath::scale8(intensity, minBrightness);
            if (rainbowStrobe)
            {
                const uint8_t hue = (uint8_t)((now / 100u) & 0xFF);
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, dimBrightness, corrFlags, corrFlags);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            }
            else
            {
                r = FastLEDMath::scale8(baseR, dimBrightness);
                g = FastLEDMath::scale8(baseG, dimBrightness);
                b = FastLEDMath::scale8(baseB, dimBrightness);
            }
        }

        // Apply to all pixels
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, r, g, b);
    }

    void reset() override
    {
        _lastUpdate = 0;
        _isOn = false;
    }

    const char* getName() override
    {
        return "Strobe";
    }

    const char* getDescription() override
    {
        return "Fast on/off flashing strobe light";
    }
};

/**
 * Pulse Effect
 *
 * Config usage:
 *  - intensity: max brightness
 *  - speed: pulse speed
 *  - option1: pulse width
 *  - option2: base hue offset / secondary shaping (if used)
 *  - feature1: sharp pulse (hard edges) vs smooth
 *  - feature2: rainbow pulse (else fixed RGB color)
 *  - feature3: enables HSV->RGB correction flags (yellow + green)
 *
 * Similar to breathing but with adjustable pulse width and more dramatic effect.
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

        const auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();

        // Snapshot config parameters for a consistent frame
        const uint8_t speed = config.speed;         // pulse speed
        const uint8_t intensity = config.intensity; // max brightness
        const uint8_t option1 = config.option1;     // pulse width
        const uint8_t option2 = config.option2;     // base hue offset / secondary shaping (if used)
        const bool sharpPulse = config.feature1;    // sharp pulse (hard edges) vs smooth
        const bool rainbowPulse = config.feature2;  // rainbow pulse (else fixed RGB color)
        const bool corrFlags = config.feature3;     // enables HSV->RGB correction flags (yellow + green)
        const uint8_t baseR = config.r();
        const uint8_t baseG = config.g();
        const uint8_t baseB = config.b();

        _time += deltaTime;

        // Use option1 for pulse width (10-200, default 100)
        uint8_t pulseWidth = option1 ? option1 : 100;
        if (pulseWidth < 10) pulseWidth = 10;
        else if (pulseWidth > 200)
            pulseWidth = 200;

        // Use option2 for gamma correction strength (0-255, default 128)
        const uint8_t gamma = option2 ? option2 : 128;

        // Calculate pulse rate from speed (map 0-255 to 10-120 BPM)
        const uint16_t bpm = 10u + ((uint16_t)speed * 110u) / 255u;

        // Calculate pulse range based on width
        const uint8_t minVal = sharpPulse ? 0 : (uint8_t)((255u - pulseWidth) / 2u);
        const uint8_t maxVal = sharpPulse ? pulseWidth : 255;

        uint8_t pulse = FastLEDMath::beatsin8(bpm, minVal, maxVal);

        // Apply gamma correction
        if (gamma > 0)
        {
            const uint8_t squared = FastLEDMath::scale8(pulse, pulse);
            pulse = FastLEDMath::lerp8by8(pulse, squared, gamma);
        }

        const uint8_t currentBrightness = FastLEDMath::scale8(intensity, pulse);

        uint8_t r, g, b;
        if (rainbowPulse)
        {
            const uint8_t hue = (uint8_t)((_time / 30u) & 0xFF);
            const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness, corrFlags, corrFlags);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
        }
        else
        {
            r = FastLEDMath::scale8(baseR, currentBrightness);
            g = FastLEDMath::scale8(baseG, currentBrightness);
            b = FastLEDMath::scale8(baseB, currentBrightness);
        }

        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, r, g, b);
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName() override
    {
        return "Pulse";
    }

    const char* getDescription() override
    {
        return "Dramatic pulse with adjustable width";
    }
};