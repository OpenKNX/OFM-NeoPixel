#include "EffectWipe.h"
#include "../Segment.h"

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

    // Use config getter methods for primary color (wipe color)
    uint8_t r = config.r();
    uint8_t g = config.g();
    uint8_t b = config.b();
    uint8_t w = config.w();

    // Use config getter methods for secondary color (background color)
    uint8_t bgR = config.r2();
    uint8_t bgG = config.g2();
    uint8_t bgB = config.b2();
    uint8_t bgW = config.w2();

    state.lastUpdate += deltaTime; // Speed control: accumulate time

    if (state.lastUpdate >= config.speed)
    {
        state.lastUpdate = 0;

        uint16_t length = segment->getLength();

        // Set pixel at current position based on direction (stored in mode field)
        WipeDirection dir = (WipeDirection)config.mode;

        switch (dir) // Determine which wipe direction to use
        {
            case WIPE_LEFT_TO_RIGHT: // Simple left-to-right, direct mapping
                if (state.position < length)
                {
                    if (w > 0)
                    {
                        segment->setPixel(state.position, r, g, b, w); // Set pixel (wipe color)
                    }
                    else
                    {
                        segment->setPixel(state.position, r, g, b); // Set pixel (wipe color)
                    }
                }
                break;

            case WIPE_RIGHT_TO_LEFT: // Right-to-left, reverse index
                if (state.position < length)
                {
                    if (w > 0)
                    {
                        segment->setPixel(length - 1 - state.position, r, g, b, w); // Set pixel (wipe color)
                    }
                    else
                    {
                        segment->setPixel(length - 1 - state.position, r, g, b); // Set pixel (wipe color)
                    }
                }
                break;

            case WIPE_TOP_TO_BOTTOM:
            case WIPE_BOTTOM_TO_TOP:
                // For 2D matrix - would need width/height from config
                // For now, treat as left-to-right (1D strips)
                if (state.position < length)
                {
                    if (w > 0)
                    {
                        segment->setPixel(state.position, r, g, b, w);
                    }
                    else
                    {
                        segment->setPixel(state.position, r, g, b);
                    }
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
                        if (w > 0)
                        {
                            segment->setPixel(idx, r, g, b, w);
                        }
                        else
                        {
                            segment->setPixel(idx, r, g, b);
                        }
                    }
                }
                else
                {
                    // Odd: go left from center
                    int16_t idx = center - offset - 1;
                    if (idx >= 0)
                    {
                        if (w > 0)
                        {
                            segment->setPixel(idx, r, g, b, w);
                        }
                        else
                        {
                            segment->setPixel(idx, r, g, b);
                        }
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
                    if (offset < length / 2)
                    {
                        if (w > 0)
                        {
                            segment->setPixel(offset, r, g, b, w);
                        }
                        else
                        {
                            segment->setPixel(offset, r, g, b);
                        }
                    }
                }
                else
                {
                    if (offset < length / 2) // From the right edge, towards center
                    {
                        if (w > 0)
                        {
                            segment->setPixel(length - 1 - offset, r, g, b, w);
                        }
                        else
                        {
                            segment->setPixel(length - 1 - offset, r, g, b);
                        }
                    }
                }
            }
            break;
        }
        state.position++;             // Advance position
        if (state.position >= length) // Reset when complete
        {
            for (uint16_t i = 0; i < length; i++) // Clear all pixels to background color
            {
                if (bgW > 0)
                {
                    segment->setPixel(i, bgR, bgG, bgB, bgW);
                }
                else
                {
                    segment->setPixel(i, bgR, bgG, bgB);
                }
            }
            state.position = 0;
        }
    }
}
