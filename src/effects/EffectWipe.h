/**
 * @file EffectWipe.h
 * @brief Wipe/Slide Effect - STATELESS
 *
 * Color wipe from one side to another
 * Supports 6 directions: L-R, R-L, T-B, B-T, Center-Out, Edges-In
 *
 * Parameters:
 *   [0] Direction (0-5) - Wipe direction (see WipeDirection enum)
 *
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
 *   - primaryRGBW: Wipe color (RGB packed in upper 24 bits)
 *   - secondaryRGBW: Background color (RGB packed in upper 24 bits)
 */

#include "../Segment.h"
#include "Effect.h"
#include <stdint.h>

class Segment; // Forward declaration

/**
 * Wipe Direction - stored in EffectConfig.mode or parameter
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
    EffectWipe() = default;
    virtual ~EffectWipe() = default;
    
    void update(Segment* segment, uint32_t deltaTime) override;
    const char* getName() override { return "Wipe"; }

    // ====================================================================
    // Parameter API
    // ====================================================================
    uint8_t getParameterCount() const override { return 1; }

    const char* getParameterName(uint8_t index) const override
    {
        return (index == 0) ? "Direction" : nullptr;
    }

    ParameterType getParameterType(uint8_t index) const override
    {
        return ParameterType::PARAM_ENUM;
    }

    uint32_t getParameterDefault(uint8_t index) const override
    {
        return 0; // WIPE_LEFT_TO_RIGHT
    }

    uint32_t getParameter(const Segment* segment, uint8_t index) const override
    {
        if (!segment || index != 0) return 0;
        return segment->getConfig().mode;
    }

    void setParameter(Segment* segment, uint8_t index, uint32_t value) override
    {
        if (!segment || index != 0) return;
        const_cast<EffectConfig&>(segment->getConfig()).mode = value;
    }

    // Enum values for Direction parameter
    uint8_t getEnumValueCount(uint8_t paramIndex) const override
    {
        return (paramIndex == 0) ? 6 : 0;
    }

    const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const override
    {
        if (paramIndex != 0) return nullptr;
        switch (enumValue)
        {
            case WIPE_LEFT_TO_RIGHT: return "Left->Right";
            case WIPE_RIGHT_TO_LEFT: return "Right->Left";
            case WIPE_TOP_TO_BOTTOM: return "Top->Bottom";
            case WIPE_BOTTOM_TO_TOP: return "Bottom->Top";
            case WIPE_CENTER_OUT: return "Center->Out";
            case WIPE_EDGES_IN: return "Edges->In";
            default: return nullptr;
        }
    }
};

