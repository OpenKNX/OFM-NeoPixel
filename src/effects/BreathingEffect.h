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
#include "Effect.h"
#include "FastLEDMath.h"
#include "../Segment.h"

/**
 * Breathing Effect
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
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        _time += deltaTime;
        
        // Use option1 for breathing depth (50-255, default full range)
        uint8_t minBrightness = config.option1 > 0 ? config.option1 : 0;
        uint8_t maxBrightness = 255;
        
        // Use option2 for breathing curve (0=linear, 255=exponential)
        uint8_t curve = config.option2;
        
        // Use feature1 for hold at peak (0=smooth, 1=hold)
        bool holdAtPeak = config.feature1;
        
        // Use feature2 for rainbow breathing (0=set color, 1=rainbow)
        bool rainbowBreathing = config.feature2;
        
        // Calculate BPM from speed (map 0-255 to 5-60 breaths per minute)
        uint16_t bpm = 5 + ((config.speed * 55) / 255);
        
        // Use beatsin8 to get smooth breathing pattern
        uint8_t breath;
        if (holdAtPeak) {
            // Modified breathing with pause at peak
            uint8_t rawBreath = FastLEDMath::beatsin8(bpm, 0, 255);
            if (rawBreath > 200) {
                breath = 255; // Hold at peak
            } else {
                breath = FastLEDMath::scale8(rawBreath, 200); // Scale normal breathing
            }
        } else {
            breath = FastLEDMath::beatsin8(bpm, minBrightness, maxBrightness);
        }
        
        // Apply curve adjustment
        if (curve > 0) {
            // Apply exponential curve for more natural breathing
            uint8_t curved = FastLEDMath::scale8(breath, breath);
            breath = FastLEDMath::lerp8by8(breath, curved, curve);
        }
        
        // Scale the configured brightness by the breathing pattern
        uint8_t currentBrightness = FastLEDMath::scale8(config.intensity, breath);
        
        // Apply to all pixels
        uint8_t r, g, b;
        if (rainbowBreathing) {
            // Rainbow breathing mode
            uint8_t hue = (_time / 50) % 256; // Slow hue cycle
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
        } else {
            // Use configured color
            r = FastLEDMath::scale8(config.r(), currentBrightness);
            g = FastLEDMath::scale8(config.g(), currentBrightness);
            b = FastLEDMath::scale8(config.b(), currentBrightness);
        }
        
        for (uint16_t i = 0; i < length; i++) {
            segment->setPixel(i, r, g, b);
        }
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName() override
    {
        return "Breathing";
    }
};

/**
 * Strobe Effect
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
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Use option1 for on duration ratio (10-200, default 50/50)
        uint8_t onRatio = config.option1 > 0 ? config.option1 : 100; // Percentage on time
        onRatio = onRatio > 200 ? 200 : onRatio;
        
        // Use option2 for minimum brightness when "off" (0-100, default 0)
        uint8_t minBrightness = config.option2 > 0 ? config.option2 : 0;
        minBrightness = minBrightness > 100 ? 100 : minBrightness;
        
        // Use feature1 for random timing (0=regular, 1=random)
        bool randomTiming = config.feature1;
        
        // Use feature2 for rainbow strobe (0=set color, 1=rainbow)
        bool rainbowStrobe = config.feature2;
        
        // Calculate strobe interval based on speed (map 0-255 to 10-500ms)
        uint32_t baseInterval = 10 + ((255 - config.speed) * 490) / 255;
        uint32_t interval = randomTiming ? 
            (baseInterval / 2 + FastLEDMath::random16(baseInterval)) : baseInterval;
        
        uint32_t now = millis();
        if (now - _lastUpdate >= interval) {
            _lastUpdate = now;
            _isOn = !_isOn;
        }
        
        // Calculate on/off durations based on ratio
        uint32_t currentPhase = (now - _lastUpdate);
        uint32_t onDuration = (interval * onRatio) / 200; // Convert percentage to duration
        bool shouldBeOn = currentPhase < onDuration;
        
        uint8_t r, g, b;
        if (_isOn && shouldBeOn) {
            // Full brightness when on
            if (rainbowStrobe) {
                uint8_t hue = (now / 100) % 256; // Fast hue cycling
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            } else {
                r = FastLEDMath::scale8(config.r(), config.intensity);
                g = FastLEDMath::scale8(config.g(), config.intensity);
                b = FastLEDMath::scale8(config.b(), config.intensity);
            }
        } else {
            // Off or dim
            uint8_t dimBrightness = FastLEDMath::scale8(config.intensity, minBrightness);
            if (rainbowStrobe) {
                uint8_t hue = (now / 100) % 256;
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, dimBrightness);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            } else {
                r = FastLEDMath::scale8(config.r(), dimBrightness);
                g = FastLEDMath::scale8(config.g(), dimBrightness);
                b = FastLEDMath::scale8(config.b(), dimBrightness);
            }
        }
        
        // Apply to all pixels
        for (uint16_t i = 0; i < length; i++) {
            segment->setPixel(i, r, g, b);
        }
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
};

/**
 * Pulse Effect
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
        if (gamma > 0) {
            uint8_t squared = FastLEDMath::scale8(pulse, pulse); // Square for gamma curve
            pulse = FastLEDMath::lerp8by8(pulse, squared, gamma);
        }
        
        // Scale the configured brightness by the pulse pattern
        uint8_t currentBrightness = FastLEDMath::scale8(config.intensity, pulse);
        
        // Apply to all pixels
        uint8_t r, g, b;
        if (rainbowPulse) {
            // Rainbow pulse mode
            uint8_t hue = (_time / 30) % 256; // Medium speed hue cycle
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, currentBrightness);
            r = (rgb >> 16) & 0xFF;
            g = (rgb >> 8) & 0xFF;
            b = rgb & 0xFF;
        } else {
            // Use configured color
            r = FastLEDMath::scale8(config.r(), currentBrightness);
            g = FastLEDMath::scale8(config.g(), currentBrightness);
            b = FastLEDMath::scale8(config.b(), currentBrightness);
        }
        
        for (uint16_t i = 0; i < length; i++) {
            segment->setPixel(i, r, g, b);
        }
    }

    void reset() override
    {
        _time = 0;
    }

    const char* getName() override
    {
        return "Pulse";
    }
};