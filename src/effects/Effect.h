/**
 * @file Effect.h
 * @brief Base Effect Interface - STATELESS Design
 *
 * This is the base class for all effects.
 * 
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

/*
 * IMPORTANT: Effects are STATELESS!
 * - No member variables for state
 * - All config in segment->getConfig()
 * - All state in segment->getState()
 * - One singleton instance per effect type
 * - Multiple segments can share the same effect
 */

#pragma once

#include <stdint.h>

// Forward declaration
class Segment;

/**
 * Effect - Stateless Base class for NeoPixel Effects
 */
class Effect
{
  public:
    /**
     * Virtual Destructor
     */
    virtual ~Effect() {}

    /**
     * @brief Update effect - STATELESS
     *
     * Reads config from segment->getConfig()
     * Reads/writes state from segment->getState()
     * Writes pixels with segment->setPixel()
     *
     * @param segment The segment to animate (contains config & state)
     * @param deltaTime Time difference since last update in ms
     */
    virtual void update(Segment* segment, uint32_t deltaTime) = 0;

    /**
     * Reset effect state (for stateful effects)
     */
    virtual void reset() {}

    /**
     * Get effect name
     * @return Human-readable effect name
     */
    virtual const char* getName() { return "Unknown"; }

    /**
     * @brief Check if effect is done
     *
     * For finite effects (e.g. one-shot): true when done
     * For infinite effects (e.g. loop): false
     *
     * @param segment The segment
     * @return true if effect completed
     */
    virtual bool isDone(const Segment* segment) const
    {
        return false; // Default: Infinite
    }

    // NO MEMBER VARIABLES - Stateless!
};
