#pragma once

#include <stdint.h>

/**
 * @brief Timing mode for NeoPixel bit timing control
 *
 * Controls the PIO clock divider to adjust the bitrate for WS2812B/SK6812 LEDs.
 * Standard bitrate is 800 kHz.
 *
 * - AUTO: Automatically calculates correct timing based on system clock (800 kHz target)
 * - AUTO_LEGACY: Optimized for WS2812C/D onboard LEDs (960 kHz target, adapts to CPU freq)
 * - SLOW_*PCT: Reduced bitrate for long cables, poor level shifters, or cheap LED clones
 * - FAST_*PCT: Increased bitrate for performance tuning (higher refresh rates with many LEDs)
 *
 * Example at different CPU frequencies:
 * AUTO (800 kHz target):
 *   - 125 MHz: clkdiv = 15.625
 *   - 150 MHz: clkdiv = 18.750
 *   - 200 MHz: clkdiv = 25.000
 *
 * AUTO_LEGACY (960 kHz target for WS2812C/D):
 *   - 125 MHz: clkdiv = 13.021
 *   - 150 MHz: clkdiv = 15.625
 *   - 200 MHz: clkdiv = 20.833
 *   - 250 MHz: clkdiv = 26.042
 *
 * The timing modes are overclock-safe: they work correctly regardless of system clock speed
 * (125, 150, 200, 250+ MHz) because they use runtime clock detection.
 */
enum class TimingMode : uint8_t
{
    AUTO = 0,        ///< 800 kHz - Standard timing for WS2812B (auto-detects system clock)
    AUTO_LEGACY = 1, ///< 960 kHz - Optimized for WS2812C/D onboard LEDs (adapts to CPU freq)

    // Slower modes (for signal integrity with long cables/poor level shifters)
    SLOW_20PCT = 2, ///< 640 kHz (0.80x) - Maximum safety margin
    SLOW_15PCT = 3, ///< 680 kHz (0.85x)
    SLOW_10PCT = 4, ///< 720 kHz (0.90x)
    SLOW_5PCT = 5,  ///< 760 kHz (0.95x)

    // Faster modes (for performance tuning)
    FAST_5PCT = 6,  ///< 840 kHz (1.05x)
    FAST_10PCT = 7, ///< 880 kHz (1.10x)
    FAST_15PCT = 8, ///< 920 kHz (1.15x)
    FAST_20PCT = 9, ///< 960 kHz (1.20x)
    FAST_25PCT = 10 ///< 1000 kHz (1.25x) - Maximum safe overclock
};
