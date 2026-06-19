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
     * @brief Get current frames per second
     * @param updateIntervalMs Current auto-update interval in milliseconds
     * @return Calculated FPS based on interval
     */
    float getCurrentFPS(uint32_t updateIntervalMs) const
    {
        if (updateIntervalMs > 0) return 1000.0f / updateIntervalMs;
        // FTL / unlimited (interval = 0): the loop runs flat out, so the achievable
        // rate is bounded by the measured average frame time, not by an interval.
        uint32_t avgUs = getAverageTime();
        return (avgUs > 0) ? (1000000.0f / avgUs) : 0.0f;
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
};
