/**
 * @file NeoPixelManager.h
 * @brief Central Management for Multiple LED Strips
 *
 * NeoPixelManager is the main manager for all LED strips in the application
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

/*
 * NeoPixelManager manages:
 * - Multiple PhysicalStrips (1-Wire and SPI)
 * - Update loops
 * - Resource sharing
 * - Platform limits (max 7 PIO for RP2040, max 7 RMT for ESP32, etc.)
 *
 * Example:
 *   auto mgr = new NeoPixelManager();
 *   auto strip1 = mgr->addStrip(5, 100, WS2812B);
 *   auto strip2 = mgr->addStrip(6, 50, SK6812);
 *   mgr->init();
 *
 *   strip1->setAll(255, 0, 0);
 *   strip2->setPixel(0, 0, 255, 0);
 *   mgr->show();  // Update all strips
 */

#pragma once
#include "OpenKNX.h"
#include "PhysicalStrip.h"
#include "PowerManager.h"
#include "Segment.h"
#include "TimingMode.h"
#include "VirtualStrip.h"

#include <stdint.h>
#include <string>
#include <vector>

// ============================================================================
// Resource Limits Configuration
// ============================================================================

/**
 * Maximum number of physical LED strips that can be managed.
 * Can be overridden before including this header:
 *   #define NEOPIXEL_MAX_PHYSICAL_STRIPS 16
 *
 * Default: 8 (sufficient for most RP2040/ESP32 applications)
 */
#ifndef NEOPIXEL_MAX_PHYSICAL_STRIPS
    #define NEOPIXEL_MAX_PHYSICAL_STRIPS 8
#endif

/**
 * Maximum number of virtual LED strips that can be created.
 * Virtual strips are lightweight wrappers around segments of physical strips.
 *
 * Default: 4 (typical use cases)
 */
#ifndef NEOPIXEL_MAX_VIRTUAL_STRIPS
    #define NEOPIXEL_MAX_VIRTUAL_STRIPS 12
#endif

/**
 * Maximum number of segments that can be defined.
 * Segments are ranges within physical strips used by virtual strips or effects.
 *
 * Default: 16 (allows multiple segments per strip)
 */
#ifndef NEOPIXEL_MAX_SEGMENTS
    #define NEOPIXEL_MAX_SEGMENTS 16
#endif

/**
 * Enable/disable limit enforcement at runtime.
 * When enabled (1): addStrip/addVirtualStrip/addSegment will fail if limits exceeded
 * When disabled (0): Limits are only used for reserve(), exceeding them causes reallocation
 *
 * Can be overridden:
 *   #define NEOPIXEL_ENFORCE_LIMITS 0  // Disable enforcement
 *
 * Default: 1 (enabled - recommended for stability)
 */
#ifndef NEOPIXEL_ENFORCE_LIMITS
    #define NEOPIXEL_ENFORCE_LIMITS 1
#endif

// ============================================================================
// Forward declarations
// ============================================================================
class Effect;

/**
 * Statistics and debug information
 */
struct ManagerStats
{
    uint32_t totalStrips;    // Number of strips
    uint32_t activeStrips;   // Initialized strips
    uint32_t totalLeds;      // Total count of all LEDs
    uint32_t lastUpdateTime; // Last update time (ms)
    uint32_t updateCount;    // Number of updates since start
    uint32_t errorCount;     // Error counter
};

/**
 * NeoPixelManager - Central Management
 */
class NeoPixelManager
{
  public:
    inline const std::string logPrefix() { return "NeoPixelManager"; }

    NeoPixelManager();
    ~NeoPixelManager();

