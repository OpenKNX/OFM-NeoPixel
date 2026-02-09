/**
 * @file HclPixelTransform.h
 * @brief HCL Pixel Transformation Callback for VirtualStrip
 *
 * Provides VirtualStrip pixel transform callback that applies HCL color temperature
 * adjustments to individual pixels based on segment configuration. 
 * This allows HCL effects to be applied at the pixel level, supporting RGB, 
 * RGBW and RGBCCT strips. The transformation is optimized for performance, 
 * with early exits and caching to minimize CPU usage during rendering.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
*/

#pragma once

#include "HclManager.h"
#include <stdint.h>
#include <vector>

// Forward declarations
class Segment;

/**
 * @brief Per-segment HCL configuration for pixel transformation
 *
 * This struct is populated by the OAM layer from ETS parameters
 * and passed to the Library's pixel transformation callback.
 */
struct HclSegmentConfig
{
    HclMode hclMode = HclMode::Disabled; ///< HCL mode for this segment
    HclConfig customHclConfig;           ///< Custom HCL config (used if hclMode == Custom)
};

/**
 * @brief Context data for HCL pixel transformation callback
 *
 * The OAM layer creates this context and passes it to VirtualStrip's
 * setPixelTransformCallback(). The Library uses it to apply HCL
 * transformations without directly accessing ETS parameters.
 */
struct HclTransformContext
{
    std::vector<Segment*> segments;               ///< All segments (for pixel ownership lookup)
    std::vector<HclSegmentConfig> segmentConfigs; ///< Per-segment HCL configurations
    HclManager* globalHclManager = nullptr;       ///< Global HCL manager (if enabled)
    HclConfig globalHclConfig;                    ///< Global HCL configuration

    // Performance optimization: Pixel→Segment Lookup Table
    // Pre-computed at setup to avoid O(n) search on every pixel (8,280x/second!)
    // Maps pixelIndex → segmentIndex in O(1) time
    std::vector<uint8_t> pixelToSegmentIndex; ///< [pixelIndex] = segmentIndex (0xFF = no segment)

    // Performance optimization: Cache Kelvin→RGB conversion
    // Kelvin changes rarely (every few minutes), but callback is called 1000s of times/second
    uint16_t cachedKelvin = 0; ///< Last Kelvin value used for RGB calculation
    uint8_t cachedKr = 0;      ///< Cached red component for current Kelvin
    uint8_t cachedKg = 0;      ///< Cached green component for current Kelvin
    uint8_t cachedKb = 0;      ///< Cached blue component for current Kelvin

    // Performance optimization: Cache reference RGB (6500K) for brightness compensation
    // This is constant and only needs to be calculated once
    uint8_t cachedRefR = 0; ///< Cached reference red (6500K)
    uint8_t cachedRefG = 0; ///< Cached reference green (6500K)
    uint8_t cachedRefB = 0; ///< Cached reference blue (6500K)
    bool refCached = false; ///< Whether reference RGB is cached
};

/**
 * @brief HCL Pixel Transformation Utilities
 *
 * Provides performance-optimized functions for HCL color temperature
 * transformations at the pixel level. Used by VirtualStrip callback.
 */
class HclPixelTransform
{
  public:
    /**
     * @brief Calculate HCL blend weight from saturation
     *
     * @param applyMode HCL apply mode (0=AllColors, 1=WhiteOnly, 2=HighSaturation)
     * @param sat Current pixel saturation (0-255)
     * @param threshold Saturation threshold from config
     * @return uint8_t Weight (0-255), 0 means skip HCL completely
     */
    static uint8_t hclWeightFromSat(uint8_t applyMode, uint8_t sat, uint8_t threshold);

    /**
     * @brief Convert Kelvin temperature to RGB values
     *
     * @param kelvin Color temperature in Kelvin (2000-9000)
     * @param r Output red component (0-255)
     * @param g Output green component (0-255)
     * @param b Output blue component (0-255)
     */
    static void kelvinToRGB(uint16_t kelvin, uint8_t& r, uint8_t& g, uint8_t& b);

