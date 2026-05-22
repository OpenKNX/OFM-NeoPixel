/**
 * @file Segment.h
 * @brief LED Segment - range with its own effect
 *
 * A segment defines an LED range (start to end) in a VirtualStrip
 * or PhysicalStrip and can have an effect.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
/**
 * Example:
 *   VirtualStrip* vstrip = new VirtualStrip(200);
 *   vstrip->attachPhysicalStrip(strip0, 0);
 *   vstrip->attachPhysicalStrip(strip1, 100);
 *
 *   Segment* seg1 = new Segment(vstrip, 0, 99);      // First half
 *   Segment* seg2 = new Segment(vstrip, 100, 199);   // Second half
 *
 *   seg1->setEffect(someEffect);
 *   seg2->setEffect(otherEffect);
 *
 *   // In loop:
 *   seg1->update(deltaTime);  // Effect calculates new pixels
 *   seg2->update(deltaTime);
 *   vstrip->show();           // Send all pixels
 */

#include "LedState.h"
#include "VirtualStrip.h"
#include <stdint.h>
#include <string>

class Effect; // Forward declaration

/**
 * Effect Configuration - User-configurable parameters
 * ~40 bytes per Segment
 */
struct EffectConfig
{
    uint8_t speed;              // Speed 1-255
    uint8_t intensity;          // Intensity 1-255
    uint8_t brightness;         // Software brightness 0-255 (255 = no dimming, ALL LED types)
    uint8_t hardwareBrightness; // Hardware brightness 0-255 (255 = max, only for APA102/SK9822)
    uint32_t primaryRGBW;       // Primary color (RGBW packed: R<<24 | G<<16 | B<<8 | W)
    uint32_t secondaryRGBW;     // Secondary color (RGBW packed)
    uint8_t reverse;            // Reverse direction (0/1)
    uint8_t count;              // Count (e.g. dots, drops)
    uint8_t fade;               // Fade amount 0-255
    uint8_t mode;               // Effect-specific mode
    uint8_t effectType;         // Current effect type ID (for parameter loading)

    // 5-Channel extension: Additional white channels for RGBCCT strips
    uint8_t primaryCW;   // Primary Cool White (0-255) for 5-channel strips
    uint8_t secondaryCW; // Secondary Cool White (0-255) for 5-channel strips

    // OAM Interface Parameters
    uint8_t option1; // Effekt Option 1 (0-255) - Effect-specific parameter
    uint8_t option2; // Effekt Option 2 (0-255) - Effect-specific parameter
    uint8_t option3; // Effekt Option 3 (0-255) - Effect-specific parameter

    bool feature1; // Effekt Feature 1 (true/false) - Effect-specific boolean
    bool feature2; // Effekt Feature 2 (true/false) - Effect-specific boolean
    bool feature3; // Effekt Feature 3 (true/false) - Effect-specific boolean

    // Legacy 32-bit options for backward compatibility
    uint32_t legacyOption1; // Legacy option1 (32-bit)
    uint32_t legacyOption2; // Legacy option2 (32-bit)

    // Convenience accessors for color components (RGBW)
    inline uint8_t r() const { return (primaryRGBW >> 24) & 0xFF; }
    inline uint8_t g() const { return (primaryRGBW >> 16) & 0xFF; }
    inline uint8_t b() const { return (primaryRGBW >> 8) & 0xFF; }
    inline uint8_t w() const { return primaryRGBW & 0xFF; }  // Warm White for 5-channel, single White for 4-channel
    inline uint8_t ww() const { return primaryRGBW & 0xFF; } // Alias for w() - Warm White
    inline uint8_t cw() const { return primaryCW; }          // Cool White (5-channel only)

    inline uint8_t r2() const { return (secondaryRGBW >> 24) & 0xFF; }
    inline uint8_t g2() const { return (secondaryRGBW >> 16) & 0xFF; }
    inline uint8_t b2() const { return (secondaryRGBW >> 8) & 0xFF; }
    inline uint8_t w2() const { return secondaryRGBW & 0xFF; }  // Warm White for 5-channel, single White for 4-channel
    inline uint8_t ww2() const { return secondaryRGBW & 0xFF; } // Alias for w2() - Warm White
    inline uint8_t cw2() const { return secondaryCW; }          // Cool White (5-channel only)

