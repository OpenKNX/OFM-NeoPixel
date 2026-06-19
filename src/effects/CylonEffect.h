/**
 * @file CylonEffect.h
 * @brief Cylon/Knight Rider effect - STATELESS, 1D + 2D
 *
 * A single LED eye bouncing back and forth with a trailing fade.
 * Classic "KITT" effect from Knight Rider / Cylon from Battlestar Galactica.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * Consolidated effect: the former "Cylon 2D" effect is now the 2D render path
 * of this effect (getCapabilities() advertises DIM_1D|DIM_2D, update2D()).
 *
 * Parameters:
 *   [0] Speed (0-255)      - Movement speed (0 = auto-scale)
 *   [1] Hue (0-255)        - Eye color
 *   [2] EyeSize (1-20)     - Size of the eye
 *   [3] FadeAmount (1-255) - Trail fade speed
 *   [4] Direction (bool)   - 2D: 0=horizontal, 1=vertical
 *   [5] Mode (0/1)         - 0=Larson (bounce), 1=Center double-cylon ("Tor")
 *
 * Config usage (ETS mapping):
 *   - config.speed      -> Speed
 *   - config.option1    -> Hue
 *   - config.option2    -> EyeSize
 *   - config.option3    -> FadeAmount
 *   - config.feature1   -> Direction (2D)
 *   - config.mode       -> Mode (0=Larson, 1=Center)
 *   - config.intensity  -> Brightness
 *   - config.reverse    -> Invert initial direction
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
  private:
    uint32_t _timeAcc = 0; // for 2D timing

  public:
    CylonEffect() = default;

    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Cylon", "Cylon");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Klassischer KITT/Cylon-Effekt mit hin- und herspringendem Lichtauge und verblassendem Schweif. Unterstützt 1D-Streifen und 2D-Matrizen. Mode 1 erzeugt zwei gespiegelte Augen aus der Mitte (Tor).",
            "Classic KITT/Cylon effect with bouncing light eye and fading trail. Supports 1D strips and 2D matrices. Mode 1 creates two mirrored eyes from the center (gate).");
    }

    uint8_t getCapabilities() const override { return DIM_1D | DIM_2D; }

    // ====================================================================
    // Parameter API - Self-Describing
    // ====================================================================
    uint8_t getParameterCount() const override { return 6; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "EyeSize";
            case 3: return "FadeAmount";
            case 4: return "Direction";
            case 5: return "Mode";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Geschwindigkeit: Bewegungstempo des Lichtauges (0=automatisch skaliert, höhere Werte=schnellere Bewegung).",
                "Speed: Movement tempo of the light eye (0=auto-scaled, higher values=faster movement).");
            case 1: return PARAM_DESC_DE_EN(
                "Farbton: HSV-Farbwert für das Lichtauge (0=rot, 85=grün, 170=blau).",
                "Hue: HSV color value for the light eye (0=red, 85=green, 170=blue).");
            case 2: return PARAM_DESC_DE_EN(
                "Augengröße: Anzahl der LEDs die das Auge bilden (1-20).",
                "Eye Size: Number of LEDs forming the eye (1-20).");
            case 3: return PARAM_DESC_DE_EN(
                "Schweif-Geschwindigkeit: Wie schnell der Schweif verblasst (1=langer Schweif, 255=kurzer Schweif).",
                "Trail Speed: How quickly the trail fades (1=long trail, 255=short trail).");
            case 4: return PARAM_DESC_DE_EN(
                "Richtung (nur 2D): 0=horizontal (Spalte wandert links-rechts), 1=vertikal (Zeile wandert oben-unten).",
                "Direction (2D only): 0=horizontal (column sweeps left-right), 1=vertical (row sweeps top-bottom).");
            case 5: return PARAM_DESC_DE_EN(
                "Modus: 0=Larson (klassisch hin-und-her), 1=Mitte→außen Doppel-Cylon (Tor).",
                "Mode: 0=Larson (classic back-and-forth), 1=center→outside double cylon (gate).");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return ParameterType::PARAM_UINT8; // Speed
            case 1: return ParameterType::PARAM_HUE;   // Hue
            case 2: return ParameterType::PARAM_UINT8; // EyeSize
            case 3: return ParameterType::PARAM_UINT8; // FadeAmount
            case 4: return ParameterType::PARAM_ENUM;  // Direction
            case 5: return ParameterType::PARAM_ENUM;  // Mode
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex == 4)
        {
            switch (enumValue)
            {
                case 0: return "Horizontal";
                case 1: return "Vertikal";
                default: return nullptr;
            }
        }
        if (paramIndex == 5)
        {
            switch (enumValue)
            {
                case 0: return "Larson";
                case 1: return "Mitte nach aussen";
                default: return nullptr;
            }
        }
        return nullptr;
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        if (paramIndex == 4) return 2;
        if (paramIndex == 5) return 2;
        return 0;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 20; // Speed
            case 1: return 0;  // Hue (red)
            case 2: return 5;  // EyeSize
            case 3: return 50; // FadeAmount
            case 4: return 0;  // Direction horizontal
            case 5: return 0;  // Mode (Larson)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 0; // Speed min (0=auto)
            case 1: return 0; // Hue min
            case 2: return 1; // EyeSize min
            case 3: return 1; // FadeAmount min
            case 4: return 0; // Direction false
            case 5: return 0; // Mode min
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // Hue max
            case 2: return 20;  // EyeSize max
            case 3: return 255; // FadeAmount max
            case 4: return 1;   // Direction true
            case 5: return 1;   // Mode max (0=Larson,1=Center)
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;
            case 1: return config.option1;
            case 2: return config.option2;
            case 3: return config.option3;
            case 4: return config.feature1;
            case 5: return config.mode;
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
            case 3: config.option3 = static_cast<uint8_t>(value); break;
            case 4: config.feature1 = static_cast<bool>(value); break;
            case 5: config.mode = static_cast<uint8_t>(value); break;
        }
    }

    // ====================================================================
    // 1D rendering
    // ====================================================================
    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        if (length == 0) return;

        const uint8_t speedIn = config.speed;
        const uint8_t hue = config.option1;
        const uint8_t eyeSizeIn = config.option2;
        const uint8_t fadeIn = config.option3;
        const uint8_t brightness = config.intensity;

        const bool reverse = (config.reverse != 0);
        const bool yellowBoost = config.feature2;
        const bool greenCorr = config.feature3;
        const bool centerMode = (config.mode == 1);

        const uint8_t eyeSize = (eyeSizeIn == 0) ? 4 : (eyeSizeIn > 10 ? 10 : eyeSizeIn);
        const uint8_t fadeAmount = (fadeIn == 0) ? 40 : fadeIn;
        const uint8_t speed = speedIn;

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

        if (centerMode)
        {
            // Center double-cylon: two mirrored eyes sweep from the center
            // outward and back. Position 0..(half) measured from the center.
            uint16_t center = (length - 1) / 2;
            uint16_t half = (length % 2 == 0) ? (length / 2) : (center + 1);
            if (half == 0) half = 1;

            // offset stored in position field (fixed-point * 16), bounce dir in bit15
            uint16_t posScaled = state.position & 0x7FFF;
            int8_t dirStored = (state.position & 0x8000) ? -1 : 1;
            float offset = (float)posScaled / 16.0f;

            float effectiveSpeed = (speed > 0) ? (speed / 10.0f) : (half / 125.0f);
            if (effectiveSpeed < 0.5f) effectiveSpeed = 0.5f;

            uint16_t offInt = (uint16_t)offset;
            for (uint8_t i = 0; i < eyeSize; i++)
            {
                int16_t d = (int16_t)offInt + i - (eyeSize / 2);
                if (d < 0) continue;
                uint8_t eyeBrightness = brightness;
                if (i == 0 || i == eyeSize - 1) eyeBrightness = brightness / 2;
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, eyeBrightness, yellowBoost, greenCorr);
                uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;

                int16_t right = (int16_t)center + d;
                int16_t left = (int16_t)center - d;
                if (right >= 0 && right < length) segment->setPixel(right, r, g, b);
                if (left >= 0 && left < length) segment->setPixel(left, r, g, b);
            }

            offset += effectiveSpeed * dirStored;
            if (offset >= half - 1) { offset = half - 1; dirStored = -1; }
            else if (offset <= 0) { offset = 0; dirStored = 1; }

            posScaled = (uint16_t)(offset * 16.0f) & 0x7FFF;
            state.position = posScaled | (dirStored < 0 ? 0x8000 : 0);
            return;
        }

        // Larson (classic) bounce
        uint16_t posScaled = state.position & 0x7FFF;
        int8_t dirStored = (state.position & 0x8000) ? -1 : 1;
        int8_t direction = reverse ? (int8_t)-dirStored : dirStored;
        float position = (float)posScaled / 16.0f;

        float effectiveSpeed = (speed > 0) ? (speed / 10.0f) : (length / 125.0f);
        if (effectiveSpeed < 0.5f) effectiveSpeed = 0.5f;

        uint16_t pixelPos = (uint16_t)position;
        for (uint8_t i = 0; i < eyeSize; i++)
        {
            int16_t pos = pixelPos + i - (eyeSize / 2);
            if (pos >= 0 && pos < length)
            {
                uint8_t eyeBrightness = brightness;
                if (i == 0 || i == eyeSize - 1) eyeBrightness = brightness / 2;
                uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, eyeBrightness, yellowBoost, greenCorr);
                segment->setPixel(pos, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
        }

        position += effectiveSpeed * direction;
        if (position >= length - 1) { position = length - 1; direction = -1; }
        else if (position <= 0) { position = 0; direction = 1; }

        posScaled = (uint16_t)(position * 16.0f) & 0x7FFF;
        int8_t dirToStore = reverse ? (int8_t)-direction : direction;
        state.position = posScaled | (dirToStore < 0 ? 0x8000 : 0);
    }

    // ====================================================================
    // 2D rendering (row/column sweep across a matrix)
    // ====================================================================
    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D()) { update(segment, deltaTime); return; }

        uint16_t renderW = segment->getRenderWidth();
        uint16_t renderH = segment->getRenderHeight();
        if (renderW == 0 || renderH == 0) return;

        _timeAcc += deltaTime;
        auto& cfg = segment->getConfig();
        uint8_t hue = cfg.option1;
        uint8_t eyeSize = cfg.option2 ? cfg.option2 : 1;
        uint8_t fadeAmt = cfg.option3 ? cfg.option3 : 50;
        uint16_t speed = (uint16_t)cfg.speed + 1;
        bool vertical = cfg.feature1;

        uint16_t dim = vertical ? renderH : renderW;
        if (dim < 2) return;

        uint16_t stepMs = (uint16_t)(256 - (speed > 255 ? 255 : speed));
        if (stepMs == 0) stepMs = 1;
        uint32_t step = (_timeAcc / stepMs) % ((uint32_t)(dim - 1) * 2);
        uint16_t pos;
        if (step < dim)
            pos = (uint16_t)step;
        else
            pos = (uint16_t)(2 * (dim - 1) - step);

        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, 255);
        uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;

        // Fade all pixels
        for (uint8_t y = 0; y < geo.height; y++)
            for (uint8_t x = 0; x < geo.width; x++)
            {
                uint16_t idx = segment->xyToIndex(x, y);
                if (idx == 0xFFFF) continue;
                uint8_t pr, pg, pb;
                segment->getPixel(idx, pr, pg, pb);
                pr = FastLEDMath::scale8(pr, 255 - fadeAmt);
                pg = FastLEDMath::scale8(pg, 255 - fadeAmt);
                pb = FastLEDMath::scale8(pb, 255 - fadeAmt);
                segment->setPixelXY(x, y, pr, pg, pb);
            }

        // Draw eye band
        for (uint8_t e = 0; e < eyeSize; e++)
        {
            uint16_t epos = pos + e;
            if (epos >= dim) break;
            if (vertical)
                for (uint16_t x = 0; x < renderW; x++)
                    segment->setPixelGlobalXY(x, epos, r, g, b);
            else
                for (uint16_t y = 0; y < renderH; y++)
                    segment->setPixelGlobalXY(epos, y, r, g, b);
        }
    }

    void reset() override {}
};
