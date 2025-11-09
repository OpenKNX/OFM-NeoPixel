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
    uint8_t speed;            // Speed 1-255
    uint8_t intensity;        // Intensity 1-255
    uint8_t brightness;       // Segment brightness 0-255 (255 = no dimming, all strips)
    uint8_t apa102Brightness; // APA102 hardware brightness 0-255 (255 = max, only for APA102)
    uint32_t primaryRGBW;     // Primary color (RGBW packed)
    uint32_t secondaryRGBW;   // Secondary color (RGBW packed)
    uint8_t reverse;          // Reverse direction (0/1)
    uint8_t count;            // Count (e.g. dots, drops)
    uint8_t fade;             // Fade amount 0-255
    uint8_t mode;             // Effect-specific mode

    uint32_t option1; // Additional parameter 1
    uint32_t option2; // Additional parameter 2

    // Default Constructor
    EffectConfig()
        : speed(128), intensity(128), brightness(255), apa102Brightness(255),
          primaryRGBW(0xFFFFFFFF), secondaryRGBW(0x00000000),
          reverse(0), count(1), fade(128), mode(0),
          option1(0), option2(0) {}
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
    uint16_t getLength() const { return _length; }                        // Get segment length
    VirtualStrip* getVirtualStrip() { return _virtualStrip; }             // Get parent virtual strip
    const VirtualStrip* getVirtualStrip() const { return _virtualStrip; } // Get parent virtual strip (const)

    // ====================================================================
    // Effect Management
    // ====================================================================
    void setEffect(Effect* effect);                       // Set effect (Stateless - no init!)
    Effect* getEffect() { return _effect; }               // Get current effect
    const Effect* getEffect() const { return _effect; }   // Get current effect (const)
    bool hasEffect() const { return _effect != nullptr; } // Check if effect is set
    void clearEffect() { _effect = nullptr; }             // Remove effect

    // ====================================================================
    // Pixel Control
    // ====================================================================
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void clear();
    void clearAll();
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;

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
    EffectConfig& getConfig() { return _config; }                               // Get effect configuration (read/write)
    const EffectConfig& getConfig() const { return _config; }                   // Get effect configuration (read-only)
    EffectState& getState() { return _state; }                                  // Get effect state (read/write)
    const EffectState& getState() const { return _state; }                      // Get effect state (read-only)
    void setBrightness(uint8_t brightness) { _config.brightness = brightness; } // Set segment brightness (0-255)
    uint8_t getBrightness() const { return _config.brightness; }                // Get segment brightness (0-255)

    /**
     * @brief Set APA102 hardware brightness (0-255, default 255 = max)
     * Only affects APA102 strips, uses hardware brightness feature
     */
    void setAPA102Brightness(uint8_t brightness) { _config.apa102Brightness = brightness; } // Set APA102 hardware brightness (0-255, default 255 = max)
    uint8_t getAPA102Brightness() const { return _config.apa102Brightness; }                // Get APA102 hardware brightness (0-255)

  private:
    VirtualStrip* _virtualStrip; // Belongs to this virtual strip
    uint16_t _startLed;          // Start LED in virtual strip
    uint16_t _endLed;            // End LED in virtual strip (inclusive)
    uint16_t _length;            // Length (endLed - startLed + 1)
    Effect* _effect;             // Current effect (nullptr = none)
    bool _dirty;                 // Pixels changed?
    bool _paused;                // Effect paused?
    LedState _ledState;          // State machine (IDLE, RUNNING, etc)
    EffectConfig _config;        // Effect configuration (~40 bytes)
    EffectState _state;          // Effect runtime variables (~12 bytes)
};
