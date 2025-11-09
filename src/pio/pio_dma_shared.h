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

    #define MAX_DMA_CHANNELS 12 // Define maximum DMA channels available on RP2040 and RP2350

    #include <Arduino.h>
    #include <hardware/dma.h>
    #include <stdint.h>

// Forward declarations
//
class PIO_NeoPixel_Serial; // Forward declaration for Serial strips
class PIO_NeoPixel_SPI;    // Forward declaration for SPI strips

// Global DMA handler registries (shared between Serial and SPI)
//
inline PIO_NeoPixel_Serial* g_serialHandlers[12] = {nullptr};
inline PIO_NeoPixel_SPI* g_spiHandlers[12] = {nullptr};

void unifiedDmaIRQHandler(); // Unified DMA IRQ Handler declaration

#endif // ARDUINO_ARCH_RP2040
