/**
 * @file EffectWipe.h
 * @brief Wipe/Slide Effect - STATELESS
 *
 * Color wipe from one side to another
 * Supports 6 directions: L-R, R-L, T-B, B-T, Center-Out, Edges-In

 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once

/**
 * State stored in Segment's EffectState:
 *   - position: Current wipe position (0..length-1)
 *   - lastUpdate: Time accumulator for speed control
 *
 * Config from Segment's EffectConfig:
 *   - speed: ms per step
 *   - direction: WipeDirection (0..5)
 *   - primaryRGBW: Wipe color (RGB packed in upper 24 bits)
 *   - secondaryRGBW: Background color (RGB packed in upper 24 bits)
 */

#include "Effect.h"
#include <stdint.h>

class Segment; // Forward declaration

/**
 * Wipe Direction - stored in EffectConfig.direction
 */
enum WipeDirection : uint8_t
{
    WIPE_LEFT_TO_RIGHT = 0, // Simple left-to-right
    WIPE_RIGHT_TO_LEFT = 1, // Right-to-left
    WIPE_TOP_TO_BOTTOM = 2, // Top-to-bottom (for 2D matrix)
    WIPE_BOTTOM_TO_TOP = 3, // Bottom-to-top (for 2D matrix)
    WIPE_CENTER_OUT = 4,    // From center outwards (both directions)
    WIPE_EDGES_IN = 5       // From edges towards center (both directions)
};

/**
 * EffectWipe - STATELESS Color Wipe Effect
 *
 * Wipes color across LEDs from one side to another
 * Zero member variables - all state in Segment
 */
class EffectWipe : public Effect
{
  public:
    EffectWipe() = default;          // Default constructor - STATELESS
    virtual ~EffectWipe() = default; // Default destructor
    void update(Segment* segment, uint32_t deltaTime) override;
    const char* getName() override { return "Wipe"; }
};
