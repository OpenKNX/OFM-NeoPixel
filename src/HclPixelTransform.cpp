#include "HclPixelTransform.h"
#include "IHardwareDriver.h"
#include <algorithm>
#include "LedState.h"
#include <Arduino.h>
#include <math.h>

/**
 * @brief HCL pixel transformation callback for VirtualStrip
 *
 * This callback is called by VirtualStrip during syncToPhysical() for each pixel.
 * It applies HCL color temperature adjustment based on the segment's HCL configuration.
 *
 * PERFORMANCE CRITICAL: Called for EVERY pixel on EVERY frame!
 * - Optimized for minimal CPU usage
 * - Early returns to avoid unnecessary work
 * - Direct segment lookup (segments store start/length)
 *
 * @param pixelIndex Index of the pixel being processed
 * @param r Red component (in/out)
 * @param g Green component (in/out)
 * @param b Blue component (in/out)
 * @param ww Warm White component (in/out, nullptr for RGB strips)
 * @param cw Cool White component (in/out, nullptr for RGB/RGBW strips)
 * @param userData Pointer to HclTransformContext
 */
void HclPixelTransform::Callback(uint16_t pixelIndex,
                                 uint8_t& r, uint8_t& g, uint8_t& b,
                                 uint8_t* ww, uint8_t* cw,
                                 void* userData)
{
    CallbackForOutput(pixelIndex, r, g, b, ww, cw, userData, 0, 0);
}

void HclPixelTransform::CallbackForOutput(uint16_t pixelIndex, uint8_t& r, uint8_t& g, uint8_t& b,
                                        uint8_t* ww, uint8_t* cw, void* userData,
                                        uint16_t warmKelvin, uint16_t coolKelvin)
{
    // FAST PATH: Early return if no context
    if (!userData) return;

    HclTransformContext* ctx = static_cast<HclTransformContext*>(userData);

    // FAST PATH: Early return if no segments configured
    const size_t segCount = ctx->segments.size();
    if (segCount == 0) return;

    if (pixelIndex >= ctx->pixelToSegmentIndex.size()) return; // Out of range

    const uint8_t segmentIndex = ctx->pixelToSegmentIndex[pixelIndex];
    if (segmentIndex == 0xFF) return; // Pixel doesn't belong to any segment

    // FAST PATH: Check if HCL is enabled for this segment
    if (segmentIndex >= ctx->segmentConfigs.size()) return;
    const NeoHclSegmentConfig& segCfg = ctx->segmentConfigs[segmentIndex];

    // FAST PATH: Skip if HCL disabled
    if (segCfg.hclMode == NeoHclMode::Disabled) return;

    // Determine LightManager source and config (Global vs Custom)
    const NeoHclConfig* hclConfig;

    if (segCfg.hclMode == NeoHclMode::Global)
    {
        hclConfig = &ctx->globalHclConfig;
    }
    else // NeoHclMode::Custom
    {
        hclConfig = &segCfg.customHclConfig;
    }

    NeoHclConfig outputConfig = *hclConfig;
    if (warmKelvin > 0 && warmKelvin < coolKelvin)
    {
        outputConfig.minKelvin = warmKelvin;
        outputConfig.maxKelvin = coolKelvin;
    }
    hclConfig = &outputConfig;

    const uint8_t masterNum = hclConfig->resolvedMaster;
    if (masterNum == 0 || masterNum > ctx->masterStates.size()) return;

    NeoHclMasterState& masterState = ctx->masterStates[masterNum - 1];
    if (masterState.blocked || masterState.kelvin == 0) return;

    // PERFORMANCE OPTIMIZATION: Cache Kelvin -> RGB conversion per LightManager master.
    if (masterState.kelvin != masterState.cachedKelvin)
    {
        kelvinToRGB(masterState.kelvin, masterState.cachedKr, masterState.cachedKg, masterState.cachedKb);
        masterState.cachedKelvin = masterState.kelvin;
    }

    // PERFORMANCE OPTIMIZATION: Cache reference RGB (6500K) for brightness compensation
    // This is constant and only needs to be calculated ONCE
    if (!ctx->refCached)
    {
        kelvinToRGB(6500, ctx->cachedRefR, ctx->cachedRefG, ctx->cachedRefB);
        ctx->refCached = true;
    }

    // FAST PATH: Early exit for AllColors mode (saturation doesn't matter)
    const uint8_t applyMode = static_cast<uint8_t>(hclConfig->applyMode);
    uint8_t weight = 255;
    if (applyMode == 1) // AllColors - always apply, skip saturation calculation
    {
        applyToPixelCached(ctx, masterState, weight, *hclConfig, r, g, b, ww, cw);
    }

    else
    {
        // For WhiteOnly and HighSaturation modes: Calculate saturation
        const uint8_t vmax = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
        const uint8_t vmin = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
        const uint8_t sat = (vmax == 0) ? 0 : (uint8_t)(((uint16_t)(vmax - vmin) * 255u) / vmax);

        // Check if we should apply HCL based on saturation and mode
        weight = hclWeightFromSat(applyMode, sat, hclConfig->saturationThreshold);
        if (weight == 0) return;

        // Apply HCL transformation to this pixel using cached values from context
        applyToPixelCached(ctx, masterState, weight, *hclConfig, r, g, b, ww, cw);
    }
}

// ============================================================================
// HclPixelTransform Class Implementation
// ============================================================================

uint8_t HclPixelTransform::hclWeightFromSat(uint8_t applyMode, uint8_t sat, uint8_t threshold)
{
    // Use linear curve (0) for weight calculation in callback
    return hclWeightFromSatInternal(sat, threshold, applyMode, 0);
}

