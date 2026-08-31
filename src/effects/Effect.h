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
#include <string.h>

// ====================================================================
// Multi-Language Helper Macro
// ====================================================================
/**
 * @brief Compile the parameter description texts into the firmware
 *
 * Parameter descriptions are build-time documentation, not runtime data: the
 * ETS help files under src/Baggages/Help_de are generated from these literals
 * by scripts/Build-EffectParameters.ps1, which parses the headers as text and
 * never looks at the binary. No caller of getParameterDescription() exists in
 * any product, but the method is virtual, so the linker cannot drop it and its
 * texts stay in flash - about 20 KB on a full effect set.
 *
 * Default off. Set -D NEOPIXEL_EFFECT_PARAM_DESCRIPTIONS=1 to keep the texts in
 * the image, e.g. when adding a console command that prints them.
 */
#ifndef NEOPIXEL_EFFECT_PARAM_DESCRIPTIONS
    #define NEOPIXEL_EFFECT_PARAM_DESCRIPTIONS 0
#endif

/**
 * @brief Helper macro for multi-language parameter descriptions
 * @param de_text German text
 * @param en_text English text
 * @return Selected text based on language parameter, or nullptr when the
 *         descriptions are compiled out - the same value the Effect base class
 *         returns for an effect that implements no descriptions at all
 *
 * Usage:
 *   return PARAM_DESC_DE_EN("Deutscher Text", "English text");
 */
#if NEOPIXEL_EFFECT_PARAM_DESCRIPTIONS
    #define PARAM_DESC_DE_EN(de_text, en_text) \
        (lang && strcmp(lang, "de") == 0 ? (de_text) : (en_text))
#else
    // must not name the literals at all - even a discarded reference keeps them
    #define PARAM_DESC_DE_EN(de_text, en_text) ((const char*)nullptr)
#endif
/**
 * Macro for multi-language effect names
 * @param de_name German effect name
 * @param en_name English effect name
 * @return Selected name based on language parameter
 *
 * Usage:
 *   return EFFECT_NAME_DE_EN("Feuer", "Fire");
 */
#define EFFECT_NAME_DE_EN(de_name, en_name) \
    (lang && strcmp(lang, "de") == 0 ? (de_name) : (en_name))

/**
 * Macro for multi-language effect descriptions
 * @param de_desc German effect description
 * @param en_desc English effect description
 * @return Selected description based on language parameter
 *
 * Usage:
 *   return EFFECT_DESC_DE_EN("Deutscher Text", "English text");
 */
#define EFFECT_DESC_DE_EN(de_desc, en_desc) \
    (lang && strcmp(lang, "de") == 0 ? (de_desc) : (en_desc))
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
    virtual ~Effect() = default;

    /**
     * Get effect name
     * @param lang Language code ("de" or "en"), nullptr for default (English)
     * @return Human-readable effect name
     */
    virtual const char* getName(const char* lang = nullptr)
    {
        return lang && strcmp(lang, "de") == 0 ? "Unbekannt" : "Unknown";
    }

    /**
     * Get effect description
     * @param lang Language code ("de" or "en"), nullptr for default (English)
     * @return Brief description of what the effect does
     */
    virtual const char* getDescription(const char* lang = nullptr)
    {
        return lang && strcmp(lang, "de") == 0 ? "Keine Beschreibung" : "No description";
    }

    /**
     * Update effect animation
     *
     * @param segment The segment to animate (contains config & state)
     * @param deltaTime Time difference since last update in ms
     */
    virtual void update(Segment* segment, uint32_t deltaTime) = 0;

    /**
     * Reset effect state (for stateful effects)
     */
    virtual void reset() {}

    /** Called when this effect is freshly assigned to a segment. */
    virtual void onEnter(Segment* segment) {}

    /** Called before this effect is replaced or removed from a segment. */
    virtual void onExit(Segment* segment) {}

    /** Called when a paused effect resumes without resetting its runtime state. */
    virtual void onResume(Segment* segment) {}

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
     * @brief Default text for a PARAM_STRING parameter (nullptr/empty = no default).
     * getParameterDefault() can't carry a string, so string effects expose their
     * default here. Cue authoring seeds the cue's text with this when none is set,
     * so "no text configured" shows the effect's intended text instead of nothing.
     * @param index Parameter index
     * @return Default text, or nullptr if none
     */
    virtual const char* getParameterDefaultText(uint8_t index) const { return nullptr; }

    /**
     * @brief Get parameter description (multi-language)
     * @param index Parameter index (0..count-1)
     * @param lang Language code ("de" or "en"), default: "de"
     * @return Parameter description or nullptr
     */
    virtual const char* getParameterDescription(uint8_t index, const char* lang = "de") const { return nullptr; }

    /**
     * @brief Get parameter minimum value
     * @param index Parameter index
     * @return Minimum value (packed in uint32_t)
     */
    virtual uint32_t getParameterMin(uint8_t index) const { return 0; }

    /**
     * @brief Get parameter maximum value
     * @param index Parameter index
     * @return Maximum value (packed in uint32_t)
     */
    virtual uint32_t getParameterMax(uint8_t index) const { return 255; }

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

    // ====================================================================
    // 2D / 3D Capability Extension
    // ====================================================================

    /**
     * @brief Dimension capabilities bitmask.
     * Bit 0 = 1D, Bit 1 = 2D, Bit 2 = 3D.
     * Default: DIM_1D only.
     */
    enum EffectDim : uint8_t
    {
        DIM_1D = 0x01,
        DIM_2D = 0x02,
        DIM_3D = 0x04,
    };

    /**
     * @brief Return supported dimensions (bitmask of EffectDim).
     * Override to advertise 2D/3D support.
     * Default: 1D only.
     */
    virtual uint8_t getCapabilities() const { return DIM_1D; }

    /**
     * @brief 2D update — called instead of update() when segment has 2D geometry
     * and this effect advertises DIM_2D capability.
     *
     * Default implementation calls update() so all existing 1D effects work on
     * 2D segments without change (rendered line by line).
     */
    virtual void update2D(Segment* segment, uint32_t deltaTime) { update(segment, deltaTime); }

    /**
     * @brief 3D update — called instead of update() when segment has 3D geometry
     * and this effect advertises DIM_3D capability.
     * Default falls back to update2D().
     */
    virtual void update3D(Segment* segment, uint32_t deltaTime) { update2D(segment, deltaTime); }

    /**
     * @brief Helper: does this effect support 2D rendering?
     */
    bool supports2D() const { return (getCapabilities() & DIM_2D) != 0; }

    /**
     * @brief Helper: does this effect support 3D rendering?
     */
    bool supports3D() const { return (getCapabilities() & DIM_3D) != 0; }
};
