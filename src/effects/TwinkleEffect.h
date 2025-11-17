/**
 * @file TwinkleEffect.h
 * @brief Twinkle/Sparkle effects - Random sparkles and twinkling stars
 *
 * Creates random sparkles and twinkles like a starry night sky.
 * Based on FastLED's addGlitter function and twinkle examples.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "Effect.h"
#include "FastLEDMath.h"
#include "../Segment.h"

/**
 * Twinkle Effect
 * 
 * Creates random sparkles that fade in and out, like twinkling stars.
 */
class TwinkleEffect : public Effect
{
private:
    uint8_t _hue;
    
public:
    TwinkleEffect() : _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        if (length == 0) return;
        
        // Use option1 for fade rate (200-240, default 220)
        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 6) : 220;
        fadeRate = fadeRate > 240 ? 240 : fadeRate;
        
        // Use option2 for density/probability (10-200, default 100)
        uint8_t density = config.option2 > 0 ? config.option2 : 100;
        density = density > 200 ? 200 : density;
        
        // Use feature1 for color mode (0=set color/white, 1=rainbow)
        bool rainbowMode = config.feature1;
        
        // Use feature2 for variable brightness (0=fixed, 1=variable)
        bool variableBrightness = config.feature2;
        
        // Fade all pixels gradually
        for (uint16_t i = 0; i < length; i++) {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            
            // Apply configurable fade rate
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            
            segment->setPixel(i, r, g, b);
        }
        
        // Calculate sparkle probability based on speed and density
        uint8_t chanceOfTwinkle = ((config.speed * density) / 255) / 2;
        
        // Add random twinkles
        if (FastLEDMath::random8() < chanceOfTwinkle) {
            int pos = FastLEDMath::random16(length);
            
            uint8_t r, g, b;
            if (rainbowMode) {
                // Rainbow mode
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                    FastLEDMath::random8(), 255, config.intensity);
                r = (rgb >> 16) & 0xFF;
                g = (rgb >> 8) & 0xFF;
                b = rgb & 0xFF;
            } else if (config.r == 0 && config.g == 0 && config.b == 0) {
                // No color configured, use white
                uint8_t brightness = variableBrightness ? 
                    FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);
                r = g = b = brightness;
            } else {
                // Use configured color
                uint8_t brightness = variableBrightness ?
                    FastLEDMath::random8(128, 255) : 255;
                brightness = FastLEDMath::scale8(brightness, config.intensity);
                
                r = FastLEDMath::scale8(config.r, brightness);
                g = FastLEDMath::scale8(config.g, brightness);
                b = FastLEDMath::scale8(config.b, brightness);
            }
            
            segment->setPixel(pos, r, g, b);
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName() override
    {
        return "Twinkle";
    }
};

/**
 * Sparkle Effect
 * 
 * Similar to twinkle but with more random, party-like sparkles.
 */
class SparkleEffect : public Effect
{
private:
    uint8_t _hue;
    
public:
    SparkleEffect() : _hue(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        if (length == 0) return;
        
        // Use option1 for fade rate (180-220, default 200)
        uint8_t fadeRate = config.option1 > 0 ? (180 + config.option1 / 6) : 200;
        fadeRate = fadeRate > 220 ? 220 : fadeRate;
        
        // Use option2 for sparkle count (1-8, default based on speed)
        uint8_t sparkleCount = config.option2 > 0 ? 
            (1 + config.option2 / 32) : (1 + config.speed / 64);
        sparkleCount = sparkleCount > 8 ? 8 : sparkleCount;
        
        // Use option3 for probability (50-200, default 100)
        uint8_t probability = config.option3 > 0 ? config.option3 : 100;
        probability = probability > 200 ? 200 : probability;
        
        // Use feature1 for color mode (0=set color/rainbow, 1=white only)
        bool whiteOnly = config.feature1;
        
        // Use feature2 for burst mode (0=continuous, 1=burst)
        bool burstMode = config.feature2;
        
        // Fade all pixels more aggressively than twinkle
        for (uint16_t i = 0; i < length; i++) {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            
            // Apply configurable fade rate
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            
            segment->setPixel(i, r, g, b);
        }
        
        // Add multiple sparkles
        uint8_t actualSparkles = burstMode ? 
            (FastLEDMath::random8() < 50 ? sparkleCount * 2 : 0) : sparkleCount;
            
        for (uint8_t i = 0; i < actualSparkles; i++) {
            if (FastLEDMath::random8() < probability) {
                int pos = FastLEDMath::random16(length);
                
                uint8_t r, g, b;
                if (whiteOnly) {
                    // White sparkles only
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = g = b = brightness;
                } else if (config.r == 0 && config.g == 0 && config.b == 0) {
                    // No color configured, use random rainbow colors
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(
                        FastLEDMath::random8(), 255, config.intensity);
                    r = (rgb >> 16) & 0xFF;
                    g = (rgb >> 8) & 0xFF;
                    b = rgb & 0xFF;
                } else {
                    // Use configured color with random brightness
                    uint8_t brightness = FastLEDMath::random8(64, 255);
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    
                    r = FastLEDMath::scale8(config.r, brightness);
                    g = FastLEDMath::scale8(config.g, brightness);
                    b = FastLEDMath::scale8(config.b, brightness);
                }
                
                segment->setPixel(pos, r, g, b);
            }
        }
    }

    void reset() override
    {
        _hue = 0;
    }

    const char* getName() override
    {
        return "Sparkle";
    }
};