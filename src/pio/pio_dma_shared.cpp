#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_dma_shared.h"
    #include "pio_neopixel_serial.h"
    #include "pio_neopixel_spi.h"

/*
 * Unified DMA IRQ Handler (shared by both Serial and SPI)
 * @brief Handles DMA completion for all PIO NeoPixel drivers
 * @info Checks both DMA IRQ0 and IRQ1 to support mixed strip types
 *       - DMA_IRQ_0: Serial strips (WS2812B, SK6812, etc.)
 *       - DMA_IRQ_1: SPI strips (APA102, SK9822, WS2801, etc.)
 */
void __isr __not_in_flash_func(unifiedDmaIRQHandler)()
{
    // Check all DMA channels for both IRQ0 and IRQ1
    for (int i = 0; i < MAX_DMA_CHANNELS; i++)
    {
        // Check IRQ0 (Serial strips)
        if (g_serialHandlers[i] && dma_channel_get_irq0_status(i))
        {
            dma_channel_acknowledge_irq0(i);

            // Check Serial handler - call public completion handler
            if (g_serialHandlers[i])
            {
                g_serialHandlers[i]->onDmaComplete();
            }
        }

        // Check IRQ1 (SPI strips)
        if (g_spiHandlers[i] && dma_channel_get_irq1_status(i))
        {
            dma_channel_acknowledge_irq1(i);

            // Check SPI handler - call public completion handler
            if (g_spiHandlers[i])
            {
                g_spiHandlers[i]->onDmaComplete();
            }
        }
    }
}

#endif // ARDUINO_ARCH_RP2040
