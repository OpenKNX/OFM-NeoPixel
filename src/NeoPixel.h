#pragma once
/**
 * @file        NeoPixel.h
 * @brief       OpenKNX Module wrapper for NeoPixel LED control system
 *
 * This module provides integration between the NeoPixel library and OpenKNX ecosystem.
 * It handles initialization, console commands, and lifecycle management.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include "NeoPixelManager.h"
#include "OpenKNX.h"
#include "effects/EffectPool.h" // Effect Pool for LED effects

#define NeoPixel_Display_Name "NeoPixel" // Display name
#define NeoPixel_Display_Version "1.0.0" // Display version

/**
 * @brief Update speed presets for NeoPixel animations
 *
 * Back to the Future style speed settings!
 */
enum class UpdateSpeed : uint8_t
{
    SLOW = 100,    // 10 FPS  - Relaxed animations
    NORMAL = 50,   // 20 FPS  - Standard speed (default)
    FAST = 33,     // 30 FPS  - Smooth animations
    MAX = 20,      // 50 FPS  - Very responsive
    EXTREME = 12,  // 80 FPS  - Extreme smoothness!
    LUDICROUS = 4, // 120 FPS - Maximum smoothness!
    FTL = 0        // 240 FPS - Faster Than Light!
};

/**
 * @class NeoPixel
 * @brief OpenKNX Module wrapper for NeoPixel LED management
 *
 * This module provides:
 * - LED strip initialization and management
 * - Virtual strip support
 * - Segment and effect system
 * - Console commands for control and benchmarking
 * - Integration with OpenKNX ecosystem
 */
class NeoPixel : public OpenKNX::Module
{
  public:
    // OpenKNX Module interface
    inline const std::string name() override { return NeoPixel_Display_Name; }
    inline const std::string version() override { return NeoPixel_Display_Version; }

    // OpenKNX required functions
    void init();                          // Initialize the NeoPixel module
    void setup(bool configured) override; // Setup LED strips
    void loop(bool configured) override;  // Loop for the NeoPixel module

    void processInputKo(GroupObject& ko) override; // Process GroupObjects

    void showHelp() override;                                               // Show help for console commands
    bool processCommand(const std::string command, bool diagnose) override; // Process console commands

    // Constructor & Destructor
    NeoPixel();
    ~NeoPixel();

    // Manager Access
    inline NeoPixelManager* getManager() { return _manager; }

    // Quick Access Functions
    inline bool isInitialized() const { return _initialized; }
    inline uint32_t getStripCount() const { return _manager ? _manager->getStripCount() : 0; }
    inline uint32_t getTotalLeds() const { return _manager ? _manager->getTotalLedCount() : 0; }

    // Strip Management
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol = LedProtocol::WS2812B);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder);
    PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol = LedProtocol::APA102);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder);
    PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder);
    VirtualStrip* addVirtualStrip(uint16_t totalLeds, ColorOrder colorOrder = ColorOrder::GRB);

    // Update Control
    void updateAll();
    void clearAll();
    void setUpdateSpeed(UpdateSpeed speed);
    void setAutoUpdate(bool enabled);
    inline bool getAutoUpdate() const { return _autoUpdate; }
    inline uint32_t getUpdateInterval() const { return _updateInterval; }
    float getActualFps() const;

  private:
    NeoPixelManager* _manager; // NeoPixel Manager instance
    bool _initialized;         // Initialization flag
    uint32_t _lastUpdateTime;  // Last update timestamp
    uint32_t _updateInterval;  // Update interval in ms (for auto-update mode)
    bool _autoUpdate;          // Auto-update mode enabled
    uint32_t _fpsCounter;      // Frame counter for FPS measurement
    uint32_t _fpsLastMeasure;  // Last FPS measurement timestamp
    float _measuredFps;        // Measured FPS (updated every second)

    // =============================================================================
    // Console command handlers - Core (always available)
    // --> See NeoPixelConsole.cpp for implementations
    // =============================================================================
    bool processInfoCommand();
    bool processListCommand();
    bool processUpdateCommand(const std::string& args);
    bool processClearCommand();
    bool processTestCommand(const std::string& args);
    bool processSpeedCommand(const std::string& args);
    bool processAutoCommand(const std::string& args);
    bool processPerformanceCommand();

    // PhysicalStrip management commands
    bool processPhysCommand(const std::string& args);
    bool processPhysAddCommand(const std::string& args);
    bool processPhysDelCommand(const std::string& args);
    bool processPhysListCommand();
    bool processPhysTimingsCommand();
    bool processPhysTimingCommand(const std::string& args);

    // VirtualStrip management commands
    bool processVirtCommand(const std::string& args);
    bool processVirtAddCommand(const std::string& args);
    bool processVirtDelCommand(const std::string& args);
    bool processVirtListCommand();
    bool processVirtAttachCommand(const std::string& args);
    bool processVirtDetachCommand(const std::string& args);

    // Segment management commands
    bool processSegCommand(const std::string& args);
    bool processSegAddCommand(const std::string& args);
    bool processSegDelCommand(const std::string& args);
    bool processSegListCommand();
    bool processSegPauseCommand(const std::string& args);
    bool processSegResumeCommand(const std::string& args);
    bool processSegStopCommand(const std::string& args);
    bool processSegClearEffectCommand(const std::string& args);

    // Effect management commands
    bool processEffectsCommand();
    bool processEffectCommand(const std::string& args);
    bool processGarageCommand(const std::string& args);
    bool processColorCommand(const std::string& args);
    bool processBrightnessCommand(const std::string& args);
    bool processHardwareBrightnessCommand(const std::string& args);
    bool processEffectConfigCommand(const std::string& args);
    bool processPowerCommand(const std::string& args);

    // Console help helper functions
    void printDetailHelpHeader(const char* title);
    void printDetailHelpSeparator();
    void printDetailHelpParameter(const char* paramDesc);
    void printDetailHelpExample(const char* example);
    void printDetailHelpEnd();

    // Timing helper functions
    const char* getTimingModeName(TimingMode mode);
    TimingMode parseTimingMode(const char* str);

#ifdef OPENKNX_NEOPIXEL_TESTS
    bool processAnimTestStartCommand();
    bool processAnimTestStopCommand();
    bool processSimpleTestStartCommand();
    bool processSimpleTestStopCommand();
#endif

#ifdef OPENKNX_NEOPIXEL_BENCHMARK
    bool processBenchmarkCommand(const std::string& args);
#endif
}; // class NeoPixel (Public OpenKNX::Module)

extern NeoPixel neoPixelModule; // NeoPixel module instance
