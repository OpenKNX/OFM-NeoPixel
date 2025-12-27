/**
 * @file MeteorEffect.h
 * @brief Meteor effect - Random meteor shower with variable size
 *
 * Similar to comet but with random size meteors and random decay.
 * Creates a meteor shower effect with falling objects.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Meteor Effect
 *
 * Similar to comet but with random size meteors and random decay.
 */
/**
 * Config usage:
 *   - config.speed     : fall speed of meteors (higher=faster, affects movement interval)
 *   - config.option1   : meteor size (0-255, maps to min size 2-15 LEDs)
 *   - config.option2   : meteor frequency (0-255, maps to 100-5000ms interval)
 *   - config.feature1  : random colors (0=set color, 1=random per meteor)
 *   - config.feature2  : multi meteor (0=single meteor, 1=multiple meteors)
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : meteor color (if all 0 or random mode, uses random colors)
 */
class MeteorEffect : public Effect
{
  private:
    int16_t _position;
    int8_t _direction;
    uint32_t _lastUpdate;
    uint8_t _hue;
    uint8_t _meteorSize;
    uint32_t _nextMeteor;

  public:
    MeteorEffect() : _position(-1), _direction(1), _lastUpdate(0), _hue(0), _meteorSize(5), _nextMeteor(0)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        if (length == 0) return;

        // Use option1 for meteor size range (2-15, default 3-8)
        uint8_t minSize = config.option1 > 0 ? (2 + config.option1 / 20) : 3;
        uint8_t maxSize = minSize + 5;
        maxSize = maxSize > 15 ? 15 : maxSize;

        // Use option2 for meteor frequency (100-5000ms, default based on speed)
        uint32_t baseInterval = config.option2 > 0 ? (100 + config.option2 * 19) : (1000 + ((255 - config.speed) * 4000) / 255);

        // Use feature1 for random colors (0=set color, 1=random)
        bool randomColors = config.feature1;

        // Use feature2 for multiple meteors (0=single, 1=multiple)
        bool multiMeteor = config.feature2;

        uint32_t now = millis();

        // Start new meteor if needed
        if (_position < 0 && now >= _nextMeteor)
        {
            _position = length - 1;                               // Start from top
            _meteorSize = FastLEDMath::random8(minSize, maxSize); // Configurable meteor size

            if (randomColors)
            {
                _hue = FastLEDMath::random8(); // Random color
            }
            else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
            {
                _hue = FastLEDMath::random8(); // Default to random if no color set
            }

            // Schedule next meteor
            uint32_t interval = baseInterval;
            if (multiMeteor)
            {
                interval /= 3; // More frequent meteors
            }
            interval += FastLEDMath::random16(interval); // Add randomness
            _nextMeteor = now + interval;
        }

        // Update meteor position
        uint32_t moveInterval = 20 + ((255 - config.speed) * 80) / 255;
        if (now - _lastUpdate >= moveInterval && _position >= 0)
        {
            _lastUpdate = now;

            // Fade all pixels randomly for meteor rain effect
            for (uint16_t i = 0; i < length; i++)
            {
                if (FastLEDMath::random8() > 160)
                { // 60% chance to fade each pixel
                    uint8_t r, g, b;
                    segment->getPixel(i, r, g, b);

                    // Random fade amount
                    uint8_t fade = FastLEDMath::random8(200, 240);
                    r = FastLEDMath::scale8(r, fade);
                    g = FastLEDMath::scale8(g, fade);
                    b = FastLEDMath::scale8(b, fade);

                    segment->setPixel(i, r, g, b);
                }
            }

            // Draw meteor body
            for (uint8_t i = 0; i < _meteorSize; i++)
            {
                int16_t pos = _position + i;
                if (pos >= 0 && pos < length)
                {
                    uint8_t brightness = 255 - (i * 255 / _meteorSize); // Fade along body

                    uint8_t r, g, b;
                    if (randomColors || (config.r() == 0 && config.g() == 0 && config.b() == 0))
                    {
                        // Use random/rainbow hue
                        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255,
                                                                    FastLEDMath::scale8(brightness, config.intensity));
                        r = (rgb >> 16) & 0xFF;
                        g = (rgb >> 8) & 0xFF;
                        b = rgb & 0xFF;
                    }
                    else
                    {
                        // Use configured color
                        brightness = FastLEDMath::scale8(brightness, config.intensity);
                        r = FastLEDMath::scale8(config.r(), brightness);
                        g = FastLEDMath::scale8(config.g(), brightness);
                        b = FastLEDMath::scale8(config.b(), brightness);
                    }

                    segment->setPixel(pos, r, g, b);
                }
            }

            // Move meteor down
            _position--;

            // Remove meteor when it goes off screen
            if (_position + _meteorSize < 0)
            {
                _position = -1;
            }
        }
    }

    void reset() override
    {
        _position = -1;
        _direction = 1;
        _lastUpdate = 0;
        _hue = 0;
        _nextMeteor = 0;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Meteor";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Random meteor shower with variable size";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "MeteorSize";
            case 2: return "Frequency";
            case 3: return "RandomColors";
            case 4: return "MultiMeteor";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Fallgeschwindigkeit der Meteore (höher=schneller)", "Speed: Fall speed of meteors (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Meteorengröße: Min-Größe in LEDs (0-255)", "Meteor size: Min size in LEDs (0-255)");
            case 2: return PARAM_DESC_DE_EN("Häufigkeit: Wie oft Meteore erscheinen (0-255)", "Frequency: How often meteors appear (0-255)");
            case 3: return PARAM_DESC_DE_EN("Zufällige Farben bei jedem Meteor", "Random colors for each meteor");
            case 4: return PARAM_DESC_DE_EN("Mehrere Meteore gleichzeitig", "Multiple meteors at once");
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
            case 1: return 20;  // MeteorSize (min=3)
            case 2: return 0;   // Frequency (speed-based)
            case 3: return 0;   // RandomColors off
            case 4: return 0;   // MultiMeteor off
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // MeteorSize min (maps to 2)
            case 2: return 0; // Frequency min (auto)
            case 3: return 0; // RandomColors false
            case 4: return 0; // MultiMeteor false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // MeteorSize max (maps to ~15)
            case 2: return 255; // Frequency max (100ms)
            case 3: return 1;   // RandomColors true
            case 4: return 1;   // MultiMeteor true
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
            case 1: return config.option1;  // MeteorSize
            case 2: return config.option2;  // Frequency
            case 3: return config.feature1; // RandomColors
            case 4: return config.feature2; // MultiMeteor
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (fall speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // MeteorSize (head size)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Frequency (spawn rate)
            case 3: config.feature1 = static_cast<bool>(value); break;   // RandomColors
            case 4: config.feature2 = static_cast<bool>(value); break;   // MultiMeteor
            default: break;
        }
    }
};
