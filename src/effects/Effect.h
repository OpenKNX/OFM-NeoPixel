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

#include "ParameterType.h"
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
     * Get effect description
     * @return Brief description of what the effect does
     */
    virtual const char* getDescription() { return "No description"; }

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

    // ====================================================================
    // Parameter Introspection API (Self-Describing Effects)
    // ====================================================================

    /**
     * @brief Get number of custom parameters
     * @return Parameter count (0 = no custom params)
     */
    virtual uint8_t getParameterCount() const { return 0; }

    /**
     * @brief Get parameter name
     * @param index Parameter index (0..count-1)
     * @return Parameter name or nullptr
     */
    virtual const char* getParameterName(uint8_t index) const { return nullptr; }

    /**
     * @brief Get parameter type
     * @param index Parameter index
     * @return ParameterType enum
     */
    virtual ParameterType getParameterType(uint8_t index) const { return ParameterType::PARAM_UINT8; }

    /**
     * @brief Get parameter default value
     * @param index Parameter index
     * @return Default value (packed in uint32_t)
     */
    virtual uint32_t getParameterDefault(uint8_t index) const { return 0; }

    /**
     * @brief Get parameter value from segment
     * @param segment The segment
     * @param index Parameter index
     * @return Parameter value (packed in uint32_t)
     */
    virtual uint32_t getParameter(const Segment* segment, uint8_t index) const { return 0; }

    /**
     * @brief Set parameter value in segment
     * @param segment The segment
     * @param index Parameter index
     * @param value New value
     */
    virtual void setParameter(Segment* segment, uint8_t index, uint32_t value) {}

    /**
     * @brief Get enum value name (for PARAM_ENUM only)
     * @param paramIndex Parameter index
     * @param enumValue Enum value
     * @return Name or nullptr
     */
    virtual const char* getEnumValueName(uint8_t paramIndex, uint8_t enumValue) const { return nullptr; }

    /**
     * @brief Get enum value count (for PARAM_ENUM only)
     * @param paramIndex Parameter index
     * @return Number of enum values
     */
    virtual uint8_t getEnumValueCount(uint8_t paramIndex) const { return 0; }

    /**
     * @brief Initialize parameters with default values
     *
     * Calls setParameter() for each parameter with getParameterDefault()
     * Should be called once when effect is assigned to segment.
     *
     * @param segment The segment to initialize
     */
    virtual void initializeDefaults(Segment* segment)
    {
        if (!segment) return;

        uint8_t count = getParameterCount();
        for (uint8_t i = 0; i < count; i++)
        {
            uint32_t defaultValue = getParameterDefault(i);
            setParameter(segment, i, defaultValue);
        }
    }

    // NO MEMBER VARIABLES - Stateless!
};
