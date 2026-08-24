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

// ============================================================================
// LED Topology — how a linear LED chain is physically arranged in 2D/3D
// ============================================================================
/**
 * @brief Wiring topology of a 2D/3D LED matrix.
 *
 * Stored in 3 bits (values 0–7) in Flash (Segment Byte 10, Bits 5-7).
 * Default 0 = LINEAR_1D means no matrix geometry (standard 1D strip).
 *
 * Serpentine = every other row/column is reversed (most common for WS2812B panels).
 * Linear     = every row/column runs in the same direction (less common).
 */
enum class LedTopology : uint8_t
{
    LINEAR_1D = 0,          ///< No matrix — plain 1D strip (default)
    ROWS_SERPENTINE = 1,    ///< →→→→ ←←←← →→→→  (row-major, alternating direction)
    ROWS_LINEAR = 2,        ///< →→→→ →→→→ →→→→   (row-major, same direction)
    COLS_SERPENTINE = 3,    ///< ↓↑↓↑  (col-major, alternating direction)
    COLS_LINEAR = 4,        ///< ↓↓↓↓  (col-major, same direction)
    ROWS_SERPENTINE_3D = 5, ///< 3D: serpentine rows, stacked depth-first
    COLS_LINEAR_TILED = 6,  ///< 2D tiled: panel-major chain, columns linear inside each tile block
    COLS_SERP_TILED = 7,    ///< 2D tiled: panel-major chain, columns serpentine inside each tile block
};

/**
 * @brief 2D/3D geometry of a segment.
 *
 * Stored compactly: width (Byte 12), height (Byte 13), depth (Byte 14)
 * in the segment's Flash union. All 0 / topology=LINEAR_1D = normal 1D segment.
 *
 * RAM cost: 5 bytes per Segment (uint8 width/height/depth + uint8 topology + 1 pad)
 */
struct LedGeometry
{
    uint8_t width = 0;  ///< Matrix columns (0 = not set / 1D)
    uint8_t height = 0; ///< Matrix rows    (0 = not set / 1D)
    uint8_t depth = 0;  ///< Z-layers       (0 = not set / 2D or 1D)
    LedTopology topology = LedTopology::LINEAR_1D;

    bool isTiled2D() const
    {
        return topology == LedTopology::COLS_LINEAR_TILED || topology == LedTopology::COLS_SERP_TILED;
    }

    bool is1D() const { return topology == LedTopology::LINEAR_1D || width <= 1 || height <= 1; }
    bool is2D() const { return !is1D() && (depth <= 1 || isTiled2D()); }
    bool is3D() const { return !is1D() && depth > 1 && !isTiled2D(); }
};

/**
 * Effect Configuration - User-configurable parameters
 * ~40 bytes per Segment
 */
struct EffectConfig
{
    uint8_t speed;              // Speed 1-255
    uint8_t intensity;          // Intensity 1-255
    uint8_t brightness;         // Software brightness 0-255 (255 = no dimming, ALL LED types) — the EFFECTIVE render value (what scales pixels; fade/EM drive it)
    uint8_t hardwareBrightness; // Hardware brightness 0-255 (255 = max, only for APA102/SK9822)
    uint8_t masterBrightness;   // User/KO/global intent 0-255 (255 = full). EM cues render at master*cue/255, so a runtime dim survives cue switches.
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

    // Effect text parameter (PARAM_STRING) — per segment, owned by whoever sets the effect
    // (ETS config, Cue, Scene, KO). 240 chars + NUL. ETS/Cue/Scene fields stay at 14 chars;
    // longer texts are assembled via the EffectText KO (set) + EffectTextAppend KO (14-char chunks).
    char effectText[241];

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
        : speed(128), intensity(128), brightness(255), hardwareBrightness(255), masterBrightness(255),
          primaryRGBW(0xFFFFFFFF), secondaryRGBW(0x00000000),
          reverse(0), count(1), fade(128), mode(0), effectType(0),
          primaryCW(0), secondaryCW(0),
          option1(0), option2(0), option3(0),
          feature1(false), feature2(false), feature3(false),
          legacyOption1(0), legacyOption2(0) { effectText[0] = '\0'; }
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
    uint16_t getStartLed() const { return _startLed; } // Get start LED index
    uint16_t getEndLed() const { return _endLed; }     // Get end LED index (inclusive)
    /**
     * @brief Effective length for effect calculations.
     * When part of an Effektkette (virtual band), returns the TOTAL virtual band length
     * so effects compute their animation in the full virtual space.
     * Otherwise returns the physical virtual length (accounting for grouping/spacing).
     */
    uint16_t getLength() const { return (_virtualTotalLength > 0) ? _virtualTotalLength : _virtualLength; }
    uint16_t getPhysicalLength() const { return _physicalLength; }        // Get physical length (actual LEDs)
    VirtualStrip* getVirtualStrip() { return _virtualStrip; }             // Get parent virtual strip
    const VirtualStrip* getVirtualStrip() const { return _virtualStrip; } // Get parent virtual strip (const)

