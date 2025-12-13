#pragma once

#include <stdint.h>

/**
 * @brief Timing mode for NeoPixel bit timing control
 * 
 * Controls the PIO clock divider to adjust the bitrate for WS2812B/SK6812 LEDs.
 * Standard bitrate is 800 kHz.
 * 
 * - AUTO: Automatically calculates correct timing based on system clock (default)
 * - LEGACY_125MHZ: Fixed clkdiv=15.625 for onboard LEDs designed for 125MHz systems
 * - SLOW_*PCT: Reduced bitrate for long cables, poor level shifters, or cheap LED clones
 * - FAST_*PCT: Increased bitrate for performance tuning (higher refresh rates with many LEDs)
 * 
 * Example at 150 MHz system clock:
 * - AUTO:        800 kHz (clkdiv = 18.75)
 * - SLOW_10PCT:  720 kHz (clkdiv = 20.83)
 * - FAST_25PCT: 1000 kHz (clkdiv = 15.00)
 * 
 * The timing modes are overclock-safe: they work correctly regardless of system clock speed
 * (125, 150, 200, 300 MHz, etc.) because they use runtime clock detection.
 */
enum class TimingMode : uint8_t
{
    AUTO = 0,           ///< 800 kHz - Standard timing (auto-detects system clock)
    LEGACY_125MHZ = 1,  ///< Fixed clkdiv=15.625 (workaround for RP2350 onboard LEDs)
    
    // Slower modes (for signal integrity with long cables/poor level shifters)
    SLOW_20PCT = 2,     ///< 640 kHz (0.80x) - Maximum safety margin
    SLOW_15PCT = 3,     ///< 680 kHz (0.85x)
    SLOW_10PCT = 4,     ///< 720 kHz (0.90x)
    SLOW_5PCT = 5,      ///< 760 kHz (0.95x)
    
    // Faster modes (for performance tuning)
    FAST_5PCT = 6,      ///< 840 kHz (1.05x)
    FAST_10PCT = 7,     ///< 880 kHz (1.10x)
    FAST_15PCT = 8,     ///< 920 kHz (1.15x)
    FAST_20PCT = 9,     ///< 960 kHz (1.20x)
    FAST_25PCT = 10     ///< 1000 kHz (1.25x) - Maximum safe overclock
};
