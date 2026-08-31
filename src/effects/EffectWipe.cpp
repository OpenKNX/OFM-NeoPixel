#include "EffectWipe.h"
#include "../Segment.h"

void EffectWipe::onEnter(Segment* segment)
{
    if (!segment) return;
    const EffectConfig& config = segment->getConfig();
    segment->setAll(config.r2(), config.g2(), config.b2(), config.ww2(), config.cw2());
}

bool EffectWipe::isDone(const Segment* segment) const
{
    return segment && segment->getState().phase == WIPE_DONE;
}

/**
 * @brief Update the Wipe Effect
 * @param segment Pointer to segment containing config & state
 * @param deltaTime Time since last update in milliseconds
 */
void EffectWipe::update(Segment* segment, uint32_t deltaTime)
{
    if (!segment) return;

    const EffectConfig& config = segment->getConfig(); // Get config (read-only)

    EffectState& state = segment->getState(); // Get state (read/write)
    if (state.phase == WIPE_DONE) return;

    // Use config getter methods for primary color (wipe color) - 5-channel support
    uint8_t r = config.r();
    uint8_t g = config.g();
    uint8_t b = config.b();
    uint8_t ww = config.ww();
    uint8_t cw = config.cw();

    const uint8_t speedVal = config.speed; // 0 means "fastest"

    state.lastUpdate += deltaTime; // Speed control: accumulate time

    if (state.lastUpdate >= speedVal)
    {
        state.lastUpdate = 0;

        uint16_t length = segment->getLength();
        if (length == 0) return;

        // Set pixel at current position based on direction (stored in mode field)
        // Direction is stored in config.option1 (0..5)
        uint8_t dirVal = config.option1;
        if (dirVal > 5) dirVal = 5;
        WipeDirection dir = (WipeDirection)dirVal;

        switch (dir) // Determine which wipe direction to use
        {
            case WIPE_LEFT_TO_RIGHT: // Simple left-to-right, direct mapping
                if (state.position < length)
                {
                    segment->setPixel(state.position, r, g, b, ww, cw);
                }
                break;

            case WIPE_RIGHT_TO_LEFT: // Right-to-left, reverse index
                if (state.position < length)
                {
                    segment->setPixel(length - 1 - state.position, r, g, b, ww, cw);
                }
                break;

            case WIPE_TOP_TO_BOTTOM:
            case WIPE_BOTTOM_TO_TOP:
                // For 2D matrix - would need width/height from config
                // For now, treat as left-to-right (1D strips)
                if (state.position < length)
                {
                    segment->setPixel(state.position, r, g, b, ww, cw);
                }
                // ToDo: Add matrix dimensions to EffectConfig if needed
                break;

            case WIPE_CENTER_OUT: // From center outwards (both directions)
            {
                uint16_t center = length / 2;
                uint16_t offset = state.position / 2;

                if (state.position % 2 == 0)
                {
                    // Even: go right from center
                    uint16_t idx = center + offset;
                    if (idx < length)
                    {
                        segment->setPixel(idx, r, g, b, ww, cw);
                    }
                }
                else
                {
                    // Odd: go left from center
                    int16_t idx = center - offset - 1;
                    if (idx >= 0)
                    {
                        segment->setPixel(idx, r, g, b, ww, cw);
                    }
                }
            }
            break;

            case WIPE_EDGES_IN: // From both edges towards center
            {
                uint16_t offset = state.position / 2;

                if (state.position % 2 == 0)
                {
                    // Even: from left edge
                    if (offset < length)
                    {
                        segment->setPixel(offset, r, g, b, ww, cw);
                    }
                }
                else
                {
                    if (offset < length) // From the right edge, towards center
                    {
                        segment->setPixel(length - 1 - offset, r, g, b, ww, cw);
                    }
                }
            }
            break;
        }
        state.position++;             // Advance position
        if (state.position >= length)
        {
            if (config.feature1)
            {
                onEnter(segment);
                state.position = 0;
                state.lastUpdate = 0;
                state.phase = WIPE_RUNNING;
            }
            else
            {
                state.position = length;
                state.phase = WIPE_DONE;
            }
        }
    }
}