    // ====================================================================
    // Effektkette — Virtual Band (distributed rendering across devices)
    // ====================================================================
    /**
     * @brief Configure this segment as part of a virtual band (Effektkette).
     *
     * When set, getLength() returns totalLength (full band) so effects compute
     * animations across the entire virtual band. setPixel() automatically
     * translates the virtual index to a local one and silently drops pixels
     * that fall outside this segment's window — no effect code changes needed.
     *
     * @param totalLength  Total length of the virtual band (all devices combined)
     * @param offset       Where this segment starts within the virtual band
     */
    bool setVirtualBand(uint16_t totalLength, uint16_t offset);
    void clearVirtualBand();
    bool isVirtualBand() const { return _virtualTotalLength > 0; }
    uint16_t getVirtualTotalLength() const { return _virtualTotalLength; }
    uint16_t getVirtualOffset() const { return _virtualOffset; }

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
    // 2D / 3D Matrix Geometry
    // ====================================================================
    /**
     * @brief Configure this segment as a 2D LED matrix.
     *
     * The physical LED chain is reinterpreted as a width×height grid.
     * Width × height must not exceed the segment length (endLed - startLed + 1).
     *
     * @param width   Number of columns (x-axis)
     * @param height  Number of rows    (y-axis)
     * @param t       Wiring topology (default: ROWS_SERPENTINE — most WS2812B panels)
     */
    bool setGeometry(uint8_t width, uint8_t height,
                     LedTopology t = LedTopology::ROWS_SERPENTINE);

    /**
     * @brief Configure this segment as a 3D LED volume.
     * @param width   Columns (x)
     * @param height  Rows    (y)
     * @param depth   Z-layers
     * @param t       Wiring topology
     */
    bool setGeometry(uint8_t width, uint8_t height, uint8_t depth,
                     LedTopology t = LedTopology::ROWS_SERPENTINE_3D);

    /** @brief Remove matrix geometry — segment behaves as plain 1D strip. */
    void clearGeometry() { _geo = LedGeometry{}; }

    const LedGeometry& getGeometry() const { return _geo; }
    bool is2D() const { return _geo.is2D(); }
    bool is3D() const { return _geo.is3D(); }

    /**
     * @brief Set a pixel by 2D coordinates (x=column, y=row).
     *
     * Converts (x,y) to a linear segment index via xyToIndex() and
     * delegates to the standard setPixel(). Returns false if out of bounds
     * or if the segment has no 2D geometry configured.
     */
    bool setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
    bool setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

    /**
     * @brief Set a pixel in global 2D coordinates across a virtual band.
     *
     * In standalone mode this falls back to local setPixelXY().
     * In virtual-band mode, (x,y) are interpreted on a global matrix and clipped
     * to this segment's virtual window before being mapped to local panel coords.
     */
    bool setPixelGlobalXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
    bool setPixelGlobalXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

    /**
     * @brief Logical render size used by distributed 2D effects.
     *
     * For normal segments this equals local geometry width/height.
     * For distributed 2D virtual bands width may expand to the global matrix width.
     */
    uint16_t getRenderWidth() const;
    uint16_t getRenderHeight() const;

