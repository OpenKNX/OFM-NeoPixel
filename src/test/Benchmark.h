/**
 * OpenKNX NeoPixel - Comprehensive Benchmark Suite
 *
 * This file provides extensive benchmark and testing functionality when OPENKNX_NEOPIXEL_BENCHMARK is defined.
 * Includes performance comparisons, stability tests, and protocol validation for WS2812B (1-Wire) and APA102 (SPI).
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#ifdef OPENKNX_NEOPIXEL_BENCHMARK

    #include "../NeoPixelManager.h"
    #include "../PhysicalStrip.h"
    #include "OpenKNX.h"
    #include "PerformanceTracker.h"
    #include <pico/stdlib.h>

namespace OpenKNX
{
    namespace NeoPixel
    {
        class Benchmark
        {
          public:
            // ====================================================================
            // HELPER FUNCTIONS
            // ====================================================================

            inline void print_separator(const char* title = nullptr)
            {
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("═══════════════════════════════════════════════════");
                openknx.logger.color(0);
                if (title)
                {
                    openknx.logger.logWithValues("  %s", title);
                    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                    openknx.logger.log("═══════════════════════════════════════════════════");
                    openknx.logger.color(0);
                }
            }
            inline void print_end_separator()
            {
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("═══════════════════════════════════════════════════");
                openknx.logger.color(0);
                openknx.logger.log("");
            }

            inline const char* formatThroughput(float bytesPerSec)
            {
                static char buffer[64];
                if (bytesPerSec > 1000000.0f)
                    snprintf(buffer, sizeof(buffer), "%.2f MB/s", bytesPerSec / 1000000.0f);
                else if (bytesPerSec > 1000.0f)
                    snprintf(buffer, sizeof(buffer), "%.2f KB/s", bytesPerSec / 1000.0f);
                else
                    snprintf(buffer, sizeof(buffer), "%.0f B/s", bytesPerSec);
                return buffer;
            }

            // ====================================================================
            // BASIC BENCHMARKS
            // ====================================================================
            inline void benchmarkUpdateSpeed(PhysicalStrip* strip, const char* name, uint16_t iterations = 100)
            {
                if (!strip || !strip->isInitialized())
                {
                    openknx.logger.logWithValues("ERROR: Strip '%s' not initialized!", name);
                    return;
                }

                openknx.logger.logWithValues("Update Speed Test: %s", name);
                openknx.logger.logWithValues("  Strip: %d LEDs, Protocol: %d",
                                             strip->getLedCount(), (int)strip->getProtocol());

                uint32_t totalTime = 0;
                uint32_t minTime = UINT32_MAX;
                uint32_t maxTime = 0;

                // Setze Test-Pattern (alle LEDs weiß)
                strip->setAll(128, 128, 128);

                for (uint16_t i = 0; i < iterations; i++)
                {
                    // Wait for previous DMA transfer to complete
                    strip->waitForTransfer(5);

                    // Ändere Farbe leicht für jeden Frame
                    uint8_t brightness = 100 + (i % 156);
                    strip->setAll(brightness, brightness, brightness);

                    uint32_t start = micros();
                    strip->show();
                    uint32_t duration = micros() - start;

                    totalTime += duration;
                    if (duration < minTime) minTime = duration;
                    if (duration > maxTime) maxTime = duration;
                }

                float avgTime = (float)totalTime / iterations;
                float fps = 1000000.0f / avgTime;
                uint32_t bytesPerUpdate = strip->getLedCount() * 3; // RGB
                float bytesPerSec = (bytesPerUpdate * fps);

                openknx.logger.logWithValues("  Iterations:  %d", iterations);
                openknx.logger.logWithValues("  Min Time:    %d µs", minTime);
                openknx.logger.logWithValues("  Avg Time:    %.1f µs", avgTime);
                openknx.logger.logWithValues("  Max Time:    %d µs", maxTime);
                openknx.logger.logWithValues("  FPS:         %.1f", fps);
                openknx.logger.logWithValues("  Throughput:  %s", formatThroughput(bytesPerSec));
                openknx.logger.log("");
            }

            inline void benchmarkColorPatterns(PhysicalStrip* strip, const char* name)
            {
                if (!strip || !strip->isInitialized())
                {
                    openknx.logger.logWithValues("ERROR: Strip '%s' not initialized!", name);
                    return;
                }

                print_separator("Color Pattern Performance");
                openknx.logger.logWithValues("Testing %s with different patterns", name);

                struct Pattern
                {
                    const char* name;
                    uint8_t r, g, b;
                };

                Pattern patterns[] = {
                    {"Red", 255, 0, 0},
                    {"Green", 0, 255, 0},
                    {"Blue", 0, 0, 255},
                    {"White", 255, 255, 255},
                    {"Cyan", 0, 255, 255},
                    {"Magenta", 255, 0, 255},
                    {"Yellow", 255, 255, 0},
                    {"Off", 0, 0, 0}};

                openknx.logger.log("Pattern  │ Update Time │ FPS     │ CPU %");
                openknx.logger.log("─────────┼─────────────┼─────────┼──────");

                for (const auto& pattern : patterns)
                {
                    strip->setAll(pattern.r, pattern.g, pattern.b);

                    uint32_t start = micros();
                    uint32_t cpuStart = time_us_32();

                    strip->show();

                    uint32_t duration = micros() - start;
                    uint32_t cpuTime = time_us_32() - cpuStart;

                    float fps = 1000000.0f / duration;
                    float cpuPercent = (cpuTime / (float)duration) * 100.0f;

                    openknx.logger.logWithValues("%-8s │ %7d µs  │ %7.1f │ %5.1f%%",
                                                 pattern.name, duration, fps, cpuPercent);
                }
                print_end_separator();
            }

            // ====================================================================
            // SIZE COMPARISON
            // ====================================================================

            /**
             * @brief Compare performance with different LED counts
             * Uses existing strips to show performance scaling dynamically
             *
             * @param manager Pointer to NeoPixelManager containing strips
             */
            inline void benchmarkSizeComparison(NeoPixelManager* manager)
            {
                print_separator("LED Count Scaling Test");

                if (!manager)
                {
                    openknx.logger.log("ERROR: No NeoPixelManager available");
                    return;
                }

                uint32_t stripCount = manager->getStripCount();
                if (stripCount == 0)
                {
                    openknx.logger.log("ERROR: No strips available");
                    return;
                }

                openknx.logger.logWithValues("Testing %d available strip(s)", stripCount);
                openknx.logger.log("");
                openknx.logger.log("Strip │ LEDs │ Update Time │ FPS     │ Throughput   │ µs/LED");
                openknx.logger.log("──────┼──────┼─────────────┼─────────┼──────────────┼───────");

                for (uint32_t i = 0; i < stripCount; i++)
                {
                    PhysicalStrip* strip = manager->getStrip(i);
                    if (!strip)
                    {
                        openknx.logger.logWithValues("ERROR: Strip %d is null", i);
                        continue;
                    }

                    uint16_t ledCount = strip->getLedCount();

                    strip->setAll(128, 128, 128);

                    // 10 Messungen für Durchschnitt mit DMA-Synchronisation
                    uint32_t totalTime = 0;
                    for (int j = 0; j < 10; j++)
                    {
                        strip->waitForTransfer(100); // DMA completion sicherstellen

                        uint32_t start = micros();
                        strip->show();
                        uint32_t duration = micros() - start;
                        totalTime += duration;
                    }

                    float avgTime = totalTime / 10.0f;
                    float fps = 1000000.0f / avgTime;
                    float bytesPerSec = (ledCount * 3 * fps);
                    float usPerLed = avgTime / ledCount;

                    openknx.logger.logWithValues("  %-2d  │ %-4d │ %7.0f µs  │ %7.1f │ %-12s │ %6.2f",
                                                 i + 1, ledCount, avgTime, fps, formatThroughput(bytesPerSec), usPerLed);

                    delay(10); // Kleine Pause zwischen Tests
                }
                print_end_separator();
            }

            // ====================================================================
            // PROTOCOL COMPARISON (1-WIRE vs SPI)
            // ====================================================================

  
            inline void compareProtocols(uint32_t serialPin, uint32_t spiMosiPin, uint32_t spiSckPin, uint16_t ledCount = 64)
            {
                print_separator("1-Wire (WS2812B) vs SPI (APA102) Comparison");
                openknx.logger.logWithValues("Testing with %d LEDs", ledCount);

                // Test WS2812B (1-Wire Serial)
                PhysicalStrip* ws2812b = new PhysicalStrip(serialPin, ledCount, LedProtocol::WS2812B);
                if (!ws2812b || !ws2812b->init())
                {
                    openknx.logger.log("ERROR: Could not initialize WS2812B strip!");
                    if (ws2812b) delete ws2812b;
                    return;
                }

                // Test APA102 (SPI)
                PhysicalStrip* apa102 = new PhysicalStrip(spiMosiPin, ledCount, LedProtocol::APA102, DriverType::SPI_PIO);
                if (!apa102 || !apa102->init())
                {
                    openknx.logger.log("ERROR: Could not initialize APA102 strip!");
                    if (apa102) delete apa102;
                    delete ws2812b;
                    return;
                }

                openknx.logger.log("Protocol │ Update Time │ FPS     │ Throughput   │ CPU %");
                openknx.logger.log("─────────┼─────────────┼─────────┼──────────────┼──────");

                // Test WS2812B
                {
                    ws2812b->setAll(128, 128, 128);
                    uint32_t totalTime = 0;
                    uint32_t totalCpu = 0;

                    for (int i = 0; i < 20; i++)
                    {
                        ws2812b->waitForTransfer(5); // DMA completion

                        uint32_t cpuStart = time_us_32();
                        uint32_t start = micros();
                        ws2812b->show();
                        uint32_t duration = micros() - start;
                        uint32_t cpuTime = time_us_32() - cpuStart;

                        totalTime += duration;
                        totalCpu += cpuTime;
                    }

                    float avgTime = totalTime / 20.0f;
                    float avgCpu = totalCpu / 20.0f;
                    float fps = 1000000.0f / avgTime;
                    float bytesPerSec = (ledCount * 3 * fps);
                    float cpuPercent = (avgCpu / avgTime) * 100.0f;

                    openknx.logger.logWithValues("WS2812B  │ %7.0f µs  │ %7.1f │ %-12s │ %5.1f%%",
                                                 avgTime, fps, formatThroughput(bytesPerSec), cpuPercent);
                }

                // Test APA102
                {
                    apa102->setAll(128, 128, 128);
                    uint32_t totalTime = 0;
                    uint32_t totalCpu = 0;

                    for (int i = 0; i < 20; i++)
                    {
                        apa102->waitForTransfer(5); // DMA completion

                        uint32_t cpuStart = time_us_32();
                        uint32_t start = micros();
                        apa102->show();
                        uint32_t duration = micros() - start;
                        uint32_t cpuTime = time_us_32() - cpuStart;

                        totalTime += duration;
                        totalCpu += cpuTime;
                    }

                    float avgTime = totalTime / 20.0f;
                    float avgCpu = totalCpu / 20.0f;
                    float fps = 1000000.0f / avgTime;
                    float bytesPerSec = (ledCount * 4 * fps); // APA102 = 4 bytes/LED
                    float cpuPercent = (avgCpu / avgTime) * 100.0f;

                    openknx.logger.logWithValues("APA102   │ %7.0f µs  │ %7.1f │ %-12s │ %5.1f%%",
                                                 avgTime, fps, formatThroughput(bytesPerSec), cpuPercent);
                }

                print_end_separator();

                delete ws2812b;
                delete apa102;
            }

            // ====================================================================
            // STABILITY TEST
            // ====================================================================

            inline void benchmarkStability(PhysicalStrip* strip, const char* name, uint16_t iterations = 1000)
            {
                if (!strip || !strip->isInitialized())
                {
                    openknx.logger.logWithValues("ERROR: Strip '%s' not initialized!", name);
                    return;
                }

                print_separator("Stability Test (Long-term)");
                openknx.logger.logWithValues("Testing %s with %d updates", name, iterations);

                uint32_t totalTime = 0;
                uint32_t minTime = UINT32_MAX;
                uint32_t maxTime = 0;
                uint16_t errors = 0;

                for (uint16_t i = 0; i < iterations; i++)
                {
                    // Wait for previous transfer to complete (DMA!)
                    strip->waitForTransfer(5); // 5ms timeout should be sufficient for all cases

                    // Wechselndes Pattern
                    uint8_t color = (i % 3);
                    if (color == 0) strip->setAll(255, 0, 0); // Red
                    else if (color == 1)
                        strip->setAll(0, 255, 0); // Green
                    else
                        strip->setAll(0, 0, 255); // Blue

                    uint32_t start = micros();
                    bool success = strip->show();
                    uint32_t duration = micros() - start;

                    if (!success) errors++;

                    totalTime += duration;
                    if (duration < minTime) minTime = duration;
                    if (duration > maxTime) maxTime = duration;

                    // Progress alle 100 Updates
                    if ((i + 1) % 100 == 0)
                    {
                        openknx.logger.logWithValues("  Progress: %d/%d updates...", i + 1, iterations);
                    }
                }

                float avgTime = (float)totalTime / iterations;
                float variance = maxTime - minTime;
                float variancePercent = (variance / avgTime) * 100.0f;
                float successRate = ((iterations - errors) / (float)iterations) * 100.0f;

                openknx.logger.log("Stability Results:");
                openknx.logger.logWithValues("  Iterations:   %d", iterations);
                openknx.logger.logWithValues("  Errors:       %d", errors);
                openknx.logger.logWithValues("  Success Rate: %.2f%%", successRate);
                openknx.logger.logWithValues("  Min Time:     %d µs", minTime);
                openknx.logger.logWithValues("  Avg Time:     %.1f µs", avgTime);
                openknx.logger.logWithValues("  Max Time:     %d µs", maxTime);
                openknx.logger.logWithValues("  Variance:     %d µs (%.1f%%)", (uint32_t)variance, variancePercent);

                if (errors == 0)
                {
                    openknx.logger.log("  Status: PERFECT - No errors detected!");
                }
                else if (successRate > 99.0f)
                {
                    openknx.logger.log("  Status: GOOD - Minor errors detected");
                }
                else
                {
                    openknx.logger.log("  Status: WARNING - Significant errors!");
                }
                print_end_separator();
            }

            // ====================================================================
            // DMA vs CPU COMPARISON
            // ====================================================================

            inline void compareDMA(uint32_t pin, uint16_t ledCount, LedProtocol protocol)
            {
                print_separator("DMA vs CPU Transfer Comparison");
                openknx.logger.logWithValues("Testing %d LEDs with protocol %d", ledCount, (int)protocol);

                // Test mit DMA (default für 1-Wire)
                PhysicalStrip* dmaStrip = new PhysicalStrip(pin, ledCount, protocol, DriverType::SERIAL_1WIRE);
                if (!dmaStrip || !dmaStrip->init())
                {
                    openknx.logger.log("ERROR: Could not initialize DMA strip!");
                    if (dmaStrip) delete dmaStrip;
                    return;
                }

                openknx.logger.log("Mode     │ Update Time │ FPS     │ CPU Usage");
                openknx.logger.log("─────────┼─────────────┼─────────┼──────────");

                // DMA Test
                {
                    dmaStrip->setAll(128, 128, 128);
                    uint32_t totalTime = 0;

                    for (int i = 0; i < 50; i++)
                    {
                        dmaStrip->waitForTransfer(5); // DMA completion

                        uint32_t start = micros();
                        dmaStrip->show();
                        uint32_t duration = micros() - start;
                        totalTime += duration;
                    }

                    float avgTime = totalTime / 50.0f;
                    float fps = 1000000.0f / avgTime;

                    openknx.logger.logWithValues("DMA      │ %7.0f µs  │ %7.1f │ Very Low",
                                                 avgTime, fps);
                }

                openknx.logger.log("");
                openknx.logger.log("Note: CPU-based transfer not implemented (PIO always uses DMA)");
                print_end_separator();

                delete dmaStrip;
            }

            // ====================================================================
            // CPU USAGE ANALYSIS
            // ====================================================================

            inline void analyzeCpuUsage(PhysicalStrip* strip, const char* name)
            {
                if (!strip || !strip->isInitialized())
                {
                    openknx.logger.logWithValues("ERROR: Strip '%s' not initialized!", name);
                    return;
                }

                print_separator("CPU Usage Analysis");
                openknx.logger.logWithValues("Analyzing %s", name);

                // Note: PerformanceMonitor integration available in strip
                // Test verschiedene Szenarien
                struct Scenario
                {
                    const char* name;
                    uint8_t r, g, b;
                };

                Scenario scenarios[] = {
                    {"Static White", 255, 255, 255},
                    {"Static Red", 255, 0, 0},
                    {"Static Off", 0, 0, 0},
                };

                for (const auto& scenario : scenarios)
                {
                    openknx.logger.logWithValues("Scenario: %s", scenario.name);
                    strip->setAll(scenario.r, scenario.g, scenario.b);

                    // Measure 10 Updates
                    uint32_t totalTime = 0;
                    uint32_t minTime = UINT32_MAX;
                    uint32_t maxTime = 0;

                    for (int i = 0; i < 10; i++)
                    {
                        strip->waitForTransfer(5); // DMA completion

                        uint32_t start = micros();
                        strip->show();
                        uint32_t duration = micros() - start;

                        totalTime += duration;
                        if (duration < minTime) minTime = duration;
                        if (duration > maxTime) maxTime = duration;

                        delay(1);
                    }

                    float avgTime = totalTime / 10.0f;
                    float cpuUsage = (avgTime / 1000000.0f) * 100.0f; // Rough estimate

                    openknx.logger.logWithValues("  Avg Time:    %.1f µs", avgTime);
                    openknx.logger.logWithValues("  Min Time:    %d µs", minTime);
                    openknx.logger.logWithValues("  Max Time:    %d µs", maxTime);
                    openknx.logger.logWithValues("  Est. CPU:    %.2f%%", cpuUsage);
                    openknx.logger.log("");
                }
                print_end_separator();
            }

            // ====================================================================
            // COMPREHENSIVE BENCHMARK SUITE
            // ====================================================================

            inline void runAllBenchmarks(PhysicalStrip* strip, const char* name)
            {
                if (!strip || !strip->isInitialized())
                {
                    openknx.logger.logWithValues("ERROR: Strip '%s' not initialized!", name);
                    return;
                }

                print_separator("COMPREHENSIVE BENCHMARK SUITE");
                openknx.logger.logWithValues("Testing: %s", name);
                openknx.logger.logWithValues("LEDs: %d, Protocol: %d, Driver: %s",
                                             strip->getLedCount(), (int)strip->getProtocol(), strip->getDriverName());
                openknx.logger.log("");

                // 1. Update Speed
                benchmarkUpdateSpeed(strip, name, 100);
                delay(100);

                // 2. Color Patterns
                benchmarkColorPatterns(strip, name);
                delay(100);

                // 3. CPU Analysis
                analyzeCpuUsage(strip, name);
                delay(100);

                // 4. Stability (1000 updates)
                benchmarkStability(strip, name, 1000);
                delay(100);

                print_separator("BENCHMARK COMPLETE");
            }

        }; // class Benchmark
    } // namespace NeoPixel
} // namespace OpenKNX

#endif // OPENKNX_NEOPIXEL_BENCHMARK
