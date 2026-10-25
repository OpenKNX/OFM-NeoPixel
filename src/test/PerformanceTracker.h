/**
 * @file PerformanceTracker.h
 * @brief Simple Performance Tracking for NeoPixel auto-update loop
 *
 * Lightweight performance monitoring that tracks update timing statistics.
 * Used by `neo perf` command to display performance metrics.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Lightweight performance tracker for NeoPixel updates
 *
 * Tracks timing statistics during auto-update mode:
 * - Frame count
 * - Min/Max/Avg update times
 * - Render tick intervals (min/max/avg, lateness histogram)
 * - Deferred passes and stale ticks
 * - Session uptime
 *
 * Zero overhead when auto-update is disabled.
 */
class PerformanceTracker
{
  public:
    /**
     * @brief Constructor - initializes all metrics to zero
     */
    PerformanceTracker()
    {
        reset();
    }

    /**
     * @brief Reset all performance metrics
     */
    void reset()
    {
        frameCount = 0;
        minUpdateTime = UINT32_MAX;
        maxUpdateTime = 0;
        totalUpdateTime = 0;
        sessionStartTime = millis();

        intervalTicks = 0;
        minIntervalMs = UINT32_MAX;
        maxIntervalMs = 0;
        totalIntervalMs = 0;
        for (uint8_t i = 0; i < kLateBins; i++) lateHistogram[i] = 0;
        histTargetMs = 0;
        histTargetChanged = false;
        deferredPasses = 0;
        staleTicks = 0;
    }

    /// Lateness histogram bins: 0 | 1-2 | 3-5 | 6-10 | 11..target-1 (FTL: >=11) | >=target (missed >= 1 tick)
    static constexpr uint8_t kLateBins = 6;

    /**
     * @brief Record one render tick interval
     * @param intervalMs deltaTime the effects receive in ms (since the previous tick or the last frame clock restart)
     * @param targetMs Configured update interval in ms (0 = FTL)
     * @param stale true if the previous frame was still in flight, so this tick's frame was not sent
     * @note Freezes once intervalTicks saturates; `neo perf reset` starts a new window.
     */
    void recordInterval(uint32_t intervalMs, uint32_t targetMs, bool stale)
    {
        if (intervalTicks == UINT32_MAX) return;
        intervalTicks++;
        totalIntervalMs += intervalMs;
        if (intervalMs < minIntervalMs) minIntervalMs = intervalMs;
        if (intervalMs > maxIntervalMs) maxIntervalMs = intervalMs;
        if (stale) staleTicks++;

        if (intervalTicks > 1 && targetMs != histTargetMs) histTargetChanged = true;
        histTargetMs = targetMs;

        const uint32_t late = (intervalMs > targetMs) ? (intervalMs - targetMs) : 0;
        uint8_t bin;
        if (targetMs > 0 && late >= targetMs) bin = 5;
        else if (late == 0) bin = 0;
        else if (late <= 2) bin = 1;
        else if (late <= 5) bin = 2;
        else if (late <= 10) bin = 3;
        else bin = 4;
        lateHistogram[bin]++;
    }

    /**
     * @brief Count a loop pass where a tick was due but no free loop time was left
     */
    void recordDeferredPass()
    {
        if (deferredPasses != UINT32_MAX) deferredPasses++;
    }

    /**
     * @brief Get average tick interval in ms
     * @return Average interval, or 0 if no tick recorded
     */
    float getAverageIntervalMs() const
    {
        return (intervalTicks > 0) ? (float)totalIntervalMs / (float)intervalTicks : 0.0f;
    }

    /**
     * @brief Get session window length in ms
     * @return Milliseconds since reset() was called (wraps after 49.7 days)
     */
    uint32_t getWindowMs() const
    {
        return millis() - sessionStartTime;
    }

    /**
     * @brief Record a single update cycle timing
     * @param updateTimeUs Update time in microseconds
     */
    void recordUpdate(uint32_t updateTimeUs)
    {
        frameCount++;
        if (updateTimeUs < minUpdateTime) minUpdateTime = updateTimeUs;
        if (updateTimeUs > maxUpdateTime) maxUpdateTime = updateTimeUs;
        totalUpdateTime += updateTimeUs;
    }

    /**
     * @brief Get average update time in microseconds
     * @return Average update time, or 0 if no frames recorded
     */
    uint32_t getAverageTime() const
    {
        return (frameCount > 0) ? (totalUpdateTime / frameCount) : 0;
    }

    /**
     * @brief Get session uptime in seconds
     * @return Seconds since reset() was called
     */
    uint32_t getUptimeSeconds() const
    {
        return (millis() - sessionStartTime) / 1000;
    }

    /**
     * @brief Get frames per second derived from the average render time (not measured)
     * @param updateIntervalMs Current auto-update interval in milliseconds
     * @return min(target FPS, 1e6 / avg render time); FTL: 1e6 / avg render time
     */
    float getCurrentFPS(uint32_t updateIntervalMs) const
    {
        // Achievable rate from the measured average frame (render) time.
        uint32_t avgUs = getAverageTime();
        const float achievable = (avgUs > 0) ? (1000000.0f / avgUs) : 0.0f;

        // FTL / unlimited (interval = 0): the loop runs flat out → the measured rate.
        if (updateIntervalMs == 0) return achievable;

        // Throttled: the real rate is the target, but capped by what the frame work
        // actually allows. So when the render exceeds the interval (e.g. ludicrous/max
        // overload) this reports the true ~achievable FPS instead of the unreachable
        // target — and Throughput (= LEDs × thisFPS) stays honest too.
        const float target = 1000.0f / updateIntervalMs;
        return (achievable > 0.0f && achievable < target) ? achievable : target;
    }

    /**
     * @brief Check if any data has been recorded
     * @return true if at least one frame was recorded
     */
    bool hasData() const
    {
        return frameCount > 0;
    }

    // Public metrics (read-only access recommended)
    uint32_t frameCount;       ///< Total frames/updates processed
    uint32_t minUpdateTime;    ///< Minimum update time (µs)
    uint32_t maxUpdateTime;    ///< Maximum update time (µs)
    uint64_t totalUpdateTime;  ///< Cumulative update time (µs)
    uint32_t sessionStartTime; ///< Session start timestamp (ms)

    uint32_t intervalTicks;                ///< Render ticks with a recorded interval
    uint32_t minIntervalMs;                ///< Minimum tick interval (ms)
    uint32_t maxIntervalMs;                ///< Maximum tick interval (ms)
    uint64_t totalIntervalMs;              ///< Cumulative tick interval (ms)
    uint32_t lateHistogram[kLateBins];     ///< Ticks per lateness bin (interval - target)
    uint32_t histTargetMs;                 ///< Target interval of the last recorded tick (ms)
    bool histTargetChanged;                ///< Target changed during the window, histogram mixes targets
    uint32_t deferredPasses;               ///< Loop passes with a due tick but no free loop time
    uint32_t staleTicks;                   ///< Ticks that found the previous frame still in flight
};
