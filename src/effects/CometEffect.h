/**
 * @file CometEffect.h
 * @brief Comet effect - moving dot with trailing tail (Comet / Meteor / Sinelon)
 *
 * Consolidated effect. Mode selects the movement/rendering style:
 *   Mode 0 (Comet):   moving dot with fading tail, wrap or bounce
 *   Mode 1 (Meteor):  random meteor shower, falling multi-meteors
 *   Mode 2 (Sinelon): colored dot sweeping with sine-wave / linear bounce motion
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Comet Effect (Comet / Meteor / Sinelon)
 *
 * Config usage:
 *   - config.speed     : movement speed (higher=faster)
 *   - config.option1   : Comet/Sinelon fade rate, Meteor meteor size
 *   - config.option2   : Comet tail length, Sinelon dot size, Meteor frequency
 *   - config.feature1  : Comet bounce, Sinelon rainbow, Meteor random colors
 *   - config.feature2  : Comet rainbow, Sinelon bounce, Meteor multi-meteor
 *   - config.mode      : 0=Comet, 1=Meteor, 2=Sinelon
 *   - config.intensity : master brightness (0-255)
 *   - config.r/g/b     : color (if all 0, uses cycling rainbow)
 */
class CometEffect : public Effect
{
  private:
    int16_t _position;
    int8_t _direction;
    uint32_t _lastUpdate;
    uint8_t _hue;
    uint8_t _tailLength;
    // Meteor-specific state
    uint8_t _meteorSize;
    uint32_t _nextMeteor;
    // Sinelon-specific state
    int16_t _sinePosition;
    bool _sineDirection;

  public:
    CometEffect()
        : _position(0), _direction(1), _lastUpdate(0), _hue(0), _tailLength(10),
          _meteorSize(5), _nextMeteor(0), _sinePosition(0), _sineDirection(true)
    {
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        if (segment->getLength() == 0) return;

        switch (segment->getConfig().mode)
        {
            case 1: updateMeteor(segment, deltaTime); break;
            case 2: updateSinelon(segment, deltaTime); break;
            default: updateComet(segment, deltaTime); break;
        }
    }

    // ── Mode 0: Comet ────────────────────────────────────────────────────
    void updateComet(Segment* segment, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 5) : 230;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        _tailLength = config.option2 > 0 ? config.option2 : 10;
        _tailLength = _tailLength < 5 ? 5 : (_tailLength > 30 ? 30 : _tailLength);

        bool bounceMode = config.feature1;
        bool rainbowMode = config.feature2;

