#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "ParameterType.h"

/**
 * @brief Palette effect (FastLED-style ColorFromPalette)
 *
 * Renders a 16-color palette across the segment, optionally scrolling over time.
 *
 * Uses config parameters:
 *  - config.speed     : scroll speed (0 => very slow)
 *  - config.intensity : master brightness (scales RGB)
 *  - config.option1   : palette id (0..N-1)
 *  - config.option2   : blend (0=nearest, 1=linear)
 *  - config.option3   : index spacing (0 => auto-fit)
 *  - config.reverse   : reverse scroll direction
 *  - config.feature1  : auto palette cycle (periodic palette id advance)
 *
 * Note: hsv2rgb_rainbow correction flags are not used here because palettes are RGB.
 */
class PaletteEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override { return "Palette"; }
    const char* getDescription(const char* lang = nullptr) override { return "Fixed 16-color palettes with optional blending + scroll"; }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return "Speed";
            case 1: return "Palette";
            case 2: return "Blend";
            case 3: return "Spacing";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Scroll-Geschwindigkeit (0-255)", "Speed: Scroll speed (0-255)");
            case 1: return PARAM_DESC_DE_EN("Palette: Farbpalette auswählen (0-N)", "Palette: Select color palette (0-N)");
            case 2: return PARAM_DESC_DE_EN("Überblendung: Farbüberblendung (0=aus, 1=an)", "Blend: Color blending (0=off, 1=on)");
            case 3: return PARAM_DESC_DE_EN("Abstand: Farbabstand (0=automatisch)", "Spacing: Color spacing (0=auto)");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t idx) const override
    {
        switch (idx)
        {
            case 1: return ParameterType::PARAM_ENUM; // Palette
            case 2: return ParameterType::PARAM_BOOL; // Blend
            default: return ParameterType::PARAM_UINT8;
        }
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 1) return nullptr;
        switch (enumValue)
        {
            case 0: return "Regenbogen";
            case 1: return "Hitze";
            case 2: return "Ozean";
            case 3: return "Wald";
            default: return nullptr;
        }
    }

    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 1) ? 4 : 0;
    }

    uint32_t getParameterMin(uint8_t idx) const override
    {
        switch (idx)
        {
            case 1: return 0; // Palette
            case 2: return 0; // Blend
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t idx) const override
    {
        switch (idx)
        {
            case 1: return 3; // Palette (Rainbow/Heat/Ocean/Forest)
            case 2: return 1; // Blend
            default: return 255;
        }
    }

    uint32_t getParameterDefault(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return 32;
            case 1: return 0;
            case 2: return 1;
            case 3: return 0;
            default: return 0;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t idx) const override
    {
        if (!segment) return 0;
        const auto& c = segment->getConfig();
        switch (idx)
        {
            case 0: return c.speed;
            case 1: return c.option1;
            case 2: return c.option2;
            case 3: return c.option3;
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (animation speed)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Palette (palette selection)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Blend (blending mode)
            case 3: config.option3 = static_cast<uint8_t>(value); break; // Spacing (color spacing)
            default: break;
        }
    }

    struct RGB
    {
        uint8_t r, g, b;
    };

    static RGB lerp(const RGB& a, const RGB& b, uint8_t frac)
    {
        return {
            FastLEDMath::lerp8by8(a.r, b.r, frac),
            FastLEDMath::lerp8by8(a.g, b.g, frac),
            FastLEDMath::lerp8by8(a.b, b.b, frac),
        };
    }

    // A few small built-in palettes (16 entries each)
    static const RGB* getPalette(uint8_t id, uint8_t& count)
    {
        static const RGB rainbow16[16] = {
            {255, 0, 0}, {255, 64, 0}, {255, 128, 0}, {255, 192, 0}, {255, 255, 0}, {128, 255, 0}, {0, 255, 0}, {0, 255, 128}, {0, 255, 255}, {0, 128, 255}, {0, 0, 255}, {128, 0, 255}, {255, 0, 255}, {255, 0, 128}, {255, 0, 64}, {255, 0, 0}};
        static const RGB heat16[16] = {
            {0, 0, 0}, {64, 0, 0}, {128, 0, 0}, {192, 0, 0}, {255, 0, 0}, {255, 64, 0}, {255, 128, 0}, {255, 192, 0}, {255, 255, 0}, {255, 255, 64}, {255, 255, 128}, {255, 255, 192}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}};
        static const RGB ocean16[16] = {
            {0, 0, 32}, {0, 0, 64}, {0, 0, 96}, {0, 0, 128}, {0, 32, 160}, {0, 64, 192}, {0, 96, 224}, {0, 128, 255}, {0, 160, 224}, {0, 192, 192}, {0, 224, 160}, {0, 255, 128}, {0, 224, 96}, {0, 192, 64}, {0, 160, 32}, {0, 128, 16}};
        static const RGB forest16[16] = {
            {0, 8, 0}, {0, 16, 0}, {0, 32, 0}, {0, 64, 0}, {16, 96, 0}, {32, 128, 0}, {64, 160, 0}, {96, 192, 0}, {128, 224, 0}, {96, 192, 32}, {64, 160, 64}, {32, 128, 96}, {16, 96, 64}, {8, 64, 32}, {4, 32, 16}, {0, 16, 8}};

        count = 16;
        switch (id % 4)
        {
            case 0: return rainbow16;
            case 1: return heat16;
            case 2: return ocean16;
            default: return forest16;
        }
    }

    static RGB colorFromPalette(uint8_t paletteId, uint8_t index, bool blend)
    {
        uint8_t count = 0;
        const RGB* pal = getPalette(paletteId, count);
        const uint8_t i0 = (index >> 4) & 0x0F;
        const uint8_t i1 = (uint8_t)((i0 + 1) & 0x0F);
        const uint8_t frac = (index & 0x0F) << 4;

        if (!blend)
            return pal[i0];

        return lerp(pal[i0], pal[i1], frac);
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& state = segment->getState();
        auto& config = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // Snapshot config per frame
        const uint8_t speedVal = config.speed;         // scroll speed
        const uint8_t intensityVal = config.intensity; // master brightness
        const uint8_t paletteId = config.option1;      // palette id
        const bool blend = (config.option2 != 0);      // blend mode
        uint8_t spacing = config.option3;              // index spacing
        const bool reverseDir = (config.reverse != 0); // reverse direction
        const bool autoCycle = config.feature1;        // auto palette cycle

        if (spacing == 0)
        {
            uint16_t d = 256 / length;
            spacing = (d == 0) ? 1 : (uint8_t)d;
        }

        // scroll index over time
        const uint16_t intervalMs = (uint16_t)(1 + (255 - speedVal));
        state.counter += (uint16_t)deltaTime;
        while (state.counter >= intervalMs)
        {
            state.counter -= intervalMs;
            state.position = (uint16_t)((state.position + 1) & 0xFF);
        }

        // optional palette cycling (~every few seconds depending on speed)
        uint8_t pal = paletteId;
        if (autoCycle)
        {
            // use aux1 as slow timer
            state.aux1 += (uint16_t)deltaTime;
            if (state.aux1 >= 4000u)
            {
                state.aux1 = 0;
                pal = (uint8_t)(paletteId + 1);
            }
        }

        const uint8_t baseIndex = (uint8_t)(state.position & 0xFF);

        for (uint16_t i = 0; i < length; i++)
        {
            const uint8_t posIdx = (uint8_t)((uint16_t)i * (uint16_t)spacing);
            const uint8_t idx = reverseDir ? (uint8_t)(baseIndex - posIdx) : (uint8_t)(baseIndex + posIdx);

            RGB c = colorFromPalette(pal, idx, blend);

            // apply intensity scaling
            c.r = FastLEDMath::scale8(c.r, intensityVal);
            c.g = FastLEDMath::scale8(c.g, intensityVal);
            c.b = FastLEDMath::scale8(c.b, intensityVal);

            segment->setPixel(i, c.r, c.g, c.b);
        }
    }
};
