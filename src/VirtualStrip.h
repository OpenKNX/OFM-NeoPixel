/**
 * @file VirtualStrip.h
 * @brief Virtual LED Strip - Unified RGB/RGBW Buffer for Multiple PhysicalStrips
 *
 * VirtualStrip provides a unified pixel buffer that can be mapped to multiple
 * PhysicalStrips at different offsets. This enables:
 * - Central animations across multiple physical strips
 * - Unified RGB/RGBW buffer management (always in RGB format internally)
 * - Simplified pixel addressing (one logical strip, multiple hardware strips)
 * - Automatic ColorOrder conversion (happens in PhysicalStrip, not here)
 *
 * IMPORTANT: VirtualStrip ALWAYS stores pixels in RGB/RGBW format internally!
 * ColorOrder parameter is only used to determine bytes per LED (3 vs 4).
 * ColorOrder conversion to hardware format (GRB, BGR, etc.) happens in PhysicalStrip.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
/*
 * Example:
 *   PhysicalStrip* strip0 = mgr->addPhysicalStrip(3, 100, ColorOrder::GRB);   // 100 LEDs, GRB hardware
 *   PhysicalStrip* strip1 = mgr->addPhysicalStrip(7, 100, ColorOrder::RGB);   // 100 LEDs, RGB hardware
 *   VirtualStrip* vstrip = new VirtualStrip(200, ColorOrder::RGB);            // 200 logical LEDs, RGB buffer
 *   vstrip->attachPhysicalStrip(strip0, 0);                                   // Offset 0
 *   vstrip->attachPhysicalStrip(strip1, 100);                                 // Offset 100
 *
 *   vstrip->setPixel(0, 255, 0, 0);       // LED 0 on strip0 (auto-converted to GRB)
 *   vstrip->setPixel(150, 0, 255, 0);     // LED 50 on strip1 (stays RGB)
 *   vstrip->show();                       // Sends to BOTH with correct ColorOrder
 */
#include "PhysicalStrip.h"
#include <stdint.h>
#include <string>
#include <vector>

class PowerManager; // Forward declaration

struct VirtualToPhysicalMapping
{
    PhysicalStrip* physicalStrip; // Pointer to PhysicalStrip
    uint16_t virtualOffset;       // Where does this strip start in virtual?
    uint16_t physicalLedCount;    // How many LEDs does this physical have?
};

class VirtualStrip
{
  public:
    inline const std::string logPrefix() { return "VirtualStrip"; }

    VirtualStrip(uint16_t totalLeds, ColorOrder colorOrder = ColorOrder::RGB);
    ~VirtualStrip();

    // ====================================================================
    // Physical Strip Management
    // ====================================================================
    bool attachPhysicalStrip(PhysicalStrip* physicalStrip, uint16_t offset);
    bool detachPhysicalStrip(PhysicalStrip* physicalStrip);
    uint16_t getPhysicalStripCount() const { return _physicalStrips.size(); }
    PhysicalStrip* getPhysicalStrip(uint16_t index) const;