        uint32_t interval = 10 + ((255 - config.speed) * 90) / 255;
        uint32_t now = millis();
        if (now - _lastUpdate < interval) return;
        _lastUpdate = now;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            segment->setPixel(i, r, g, b);
        }

        for (uint8_t t = 1; t <= _tailLength; t++)
        {
            int16_t tailPos = _position - (_direction * t);
            if (tailPos >= 0 && tailPos < length)
            {
                uint8_t brightness = 255 / (t + 1);
                uint8_t r, g, b;
                if (rainbowMode)
                {
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue - (t * 10), 255, FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
                }
                else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                {
                    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, FastLEDMath::scale8(brightness, config.intensity));
                    r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
                }
                else
                {
                    brightness = FastLEDMath::scale8(brightness, config.intensity);
                    r = FastLEDMath::scale8(config.r(), brightness);
                    g = FastLEDMath::scale8(config.g(), brightness);
                    b = FastLEDMath::scale8(config.b(), brightness);
                }
                segment->setPixel(tailPos, r, g, b);
            }
        }

        if (_position >= 0 && _position < length)
        {
            uint8_t r, g, b;
            if (rainbowMode || (config.r() == 0 && config.g() == 0 && config.b() == 0))
            {
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
                r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
                _hue += 2;
            }
            else
            {
                r = FastLEDMath::scale8(config.r(), config.intensity);
                g = FastLEDMath::scale8(config.g(), config.intensity);
                b = FastLEDMath::scale8(config.b(), config.intensity);
            }
            segment->setPixel(_position, r, g, b);
        }

        _position += _direction;
        if (bounceMode)
        {
            if (_position >= length) { _position = length - 1; _direction = -1; }
            else if (_position < 0) { _position = 0; _direction = 1; }
        }
        else
        {
            if (_position >= length) _position = 0;
            else if (_position < 0) _position = length - 1;
        }
    }

    // ── Mode 1: Meteor ───────────────────────────────────────────────────
    void updateMeteor(Segment* segment, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        uint8_t minSize = config.option1 > 0 ? (2 + config.option1 / 20) : 3;
        uint8_t maxSize = minSize + 5;
        maxSize = maxSize > 15 ? 15 : maxSize;

        uint32_t baseInterval = config.option2 > 0 ? (100 + config.option2 * 19) : (1000 + ((255 - config.speed) * 4000) / 255);

        bool randomColors = config.feature1;
        bool multiMeteor = config.feature2;

        uint32_t now = millis();

        if (_position < 0 && now >= _nextMeteor)
        {
            _position = length - 1;
            _meteorSize = FastLEDMath::random8(minSize, maxSize);

            if (randomColors)
                _hue = FastLEDMath::random8();
            else if (config.r() == 0 && config.g() == 0 && config.b() == 0)
                _hue = FastLEDMath::random8();

            uint32_t interval = baseInterval;
            if (multiMeteor) interval /= 3;
            interval += FastLEDMath::random16(interval);
            _nextMeteor = now + interval;
        }

        uint32_t moveInterval = 20 + ((255 - config.speed) * 80) / 255;
        if (now - _lastUpdate >= moveInterval && _position >= 0)
        {
            _lastUpdate = now;

            for (uint16_t i = 0; i < length; i++)
            {
                if (FastLEDMath::random8() > 160)
                {
                    uint8_t r, g, b;
                    segment->getPixel(i, r, g, b);
                    uint8_t fade = FastLEDMath::random8(200, 240);
                    r = FastLEDMath::scale8(r, fade);
                    g = FastLEDMath::scale8(g, fade);
                    b = FastLEDMath::scale8(b, fade);
                    segment->setPixel(i, r, g, b);
                }
            }

            for (uint8_t i = 0; i < _meteorSize; i++)
            {
                int16_t pos = _position + i;
                if (pos >= 0 && pos < length)
                {
                    uint8_t brightness = 255 - (i * 255 / _meteorSize);
                    uint8_t r, g, b;
                    if (randomColors || (config.r() == 0 && config.g() == 0 && config.b() == 0))
                    {
                        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, FastLEDMath::scale8(brightness, config.intensity));
                        r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
                    }
                    else
                    {
                        brightness = FastLEDMath::scale8(brightness, config.intensity);
                        r = FastLEDMath::scale8(config.r(), brightness);
                        g = FastLEDMath::scale8(config.g(), brightness);
                        b = FastLEDMath::scale8(config.b(), brightness);
                    }
                    segment->setPixel(pos, r, g, b);
                }
            }

            _position--;
            if (_position + _meteorSize < 0) _position = -1;
        }
    }

    // ── Mode 2: Sinelon ──────────────────────────────────────────────────
    void updateSinelon(Segment* segment, uint32_t deltaTime)
    {
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();

        uint8_t fadeRate = config.option1 > 0 ? (200 + config.option1 / 5) : 235;
        fadeRate = fadeRate > 250 ? 250 : fadeRate;

        uint8_t dotSize = config.option2 > 0 ? config.option2 : 1;
        dotSize = dotSize > 5 ? 5 : dotSize;

        bool rainbowMode = config.feature1;
        bool bounceMode = config.feature2;

        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t r, g, b;
            segment->getPixel(i, r, g, b);
            r = FastLEDMath::scale8(r, fadeRate);
            g = FastLEDMath::scale8(g, fadeRate);
            b = FastLEDMath::scale8(b, fadeRate);
            segment->setPixel(i, r, g, b);
        }

        int pos;
        if (bounceMode)
        {
            uint16_t bpm = 5 + ((config.speed * 45) / 255);
            _sinePosition += _sineDirection ? bpm / 10 : -(bpm / 10);
            if (_sinePosition >= (length - 1) * 16) { _sinePosition = (length - 1) * 16; _sineDirection = false; }
            else if (_sinePosition <= 0) { _sinePosition = 0; _sineDirection = true; }
            pos = _sinePosition / 16;
        }
        else
        {
            uint16_t bpm = 5 + ((config.speed * 45) / 255);
            pos = FastLEDMath::beatsin16(bpm, 0, length - 1);
        }

        uint8_t r, g, b;
        if (rainbowMode || (config.r() == 0 && config.g() == 0 && config.b() == 0))
        {
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(_hue, 255, config.intensity);
            r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF;
            _hue++;
        }
        else
        {
            r = FastLEDMath::scale8(config.r(), config.intensity);
            g = FastLEDMath::scale8(config.g(), config.intensity);
            b = FastLEDMath::scale8(config.b(), config.intensity);
        }

        for (uint8_t d = 0; d < dotSize && (pos + d) < length; d++)
            segment->setPixel(pos + d, r, g, b);
    }

    void reset() override
    {
        _position = 0;
        _direction = 1;
        _lastUpdate = 0;
        _hue = 0;
        _nextMeteor = 0;
        _sinePosition = 0;
        _sineDirection = true;
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Comet";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Bewegter Punkt mit Schweif: Comet, Meteor oder Sinelon je nach Modus",
            "Moving dot with tail: Comet, Meteor or Sinelon depending on mode");
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 6; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "FadeRate";
            case 2: return "TailLength";
            case 3: return "BounceMode";
            case 4: return "RainbowMode";
            case 5: return "Mode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Bewegungsgeschwindigkeit (höher=schneller)", "Speed: Movement speed (higher=faster)");
            case 1: return PARAM_DESC_DE_EN("Ausblendrate / Meteorengröße", "Fade rate / Meteor size");
            case 2: return PARAM_DESC_DE_EN("Schweiflänge (Comet) / Punktgröße (Sinelon) / Häufigkeit (Meteor)", "Tail length (Comet) / Dot size (Sinelon) / Frequency (Meteor)");
            case 3: return PARAM_DESC_DE_EN("Sprungmodus (Comet) / Zufallsfarben (Meteor) / Regenbogen (Sinelon)", "Bounce (Comet) / Random colors (Meteor) / Rainbow (Sinelon)");
            case 4: return PARAM_DESC_DE_EN("Regenbogen (Comet) / Multi-Meteor (Meteor) / Sprungmodus (Sinelon)", "Rainbow (Comet) / Multi-meteor (Meteor) / Bounce (Sinelon)");
            case 5: return PARAM_DESC_DE_EN("Modus: 0=Comet, 1=Meteor, 2=Sinelon", "Mode: 0=Comet, 1=Meteor, 2=Sinelon");
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
            case 5: return ParameterType::PARAM_ENUM; // Mode
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 5) return nullptr;
        switch (enumValue)
        {
            case 0: return "Komet";
            case 1: return "Meteor";
            case 2: return "Sinelon";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 5) ? 3 : 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 128; // Speed
            case 1: return 150; // FadeRate
            case 2: return 10;  // TailLength
            case 3: return 0;   // BounceMode off
            case 4: return 0;   // RainbowMode off
            case 5: return 0;   // Mode (Comet)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;
            case 1: return 0;
            case 2: return 5;
            case 3: return 0;
            case 4: return 0;
            case 5: return 0; // Mode min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255;
            case 1: return 250;
            case 2: return 30;
            case 3: return 1;
            case 4: return 1;
            case 5: return 2; // Mode max (0..2)
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
            case 1: return config.option1;  // FadeRate / MeteorSize
            case 2: return config.option2;  // TailLength / DotSize / Frequency
            case 3: return config.feature1; // BounceMode / RandomColors / Rainbow
            case 4: return config.feature2; // RainbowMode / MultiMeteor / Bounce
            case 5: return config.mode;     // Mode
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
            case 3: config.feature1 = static_cast<bool>(value); break;
            case 4: config.feature2 = static_cast<bool>(value); break;
            case 5: config.mode = static_cast<uint8_t>(value); break;
            default: break;
        }
    }
};
