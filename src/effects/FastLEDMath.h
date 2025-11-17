/**
 * @file FastLEDMath.h
 * @brief FastLED-compatible math helpers for LED effects
 *
 * Port of FastLED's essential math functions for use with NeoPixel library.
 * Based on FastLED library (MIT License) - https://github.com/FastLED/FastLED
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace FastLEDMath
{

    // ============================================================================
    // Sine wave functions
    // ============================================================================

    /**
     * @brief Fast 8-bit sine approximation
     * @param theta Input angle (0-255 maps to 0-360°)
     * @return Sine value (0-255, offset by 128)
     */
    inline uint8_t sin8(uint8_t theta)
    {
        static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
        uint8_t offset = theta;
        if (theta & 0x40)
        {
            offset = (uint8_t)255 - offset;
        }
        offset &= 0x3F; // 0-63

        uint8_t secoffset = offset & 0x0F; // 0-15
        if (theta & 0x40) secoffset++;

        uint8_t section = offset >> 4; // 0-3
        uint8_t s2 = section * 2;
        const uint8_t* p = b_m16_interleave;
        p += s2;
        uint8_t b = *p;
        p++;
        uint8_t m16 = *p;

        uint8_t mx = (m16 * secoffset) >> 4;

        int8_t y = mx + b;
        if (theta & 0x80) y = -y;

        y += 128;

        return y;
    }

    /**
     * @brief Fast 16-bit sine approximation
     * @param theta Input angle (0-65535 maps to 0-360°)
     * @return Sine value (0-65535, offset by 32768)
     */
    inline uint16_t sin16(uint16_t theta)
    {
        static const uint16_t base[] = {
            0, 6393, 12539, 18204, 23170, 27245, 30273, 32137};
        static const uint8_t slope[] = {
            49, 48, 44, 38, 31, 23, 14, 4};

        uint16_t offset = (theta & 0x3FFF) >> 3; // 0-8191
        if (theta & 0x4000) offset = 2047 - offset;

        uint8_t section = offset / 256; // 0-7
        uint16_t b = base[section];
        uint8_t m = slope[section];

        uint8_t secoffset8 = (uint8_t)(offset) / 2;

        uint16_t mx = m * secoffset8;
        int16_t y = mx + b;

        if (theta & 0x8000) y = -y;

        y += 32768;

        return y;
    }

    // ============================================================================
    // Scaling Functions (MUST be before beatsin!)
    // ============================================================================

    /**
     * @brief Scale 8-bit value by 8-bit factor
     * @param i Value to scale (0-255)
     * @param scale Scale factor (0-255, where 255 = 100%)
     * @return Scaled value
     */
    inline uint8_t scale8(uint8_t i, uint8_t scale)
    {
        return ((uint16_t)i * (uint16_t)(scale)) >> 8;
    }

    /**
     * @brief Scale 16-bit value by 16-bit factor
     */
    inline uint16_t scale16(uint16_t i, uint16_t scale)
    {
        return ((uint32_t)i * (uint32_t)(scale)) >> 16;
    }

    /**
     * @brief Scale 8-bit value with video correction (prevents full black)
     */
    inline uint8_t scale8_video(uint8_t i, uint8_t scale)
    {
        uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
        return j;
    }

    // ============================================================================
    // Sine Wave Functions (using scale8/scale16)
    // ============================================================================

    /**
     * @brief Generates sine wave with custom range
     * @param beats_per_minute BPM for oscillation speed
     * @param lowest Minimum output value
     * @param highest Maximum output value
     * @param timebase Optional time offset (default: millis())
     * @return Value oscillating between lowest and highest
     */
    inline uint8_t beatsin8(uint16_t beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                            uint32_t timebase = 0, uint8_t phase_offset = 0)
    {
        if (timebase == 0) timebase = millis();
        uint16_t beat = (timebase * beats_per_minute * 280) >> 16;
        uint8_t beatsin = sin8(beat + phase_offset);
        uint8_t rangewidth = highest - lowest;
        uint8_t scaledbeat = scale8(beatsin, rangewidth);
        uint8_t result = lowest + scaledbeat;
        return result;
    }

    /**
     * @brief 16-bit beatsin variant
     */
    inline uint16_t beatsin16(uint16_t beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535,
                              uint32_t timebase = 0, uint16_t phase_offset = 0)
    {
        if (timebase == 0) timebase = millis();
        uint16_t beat = (timebase * beats_per_minute) >> 8;
        uint16_t beatsin = sin16(beat + phase_offset);
        uint16_t rangewidth = highest - lowest;
        uint16_t scaledbeat = scale16(beatsin, rangewidth);
        uint16_t result = lowest + scaledbeat;
        return result;
    }

    /**
     * @brief High-precision beatsin (88 = 8.8 fixed point)
     */
    inline uint16_t beatsin88(uint16_t beats_per_minute_88, uint16_t lowest = 0, uint16_t highest = 65535,
                              uint32_t timebase = 0, uint16_t phase_offset = 0)
    {
        if (timebase == 0) timebase = millis();
        uint16_t beat = (timebase * beats_per_minute_88) >> 8;
        uint16_t beatsin = sin16(beat + phase_offset);
        uint16_t rangewidth = highest - lowest;
        uint16_t scaledbeat = scale16(beatsin, rangewidth);
        uint16_t result = lowest + scaledbeat;
        return result;
    }

    // ============================================================================
    // Color Blending & Mixing
    // ============================================================================

    /**
     * @brief Blend two 8-bit values
     * @param a First value
     * @param b Second value
     * @param amountOfB Blend amount (0 = all a, 255 = all b)
     */
    inline uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB)
    {
        uint8_t result = scale8(b - a, amountOfB) + a;
        return result;
    }

    /**
     * @brief Subtract from value with floor at 0
     */
    inline uint8_t qsub8(uint8_t i, uint8_t j)
    {
        int t = i - j;
        if (t < 0) t = 0;
        return t;
    }

    /**
     * @brief Add to value with ceiling at 255
     */
    inline uint8_t qadd8(uint8_t i, uint8_t j)
    {
        unsigned int t = i + j;
        if (t > 255) t = 255;
        return t;
    }

    // ============================================================================
    // HSV to RGB Conversion
    // ============================================================================

    /**
     * @brief HSV to RGB conversion (FastLED compatible)
     * @param h Hue (0-255)
     * @param s Saturation (0-255)
     * @param v Value/Brightness (0-255)
     * @return 32-bit RGB (0x00RRGGBB)
     */
    inline uint32_t hsv2rgb_rainbow(uint8_t h, uint8_t s, uint8_t v)
    {
        // Yellow has a higher inherent brightness than
        // any other color; 'pure' yellow is perceived to
        // be 93% as bright as white. In order to make
        // yellow appear the correct relative brightness,
        // it has to be rendered brighter than all other
        // colors. Level Y1 is a moderate boost, the default.

        const uint8_t Y1 = 1;
        const uint8_t Y2 = 0; // Yellow boost level 2 (0=off for most LEDs)

        // G2/Gscale: Color correction for specific LED types (currently unused)
        // In FastLED these were used for LEDs with oversaturated green
        // Keep for future LED type color correction support
        [[maybe_unused]] const uint8_t G2 = 0;     // Green divide by 2
        [[maybe_unused]] const uint8_t Gscale = 0; // Green scaling

        uint8_t offset = h & 0x1F; // 0-31

        uint8_t offset8 = offset << 3;

        uint8_t third = scale8(offset8, (256 / 3));

        uint8_t r, g, b;

        if (!(h & 0x80))
        {
            if (!(h & 0x40))
            {
                // 0-63: red to yellow-ish
                r = 255 - third;
                g = third;
                b = 0;
            }
            else
            {
                // 64-127: yellow-ish to green
                r = 171;
                g = 85 + third;
                b = 0;
            }
            if (Y1 == 1)
            {
                r = (r * 3) >> 2;
            }
            if (Y2 == 1)
            {
                r = r >> 1;
            }
        }
        else
        {
            if (!(h & 0x40))
            {
                // 128-191: green to cyan-ish
                r = 0;
                g = 255 - third;
                b = third;
            }
            else
            {
                // 192-255: cyan-ish to blue
                r = 0;
                g = 171 - third;
                b = 85 + third;
            }
        }

        if (s != 255)
        {
            if (s == 0)
            {
                r = 255;
                b = 255;
                g = 255;
            }
            else
            {
                uint8_t desat = 255 - s;
                desat = scale8(desat, desat);

                uint8_t satscale = 255 - desat;

                if (r) r = scale8(r, satscale);
                if (g) g = scale8(g, satscale);
                if (b) b = scale8(b, satscale);

                uint8_t brightness_floor = desat;
                r += brightness_floor;
                g += brightness_floor;
                b += brightness_floor;
            }
        }

        if (v != 255)
        {
            v = scale8_video(v, v);
            if (v)
            {
                if (r) r = scale8(r, v);
                if (g) g = scale8(g, v);
                if (b) b = scale8(b, v);
            }
            else
            {
                r = 0;
                g = 0;
                b = 0;
            }
        }

        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    // ============================================================================
    // Noise & Random
    // ============================================================================

    /**
     * @brief Fast random number generator (0-255)
     */
    inline uint8_t random8()
    {
        static uint16_t seed = 1;
        seed = (seed * 2053) + 13849;
        return (uint8_t)(seed >> 8);
    }

    /**
     * @brief Fast random in range [0, lim)
     */
    inline uint8_t random8(uint8_t lim)
    {
        uint8_t r = random8();
        r = scale8(r, lim);
        return r;
    }

    /**
     * @brief Fast random in range [min, max)
     */
    inline uint8_t random8(uint8_t min, uint8_t max)
    {
        uint8_t delta = max - min;
        uint8_t r = random8(delta) + min;
        return r;
    }

    /**
     * @brief 16-bit random number generator
     */
    inline uint16_t random16()
    {
        return (uint16_t(random8()) << 8) | random8();
    }

    /**
     * @brief 16-bit random in range [0, lim)
     */
    inline uint16_t random16(uint16_t lim)
    {
        uint32_t r = random16();
        r = (r * uint32_t(lim)) >> 16;
        return uint16_t(r);
    }

    /**
     * @brief Linear interpolation between a and b by amount
     * @param a First value
     * @param b Second value  
     * @param frac Amount (0-255, where 0=all a, 255=all b)
     */
    inline uint8_t lerp8by8(uint8_t a, uint8_t b, uint8_t frac)
    {
        if (frac == 0) return a;
        if (frac == 255) return b;
        
        uint8_t result;
        if (b > a) {
            uint8_t delta = b - a;
            uint8_t scaled = scale8(delta, frac);
            result = a + scaled;
        } else {
            uint8_t delta = a - b;
            uint8_t scaled = scale8(delta, frac);
            result = a - scaled;
        }
        return result;
    }

    // ============================================================================
    // Dimming & Fading
    // ============================================================================

    /**
     * @brief Reduce brightness by scaling factor
     * @param value RGB component (0-255)
     * @param fadefactor Amount to fade (255 = black, 0 = no change)
     */
    inline uint8_t fadeToBlackBy(uint8_t value, uint8_t fadefactor)
    {
        return scale8(value, 255 - fadefactor);
    }

    /**
     * @brief Dim value toward black
     */
    inline uint8_t dim8_raw(uint8_t x)
    {
        return scale8(x, x);
    }

    /**
     * @brief Dim with video correction
     */
    inline uint8_t dim8_video(uint8_t x)
    {
        return scale8_video(x, x);
    }

    /**
     * @brief Brighten value (inverse of dim)
     */
    inline uint8_t brighten8_raw(uint8_t x)
    {
        uint8_t ix = 255 - x;
        return 255 - scale8(ix, ix);
    }

} // namespace FastLEDMath
