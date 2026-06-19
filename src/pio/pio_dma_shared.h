/**
 * @file pio_dma_shared.h
 * @brief Shared DMA IRQ management for PIO NeoPixel drivers
 *
 * Unified DMA IRQ handling for all LED strips:
 * - DMA_IRQ_0: Used by ALL Serial strips (WS2812B, SK6812, etc.)
 * - DMA_IRQ_1: Used by ALL SPI strips (APA102, SK9822, WS2801, etc.)
 *
 * Checks both IRQs to support mixed strip types.
 * This allows up to 12 total strips (limited by DMA channels) with only 2 IRQs. Check pio_dma_shared.cpp for implementation.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once

#if defined(ARDUINO_ARCH_RP2040)

    #include <Arduino.h>
    #include <hardware/dma.h>
    #include <stdint.h>

    // DMA channel count is chip-specific: RP2040 has 12, RP2350 has 16. Use the SDK's
    // NUM_DMA_CHANNELS so the handler registries and every bounds check match the actual
    // hardware. A hardcoded 12 let dma_claim_unused_channel() hand out channel 12..15 on
    // RP2350 → out-of-bounds write into the [12] registry AND that channel's completion
    // IRQ never registered/acked by the i<12 loop → per-frame wedge → watchdog reboot.
    #ifdef NUM_DMA_CHANNELS
        #define MAX_DMA_CHANNELS NUM_DMA_CHANNELS
    #else
        #define MAX_DMA_CHANNELS 16
    #endif

// Forward declarations
//
class PIO_NeoPixel_Serial; // Forward declaration for Serial strips
class PIO_NeoPixel_SPI;    // Forward declaration for SPI strips

// Global DMA handler registries (shared between Serial and SPI)
//
inline PIO_NeoPixel_Serial* volatile g_serialHandlers[MAX_DMA_CHANNELS] = {nullptr};
inline PIO_NeoPixel_SPI* volatile g_spiHandlers[MAX_DMA_CHANNELS] = {nullptr};

void unifiedDmaIRQHandler(); // Unified DMA IRQ Handler declaration

#endif // ARDUINO_ARCH_RP2040
