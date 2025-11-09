/**
 * @file SimpleTest.cpp
 * @brief Simple Hardware Test Implementation
 */

#include "test/SimpleTest.h"
#include "OpenKNX/Log/Logger.h"
#include <Arduino.h>
#include <hardware/gpio.h>

// Fast GPIO macros (RP2040 direct register access)
#define GPIO_SET(pin) sio_hw->gpio_set = (1u << (pin))
#define GPIO_CLR(pin) sio_hw->gpio_clr = (1u << (pin))

// NOP macros for precise timing
#define NOP1 __asm__ volatile("nop")
#define NOP5 \
    NOP1;    \
    NOP1;    \
    NOP1;    \
    NOP1;    \
    NOP1
#define NOP10 \
    NOP5;     \
    NOP5

bool SimpleTest::init(uint8_t pin, uint8_t ledCount)
{
    _pin = pin;
    _ledCount = ledCount;

    logInfoP("=== SimpleTest Init ===");
    logInfoP("GPIO: %d, LEDs: %d", pin, ledCount);
    logInfoP("Protocol: WS2812B (Direct Bit-Banging)");

    gpio_init(_pin);
    gpio_set_dir(_pin, GPIO_OUT);
    GPIO_CLR(_pin);

    delay(100);

    _running = true;
    _testPhase = 0;
    _lastPhaseChange = millis();

    logInfoP("SimpleTest Init COMPLETE");
    return true;
}

void SimpleTest::loop()
{
    if (!_running) return;

    uint32_t now = millis();

    // Update every 2 seconds
    if (now - _lastPhaseChange < 2000) return;
    _lastPhaseChange = now;

    logInfoP("=== SimpleTest Phase %d ===", _testPhase);

    switch (_testPhase)
    {
        case 0:
            logInfoP("ALL OFF");
            setAllColor(0, 0, 0);
            break;

        case 1:
            logInfoP("LED 0 = DIM RED");
            sendColor(16, 0, 0);
            for (uint8_t i = 1; i < _ledCount; i++)
            {
                sendColor(0, 0, 0);
            }
            sendReset();
            break;

        case 2:
            logInfoP("LED 0 = DIM GREEN");
            sendColor(0, 16, 0);
            for (uint8_t i = 1; i < _ledCount; i++)
            {
                sendColor(0, 0, 0);
            }
            sendReset();
            break;

        case 3:
            logInfoP("LED 0 = DIM BLUE");
            sendColor(0, 0, 16);
            for (uint8_t i = 1; i < _ledCount; i++)
            {
                sendColor(0, 0, 0);
            }
            sendReset();
            break;

        case 4:
            logInfoP("ALL DIM WHITE");
            setAllColor(16, 16, 16);
            break;

        case 5:
            logInfoP("RAINBOW");
            // Rainbow pattern
            for (uint8_t i = 0; i < _ledCount; i++)
            {
                uint8_t hue = (i * 256 / _ledCount) % 256;
                if (hue < 85)
                {
                    sendColor(16, hue * 16 / 85, 0);
                }
                else if (hue < 170)
                {
                    sendColor((170 - hue) * 16 / 85, 16, 0);
                }
                else
                {
                    sendColor(0, 16, (hue - 170) * 16 / 85);
                }
            }
            sendReset();
            break;

        default:
            _testPhase = 0;
            return;
    }

    _testPhase++;
    if (_testPhase > 5) _testPhase = 0;
}

void SimpleTest::stop()
{
    if (!_running) return;

    logInfoP("SimpleTest stopping...");
    setAllColor(0, 0, 0);
    _running = false;
    logInfoP("SimpleTest stopped");
}

void SimpleTest::runOnce()
{
    logInfoP("SimpleTest One-Shot:");

    // Test sequence
    logInfoP("1. All OFF");
    setAllColor(0, 0, 0);
    delay(500);

    logInfoP("2. All RED");
    setAllColor(16, 0, 0);
    delay(500);

    logInfoP("3. All GREEN");
    setAllColor(0, 16, 0);
    delay(500);

    logInfoP("4. All BLUE");
    setAllColor(0, 0, 16);
    delay(500);

    logInfoP("5. All WHITE");
    setAllColor(16, 16, 16);
    delay(500);

    logInfoP("6. All OFF");
    setAllColor(0, 0, 0);

    logInfoP("One-Shot Test Complete");
}

// ========== LOW-LEVEL BIT-BANGING ==========

void SimpleTest::sendBit0()
{
    GPIO_SET(_pin);
    // 350ns HIGH = ~44 cycles (minus overhead ~15 cycles = 29 NOPs)
    NOP10;
    NOP10;
    NOP5;
    NOP1;
    NOP1;
    NOP1;
    NOP1;
    GPIO_CLR(_pin);
    // 800ns LOW = ~100 cycles (minus overhead ~15 cycles = 85 NOPs)
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP5;
}

void SimpleTest::sendBit1()
{
    GPIO_SET(_pin);
    // 700ns HIGH = ~88 cycles (minus overhead ~15 = 73 NOPs)
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP1;
    NOP1;
    NOP1;
    GPIO_CLR(_pin);
    // 600ns LOW = ~75 cycles (minus overhead ~15 = 60 NOPs)
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
    NOP10;
}

void SimpleTest::sendByte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        if (byte & (1 << i))
        {
            sendBit1();
        }
        else
        {
            sendBit0();
        }
    }
}

void SimpleTest::sendColor(uint8_t r, uint8_t g, uint8_t b)
{
    sendByte(g); // WS2812B is GRB order!
    sendByte(r);
    sendByte(b);
}

void SimpleTest::sendReset()
{
    GPIO_CLR(_pin);
    delayMicroseconds(60);
}

void SimpleTest::setAllColor(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < _ledCount; i++)
    {
        sendColor(r, g, b);
    }
    sendReset();
}
