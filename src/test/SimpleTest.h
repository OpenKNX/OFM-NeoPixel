/**
 * @file SimpleTest.h
 * @brief Simple Hardware Test for WS2812B (No Library!)
 *
 * Direct bit-banging test to verify LED hardware functionality.
 * Sends raw timing signals for WS2812B protocol.
 * 
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include "NeoPixelManager.h"
#include <stdint.h>
#include <string>

/**
 * @brief Simple Hardware Test (Singleton, Direct Bit-Banging)
 *
 * Minimal test that directly sends WS2812B timing signals
 * to verify hardware functionality without using any library.
 */
class SimpleTest
{
  public:
    inline std::string logPrefix() { return "SimpleTest"; }

    static SimpleTest& instance()
    {
        static SimpleTest inst;
        return inst;
    }

    bool init(uint8_t pin = 9, uint8_t ledCount = 8);

    /**
     * Cycles through basic colors:
     * - All RED
     * - All GREEN
     * - All BLUE
     * - All WHITE
     * - Rainbow
     * - All OFF
     */
    void loop();
    void stop();
    bool isRunning() { return _running; }
    void runOnce();

  private:
    SimpleTest() = default;
    ~SimpleTest() = default;
    SimpleTest(const SimpleTest&) = delete;
    SimpleTest& operator=(const SimpleTest&) = delete;

    uint8_t _pin = 9;
    uint8_t _ledCount = 8;
    bool _running = false;
    uint8_t _testPhase = 0;
    uint32_t _lastPhaseChange = 0;

    // Low-level bit-banging functions
    void sendBit0();
    void sendBit1();
    void sendByte(uint8_t byte);
    void sendColor(uint8_t r, uint8_t g, uint8_t b);
    void sendReset();
    void setAllColor(uint8_t r, uint8_t g, uint8_t b);
};