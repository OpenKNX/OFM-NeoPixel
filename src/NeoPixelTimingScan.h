#pragma once
#include <stdint.h>

class PhysicalStrip;

/**
 * Complete, backend-neutral timing candidates for qualification. Every profile
 * carries all four symbols, so PIO and RMT exercise a genuinely distinct
 * waveform rather than retaining a stale custom configuration.
 */
struct CloneTimingProfile
{
    const char* name;
    uint16_t    t0hNs;
    uint16_t    t0lNs;
    uint16_t    t1hNs;
    uint16_t    t1lNs;
    uint32_t    resetUs;  ///< Reset/latch time in µs (0 = protocol default)
    const char* desc;     ///< Human-readable description
};

extern const CloneTimingProfile kCloneProfiles[];
extern const uint8_t            kCloneProfileCount;
extern const uint32_t           kScanColorDurationMs;
extern const uint32_t           kScanPauseDurationMs;
extern const uint32_t           kScanWaitTimeoutMs;   ///< Auto-advance timeout while waiting for user input (ms)

bool applyCloneTimingProfile(PhysicalStrip* strip, const CloneTimingProfile& profile);
bool writeCloneTimingStressPayload(PhysicalStrip* strip);
