/**
 * @file pio_neopixel_spi.h
 * @brief PIO-based SPI NeoPixel Driver for RP2040/RP2350
 *
 * Hardware-accelerated driver for APA102/SK9822 and other SPI-based LED protocols.
 * Uses RP2040/RP2350's PIO (Programmable I/O) subsystem for flexible SPI communication.
 *
 * Key Features:
 * - Zero-CPU timing via PIO state machines
 * - DMA-based transfers for maximum efficiency
 * - Flexible pin assignment (not limited to hardware SPI pins)
 * - Multiple independent SPI instances via state machines
 * - Automatic resource management (PIO/SM/DMA)
 * - Support for APA102, SK9822, WS2801, LPD8806
 * - Variable SPI frequencies (tested up to 20MHz)
 *
 * Technical Details:
 * - Uses PIO state machines for SPI clock and data
 * - DMA for zero-copy memory transfers
 * - Shares DMA IRQ with other strip types (see pio_dma_shared.h)
 * - Automatic clock divider calculation for exact timing
 * - MSB-first transmission (standard SPI)
 * - SPI Mode 0: CPOL=0, CPHA=0
 *
 * APA102/SK9822 Protocol:
 * - Start Frame: 32 bits LOW (0x00000000)
 * - LED Data: 32 bits per LED [111bbbbb][B][G][R]
 * - End Frame: 32 bits HIGH (0xFFFFFFFF)
 * - Default frequency: 10MHz (adjustable)
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#if defined(ARDUINO_ARCH_RP2040)

    #include "../IHardwareDriver.h"
    #include "hardware/clocks.h"
    #include "hardware/dma.h"
    #include "hardware/pio.h"
    #include <stdint.h>
    #include <stdlib.h>

/**
 * PIO NeoPixel SPI Instance
 */
struct pio_neopixel_spi_inst
{
    PIO pio;      // PIO instance (pio0, pio1, pio2)
    uint sm;      // State machine index (0-3)
    uint offset;  // Program offset in PIO memory
    uint clkPin;  // GPIO pin for clock
    uint mosiPin; // GPIO pin for MOSI/data
    uint csPin;   // Chip select pin (-1 if not used)

    uint16_t ledCount;    // Number of LEDs
    uint8_t bytesPerLed;  // Bytes per LED (typically 4 for APA102)
    LedProtocol protocol; // LED protocol type

    // APA102/SK9822 format: 111xxxxx (brightness 5 bits) + BGR
    // Brightness byte: 0xE0 | (brightness & 0x1F)
    // According to protocol, first byte is brightness/gray level byte, then B, G, R
    // The highest 3 bits are '111' followed by 5 bits of brightness
    // Example: brightness=31 -> 0xFF(11111111) (full brightness), brightness=0 -> 0xE0(11100000) (off)
    bool hasGlobalBrightness; // APA102/SK9822 have global brightness

    uint8_t* buffer;   // LED data buffer
    size_t bufferSize; // Buffer size in bytes

    uint32_t spiFrequency; // SPI frequency (Hz)

    int dmaChannel;     // DMA channel (-1 if not used)
    int dmaIrqNum;      // DMA IRQ number (0 or 1, -1 if not used)
    bool useDMA;        // Use DMA for transfers
    bool initialized;   // Initialization state
    volatile bool busy; // Transfer in progress
};
typedef struct pio_neopixel_spi_inst pio_neopixel_spi_inst_t;

/**
 * PIO NeoPixel SPI Driver Class
 *
 * Implements IHardwareDriver interface using RP2040/RP2350 PIO
 */
class PIO_NeoPixel_SPI : public IHardwareDriver
{
  public:
    PIO_NeoPixel_SPI(
        uint clkPin,
        uint mosiPin,
        uint16_t ledCount,
        LedProtocol protocol,
        uint32_t frequency = 10000000,
        int csPin = -1,
        bool useDMA = true);

    virtual ~PIO_NeoPixel_SPI();

    // IHardwareDriver interface implementation
    bool init() override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override;
    bool show() override;
    bool isBusy() override;
    void clear() override;
    uint16_t getLedCount() const override { return _inst ? _inst->ledCount : 0; }
    LedProtocol getProtocol() const override { return _inst ? _inst->protocol : LedProtocol::WS2801; }
    DriverCapabilities getCapabilities() const override;
    uint8_t* getBuffer() override { return _inst ? _inst->buffer : nullptr; }
    size_t getBufferSize() const override { return _inst ? _inst->bufferSize : 0; }
    bool isInitialized() const override { return _inst ? _inst->initialized : false; }
    DriverImplementation getDriverType() const override { return DriverImplementation::PIO_SPI; }

    inline PIO getPio() const { return _inst ? _inst->pio : nullptr; }
    inline uint getStateMachine() const { return _inst ? _inst->sm : 0; }
    inline int getDmaChannel() const { return _inst ? _inst->dmaChannel : -1; }
    inline bool isDMAenabled() const { return _inst ? _inst->useDMA : false; }
    inline uint getprogramOffset() const { return _inst ? _inst->offset : 0; }
    inline ColorOrder getColorOrder() const { return ColorOrder::BGR; } // APA102/SK9822 use BGR order
    inline uint8_t getBytesPerLed() const { return _inst ? _inst->bytesPerLed : 0; }
    inline uint32_t getSpiFrequency() const { return _inst ? _inst->spiFrequency : 0; }
    inline uint getClkPin() const { return _inst ? _inst->clkPin : 0; }
    inline uint getMosiPin() const { return _inst ? _inst->mosiPin : 0; }

    void onDmaComplete();

  private:
    pio_neopixel_spi_inst_t* _inst;

    bool initPIO();
    bool initDMA();
    void sendDataPIO();
    void sendDataDMA();
    void sendStartFrame();
    void sendEndFrame();
    static void dmaIRQHandler();
    void rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 31);

    static PIO_NeoPixel_SPI* _dmaHandlers[12];
    static void registerDMAHandler(int channel, PIO_NeoPixel_SPI* instance);
    static void unregisterDMAHandler(int channel);
};

#endif // ARDUINO_ARCH_RP2040
