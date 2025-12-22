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

        // Use option1 for cooling (20-100, default 55)
        _cooling = 20 + ((config.option1 > 0 ? config.option1 : 90) * 80) / 255;

        // Use option2 for sparking (50-200, default 120)
        _sparking = 50 + ((config.option2 > 0 ? config.option2 : 120) * 150) / 255;

        // Use feature1 for reverse direction
        _reverseDirection = config.feature1;

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
            if (config.feature2)
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
            r = FastLEDMath::scale8(r, config.intensity);
            g = FastLEDMath::scale8(g, config.intensity);
            b = FastLEDMath::scale8(b, config.intensity);

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

    const char* getName() override
    {
        return "Fire";
    }

    const char* getDescription() override
    {
        return "Realistic fire simulation with flickering";
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