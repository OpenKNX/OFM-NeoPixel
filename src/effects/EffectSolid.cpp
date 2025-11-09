#include "EffectSolid.h"
#include "../Segment.h"

/**
 * @brief Update - Fills all pixels with primaryRGBW color
 * @param segment The segment to update
 * @param deltaTime Time since last update (ms) - unused
 */
void EffectSolid::update(Segment* segment, uint32_t deltaTime)
{
    if (!segment) return;

    // Read the effect configuration from the segment
    const EffectConfig& config = segment->getConfig();

    // Unpack RGBW from 32-bit value (0xRRGGBBWW)
    uint8_t r = (config.primaryRGBW >> 24) & 0xFF;
    uint8_t g = (config.primaryRGBW >> 16) & 0xFF;
    uint8_t b = (config.primaryRGBW >> 8) & 0xFF;
    uint8_t w = config.primaryRGBW & 0xFF;

    // Set all pixels in the segment to this color
    for (uint16_t i = 0; i < segment->getLength(); i++)
    {
        segment->setPixel(i, r, g, b, w); // Set pixel with RGBW
    }
}