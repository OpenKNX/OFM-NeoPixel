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
    
    // Primary color components (individual fields)
    uint8_t primaryR;           // Primary Red (0-255)
    uint8_t primaryG;           // Primary Green (0-255)
    uint8_t primaryB;           // Primary Blue (0-255)
    uint8_t primaryW;           // Primary White (0-255)
    
    // Secondary color components (individual fields)
    uint8_t secondaryR;         // Secondary Red (0-255)
    uint8_t secondaryG;         // Secondary Green (0-255)
    uint8_t secondaryB;         // Secondary Blue (0-255)
    uint8_t secondaryW;         // Secondary White (0-255)
    
    uint8_t reverse;            // Reverse direction (0/1)
    uint8_t count;              // Count (e.g. dots, drops)
    uint8_t fade;               // Fade amount 0-255
    uint8_t mode;               // Effect-specific mode

    // OAM Interface Parameters
    uint8_t option1;            // Effekt Option 1 (0-255) - Effect-specific parameter
    uint8_t option2;            // Effekt Option 2 (0-255) - Effect-specific parameter  
    uint8_t option3;            // Effekt Option 3 (0-255) - Effect-specific parameter
    
    bool feature1;              // Effekt Feature 1 (true/false) - Effect-specific boolean
    bool feature2;              // Effekt Feature 2 (true/false) - Effect-specific boolean
    bool feature3;              // Effekt Feature 3 (true/false) - Effect-specific boolean

    // Legacy 32-bit options for backward compatibility
    uint32_t legacyOption1;     // Legacy option1 (32-bit)
    uint32_t legacyOption2;     // Legacy option2 (32-bit)

    // Convenience accessors for color components
    inline uint8_t r() const { return primaryR; }
    inline uint8_t g() const { return primaryG; }
    inline uint8_t b() const { return primaryB; }
    inline uint8_t w() const { return primaryW; }
    
    inline uint8_t r2() const { return secondaryR; }
    inline uint8_t g2() const { return secondaryG; }
    inline uint8_t b2() const { return secondaryB; }
    inline uint8_t w2() const { return secondaryW; }

    // Default Constructor
    EffectConfig()
        : speed(128), intensity(128), brightness(255), hardwareBrightness(255),
          primaryR(255), primaryG(255), primaryB(255), primaryW(255),
          secondaryR(0), secondaryG(0), secondaryB(0), secondaryW(0),
          reverse(0), count(1), fade(128), mode(0),
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
    uint16_t getLength() const { return _length; }                        // Get segment length
    VirtualStrip* getVirtualStrip() { return _virtualStrip; }             // Get parent virtual strip
    const VirtualStrip* getVirtualStrip() const { return _virtualStrip; } // Get parent virtual strip (const)

    // ====================================================================
    // Effect Management
    // ====================================================================
    void setEffect(Effect* effect , bool initializeDefaults = false);                       // Set effect (Stateless - no init!)
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
    inline uint32_t getPrimaryColor() const { 
        return ((uint32_t)_config.primaryR << 24) | ((uint32_t)_config.primaryG << 16) | 
               ((uint32_t)_config.primaryB << 8) | (uint32_t)_config.primaryW; 
    }     // Get primary color
    inline uint32_t getSecondaryColor() const { 
        return ((uint32_t)_config.secondaryR << 24) | ((uint32_t)_config.secondaryG << 16) | 
               ((uint32_t)_config.secondaryB << 8) | (uint32_t)_config.secondaryW; 
    } // Get secondary color
    inline bool setPrimaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t w)
    {
        _config.primaryR = r & 0xFF;
        _config.primaryG = g & 0xFF;
        _config.primaryB = b & 0xFF;
        _config.primaryW = w & 0xFF;
        return true;
    } // Set primary color
    inline bool setSecondaryColor(uint32_t r, uint32_t g, uint32_t b, uint32_t w)
    {
        _config.secondaryR = r & 0xFF;
        _config.secondaryG = g & 0xFF;
        _config.secondaryB = b & 0xFF;
        _config.secondaryW = w & 0xFF;
        return true;
    } // Set secondary color

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
    void setBrightness(uint8_t brightness) { _config.brightness = brightness; }
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
    uint16_t _length;            // Length (endLed - startLed + 1)
    Effect* _effect;             // Current effect (nullptr = none)
    bool _dirty;                 // Pixels changed?
    bool _paused;                // Effect paused?
    LedState _ledState;          // State machine (IDLE, RUNNING, etc)
    EffectConfig _config;        // Effect configuration (~40 bytes)
    EffectState _state;          // Effect runtime variables (~12 bytes)
};
