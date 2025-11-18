#include "EffectSolid.h"
#include "../Segment.h"

/**
 * @brief Update - Fills all pixels with config color components
 * @param segment The segment to update
 * @param deltaTime Time since last update (ms) - unused
 */
void EffectSolid::update(Segment* segment, uint32_t deltaTime)
{
    if (!segment) return;

    // Read the effect configuration from the segment
    const EffectConfig& config = segment->getConfig();

    // Use config getter methods for RGBW components
    uint8_t r = config.r();
    uint8_t g = config.g();
    uint8_t b = config.b();
    uint8_t w = config.w();

    // Set all pixels in the segment to this color
    for (uint16_t i = 0; i < segment->getLength(); i++)
    {
        segment->setPixel(i, r, g, b, w); // Set pixel with RGBW
    }
}