#ifdef OPENKNX_NEOPIXEL_TESTS
/**
 * @file AnimationTest.h
 * @brief Animation Test System for NeoPixel
 *
 * Provides comprehensive animation tests:
 * - Rainbow chase patterns
 * - Matrix color tests (R/G/B/W)
 * - Virtual 9x8 matrix mode
 * - Performance monitoring
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

    #pragma once

    #include "NeoPixelManager.h"
    #include <stdint.h>
    #include <string>

/**
 * @brief Animation Test System (Singleton)
 *
 * Can run in two modes:
 * 1. VIRTUAL_TEST_MODE: 9x8 matrix (8x8 + 1x8 reversed)
 * 2. SEPARATE_MODE: Two independent strips
 */
class AnimationTest
{
  public:
    inline std::string logPrefix() { return "AnimationTest"; }
    static void print_separator(const char* title = nullptr);
    static void print_end_separator();
    static AnimationTest& instance()
    {
        static AnimationTest inst;
        return inst;
    }

    bool init(
        NeoPixelManager* manager, // NeoPixel Manager
        uint8_t pin1 = 9,         // GPIO for 8-LED strip
        uint8_t ledCount1 = 8,    // Number of LEDs in first strip
        uint8_t pin2 = 22,        // GPIO for 64-LED strip
        uint8_t ledCount2 = 64,   // Number of LEDs in second strip
        bool virtualMode = true,  // Virtual 9x8 matrix mode
        uint8_t brightness = 10); // Brightness (1-255)

    bool loop();
    void start();
    void stop();
    void cleanup();
    bool isRunning() { return _running; }
    void printStats();
    void setUpdateInterval(uint32_t intervalMs) { _updateInterval = intervalMs; }
    void setBrightness(uint8_t brightness) { _brightness = brightness; }

  private:
    AnimationTest() = default; // Private constructor for singleton
    ~AnimationTest() = default;
    AnimationTest(const AnimationTest&) = delete;
    AnimationTest& operator=(const AnimationTest&) = delete;

    NeoPixelManager* _manager = nullptr;
    VirtualStrip* _strip8 = nullptr;
    VirtualStrip* _matrix8x8 = nullptr;
    PhysicalStrip* _physStrip1 = nullptr;
    PhysicalStrip* _physStrip2 = nullptr;

    bool _running = false;
    bool _virtualMode = true;
    uint8_t _brightness = 10;
    uint8_t _pin1 = 9, _pin2 = 22;
    uint8_t _ledCount1 = 8, _ledCount2 = 64;

    // Timing
    uint32_t _updateInterval = 50;
    uint32_t _logInterval = 10000;
    uint32_t _lastUpdateTime = 0;
    uint32_t _lastLogTime = 0;
    uint32_t _testStartTime = 0;
    uint32_t _frameCount = 0;

    // Performance tracking
    uint32_t _minUpdateTime = UINT32_MAX;
    uint32_t _maxUpdateTime = 0;
    uint64_t _totalUpdateTime = 0;

    // Animation state
    uint8_t _animPhase = 0;
    uint32_t _lastPhaseChange = 0;

    // Helper methods
    void runVirtualAnimation();
    void runSeparateAnimation();
    void updatePerformanceStats(uint32_t updateTime);
    void logPerformanceStats();
    void initRainbowTable(uint8_t brightness);
    bool runSegmentlessUpdateRegression();
};
#endif // OPENKNX_NEOPIXEL_TESTS
