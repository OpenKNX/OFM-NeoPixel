#include "HclPixelTransform.h"
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
        const uint8_t curve = (hclConfig->preserveCurve <= 2) ? hclConfig->preserveCurve : 0;
        weight = hclWeightFromSatInternal(sat, hclConfig->saturationThreshold, applyMode, curve);
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

    const bool isRGBCCT = ww && cw;

    // For RGB/RGBW/RGBCCT
    const uint8_t wIn = (ww) ? *ww : 0;
    const uint8_t cwIn = (cw) ? *cw : 0;

    // Weight is already calculated by caller (includes saturation + curve)
    if (weight == 0) return;

    uint8_t strength = (config.strength > 100) ? 100 : config.strength;
    uint16_t wFrac16 = (uint16_t)weight * (uint16_t)strength;
    uint8_t frac = (uint8_t)((wFrac16 + 50u) / 100u);

    if (frac == 0) return;

    // Use current pixel value/brightness basis
    const uint8_t vmax = u8_max3(r, g, b);
    uint8_t v = vmax;
    // On a true RGBCCT strip, existing WW/CW light is rebalanced below.  It
    // must not become an RGB Kelvin approximation as well; otherwise a direct
    // CCT command lights RGB dies in addition to the white channels.
    if (!isRGBCCT && wIn > v) v = wIn;

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

    // RGBCCT: direct existing white light is rebalanced between warm and cool
    // channels, while neutral RGB content is extracted into the same CCT pair.
    // This preserves total white intensity and makes LightManager HCL effective
    // on five-channel strips instead of silently skipping them.
    if (isRGBCCT)
    {
        uint16_t minKelvin = config.minKelvin;
        uint16_t maxKelvin = config.maxKelvin;
        if (minKelvin >= maxKelvin)
        {
            minKelvin = 2700;
            maxKelvin = 6500;
        }

        const uint16_t kelvin = (masterState.kelvin < minKelvin) ? minKelvin :
                                (masterState.kelvin > maxKelvin) ? maxKelvin :
                                masterState.kelvin;
        const uint32_t range = (uint32_t)maxKelvin - minKelvin;
        const uint8_t coolFraction = (uint8_t)(((uint32_t)(kelvin - minKelvin) * 255u + range / 2u) / range);
        const uint8_t warmFraction = (uint8_t)(255u - coolFraction);

        // White channels use a constant-sum brightness basis.  A direct CCT
        // command produces WW+CW=255, therefore it remains at full intensity
        // while HCL changes only the temperature ratio.
        const uint16_t sourceWhiteSum = (uint16_t)wIn + (uint16_t)cwIn;
        const uint8_t sourceWhite = (sourceWhiteSum > 255u) ? 255u : (uint8_t)sourceWhiteSum;
        uint16_t targetWW = ((uint16_t)sourceWhite * warmFraction + 127u) / 255u;
        uint16_t targetCW = ((uint16_t)sourceWhite * coolFraction + 127u) / 255u;

        const int neutral = u8_min3((uint8_t)clamp_u8(tr), (uint8_t)clamp_u8(tg), (uint8_t)clamp_u8(tb));
        const uint8_t mix = (config.whiteMix > 100) ? 100 : config.whiteMix;
        const int extracted = (neutral * (int)mix + 50) / 100;
        tr -= extracted;
        tg -= extracted;
        tb -= extracted;
        targetWW += ((uint16_t)extracted * warmFraction + 127u) / 255u;
        targetCW += ((uint16_t)extracted * coolFraction + 127u) / 255u;

        r = lerp_u8(r, clamp_u8(tr), frac);
        g = lerp_u8(g, clamp_u8(tg), frac);
        b = lerp_u8(b, clamp_u8(tb), frac);
        *ww = lerp_u8(wIn, clamp_u8(targetWW), frac);
        *cw = lerp_u8(cwIn, clamp_u8(targetCW), frac);
        return;
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