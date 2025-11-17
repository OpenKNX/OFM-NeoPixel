/**
 * @file TheaterChaseEffect.h
 * @brief Theater Chase effect - Classic chase pattern like old theater marquee
 *
 * Classic theater chase effect with configurable spacing and colors.
 * Based on Adafruit NeoPixel examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "Effect.h"
#include "FastLEDMath.h"
#include "../Segment.h"

/**
 * Theater Chase Effect
 * 
 * Creates a chase pattern where every Nth pixel is lit, creating
 * a "marching ants" effect like old theater marquees.
 */
class TheaterChaseEffect : public Effect
{
private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing;      // Space between lit pixels (default 3)
    
public:
    TheaterChaseEffect() : _lastUpdate(0), _position(0), _spacing(3)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Calculate update interval based on speed (map 0-255 to 20-200ms)
        uint32_t interval = 20 + ((255 - config.speed) * 180) / 255;
        
        uint32_t now = millis();
        if (now - _lastUpdate < interval) {
            return; // Not time to update yet
        }
        _lastUpdate = now;
        
        // Use option1 for spacing (1-10, default 3)
        uint8_t spacing = config.option1 > 0 ? config.option1 : 3;
        spacing = spacing > 10 ? 10 : spacing; // Clamp to reasonable max
        
        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize; // Clamp to reasonable max
        
        // Use feature1 for trail mode (0=off, 1=on)
        bool trailMode = config.feature1;
        
        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++) {
            if (trailMode) {
                // Fade existing pixels for trail effect
                uint32_t color = segment->getPixel(i);
                uint8_t r = ((color >> 16) & 0xFF) * 0.85; // 15% fade
                uint8_t g = ((color >> 8) & 0xFF) * 0.85;
                uint8_t b = (color & 0xFF) * 0.85;
                segment->setPixel(i, r, g, b);
            } else {
                segment->setPixel(i, 0, 0, 0);
            }
        }
        
        // Light pixels starting from current position with dot size
        for (uint16_t i = _position; i < length; i += spacing) {
            // Apply master brightness to configured color
            uint8_t r = FastLEDMath::scale8(config.r, config.intensity);
            uint8_t g = FastLEDMath::scale8(config.g, config.intensity);
            uint8_t b = FastLEDMath::scale8(config.b, config.intensity);
            
            // Set multiple pixels for larger dot size
            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++) {
                segment->setPixel(i + d, r, g, b);
            }
        }
        
        // Advance position
        _position++;
        if (_position >= spacing) {
            _position = 0;
        }
    }

    void reset() override
    {
        _position = 0;
        _lastUpdate = 0;
    }

    const char* getName() override
    {
        return "Theater Chase";
    }
};

/**
 * Theater Chase Rainbow Effect
 * 
 * Same as theater chase but cycles through rainbow colors
 */
class TheaterChaseRainbowEffect : public Effect
{
private:
    uint32_t _lastUpdate;
    uint8_t _position;
    uint8_t _spacing;
    uint8_t _hue;
    
public:
    TheaterChaseRainbowEffect() : _lastUpdate(0), _position(0), _spacing(3), _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Calculate update interval based on speed
        uint32_t interval = 30 + ((255 - config.speed) * 170) / 255;
        
        uint32_t now = millis();
        if (now - _lastUpdate < interval) {
            return;
        }
        _lastUpdate = now;
        
        // Use option1 for spacing (1-10, default 3)
        uint8_t spacing = config.option1 > 0 ? config.option1 : 3;
        spacing = spacing > 10 ? 10 : spacing;
        
        // Use option2 for dot size (1-5, default 1)
        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize;
        
        // Use option3 for color change speed (1-20, default 5)
        uint8_t colorSpeed = config.option3 > 0 ? config.option3 : 5;
        colorSpeed = colorSpeed > 20 ? 20 : colorSpeed;
        
        // Use feature1 for trail mode (0=off, 1=on)
        bool trailMode = config.feature1;
        
        // Clear all pixels first (or fade if in trail mode)
        for (uint16_t i = 0; i < length; i++) {
            if (trailMode) {
                // Fade existing pixels for trail effect
                uint32_t color = segment->getPixel(i);
                uint8_t r = ((color >> 16) & 0xFF) * 0.85; // 15% fade
                uint8_t g = ((color >> 8) & 0xFF) * 0.85;
                uint8_t b = (color & 0xFF) * 0.85;
                segment->setPixel(i, r, g, b);
            } else {
                segment->setPixel(i, 0, 0, 0);
            }
        }
        
        // Light every spacing-th pixel with rainbow color
        for (uint16_t i = _position; i < length; i += spacing) {
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
            
            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;
            
            // Set multiple pixels for larger dot size
            for (uint8_t d = 0; d < dotSize && (i + d) < length; d++) {
                segment->setPixel(i + d, r, g, b);
            }
        }
        
        // Advance position and hue
        _position++;
        if (_position >= spacing) {
            _position = 0;
        }
        _hue += colorSpeed; // Configurable color change speed
    }

    void reset() override
    {
        _position = 0;
        _hue = 0;
        _lastUpdate = 0;
    }

    const char* getName() override
    {
        return "Theater Chase Rainbow";
    }
};