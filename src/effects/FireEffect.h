/**
 * @file FireEffect.h
 * @brief Fire2012 effect ported from FastLED - Realistic fire simulation
 *
 * Port of Mark Kriegsman's Fire2012 effect from FastLED.
 * Based on FastLED Fire2012.ino (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "../Segment.h"
#include "Effect.h"
#include "FastLEDMath.h"

/**
 * Fire Effect - Port of FastLED Fire2012
 *
 * This basic one-dimensional 'fire' simulation works roughly as follows:
 * There's an underlying array of 'heat' cells, that model the temperature
 * at each point along the line. Every cycle through the simulation,
 * four steps are performed:
 *  1) All cells cool down a little bit, losing heat to the air
 *  2) The heat from each cell drifts 'up' and diffuses a little
 *  3) Sometimes randomly new 'sparks' of heat are added at the bottom
 *  4) The heat from each cell is rendered as a color into the LEDs
 */
class FireEffect : public Effect
{
  private:
    // Heat array for fire simulation
    static const uint16_t MAX_HEAT_CELLS = 512; // Support up to 512 LEDs
    uint8_t _heat[MAX_HEAT_CELLS];
    uint16_t _numCells;

    // Fire parameters
    uint8_t _cooling;       // How much heat dissipates (20-100, default 55)
    uint8_t _sparking;      // Chance of new sparks (50-200, default 120)
    bool _reverseDirection; // Whether fire goes up or down

  public:
    FireEffect() : _numCells(0), _cooling(55), _sparking(120), _reverseDirection(false)
    {
        // Initialize heat array to zero
        for (uint16_t i = 0; i < MAX_HEAT_CELLS; i++)
        {
            _heat[i] = 0;
        }
    }

    void update(Segment* segment, uint32_t deltaTime) override
    {
        if (!segment) return;

        auto& config = segment->getConfig();

        const uint8_t intensity = config.intensity;    // Master brightness scaling (0..255)
        const uint8_t option1 = config.option1;        // Cooling control (0=default)
        const uint8_t option2 = config.option2;        // Sparking probability (0=default)
        const bool reverseDir = (config.reverse != 0); // Reverse direction
        const bool blueMode = config.feature2;         // Blue fire mode
        uint16_t length = segment->getLength();

        // Ensure we don't exceed our heat array
        if (length > MAX_HEAT_CELLS)
        {
            length = MAX_HEAT_CELLS;
        }

        // Update heat array size if needed
        if (_numCells != length)
        {
            _numCells = length;
            // Clear heat array for new size
            for (uint16_t i = 0; i < MAX_HEAT_CELLS; i++)
            {
                _heat[i] = 0;
            }
        }

        // option1 -> cooling (0 = default)
        const uint8_t coolIn = (option1 == 0) ? 90 : option1;
        _cooling = 20 + (coolIn * 80) / 255;

        // option2 -> sparking (0 = default)
        const uint8_t sparkIn = (option2 == 0) ? 120 : option2;
        _sparking = 50 + (sparkIn * 150) / 255;

        // Reverse direction from dedicated config.reverse flag
        _reverseDirection = reverseDir;

        // Step 1. Cool down every cell a little
        for (uint16_t i = 0; i < _numCells; i++)
        {
            _heat[i] = FastLEDMath::qsub8(_heat[i],
                                          FastLEDMath::random8(0, ((_cooling * 10) / _numCells) + 2));
        }

        // Step 2. Heat from each cell drifts 'up' and diffuses a little
        for (int k = _numCells - 1; k >= 2; k--)
        {
            _heat[k] = (_heat[k - 1] + _heat[k - 2] + _heat[k - 2]) / 3;
        }

        // Step 3. Randomly ignite new 'sparks' of heat near the bottom
        if (FastLEDMath::random8() < _sparking)
        {
            int y = FastLEDMath::random8(7);
            if (y < _numCells)
            {
                _heat[y] = FastLEDMath::qadd8(_heat[y],
                                              FastLEDMath::random8(160, 255));
            }
        }

        // Step 4. Map from heat cells to LED colors
        for (uint16_t j = 0; j < _numCells; j++)
        {
            // Use feature2 for color mode (normal fire vs blue fire)
            uint32_t color;
            if (blueMode)
            {
                color = blueFire(_heat[j]); // Blue fire variant
            }
            else
            {
                color = heatColor(_heat[j]); // Normal fire
            }

            // Apply master brightness
            uint8_t r = ((color >> 16) & 0xFF);
            uint8_t g = ((color >> 8) & 0xFF);
            uint8_t b = (color & 0xFF);

            // Scale by master brightness
            r = FastLEDMath::scale8(r, intensity);
            g = FastLEDMath::scale8(g, intensity);
            b = FastLEDMath::scale8(b, intensity);

            // Set pixel (optionally reverse direction)
            uint16_t pixelnum;
            if (_reverseDirection)
            {
                pixelnum = (_numCells - 1) - j;
            }
            else
            {
                pixelnum = j;
            }

            segment->setPixel(pixelnum, r, g, b);
        }
    }