    // Default Constructor
    EffectConfig()
        : speed(128), intensity(128), brightness(255), hardwareBrightness(255),
          primaryRGBW(0xFFFFFFFF), secondaryRGBW(0x00000000),
          reverse(0), count(1), fade(128), mode(0), effectType(0),
          primaryCW(0), secondaryCW(0),
          option1(0), option2(0), option3(0),
          feature1(false), feature2(false), feature3(false),
          legacyOption1(0), legacyOption2(0) {}
};

/**
 * Effect State - Effect runtime variables
 * ~12 bytes per Segment
 */
struct EffectState
{
    uint16_t position;   // Position in effect
    uint8_t phase;       // Phase status
    uint32_t lastUpdate; // Last update (millis)
    uint16_t counter;    // General counter
    uint16_t aux1;       // Auxiliary variable 1
    uint16_t aux2;       // Auxiliary variable 2

    // Default Constructor
    EffectState()
        : position(0), phase(0), lastUpdate(0),
          counter(0), aux1(0), aux2(0) {}
};

/**
 * Segment - LED range with effect
 */
class Segment
{
  public:
    inline const std::string logPrefix() { return "Segment"; }

    Segment(VirtualStrip* virtualStrip, uint16_t startLed, uint16_t endLed);
    ~Segment();

    // ====================================================================
    // Properties
    // ====================================================================
    uint16_t getStartLed() const { return _startLed; }                    // Get start LED index
    uint16_t getEndLed() const { return _endLed; }                        // Get end LED index (inclusive)
    uint16_t getLength() const { return _virtualLength; }                 // Get virtual length (for effects) - accounts for grouping/spacing
    uint16_t getPhysicalLength() const { return _physicalLength; }        // Get physical length (actual LEDs)
    VirtualStrip* getVirtualStrip() { return _virtualStrip; }             // Get parent virtual strip
    const VirtualStrip* getVirtualStrip() const { return _virtualStrip; } // Get parent virtual strip (const)

    // ====================================================================
    // Grouping & Spacing Configuration
    // ====================================================================
    /**
     * @brief Set grouping - how many physical LEDs show the same color
     * @param grouping Number of LEDs per group (1 = no grouping, default)
     */
    void setGrouping(uint16_t grouping);
    uint16_t getGrouping() const { return _grouping; }

    /**
     * @brief Set spacing - how many LEDs to skip (turn off) between groups
     * @param spacing Number of LEDs to skip (0 = no spacing, default)
     */
    void setSpacing(uint16_t spacing);
    uint16_t getSpacing() const { return _spacing; }

    /**
     * @brief Set reverse direction - effects run backwards
     * @param reverse true to reverse direction
     */
    void setReverse(bool reverse) { _reverse = reverse; }
    bool getReverse() const { return _reverse; }

    /**
     * @brief Set mirror effect - effects are mirrored from center
     * @param mirror true to enable mirroring
     */
    void setMirror(bool mirror) { _mirror = mirror; }
    bool getMirror() const { return _mirror; }

    /**
     * @brief Set offset - shift effect start position within segment
     * @param offset Number of LEDs to offset (0 = no offset, default)
     */
    void setOffset(uint16_t offset) { _offset = offset; }
    uint16_t getOffset() const { return _offset; }

    // ====================================================================
    // Effect Management
    // ====================================================================
    void setEffect(Effect* effect, bool initializeDefaults = false); // Set effect (Stateless - no init!)
    Effect* getEffect() { return _effect; }                          // Get current effect
    const Effect* getEffect() const { return _effect; }              // Get current effect (const)
    bool hasEffect() const { return _effect != nullptr; }            // Check if effect is set
    void clearEffect() { _effect = nullptr; }                        // Remove effect

