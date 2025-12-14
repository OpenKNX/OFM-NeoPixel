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
    void setRange(uint16_t startIndex, uint16_t length, uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void clear();
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const;
    bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;

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
    inline bool waitForCompletion(uint32_t timeoutMs = 0); // Wait for all physical strips to complete
    bool syncToPhysical(uint8_t hardwareBrightness = 255); // Sync buffer to physical strips (with optional hardware brightness)
    bool show();
    bool isAnyBusy() const;

    // ====================================================================
    // Power Management
    // ====================================================================
    void setPowerManager(PowerManager* pm) { _powerManager = pm; }        // Set power manager for current limiting
    PowerManager* getPowerManager() const { return _powerManager; }       // Get power manager
    uint8_t getHardwareBrightness() const { return _hardwareBrightness; } // Get hardware brightness for power calculation

    // ====================================================================
    // State & Properties
    // ====================================================================
    inline uint16_t getLedCount() const { return _totalLeds; }        // Virtual LED count
    inline bool hasWhiteChannel() const { return _bytesPerLed >= 4; } // True if RGBW buffer
    inline bool isDirty() const { return _dirty; }                    // Is buffer modified?
    inline void markDirty() { _dirty = true; }                        // Mark buffer as modified
    inline void setClean() { _dirty = false; }                        // Mark buffer as clean after sync
    uint16_t getTotalPhysicalLeds() const;
    size_t getMemoryUsage() const { return _bufferSize; } // Memory usage in bytes

  private:
    std::vector<VirtualToPhysicalMapping> _physicalStrips; // Attached physical strips
    uint16_t _totalLeds;                                   // Virtual LED count
    uint8_t* _buffer;                                      // Unified RGB(W) buffer (ALWAYS in RGB/RGBW format!)
    size_t _bufferSize;                                    // Buffer size in bytes
    uint8_t _bytesPerLed;                                  // Bytes per LED (3 for RGB, 4 for RGBW)
    bool _dirty;                                           // Buffer modified?
    PowerManager* _powerManager;                           // Optional power manager for current limiting
    uint8_t _hardwareBrightness;                           // Hardware brightness for APA102/SK9822 (0-255, default 255)

    PhysicalStrip* findPhysicalAtIndex(uint16_t virtualIndex, uint16_t& outPhysicalIndex) const;
    void writePixelToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
};
