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

    uint16_t ledCount;     // Number of LEDs
    uint8_t bytesPerLed;   // Bytes per LED (typically 4 for APA102)
    LedProtocol protocol;  // LED protocol type
    ColorOrder colorOrder; // Color byte order (RGB, BGR, etc.)

    // APA102/SK9822 format: 111xxxxx (brightness 5 bits) + color bytes
    // Brightness byte: 0xE0 | (brightness & 0x1F)
    // According to protocol, first byte is brightness/gray level byte, then B, G, R
    // The highest 3 bits are '111' followed by 5 bits of brightness
    // Example: brightness=31 -> 0xFF(11111111) (full brightness), brightness=0 -> 0xE0(11100000) (off)
    bool hasGlobalBrightness; // APA102/SK9822 have global brightness

    uint8_t* buffer;     // LED data buffer
    size_t bufferSize;   // Buffer size in bytes (logical)
    size_t bufferWordCount; // Buffer size in 32-bit words (actual allocation)

    uint32_t spiFrequency; // SPI frequency (Hz)
    float clkdiv;          // Actual clock divider (set during init)

    // ===== Extended SPI Configuration (SK9822/APA102 Support) =====
    uint8_t dummyLedMode;       // Dummy LED mode: 0=none, 1=physical (sacrifice LED#0), 2=virtual (LED#0 forced black)
    uint8_t startFrameCount;    // Number of start frames (1-8, default: 8)
    uint8_t endFrameCount;      // Number of end frames (1-80, default: 1 for APA102, varies for SK9822)
    uint32_t startFrameDelayUs; // Delay after start frames in microseconds (0-1000, default: 0)
    uint8_t endFramePattern;    // End frame pattern: 0x00 (APA102) or 0xFF (SK9822), default: 0x00
    LedProtocol detectedChip;   // Detected chip type after auto-detect (APA102 or SK9822)
    bool autoDetectChip;        // Auto-detect chip type on init (default: false)
    uint8_t hwBrightness;       // Current hardware brightness (16-30, default: 30)
    uint8_t minRgbValue;        // Minimum RGB value (0-255, default: 0 for APA102, 8 for APA102_CLONE)
    uint8_t maxRgbValue;        // Maximum RGB value (0-255, default: 255)
    uint8_t hwBrightnessMin;    // Min hardware brightness (0-31, default: 0 for APA102, 16 for clones)
    uint8_t hwBrightnessMax;    // Max hardware brightness (0-31, default: 31 for APA102, 30 for clones)

    int dmaChannel;     // DMA channel (-1 if not used)
    int dmaIrqNum;      // DMA IRQ number (0 or 1, -1 if not used)
    bool useDMA;        // Use DMA for transfers
    bool initialized;   // Initialization state
    volatile bool busy; // Transfer in progress
    volatile bool dirty;         // Buffer has changed since last show() (prevents flicker from redundant sends)
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
        uint32_t frequency = 2000000, // 2 MHz
        int csPin = -1,
        bool useDMA = true,
        ColorOrder colorOrder = ColorOrder::NONE);

    virtual ~PIO_NeoPixel_SPI();

    // IHardwareDriver interface implementation
    bool init() override;
    inline bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    inline bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override;
    bool show() override;
    inline bool isBusy() override;
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
    inline uint8_t getBytesPerLed() const { return _inst ? _inst->bytesPerLed : 0; }
    inline uint32_t getSpiFrequency() const { return _inst ? _inst->spiFrequency : 0; }
    inline uint getClkPin() const { return _inst ? _inst->clkPin : 0; }
    inline uint getMosiPin() const { return _inst ? _inst->mosiPin : 0; }
    inline bool getDirty() const { return _inst ? _inst->dirty : false; }
    inline float getClkdiv() const { return _inst ? _inst->clkdiv : 0.0f; }

    // IHardwareDriver ColorOrder interface
    void setColorOrder(ColorOrder order) override
    {
        if (_inst) _inst->colorOrder = order;
    }
    ColorOrder getColorOrder() const override { return _inst ? _inst->colorOrder : ColorOrder::RGB; }

    // Hardware brightness support (APA102/SK9822 have brightness byte)
    inline bool supportsHardwareBrightness() const override { return _inst && _inst->hasGlobalBrightness; }

    // Configuration management (IHardwareDriver interface)
    PhysicalStripConfig* createDefaultConfig() const override;
    bool applyConfig(const PhysicalStripConfig* config) override;

    // ===== Extended SPI Configuration API =====

    /**
     * @brief Set dummy LED mode (requires re-init)
     * @param mode 0=none (first LED wrong color), 1=physical (sacrifice LED#0), 2=virtual (LED#0 forced black)
     */
    void setDummyLedMode(uint8_t mode);
    uint8_t getDummyLedMode() const { return _inst ? _inst->dummyLedMode : 1; }

    /**
     * @brief Set start frame count (1-8, default: 8)
     */
    void setStartFrameCount(uint8_t count);
    uint8_t getStartFrameCount() const { return _inst ? _inst->startFrameCount : 8; }

    /**
     * @brief Set end frame count (1-80, default: 1)
     */
    void setEndFrameCount(uint8_t count);
    uint8_t getEndFrameCount() const { return _inst ? _inst->endFrameCount : 1; }

    /**
     * @brief Set delay after start frames in microseconds (0-1000, default: 0)
     */
    void setStartFrameDelayUs(uint32_t delayUs);
    uint32_t getStartFrameDelayUs() const { return _inst ? _inst->startFrameDelayUs : 0; }

    /**
     * @brief Set end frame pattern (0x00 for APA102, 0xFF for SK9822)
     */
    void setEndFramePattern(uint8_t pattern);
    uint8_t getEndFramePattern() const { return _inst ? _inst->endFramePattern : 0x00; }

    /**
     * @brief Enable/disable chip auto-detection
     */
    void setAutoDetectChip(bool enable);
    bool getAutoDetectChip() const { return _inst ? _inst->autoDetectChip : false; }

    /**
     * @brief Get detected chip type (after auto-detect or manual set)
     */
    LedProtocol getDetectedChip() const { return _inst ? _inst->detectedChip : _inst->protocol; }

    /**
     * @brief Run chip auto-detection (APA102 vs SK9822)
     * @return Detected chip type
     */
    LedProtocol detectChipType();

    /**
     * @brief Set hardware brightness (16-30 safe range)
     * @param brightness Hardware brightness value (16-30, clamps to safe range)
     * @note Call show() after this to update LEDs
     */
    void setHardwareBrightness(uint8_t brightness);

    /**
     * @brief Get current hardware brightness
     */
    uint8_t getHardwareBrightness() const { return _inst ? _inst->hwBrightness : 30; }

    /**
     * @brief Get actual SPI clock frequency (measured from PIO divider)
     */
    float getActualSpiFrequency() const;

    void onDmaComplete();

  private:
    pio_neopixel_spi_inst_t* volatile _inst;

    bool initPIO();
    bool initDMA();
    void sendDataPIO();
    void sendDataDMA();
    void sendStartFrame();
    void sendEndFrame();
    static void dmaIRQHandler();
    inline void rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 31);

    static PIO_NeoPixel_SPI* _dmaHandlers[12];
    static void registerDMAHandler(int channel, PIO_NeoPixel_SPI* instance);
    static void unregisterDMAHandler(int channel);
};

#endif // ARDUINO_ARCH_RP2040