void HclPixelTransform::kelvinToRGB(uint16_t kelvin, uint8_t& r, uint8_t& g, uint8_t& b)
{
    // Clamp Kelvin to reasonable range
    if (kelvin < 2000) kelvin = 2000;
    if (kelvin > 9000) kelvin = 9000;

    const float kf = (float)kelvin / 100.0f;
    float red, green, blue;

    // Red
    if (kf <= 66.0f)
    {
        red = 255.0f;
    }
    else
    {
        red = kf - 60.0f;
        red = 329.698727446f * powf(red, -0.1332047592f);
        if (red < 0.0f) red = 0.0f;
        if (red > 255.0f) red = 255.0f;
    }

    // Green
    if (kf <= 66.0f)
    {
        green = kf;
        green = 99.4708025861f * logf(green) - 161.1195681661f;
        if (green < 0.0f) green = 0.0f;
        if (green > 255.0f) green = 255.0f;
    }
    else
    {
        green = kf - 60.0f;
        green = 288.1221695283f * powf(green, -0.0755148492f);
        if (green < 0.0f) green = 0.0f;
        if (green > 255.0f) green = 255.0f;
    }

    // Blue
    if (kf >= 66.0f)
    {
        blue = 255.0f;
    }
    else if (kf <= 19.0f)
    {
        blue = 0.0f;
    }
    else
    {
        blue = kf - 10.0f;
        blue = 138.5177312231f * logf(blue) - 305.0447927307f;
        if (blue < 0.0f) blue = 0.0f;
        if (blue > 255.0f) blue = 255.0f;
    }

    r = (uint8_t)(red + 0.5f);
    g = (uint8_t)(green + 0.5f);
    b = (uint8_t)(blue + 0.5f);
}

void HclPixelTransform::applyToPixelCached(HclTransformContext* ctx,
                                           NeoHclMasterState& masterState,
                                           uint8_t weight,
                                           const NeoHclConfig& config,
                                           uint8_t& r, uint8_t& g, uint8_t& b,
                                           uint8_t* ww,
                                           uint8_t* cw)
{
    // Use cached Kelvin RGB values from the selected LightManager master.
    const uint8_t kr = masterState.cachedKr;
    const uint8_t kg = masterState.cachedKg;
    const uint8_t kb = masterState.cachedKb;

    // For RGB/RGBW
    const uint8_t wIn = (ww) ? *ww : 0;

    // Weight is already calculated by caller (includes saturation + curve)
    if (weight == 0) return;

    uint8_t strength = (config.strength > 100) ? 100 : config.strength;
    uint16_t wFrac16 = (uint16_t)weight * (uint16_t)strength;
    uint8_t frac = (uint8_t)((wFrac16 + 50u) / 100u);

    if (frac == 0) return;

    if (ww && cw)
    {
        // Native dual-white output uses the rendered white level, with RGB neutral
        // extraction governed by whiteMix. Endpoint calibration runs per physical output.
        const uint8_t neutral = u8_min3(r, g, b);
        const uint16_t level = std::min<uint16_t>(255, uint16_t(*ww) + *cw + uint16_t(neutral) * config.whiteMix / 100);
        uint8_t warm, cool;
        ProtocolHelper::kelvinToWWCW(masterState.kelvin, warm, cool, config.minKelvin, config.maxKelvin);
        const uint8_t targetCool = (uint32_t(level) * cool + 127) / 255;
        *ww = lerp_u8(*ww, level - targetCool, frac);
        *cw = lerp_u8(*cw, targetCool, frac);
        const uint8_t extracted = uint16_t(neutral) * config.whiteMix / 100;
        r = lerp_u8(r, r - extracted, frac);
        g = lerp_u8(g, g - extracted, frac);
        b = lerp_u8(b, b - extracted, frac);
        return;
    }

    // Use current pixel value/brightness basis
    const uint8_t vmax = u8_max3(r, g, b);
    uint8_t v = vmax;
    if (wIn > v) v = wIn;

    int tr = ((int)kr * (int)v) / 255;
    int tg = ((int)kg * (int)v) / 255;
    int tb = ((int)kb * (int)v) / 255;
    int tw = 0;

    // Brightness compensation using CACHED reference RGB (6500K)
    if (config.brightnessCompensation > 0)
    {
        // Use cached reference values (calculated once at startup)
        const uint32_t yRef = 54u * ctx->cachedRefR + 183u * ctx->cachedRefG + 19u * ctx->cachedRefB;
        const uint32_t yKel = 54u * kr + 183u * kg + 19u * kb;
        if (yKel > 0)
        {
            uint32_t scaleQ8 = (yRef << 8) / yKel;
            scaleQ8 = constrain(scaleQ8, 128u, 512u);

            int32_t delta = (int32_t)scaleQ8 - 256;
            scaleQ8 = (uint32_t)(256 + (delta * config.brightnessCompensation) / 100);

            tr = (int)((tr * (int)scaleQ8 + 128) >> 8);
            tg = (int)((tg * (int)scaleQ8 + 128) >> 8);
            tb = (int)((tb * (int)scaleQ8 + 128) >> 8);
        }
    }

    // RGBW: extract neutral part into W
    if (ww)
    {
        int n = u8_min3((uint8_t)clamp_u8(tr), (uint8_t)clamp_u8(tg), (uint8_t)clamp_u8(tb));
        uint8_t mix = (config.whiteMix > 100) ? 100 : config.whiteMix;
        int extracted = (n * (int)mix + 50) / 100;
        tr -= extracted;
        tg -= extracted;
        tb -= extracted;
        tw = extracted;
    }

    r = lerp_u8(r, clamp_u8(tr), frac);
    g = lerp_u8(g, clamp_u8(tg), frac);
    b = lerp_u8(b, clamp_u8(tb), frac);
    if (ww) *ww = lerp_u8(wIn, clamp_u8(tw), frac);
}