    void reset() override
    {
        // Clear heat array
        for (uint16_t i = 0; i < MAX_HEAT_CELLS; i++)
        {
            _heat[i] = 0;
        }
    }

    const char* getName(const char* lang = nullptr) override
    {
        return "Fire";
    }

    const char* getDescription(const char* lang = nullptr) override
    {
        return "Realistic fire simulation with flickering";
    }

    // Parameter API
    uint8_t getParameterCount() const override { return 5; }

    const char* getParameterName(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return "Speed";
            case 1: return "Cooling";
            case 2: return "Sparking";
            case 3: return "ReverseDirection";
            case 4: return "BlueFireMode";
            default: return "";
        }
    }

    const char* getParameterDescription(uint8_t index, const char* lang = "de") const override
    {
        switch (index)
        {
            case 0: return PARAM_DESC_DE_EN("Geschwindigkeit: Animationsgeschwindigkeit (ungenutzt)", "Speed: Animation speed (unused)");
            case 1: return PARAM_DESC_DE_EN("Kühlrate: Wie schnell das Feuer abkühlt (20-100)", "Cooling rate: How fast the fire cools down (20-100)");
            case 2: return PARAM_DESC_DE_EN("Funkenrate: Wie oft neue Funken entstehen (50-200)", "Spark rate: How often new sparks appear (50-200)");
            case 3: return PARAM_DESC_DE_EN("Richtung umkehren: Feuer nach unten", "Reverse direction: Fire downwards");
            case 4: return PARAM_DESC_DE_EN("Blaufeuermodus: Blaues statt oranges Feuer", "Blue fire mode: Blue instead of orange fire");
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
            case 0: return 128; // Speed (unused dummy)
            case 1: return 90;  // Cooling (maps to ~55 in algorithm)
            case 2: return 120; // Sparking (maps to ~120 in algorithm)
            case 3: return 0;   // ReverseDirection off
            case 4: return 0;   // BlueFireMode off (normal orange fire)
            default: return 0;
        }
    }

    uint32_t getParameterMin(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 1; // Speed min
            case 1: return 0; // Cooling min (maps to 20)
            case 2: return 0; // Sparking min (maps to 50)
            case 3: return 0; // ReverseDirection false
            case 4: return 0; // BlueFireMode false
            default: return 0;
        }
    }

    uint32_t getParameterMax(uint8_t index) const override
    {
        switch (index)
        {
            case 0: return 255; // Speed max
            case 1: return 255; // Cooling max (maps to 100)
            case 2: return 255; // Sparking max (maps to 200)
            case 3: return 1;   // ReverseDirection true
            case 4: return 1;   // BlueFireMode true
            default: return 255;
        }
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment) return 0;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: return config.speed;    // Speed (unused)
            case 1: return config.option1;  // Cooling
            case 2: return config.option2;  // Sparking
            case 3: return config.feature1; // ReverseDirection
            case 4: return config.feature2; // BlueFireMode
            default: return 0;
        }
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment) return;
        auto& config = segment->getConfig();
        switch (index)
        {
            case 0: config.speed = static_cast<uint8_t>(value); break;   // Speed (unused)
            case 1: config.option1 = static_cast<uint8_t>(value); break; // Cooling (20-100)
            case 2: config.option2 = static_cast<uint8_t>(value); break; // Sparking (50-200)
            case 3: config.feature1 = static_cast<bool>(value); break;   // ReverseDirection
            case 4: config.feature2 = static_cast<bool>(value); break;   // BlueFireMode
            default: break;
        }
    }

  private:
    /**
     * Convert heat value to fire color (black->red->yellow->white)
     * Based on FastLED's HeatColor function
     */
    uint32_t heatColor(uint8_t temperature)
    {
        uint8_t heatramp = temperature & 0x3F; // 0..63
        heatramp <<= 2;                        // scale up to 0..252

        // figure out which third of the spectrum we're in:
        if (temperature & 0x80)
        {
            // hottest third - yellow to white
            uint8_t r = 255;
            uint8_t g = 255;
            uint8_t b = heatramp;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        else if (temperature & 0x40)
        {
            // middle third - red to yellow
            uint8_t r = 255;
            uint8_t g = heatramp;
            uint8_t b = 0;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        else
        {
            // coolest third - black to red
            uint8_t r = heatramp;
            uint8_t g = 0;
            uint8_t b = 0;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }

    /**
     * Convert heat value to blue fire color (black->blue->cyan->white)
     */
    uint32_t blueFire(uint8_t temperature)
    {
        uint8_t heatramp = temperature & 0x3F; // 0..63
        heatramp <<= 2;                        // scale up to 0..252

        // figure out which third of the spectrum we're in:
        if (temperature & 0x80)
        {
            // hottest third - cyan to white
            uint8_t r = heatramp;
            uint8_t g = 255;
            uint8_t b = 255;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        else if (temperature & 0x40)
        {
            // middle third - blue to cyan
            uint8_t r = 0;
            uint8_t g = heatramp;
            uint8_t b = 255;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        else
        {
            // coolest third - black to blue
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = heatramp;
            return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
};