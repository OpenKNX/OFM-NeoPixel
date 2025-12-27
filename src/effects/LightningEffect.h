#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"
#include "ParameterType.h"

/**
 * @brief Lightning effect (FastLED-inspired)
 *
 * Random lightning strikes with short flash + decay.
 *
 * Uses config parameters:
 *  - config.speed     : strike frequency / probability (0 => rare, 255 => frequent)
 *  - config.intensity : peak brightness (0..255)
 *  - config.option1   : strike width (0 => auto)
 *  - config.option2   : decay speed (0 => default)
 *  - config.option3   : hue for colored lightning (only if feature1=1)
 *  - config.feature1  : colored lightning (0 = white, 1 = use hue option3)
 *  - config.reverse   : reverses strike position mapping (mirrors position)
 *  - config.feature2  : enable yellow brightness compensation (hsv2rgb_rainbow) [only used in colored mode]
 *  - config.feature3  : enable green correction hooks (hsv2rgb_rainbow) [only used in colored mode]
 */
class LightningEffect : public Effect
{
  public:
    const char* getName(const char* lang = nullptr) override
    {
        return EFFECT_NAME_DE_EN("Blitz", "Lightning");
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return EFFECT_DESC_DE_EN(
            "Zufällige Blitzeinschläge mit kurzen Lichtblitzen und Abklingeffekt. Simuliert realistische Gewitterblitze mit einstellbarer Häufigkeit, Breite und Helligkeit.",
            "Random lightning strikes with brief flashes and decay effect. Simulates realistic thunderstorm lightning with adjustable frequency, width and brightness.");
    }

    uint8_t getParameterCount() const override { return 4; }
    const char* getParameterName(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return "Speed";
            case 1: return "Width";
            case 2: return "Decay";
            case 3: return "Hue";
            default: return nullptr;
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN(
                "Geschwindigkeit: Häufigkeit der Blitzeinschläge (0=selten, 255=sehr häufig). Niedrige Werte erzeugen seltene, dramatische Blitze.",
                "Speed: Strike frequency (0=rare, 255=very frequent). Low values create rare, dramatic lightning strikes.");
            case 1: return PARAM_DESC_DE_EN(
                "Breite: Ausdehnung des Blitzes in LEDs (0=automatisch). Höhere Werte erzeugen breitere, diffusere Lichtbögen.",
                "Width: Lightning bolt width in LEDs (0=automatic). Higher values create wider, more diffuse light arcs.");
            case 2: return PARAM_DESC_DE_EN(
                "Abklingzeit: Geschwindigkeit des Helligkeitsabfalls nach dem Blitz (0=Standard, höher=schneller). Steuert wie lange der Nachglüh-Effekt sichtbar bleibt.",
                "Decay: Brightness fade speed after strike (0=default, higher=faster). Controls how long the afterglow effect remains visible.");
            case 3: return PARAM_DESC_DE_EN(
                "Farbton: HSV-Farbwert für farbige Blitze (0-255, nur aktiv mit Feature 1). 0=rot, 85=grün, 170=blau. Für weiße Blitze Feature 1 deaktivieren.",
                "Hue: HSV color value for colored lightning (0-255, only active with Feature 1). 0=red, 85=green, 170=blue. Disable Feature 1 for white lightning.");
            default: return "";
        }
    }

    ParameterType getParameterType(uint8_t) const override { return ParameterType::PARAM_UINT8; }
    uint32_t getParameterDefault(uint8_t idx) const override
    {
        switch (idx)
        {
            case 0: return 32;
            case 1: return 0;  // auto width
            case 2: return 40; // decay
            case 3: return 0;  // hue
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
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (strike frequency)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Width (bolt width)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Decay (fade speed)
            case 3: config.option3 = static_cast<uint8_t>(value); break; // Hue (lightning color)
            default: break;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& st = segment->getState();
        auto& cfg = segment->getConfig();
        const uint16_t length = segment->getLength();
        if (length == 0) return;

        // Snapshot config per frame
        const uint8_t speedVal = cfg.speed;         // strike frequency / probability
        const uint8_t peak = cfg.intensity;         // peak brightness
        const uint8_t widthCfg = cfg.option1;       // strike width
        const uint8_t decayCfg = cfg.option2;       // decay speed
        const uint8_t hue = cfg.option3;            // hue for colored lightning
        const bool colored = cfg.feature1;          // colored lightning
        const bool reverseDir = (cfg.reverse != 0); // reverse position mapping
        const bool yellowBoost = cfg.feature2;      // enable yellow brightness compensation
        const bool greenCorr = cfg.feature3;        // enable green correction hooks

        // Clear background (simple + deterministic)
        for (uint16_t i = 0; i < length; i++)
            segment->setPixel(i, 0, 0, 0);

        // phase: 0=idle, 1=flash, 2=decay
        st.counter += (uint16_t)deltaTime;

        if (st.phase == 0)
        {
            // Strike probability check every ~25ms
            if (st.counter >= 25u)
            {
                st.counter = 0;

                // probability: speedVal/255
                if (FastLEDMath::random8() < speedVal)
                {
                    // pick strike location and width
                    uint16_t pos = (length > 1) ? FastLEDMath::random16(length) : 0;
                    if (reverseDir && length > 0) pos = (length - 1u) - pos;

                    uint8_t w = widthCfg;
                    if (w == 0)
                        w = (length >= 8) ? 3 : 1;
                    if (w > length) w = (uint8_t)length;

                    st.aux1 = pos;     // strike center
                    st.aux2 = w;       // strike width
                    st.phase = 1;      // flash
                    st.lastUpdate = 0; // reuse as elapsed within phase
                }
            }
        }
        else
        {
            // elapsed in current phase
            st.lastUpdate += deltaTime;

            const uint16_t center = st.aux1;
            const uint8_t w = (uint8_t)st.aux2;
            const uint16_t half = (uint16_t)(w / 2u);

            uint8_t brightness = peak;

            if (st.phase == 1)
            {
                // short flash duration ~60ms
                if (st.lastUpdate >= 60u)
                {
                    st.phase = 2;
                    st.lastUpdate = 0;
                }
            }
            else if (st.phase == 2)
            {
                // decay duration controlled by option2
                const uint16_t decayMs = (decayCfg == 0) ? 300u : (uint16_t)(50u + (uint16_t)decayCfg * 8u);
                if (st.lastUpdate >= decayMs)
                {
                    st.phase = 0;
                    st.lastUpdate = 0;
                    return;
                }
                // linear fade
                brightness = (uint8_t)(peak - (uint8_t)((uint32_t)peak * st.lastUpdate / decayMs));
            }

            // render strike as a small bar
            const uint16_t start = (center > half) ? (center - half) : 0;
            const uint16_t end = (center + half < (length - 1u)) ? (center + half) : (length - 1u);

            if (!colored)
            {
                for (uint16_t i = start; i <= end; i++)
                    segment->setPixel(i, brightness, brightness, brightness);
            }
            else
            {
                const uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 0, brightness, yellowBoost, greenCorr);
                const uint8_t r = (rgb >> 16) & 0xFF;
                const uint8_t g = (rgb >> 8) & 0xFF;
                const uint8_t b = rgb & 0xFF;
                for (uint16_t i = start; i <= end; i++)
                    segment->setPixel(i, r, g, b);
            }
        }
    }
};
