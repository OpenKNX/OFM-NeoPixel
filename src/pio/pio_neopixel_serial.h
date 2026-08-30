/**
 * @file pio_neopixel_serial.h
 * @brief PIO-based 1-Wire NeoPixel Driver for RP2040/RP2350
 *
 * Hardware-accelerated driver for WS2812/SK6812 and other 1-wire LED protocols.
 * Uses RP2040/RP2350's PIO (Programmable I/O) subsystem for precise timing.
 *
 * Key Features:
 * - Zero-CPU timing via PIO state machines
 * - DMA-based transfers for maximum efficiency
 * - Supports both RGB (24-bit) and RGBW (32-bit) modes
 * - Automatic resource management (PIO/SM/DMA)
 * - Multiple LED strips on any GPIO pins
 * - Support for WS2812/WS2811 (800kHz) and the explicit WS2811_400KHZ profile.
 *
 * Technical Details:
 * - Uses PIO state machines for timing-critical protocols
 * - DMA for zero-copy memory transfers
 * - Shares DMA IRQ with other strip types (see pio_dma_shared.h)
 * - Automatic clock divider calculation for exact timing
 * - 8MHz PIO clock (125ns per cycle) for WS2812B
 * - MSB-first transmission (required by WS2812B)
 *
 * WS2812B Timing (@ 800kHz):
 * - 0-bit: 400ns HIGH + 850ns LOW = 1.25µs
 * - 1-bit: 800ns HIGH + 450ns LOW = 1.25µs
 * - Reset: >50µs LOW
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#if defined(ARDUINO_ARCH_RP2040)

    #include "../IHardwareDriver.h"
    #include "../LevelShifterType.h"
    #include "../TimingMode.h"
    #include "hardware/clocks.h"
    #include "hardware/dma.h"
    #include "hardware/pio.h"
    #include <stdint.h>
    #include <stdlib.h>

// PIO NeoPixel Serial Instance
//
struct pio_neopixel_serial_inst
{
    PIO pio;               // PIO instance (pio0, pio1, pio2)
    uint sm;               // State machine index (0-3)
    uint offset;           // Program offset in PIO memory
    uint pin;              // GPIO pin
    uint16_t ledCount;     // Number of LEDs
    uint8_t bytesPerLed;   // 3 for RGB, 4 for RGBW
    LedProtocol protocol;  // LED protocol type
    ColorOrder colorOrder; // Color byte order
    uint32_t frequency;    // Update frequency (Hz)
    TimingMode timingMode; // Timing mode for PIO clock divider calculation
    float actual_bitrate;  // Actual bitrate used (may differ for AUTO_LEGACY)
    float actual_clkdiv;   // Actual clock divider used

    uint8_t* buffer;        // LED data buffer (RGB/RGBW bytes) - user writes here
    uint8_t* bufferSending; // RGBCCT only: DMA reads from here (double-buffer)
    size_t bufferSize;      // Buffer size in bytes

    uint32_t* dmaBuffer;  // DMA transfer buffer (32-bit words) - for RGB/RGBW only, nullptr for RGBCCT
    size_t dmaBufferSize; // DMA buffer size (in words/halfwords/bytes depending on fifoWordBits)
    uint fifoWordBits;    // FIFO word size in bits (8/16/32) - for RGBCCT dynamic sizing

    int dmaChannel;     // DMA channel (-1 if not used)
    int dmaIrqNum;      // DMA IRQ number (0 or 1, -1 if not used)
    bool useDMA;        // Use DMA for transfers
    bool initialized;   // Initialization state
    volatile bool busy; // DMA transfer in progress
    volatile bool framePending; // A frame is queued or still inside drain/latch time

    // Latch timing - prevents starting new transfer too early
    volatile uint32_t fifoEmptyTime; // micros() when FIFO became empty (for reset timing)
    uint32_t resetTimeUs;            // Required reset/latch time (50-300µs depending on protocol)
    volatile bool waitingForReset;   // True after FIFO empty, waiting for reset pulse

    LevelShifterType levelShifterType; // Level-shifter type (NONE or TXS0108E)

    // Per-strip PIO program: the 4 instruction words carry this protocol's cycle
    // ratio in their delay fields, so one program source serves every chip.
    uint16_t       programWords[4];
    pio_program_t  program;
    bool           inverted;      // drive the complemented waveform (TM1814 family)

    // Realized signal, for diagnostics. What the hardware actually produces,
    // not what was requested.
    uint16_t realizedT0hNs;
    uint16_t realizedT0lNs;
    uint16_t realizedT1hNs;
    uint16_t realizedT1lNs;
    uint16_t realizedBitNs;
    uint8_t  cyclesPerBit;

    // Bytes sent ahead of the pixel data. TM1814 needs its C1/C2 constant-current
    // command pair there; every other protocol leaves this at 0.
    uint8_t prefixBytes;

    /// Per-strip channel swap (ProtocolHelper::ChannelSwap).
    uint8_t channelSwap;

    /// 0 = chip profile, 1 = force normal, 2 = force inverted.
    uint8_t polarityOverride;
};
typedef struct pio_neopixel_serial_inst pio_neopixel_serial_inst_t;

// PIO NeoPixel Serial Driver Class - Implements IHardwareDriver interface using RP2040/RP2350 PIO
//
class PIO_NeoPixel_Serial : public IHardwareDriver
{
  public:
    PIO_NeoPixel_Serial(uint pin, uint16_t ledCount, LedProtocol protocol, bool useDMA = true, TimingMode timingMode = TimingMode::AUTO);
    virtual ~PIO_NeoPixel_Serial();

    // IHardwareDriver interface implementation
    bool init() override;
    void setPolarityOverride(uint8_t mode) override { if (_inst) _inst->polarityOverride = mode; }
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) override;
    bool show() override;
    bool isBusy() override;
    void clear() override;
    uint16_t getLedCount() const override { return _inst ? _inst->ledCount : 0; }
    LedProtocol getProtocol() const override { return _inst ? _inst->protocol : LedProtocol::WS2812B; }
    DriverCapabilities getCapabilities() const override;
    uint8_t* getBuffer() override { return _inst ? _inst->buffer : nullptr; }
    size_t getBufferSize() const override { return _inst ? _inst->bufferSize : 0; }
    bool isInitialized() const override { return _inst ? _inst->initialized : false; }
    uint32_t getTransferTimeoutUs() const override;
    DriverImplementation getDriverType() const override { return DriverImplementation::PIO_SERIAL; }

    /**
     * @brief Get active PIO instance (pio0/pio1)
     * @return PIO instance or nullptr if not initialized
     */
    inline PIO getPio() const { return _inst ? _inst->pio : nullptr; }

    /**
     * @brief Get active state machine index (0-3)
     * @return State machine index or 0 if not initialized
     */
    inline uint getStateMachine() const { return _inst ? _inst->sm : 0; }

    /**
     * @brief Get active DMA channel number
     * @return DMA channel or -1 if DMA not used
     */
    inline int getDmaChannel() const { return _inst ? _inst->dmaChannel : -1; }

    /**
     * @brief Check if DMA transfers are enabled
     * @return true if using DMA, false otherwise
     */
    inline bool isDmaEnabled() const { return _inst ? _inst->useDMA : false; }

    /// Realized 0-bit high time in ns, as produced by the hardware.
    inline uint16_t getRealizedT0hNs() const { return _inst ? _inst->realizedT0hNs : 0; }
    /// Realized 0-bit low time in ns.
    inline uint16_t getRealizedT0lNs() const { return _inst ? _inst->realizedT0lNs : 0; }
    /// Realized 1-bit high time in ns.
    inline uint16_t getRealizedT1hNs() const { return _inst ? _inst->realizedT1hNs : 0; }
    /// Realized 1-bit low time in ns.
    inline uint16_t getRealizedT1lNs() const { return _inst ? _inst->realizedT1lNs : 0; }
    /// Realized bit period in ns.
    inline uint16_t getRealizedBitNs() const { return _inst ? _inst->realizedBitNs : 0; }
    /// PIO cycles the program spends per bit.
    inline uint8_t getCyclesPerBit() const { return _inst ? _inst->cyclesPerBit : 0; }
    /// Latch time enforced between frames, in us.
    inline uint32_t getResetTimeUs() const { return _inst ? _inst->resetTimeUs : 0; }
    /// Whether the output drives the complemented waveform.
    inline bool isInverted() const { return _inst ? _inst->inverted : false; }

    /**
     * @brief Get PIO program offset in instruction memory
     * @return Program offset or 0 if not initialized
     */
    inline uint getProgramOffset() const { return _inst ? _inst->offset : 0; }

    /**
     * @brief Get LED protocol frequency in Hz (target frequency)
     * @return Frequency (e.g. 800000 for WS2812B) or 0 if not initialized
     */
    inline uint32_t getFrequency() const { return _inst ? _inst->frequency : 0; }

    /**
     * @brief Get actual bitrate used by PIO (may differ from target for AUTO_LEGACY)
     * @return Actual bitrate in Hz or 0 if not initialized
     */
    inline float getActualBitrate() const { return _inst ? _inst->actual_bitrate : 0.0f; }

    /**
     * @brief Get actual clock divider used by PIO
     * @return Actual clkdiv or 0 if not initialized
     */
    inline float getActualClkdiv() const { return _inst ? _inst->actual_clkdiv : 0.0f; }

    /**
     * @brief Get LED color byte order (RGB/GRB/etc)
     * @return ColorOrder enum value
     */
    ColorOrder getColorOrder() const override { return _inst ? _inst->colorOrder : ColorOrder::RGB; }

    /**
     * @brief Set LED color byte order (override protocol default)
     * @param order New ColorOrder (RGB, GRB, BGR, etc.)
     */
    void setColorOrder(ColorOrder order) override
    {
        if (_inst) _inst->colorOrder = order;
    }

    // Configuration management (IHardwareDriver interface)
    PhysicalStripConfig* createDefaultConfig() const override;
    bool applyConfig(const PhysicalStripConfig* config) override;

    /**
     * @brief Get bytes per LED (3=RGB, 4=RGBW)
     * @return Bytes per LED or 3 if not initialized
     */
    inline uint8_t getBytesPerLed() const { return _inst ? _inst->bytesPerLed : 3; }

    // Static resource detection methods
    static uint getAvailableStateMachines();
    static uint getAvailableDmaChannels();

    void onDmaComplete();

  private:
    pio_neopixel_serial_inst_t* volatile _inst;

    bool initPIO();
    bool initDMA();
    bool sendDataPIO();
    void sendDataDMA();
    void packDataToDMABuffer();
    static void dmaIRQHandler();
    void rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww = 0, uint8_t cw = 0);

    static void registerDMAHandler(int channel, PIO_NeoPixel_Serial* instance);
    static void unregisterDMAHandler(int channel);
};

#endif // ARDUINO_ARCH_RP2040