    // ====================================================================
    // Pixel Control
    // ====================================================================
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw); // 5-channel RGBCCT
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw); // 5-channel RGBCCT
    void clear();
    void clearAll();
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& ww, uint8_t& cw) const; // 5-channel RGBCCT
    inline uint32_t getPrimaryColor() const { return _config.primaryRGBW; }                            // Get primary color
    inline uint32_t getSecondaryColor() const { return _config.secondaryRGBW; }                        // Get secondary color
    inline bool setPrimaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t w)
    {
        _config.primaryRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)w;
        return true;
    } // Set primary color (4-channel RGBW)

    inline bool setPrimaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t ww, uint32_t cw)
    {
        _config.primaryRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)ww;
        _config.primaryCW = (uint8_t)cw;
        return true;
    } // Set primary color (5-channel RGBCCT)

    inline bool setSecondaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t w)
    {
        _config.secondaryRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)w;
        return true;
    } // Set secondary color (4-channel RGBW)

    inline bool setSecondaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t ww, uint32_t cw)
    {
        _config.secondaryRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)ww;
        _config.secondaryCW = (uint8_t)cw;
        return true;
    } // Set secondary color (5-channel RGBCCT)

    // ====================================================================
    // Update & Control
    // ====================================================================
    void update(uint32_t deltaTime);   // Update Segment (calls effect) - STATELESS
    void pause() { _paused = true; }   // Pause effect
    void resume() { _paused = false; } // Resume effect
    void stop();
    bool isPaused() const { return _paused; }               // Check if effect is paused
    bool isDirty() const { return _dirty; }                 // Check if segment is dirty
    void setDirty(bool dirty) { _dirty = dirty; }           // Set dirty flag
    void setClean() { _dirty = false; }                     // Mark segment as clean
    LedState getLedState() const { return _ledState; }      // Get current LED state (States: IDLE, RUNNING, TRANSITIONING, ERROR)
    void setLedState(LedState state) { _ledState = state; } // Set LED state

    // ====================================================================
    // Effect Configuration & State Access
    // ====================================================================
    EffectConfig& getConfig() { return _config; }             // Get effect configuration (read/write)
    const EffectConfig& getConfig() const { return _config; } // Get effect configuration (read-only)
    EffectState& getState() { return _state; }                // Get effect state (read/write)
    const EffectState& getState() const { return _state; }    // Get effect state (read-only)

    /**
     * @brief Set software brightness (0-255, default 255 = no dimming)
     * Applies RGB multiplication in Effect::update() - works for ALL LED types
     */
    inline void setBrightness(uint8_t brightness) { _config.brightness = brightness; }
    uint8_t getBrightness() const { return _config.brightness; }

    /**
     * @brief Set hardware brightness (0-255, default 255 = max)
     * Only effective for APA102/SK9822 strips (uses hardware global brightness feature)
     * Silently ignored for WS2812B, SK6812, etc. (they don't support hardware brightness)
     */
    void setHardwareBrightness(uint8_t brightness) { _config.hardwareBrightness = brightness; }
    uint8_t getHardwareBrightness() const { return _config.hardwareBrightness; }

  private:
    VirtualStrip* _virtualStrip; // Belongs to this virtual strip
    uint16_t _startLed;          // Start LED in virtual strip
    uint16_t _endLed;            // End LED in virtual strip (inclusive)
    uint16_t _physicalLength;    // Physical length (endLed - startLed + 1)
    uint16_t _virtualLength;     // Virtual length for effects (after grouping/spacing)
    uint16_t _grouping;          // LEDs per group (1 = no grouping)
    uint16_t _spacing;           // LEDs to skip between groups (0 = no spacing)
    uint16_t _offset;            // Offset for effect start position (0 = no offset)
    bool _reverse;               // Reverse effect direction
    bool _mirror;                // Mirror effect from center
    Effect* _effect;             // Current effect (nullptr = none)
    bool _dirty;                 // Pixels changed?
    bool _paused;                // Effect paused?
    LedState _ledState;          // State machine (IDLE, RUNNING, etc)
    EffectConfig _config;        // Effect configuration (~40 bytes)
    EffectState _state;          // Effect runtime variables (~12 bytes)

    // Helper: Recalculate virtual length based on grouping/spacing
    void recalculateVirtualLength();

    // Helper: Map virtual index to physical LED indices
    // Returns start index in physical strip; groupSize LEDs starting from there get the color
    uint16_t mapVirtualToPhysical(uint16_t virtualIndex) const;
};