    /**
     * @brief Set a pixel by 3D coordinates (x, y, z).
     */
    bool setPixelXYZ(uint8_t x, uint8_t y, uint8_t z, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Convert 2D (x,y) to a linear segment index using the configured topology.
     * @return Linear index within the segment [0 .. physicalLength-1],
     *         or 0xFFFF if out of bounds / no geometry set.
     */
    uint16_t xyToIndex(uint8_t x, uint8_t y) const;

    /**
     * @brief Convert 3D (x,y,z) to a linear segment index.
     * @return Linear index, or 0xFFFF if out of bounds.
     */
    uint16_t xyzToIndex(uint8_t x, uint8_t y, uint8_t z) const;

    // ====================================================================
    // Effect Management
    // ====================================================================
    void setEffect(Effect* effect, bool initializeDefaults = false); // Set effect (Stateless - no init!)
    Effect* getEffect() { return _effect; }                          // Get current effect
    const Effect* getEffect() const { return _effect; }              // Get current effect (const)
    bool hasEffect() const { return _effect != nullptr; }            // Check if effect is set
    void clearEffect();                                               // Remove effect

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
        _config.primaryCW = 0;
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
        _config.secondaryCW = 0;
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

    // DIRECT-state snapshot around an EM run: saveDirectState() captures the manual
    // effect+config before an EM takes over; restoreDirectState() puts it back so
    // stopping the EM returns the segment to its manual light instead of going blank.
    void saveDirectState();
    bool restoreDirectState();
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
     * @brief Master (user/KO/global) brightness — the intent the Effektmanager scales cues against.
     * Set by the brightness KOs, global brightness and the console `neo brightness` command.
     * The EM renders each cue at master*cue/255, so dimming via KO is NOT reset on cue switch.
     * Default 255 means "no master dimming" → cues render at their own brightness (backward compatible).
     */
    inline void setMasterBrightness(uint8_t brightness) { _config.masterBrightness = brightness; }
    uint8_t getMasterBrightness() const { return _config.masterBrightness; }

    /**
     * @brief Set hardware brightness (0-255, default 255 = max)
     * Only effective for APA102/SK9822 strips (uses hardware global brightness feature)
     * Silently ignored for WS2812B, SK6812, etc. (they don't support hardware brightness)
     */
    void setHardwareBrightness(uint8_t brightness) { _config.hardwareBrightness = brightness; }
    uint8_t getHardwareBrightness() const { return _config.hardwareBrightness; }

  private:
    VirtualStrip* _virtualStrip;      // Belongs to this virtual strip
    uint16_t _startLed;               // Start LED in virtual strip
    uint16_t _endLed;                 // End LED in virtual strip (inclusive)
    uint16_t _physicalLength;         // Physical length (endLed - startLed + 1)
    uint16_t _virtualLength;          // Virtual length for effects (after grouping/spacing)
    uint16_t _grouping;               // LEDs per group (1 = no grouping)
    uint16_t _spacing;                // LEDs to skip between groups (0 = no spacing)
    uint16_t _offset;                 // Offset for effect start position (0 = no offset)
    bool _reverse;                    // Reverse effect direction
    bool _mirror;                     // Mirror effect from center
    Effect* _effect;                  // Current effect (nullptr = none)
    bool _ownsEffect = false;
    bool _dirty;                      // Pixels changed?
    bool _paused;                     // Effect paused?
    LedState _ledState;               // State machine (IDLE, RUNNING, etc)
    EffectConfig _config;             // Effect configuration (~40 bytes)
    EffectConfig _savedConfig;        // DIRECT-state snapshot taken before an EM started
    Effect* _savedEffect = nullptr;   // DIRECT effect pointer before the EM took over
    bool _ownsSavedEffect = false;
    bool _hasSavedState = false;      // true once a direct snapshot exists
    EffectState _state;               // Effect runtime variables (~12 bytes)
    LedGeometry _geo;                 // 2D/3D matrix geometry (5 bytes, default = 1D)
    uint16_t _virtualTotalLength = 0; ///< Effektkette: total band length (0 = standalone)
    uint16_t _virtualOffset = 0;      ///< Effektkette: this segment's start in the band

    // Helper: Recalculate virtual length based on grouping/spacing
    void recalculateVirtualLength();

    // Helper: Map virtual index to physical LED indices
    // Returns start index in physical strip; groupSize LEDs starting from there get the color
    uint16_t mapVirtualToPhysical(uint16_t virtualIndex) const;

    /**
     * @brief Translate a virtual band index to a local segment index.
     * @param index  In: virtual band index. Out: local segment index.
     * @return true if the pixel is within this segment's local window.
     */
    bool translateVirtualIndex(uint16_t& index) const;
    bool mapGlobalXYToLocal(uint16_t x, uint16_t y, uint8_t& localX, uint8_t& localY) const;
};
