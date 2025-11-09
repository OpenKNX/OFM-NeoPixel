/**
 * @file VirtualStrip.h
 * @brief Virtual LED Strip - Combination of multiple PhysicalStrips
 *
 * A VirtualStrip combines multiple PhysicalStrips into one logical
 * large strip. This enables:
 * - Central animations across multiple physical strips
 * - Unified buffer management
 * - Simplified pixel addressing
 * - Supports different color orders (RGB, GRB, RGBW, etc.)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
/*
 * Example:
 *   PhysicalStrip* strip0 = mgr->addPhysicalStrip(3, 100);   // 100 LEDs
 *   PhysicalStrip* strip1 = mgr->addPhysicalStrip(7, 100);   // 100 LEDs
 *   VirtualStrip* vstrip = new VirtualStrip(200);            // 200 logical LEDs
 *   vstrip->attachPhysicalStrip(strip0, 0);                  // Offset 0
 *   vstrip->attachPhysicalStrip(strip1, 100);                // Offset 100
 *
 *   vstrip->setPixel(0, 255, 0, 0);       // LED 0 on strip0
 *   vstrip->setPixel(150, 0, 255, 0);     // LED 50 on strip1
 *   vstrip->show();                       // Sends to BOTH
 */
#include "PhysicalStrip.h"
#include <stdint.h>
#include <string>
#include <vector>

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
    inline void setBrightness(uint8_t brightness) { _brightness = brightness; }; // Set brightness for next setPixel() calls (APA102 hardware brightness)
    inline uint8_t getBrightness() const { return _brightness; }                 // Get current brightness value

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
    bool syncToPhysical();
    bool show();
    bool isAnyBusy() const;

    // ====================================================================
    // State & Properties
    // ====================================================================
    inline uint16_t getLedCount() const { return _totalLeds; }      // Virtual LED count
    inline ColorOrder getColorOrder() const { return _colorOrder; } // Color order
    inline void setColorOrder(ColorOrder order)
    {
        _colorOrder = order;
        _dirty = true;
    }                                              // Set color order
    inline bool isDirty() const { return _dirty; } // Is buffer modified?
    inline void markDirty() { _dirty = true; }     // Mark buffer as modified
    inline void setClean() { _dirty = false; }     // Mark buffer as clean after sync
    uint16_t getTotalPhysicalLeds() const;
    size_t getMemoryUsage() const { return _bufferSize; } // Memory usage in bytes

  private:
    std::vector<VirtualToPhysicalMapping> _physicalStrips; // Attached physical strips
    uint16_t _totalLeds;                                   // Virtual LED count
    uint8_t* _buffer;                                      // Unified RGB(W) buffer
    size_t _bufferSize;                                    // Buffer size in bytes
    ColorOrder _colorOrder;                                // Color order
    uint8_t _bytesPerLed;                                  // Bytes per LED (3 or 4)
    bool _dirty;                                           // Buffer modified?
    uint8_t _brightness;                                   // Current brightness for APA102 (0-255, default 255)

    PhysicalStrip* findPhysicalAtIndex(uint16_t virtualIndex, uint16_t& outPhysicalIndex) const;
    void writePixelToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
};