    // ====================================================================
    // Virtual Pixel API
    // ====================================================================
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw);
    void setRange(uint16_t startIndex, uint16_t length, uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void clear();
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& ww, uint8_t& cw) const;

    // ====================================================================
    // Buffer Access
    // ====================================================================
    inline uint8_t* getBuffer() { return _buffer; }                // Get unified buffer
    inline const uint8_t* getBuffer() const { return _buffer; }    // Get unified buffer (const)
    inline size_t getBufferSize() const { return _bufferSize; }    // Get buffer size
    inline uint8_t getBytesPerLed() const { return _bytesPerLed; } // Get bytes per LED

    // ====================================================================
    // Sync & Transfer
    // ====================================================================
    bool waitForCompletion(uint32_t timeoutMs = 100); // Wait for all physical strips to complete (bounded: 0 would spin forever on a wedged strip)
    bool syncToPhysical();                          // Sync buffer to physical strips
    bool show();
    bool isAnyBusy() const;

    /**
     * @brief Turn off all LEDs immediately (clear + show combined)
     *
     * Convenience method that clears the buffer and immediately sends
     * the update to all physical strips. Useful for emergency shutdowns,
     * power-off states, or before ETS programming.
     */
    inline void turnOffAll()
    {
        clear();
        show();
    }

    // ====================================================================
    // Power Management
    // ====================================================================
    void setPowerManager(PowerManager* pm) { _powerManager = pm; }  // Set power manager for current limiting
    PowerManager* getPowerManager() const { return _powerManager; } // Get power manager

    /**
     * @brief Get hardware brightness for power calculation
     * @return Brightness value: For SPI strips (16-30 from config), for Serial strips (255 = full)
     *
     * NOTE: For SPI strips, reads from PhysicalStrip config. For others, returns 255.
     * Use 'neo phys config <id> brightness <16-30>' to change SPI strip brightness.
     */
    uint8_t getHardwareBrightness() const;

    // ====================================================================
    // State & Properties
    // ====================================================================
    inline uint16_t getLedCount() const { return _totalLeds; }            // Virtual LED count
    inline bool isValid() const { return _buffer != nullptr && _bufferSize != 0; }
    inline bool hasWhiteChannel() const { return _bytesPerLed >= 4; }     // True if RGBW/RGBCCT buffer
    inline bool hasDualWhiteChannel() const { return _bytesPerLed >= 5; } // True if RGBCCT buffer (5 channels)
    inline bool isDirty() const { return _dirty; }                        // Is buffer modified?
    inline void markDirty() { _dirty = true; }                            // Mark buffer as modified
    inline void setClean() { _dirty = false; }                            // Mark buffer as clean after sync
    uint16_t getTotalPhysicalLeds() const;
    size_t getMemoryUsage() const { return _bufferSize; } // Memory usage in bytes

    // ====================================================================
    // Pixel Transform Callback (for HCL, color correction, etc.)
    // ====================================================================
    /**
     * @brief Pixel transform callback type for RGBCCT (5-channel) support
     *
     * Called during syncToPhysical() for each pixel BEFORE sending to hardware.
     * This allows post-processing (HCL, gamma, etc.) without modifying the
     * effect buffer, so effects that read back pixels (Cylon fade, etc.) work correctly.
     *
     * @param pixelIndex Index of the pixel being processed (0..totalLeds-1)
     * @param r Red component (in/out)
     * @param g Green component (in/out)
     * @param b Blue component (in/out)
     * @param ww Warm White component (in/out, nullptr for RGB strips)
     * @param cw Cool White component (in/out, nullptr for RGB/RGBW strips)
     * @param userData User-provided context pointer
     *
     * For HCL color temperature control on RGBCCT strips:
     * - Warm Kelvin (2700K): ww high, cw low
     * - Cool Kelvin (6500K): ww low, cw high
     * - Neutral: blend of both
     */
    using PixelTransformCallback = void (*)(uint16_t pixelIndex, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t* ww, uint8_t* cw, void* userData);

    /**
     * @brief Set pixel transform callback for post-processing during sync
     * @param callback Function to call for each pixel during syncToPhysical()
     * @param userData User context passed to callback
     */
    void setPixelTransformCallback(PixelTransformCallback callback, void* userData = nullptr);

  private:
    std::vector<VirtualToPhysicalMapping> _physicalStrips; // Attached physical strips
    uint16_t _totalLeds;                                   // Virtual LED count
    uint8_t* _buffer;                                      // Unified RGB(W)(CCT) buffer (ALWAYS in RGB/RGBW/RGBCCT format!)
    size_t _bufferSize;                                    // Buffer size in bytes
    uint8_t _bytesPerLed;                                  // Bytes per LED (3=RGB, 4=RGBW, 5=RGBCCT)
    bool _dirty;                                           // Buffer modified?
    PowerManager* _powerManager;                           // Optional power manager for current limiting
    PixelTransformCallback _pixelTransformCallback;        // Optional post-processing callback
    void* _pixelTransformUserData;                         // User data for callback

    PhysicalStrip* findPhysicalAtIndex(uint16_t virtualIndex, uint16_t& outPhysicalIndex) const;
    void writePixelToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww = 0, uint8_t cw = 0);
};
