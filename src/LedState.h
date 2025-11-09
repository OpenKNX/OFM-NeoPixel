/**
 * @file LedState.h
 * @brief State Machine for LED Segments
 *
 * Defines the states of LED segments for
 * state management and debug tracking.
 * 
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include <stdint.h>

/**
 * LED Segment State Machine
 *
 * States:
 * - IDLE: Segment is inactive/waiting
 * - EFFECT_RUNNING: Effect is currently running
 * - TRANSITIONING: Transition between effects
 * - ERROR: Hardware error or invalid state
 */
enum class LedState : uint8_t
{
    IDLE = 0,           // Nothing to do
    EFFECT_RUNNING = 1, // Effect is active
    TRANSITIONING = 2,  // Switching between effects
    ERROR = 3           // Error state
};

/**
 * LED State Helper
 */
class LedStateHelper
{
  public:
    /**
     * Convert state to string
     */
    static const char* toString(LedState state)
    {
        switch (state)
        {
            case LedState::IDLE: return "IDLE";
            case LedState::EFFECT_RUNNING: return "RUNNING";
            case LedState::TRANSITIONING: return "TRANSIT";
            case LedState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};
