/**
 * @file Metaballs2DEffect.h
 * @brief Organic metaballs ("octopus") on a 2D LED matrix
 *
 * Several blobs drift across the matrix; each pixel is lit by the summed
 * inverse-square field of all blobs, so they merge and split organically.
 *
 * Parameters:
 *   0 Speed     — blob movement speed 1-255 (default: 50)
 *   1 Hue       — base hue 0-255 (default: 0)
 *   2 BlobCount — number of blobs 2-6 (default: 4)
 *   3 Contrast  — field contrast / edge sharpness 1-255 (default: 128)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

class Metaballs2DEffect : public Effect
{
  private:
    uint32_t _timebase = 0;

    static constexpr uint8_t MAX_BLOBS = 6;

    static uint32_t mapLinear(uint32_t value, uint32_t inMin, uint32_t inMax, uint32_t outMin, uint32_t outMax)
    {
        if (inMax <= inMin) return outMin;
        if (value <= inMin) return outMin;
        if (value >= inMax) return outMax;
        const uint32_t span = inMax - inMin;
        if (outMax >= outMin)
            return outMin + ((value - inMin) * (outMax - outMin)) / span;
        return outMin - ((value - inMin) * (outMin - outMax)) / span;
    }

  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Metaballs 2D", "Metaballs 2D");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Organische Metaballs auf einer 2D LED-Matrix: driftende Blobs, die per Feldsumme "
            "verschmelzen und sich teilen.",
            "Organic metaballs on a 2D LED matrix: drifting blobs that merge and split via a summed field.");
    }

    uint8_t getCapabilities() const override { return DIM_2D; }

    uint8_t getParameterCount() const override { return 4; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Hue";
            case 2: return "BlobCount";
            case 3: return "Contrast";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Bewegungsgeschwindigkeit der Blobs.", "Blob movement speed.");
            case 1: return PARAM_DESC_DE_EN("Basis-Farbton.", "Base hue.");
            case 2: return PARAM_DESC_DE_EN("Anzahl der Blobs.", "Number of blobs.");
            case 3: return PARAM_DESC_DE_EN("Feldkontrast / Kantenschärfe.", "Field contrast / edge sharpness.");
            default: return nullptr;
        }
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        (void)index;
        return ParameterType::PARAM_UINT8;
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1;  // Speed
            case 1: return 0;  // Hue
            case 2: return 2;  // BlobCount
            case 3: return 1;  // Contrast
        }
        return 0;
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed
            case 1: return 255; // Hue
            case 2: return 6;   // BlobCount
            case 3: return 255; // Contrast
        }
        return 255;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 50;  // Speed
            case 1: return 0;   // Hue
            case 2: return 4;   // BlobCount
            case 3: return 128; // Contrast
        }
        return 0;
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        const auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: return cfg.speed;
            case 1: return cfg.option1;
            case 2: return cfg.option2;
            case 3: return cfg.option3;
        }
        return 0;
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& cfg = segment->getConfig();
        switch (index)
        {
            case 0: cfg.speed = static_cast<uint8_t>(value); break;
            case 1: cfg.option1 = static_cast<uint8_t>(value); break;
            case 2: cfg.option2 = static_cast<uint8_t>(value); break;
            case 3: cfg.option3 = static_cast<uint8_t>(value); break;
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        (void)segment;
        (void)deltaTime;
    }

    void update2D(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;
        const auto& geo = segment->getGeometry();
        if (geo.is1D() || geo.width < 2 || geo.height < 2) return;

        _timebase += deltaTime;

        const auto& cfg = segment->getConfig();
        const uint8_t baseHue = cfg.option1;
        uint8_t blobCount = cfg.option2;
        if (blobCount < 2) blobCount = 2;
        if (blobCount > MAX_BLOBS) blobCount = MAX_BLOBS;
        const uint8_t contrast = cfg.option3 ? cfg.option3 : 128;
        const uint16_t w = geo.width;
        const uint16_t h = geo.height;

        const uint32_t spd = mapLinear(cfg.speed, 1, 255, 2, 50);
        const uint8_t phase = static_cast<uint8_t>((_timebase * spd) >> 11);

        // Blob centres derived from sine waves at distinct rates/offsets.
        uint8_t bx[MAX_BLOBS];
        uint8_t by[MAX_BLOBS];
        for (uint8_t b = 0; b < blobCount; b++)
        {
            const uint8_t ox = FastLEDMath::sin8(static_cast<uint8_t>(phase * (b + 1) / 2 + b * 53));
            const uint8_t oy = FastLEDMath::sin8(static_cast<uint8_t>(phase * (b + 2) / 3 + b * 97 + 64));
            bx[b] = static_cast<uint8_t>((static_cast<uint16_t>(ox) * (w - 1)) / 255);
            by[b] = static_cast<uint8_t>((static_cast<uint16_t>(oy) * (h - 1)) / 255);
        }

        for (uint16_t y = 0; y < h; y++)
        {
            for (uint16_t x = 0; x < w; x++)
            {
                uint16_t field = 0;
                for (uint8_t b = 0; b < blobCount; b++)
                {
                    const int16_t dx = static_cast<int16_t>(x) - bx[b];
                    const int16_t dy = static_cast<int16_t>(y) - by[b];
                    const uint16_t d2 = static_cast<uint16_t>(dx * dx + dy * dy);
                    field += static_cast<uint16_t>(1024u / (d2 + 1));
                }

                uint32_t f = (static_cast<uint32_t>(field) * contrast) >> 7;
                const uint8_t bri = (f > 255) ? 255 : static_cast<uint8_t>(f);
                const uint8_t hue = static_cast<uint8_t>(baseHue + (bri >> 2));
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, bri);
                segment->setPixelXY(x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            }
        }
    }
};
