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

    // Use config getter methods for RGBCCT components
    uint8_t r = config.r();
    uint8_t g = config.g();
    uint8_t b = config.b();
    uint8_t ww = config.ww(); // Warm White (or single White for RGBW)
    uint8_t cw = config.cw(); // Cool White (0 for non-5-channel strips)

    // Set all pixels in the segment
    // Use 5-channel setPixel which works for all strip types:
    // - For RGB strips: ww and cw are ignored
    // - For RGBW strips: ww is used as white, cw is ignored
    // - For RGBCCT strips: both ww and cw are used
    for (uint16_t i = 0; i < segment->getLength(); i++)
    {
        segment->setPixel(i, r, g, b, ww, cw);
    }
}