    /**
     * @brief Apply HCL transformation using cached Kelvin RGB values
     *
     * PERFORMANCE OPTIMIZED: Uses pre-calculated Kelvin RGB instead of computing it.
     *
     * @param ctx Transform context with cached RGB values
     * @param weight Blend weight (0-255)
     * @param config HCL configuration
     * @param r Pixel red (in/out)
     * @param g Pixel green (in/out)
     * @param b Pixel blue (in/out)
     * @param ww Pixel warm white (in/out, nullable)
     * @param cw Pixel cool white (in/out, nullable)
     */
    static void applyToPixelCached(HclTransformContext* ctx,
                                   uint8_t weight,
                                   const HclConfig& config,
                                   uint8_t& r, uint8_t& g, uint8_t& b,
                                   uint8_t* ww, uint8_t* cw);

    /**
     * @brief HCL pixel transformation callback for VirtualStrip
     *
     * This callback applies HCL color temperature adjustments to pixels
     * based on segment configuration. It is designed to be registered
     * with VirtualStrip::setPixelTransformCallback().
     *
     * @param pixelIndex Index of the pixel being processed
     * @param r Red component (in/out)
     * @param g Green component (in/out)
     * @param b Blue component (in/out)
     * @param ww Warm White component (in/out, nullptr for RGB strips)
     * @param cw Cool White component (in/out, nullptr for RGB/RGBW strips)
     * @param userData Pointer to HclTransformContext
     */
    static void Callback(uint16_t pixelIndex,
                         uint8_t& r, uint8_t& g, uint8_t& b,
                         uint8_t* ww, uint8_t* cw,
                         void* userData);

  private:
    static inline uint8_t u8_max3(uint8_t a, uint8_t b, uint8_t c)
    {
        return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
    }

    static inline uint8_t u8_min3(uint8_t a, uint8_t b, uint8_t c)
    {
        return (a < b) ? ((a < c) ? a : c) : ((b < c) ? b : c);
    }

    static inline uint8_t lerp_u8(uint8_t a, uint8_t b, uint8_t frac /*0..255*/)
    {
        const uint16_t inv = 255u - frac;
        return (uint8_t)((a * inv + b * (uint16_t)frac + 127u) / 255u);
    }

    static inline uint8_t clamp_u8(int v)
    {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return (uint8_t)v;
    }

    // Integer smoothstep (t in 0..255)
    static inline uint8_t smoothstep_u8(uint8_t t)
    {
        const uint32_t tt = (uint32_t)t * (uint32_t)t;
        const uint32_t a = 3u * 255u - 2u * (uint32_t)t;
        const uint32_t v = (tt * a + (255u * 255u / 2u)) / (255u * 255u);
        return (uint8_t)((v > 255u) ? 255u : v);
    }

    // Internal helper for weight calculation with curve support
    static inline uint8_t hclWeightFromSatInternal(uint8_t sat, uint8_t threshold, uint8_t applyMode, uint8_t curve)
    {
        uint32_t w = 0;

        if (applyMode == 1) // WhiteOnly
        {
            // Only low-saturation (whitish) colors get HCL
            if (threshold == 0)
            {
                w = (sat == 0) ? 255u : 0u;
            }
            else
            {
                if (sat >= threshold) return 0;
                w = 255u - ((uint32_t)sat * 255u) / threshold;
            }
        }
        else if (applyMode == 0) // AllColors
        {
            // ALL colors get HCL, regardless of saturation
            w = 255u;
        }
        else // HighSaturation (applyMode == 2)
        {
            // Only high-saturation (colorful) colors get HCL
            // Inverted logic from WhiteOnly
            if (threshold >= 255)
            {
                w = (sat == 255) ? 255u : 0u;
            }
            else
            {
                if (sat < threshold) return 0;
                const uint32_t denom = (255u - (uint32_t)threshold);
                w = (((uint32_t)sat - (uint32_t)threshold) * 255u) / denom;
            }
        }

        if (w > 255u) w = 255u;
        uint8_t w8 = (uint8_t)w;

        // Apply curve
        if (curve == 1)
        {
            w8 = smoothstep_u8(w8);
        }

        return w8;
    }
}; // Class HclPixelTransform