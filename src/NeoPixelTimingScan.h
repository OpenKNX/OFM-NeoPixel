#pragma once
#include "PhysicalStripConfig.h" // TimingMode

/**
 * Predefined timing presets for diagnosing non-responsive or clone LED strips.
 * Qualify cycles through all profiles in sequence, showing a distinct color per
 * profile on the FULL strip — user applies whichever profile lights up all LEDs.
 */
struct CloneTimingProfile
{
    const char* name;
    TimingMode  mode;     ///< BASE timing mode (controls clkdiv / bitrate)
    uint32_t    resetUs;  ///< Reset/latch time in µs (0 = protocol default)
    uint8_t     r, g, b; ///< Visual feedback color for this profile
    const char* desc;     ///< Human-readable description
};

extern const CloneTimingProfile kCloneProfiles[];
extern const uint8_t            kCloneProfileCount;
extern const uint32_t           kScanColorDurationMs;
extern const uint32_t           kScanPauseDurationMs;
extern const uint32_t           kScanWaitTimeoutMs;   ///< Auto-advance timeout while waiting for user input (ms)