    // ====================================================================
    // Strip Management
    // ====================================================================
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol = LedProtocol::WS2812B);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder, TimingMode timingMode);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder, TimingMode timingMode);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder, uint32_t frequencyHz);
    bool removeStrip(PhysicalStrip* strip);
    PhysicalStrip* getStrip(uint32_t index);
    uint32_t getStripCount() const { return _strips.size(); }

    PhysicalStrip* findStripByPin(uint32_t pin);

    // ====================================================================
    // Initialization & Control
    // ====================================================================
    bool init();
    bool isInitialized() const { return _initialized; }
    void reset();
    uint32_t getErrorCount() const { return _errorCount; }

    // ====================================================================
    // Update Control (4-Phase Pipeline)
    // ====================================================================
    void update(uint32_t deltaTime);        // Phase 1-4: Full pipeline (non-blocking)
    void updateEffects(uint32_t deltaTime); // Phase 1: Effect calculations only
    void applyPowerLimit();                 // Phase 2: Global power scaling
    void syncAll();                         // Phase 3: VirtualStrip → PhysicalStrip
    bool showAll();                         // Phase 4: Hardware transfer
    bool updateAll();                       // Phase 2-4: Convenience method
    bool waitForAll(uint32_t timeoutMs = 0);
    bool waitForStrip(PhysicalStrip* strip, uint32_t timeoutMs = 0);
    bool isAnyBusy() const;
    bool areAllReady() const;

    // ====================================================================
    // Virtual Strip Management (NEW)
    // ====================================================================
    VirtualStrip* addVirtualStrip(uint16_t totalLeds, ColorOrder colorOrder = ColorOrder::RGB);
    bool attachPhysicalToVirtual(VirtualStrip* vstrip, PhysicalStrip* pstrip, uint16_t offset);
    VirtualStrip* getVirtualStrip(uint32_t index);
    uint32_t getVirtualStripCount() const { return _virtualStrips.size(); }
    bool removeVirtualStrip(VirtualStrip* vstrip);
    bool removeVirtualStrip(uint32_t index);

    // ====================================================================
    // Segment Management (NEW)
    // ====================================================================
    Segment* addSegment(VirtualStrip* vstrip, uint16_t startLed, uint16_t endLed);
    Segment* getSegment(uint32_t index);
    uint32_t getSegmentCount() const { return _segments.size(); }
    bool removeSegment(Segment* segment);
    bool removeSegment(uint32_t index);
    bool attachEffect(Segment* segment, Effect* effect);

    // ====================================================================
    // Convenience Functions
    // ====================================================================
    void setAllRGB(uint8_t r, uint8_t g, uint8_t b);
    void setAllRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void clearAll();
    void allOff();

    // ====================================================================
    // Statistics & Debug
    // ====================================================================
    ManagerStats getStats() const;
    void printDebugInfo();

    // ====================================================================
    // Performance Monitoring (NEW!)
    // ====================================================================
    uint32_t getTotalMemoryUsage() const;
    uint32_t getTotalLedCount() const;
    static uint32_t getMaxStrips();

    // ====================================================================
    // Power Management (NEW!)
    // ====================================================================

    /**
     * @brief Get power manager
     * @return Pointer to PowerManager
     */
    PowerManager* getPowerManager() { return &_powerManager; }

    /**
     * @brief Set maximum current limit
     * @param maxCurrentMA Maximum current in milliamps
     */
    void setMaxCurrent(uint32_t maxCurrentMA) { _powerManager.setMaxCurrent(maxCurrentMA); }

    /**
     * @brief Enable/disable power management
     * @param enabled true = limit current, false = no limiting
     */
    void setPowerManagementEnabled(bool enabled) { _powerManager.setEnabled(enabled); }

    /**
     * @brief Get estimated total power consumption
     * @return Power in Watts
     */
    float getTotalPowerWatts() const;

  private:
    std::vector<PhysicalStrip*> _strips;       // Physical strips
    std::vector<VirtualStrip*> _virtualStrips; // Virtual strips
    std::vector<Segment*> _segments;           // Segments
    bool _initialized;                         // Initialization state of manager
    uint32_t _lastUpdateTime;                  // Last update time in milliseconds
    uint32_t _updateCount;                     // Number of updates since start
    uint32_t _errorCount;                      // Error counter
    PowerManager _powerManager;                // Power/Current management

    // Private helper methods
    bool checkResourcesAvailable(LedProtocol protocol);                               // Check if resources are available for new strip
    void countResourceUsage(uint32_t& pioUsed, uint32_t& spiUsed, uint32_t& rmtUsed); // Count current resource usage
};
