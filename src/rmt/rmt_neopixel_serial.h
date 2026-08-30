/**
 * @file rmt_neopixel_serial.h
 * @brief ESP32: RMT-based 1-Wire NeoPixel Driver
 *
 * Uses ESP32 RMT (Remote Control Module) to control WS2812/SK6812 LEDs.
 *
 * RMT channels per variant:
 * - ESP32 (Original):  8 TX channels
 * - ESP32-S2:          4 TX channels
 * - ESP32-S3:          4 TX channels
 * - ESP32-C3:          2 TX channels
 * - ESP32-C5:          4 TX channels
 * - ESP32-C6:          2 TX channels
 *
 * Features:
 * - Hardware-precise timing (like PIO, but fewer channels)
 * - Automatic channel allocation
 * - Optional DMA request on targets/frameworks that support it
 * - Runtime fallback to non-DMA mode if DMA is unavailable
 * - Support for RGB and RGBW
 */

#pragma once

#if defined(ARDUINO_ARCH_ESP32)

    #include "../IHardwareDriver.h"
    #include "../LevelShifterType.h"
    #include <driver/rmt_tx.h>
    #include <stdint.h>
    #include <stdlib.h>

/**
 * RMT NeoPixel Serial Instance
 */
struct rmt_neopixel_serial_inst
{
    rmt_channel_handle_t channel; // RMT channel
    rmt_encoder_handle_t encoder; // Encoder for LED data

    uint32_t pin;          // GPIO pin
    uint16_t ledCount;     // Number of LEDs
    uint8_t bytesPerLed;   // 3 for RGB, 4 for RGBW
    LedProtocol protocol;  // LED protocol
    ColorOrder colorOrder; // Color order

    uint8_t* buffer;   // LED data buffer
    size_t bufferSize; // Buffer size in bytes

    // Custom timing (0 = use protocol default)
    uint16_t customT0H; ///< T0H override in ns (0 = default)
    uint16_t customT0L; ///< T0L override in ns (0 = default)
    uint16_t customT1H; ///< T1H override in ns (0 = default)
    uint16_t customT1L; ///< T1L override in ns (0 = default)

    bool initialized;   // Initialized?
    volatile bool busy; // Transfer running?

    // Latch/reset gate. The chip needs a quiet line after the last bit before it
    // accepts the next frame; "transfer done" is not the same as "ready".
    uint32_t          resetTimeUs;
    volatile uint32_t lastTxEndUs;
    volatile bool     waitingForReset;
    uint32_t          bitPeriodNs;
    volatile uint32_t transferStartedUs;

    // Realized signal, for diagnostics: what the encoder actually emits.
    uint16_t realizedT0hNs;
    uint16_t realizedT0lNs;
    uint16_t realizedT1hNs;
    uint16_t realizedT1lNs;

    // Bytes sent ahead of the pixel data (TM1814 C1/C2 current commands).
    uint8_t prefixBytes;

    /// Per-strip channel swap (ProtocolHelper::ChannelSwap).
    uint8_t channelSwap;

    /// 0 = chip profile, 1 = force normal, 2 = force inverted.
    uint8_t polarityOverride;

    LevelShifterType levelShifterType; // Level-shifter type (NONE or TXS0108E)
};
typedef struct rmt_neopixel_serial_inst rmt_neopixel_serial_inst_t;

/**
 * ESP32 RMT NeoPixel Serial Driver
 */
class RMT_NeoPixel_Serial : public IHardwareDriver
{
  public:
    /**
     * Constructor
     * @param pin GPIO pin for data
     * @param ledCount Number of LEDs
     * @param protocol LED protocol (WS2812, SK6812, etc.)
     */
    RMT_NeoPixel_Serial(uint32_t pin, uint16_t ledCount, LedProtocol protocol);

    /**
     * Destructor
     */
    virtual ~RMT_NeoPixel_Serial();

    // IHardwareDriver interface
    bool init() override;
    void setPolarityOverride(uint8_t mode) override { if (_inst) _inst->polarityOverride = mode; }
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) override;
    bool show() override;
    bool isBusy() override;
    uint32_t getTransferTimeoutUs() const override;
    void clear() override;
    uint16_t getLedCount() const override { return _inst ? _inst->ledCount : 0; }
    LedProtocol getProtocol() const override { return _inst ? _inst->protocol : LedProtocol::WS2812; }
    DriverCapabilities getCapabilities() const override;
    uint8_t* getBuffer() override { return _inst ? _inst->buffer : nullptr; }
    size_t getBufferSize() const override { return _inst ? _inst->bufferSize : 0; }
    bool isInitialized() const override { return _inst ? _inst->initialized : false; }
    DriverImplementation getDriverType() const override { return DriverImplementation::RMT_SERIAL; }

    // Configuration interface
    PhysicalStripConfig* createDefaultConfig() const override;
    bool applyConfig(const PhysicalStripConfig* config) override;

    // ESP32-specific getters for status reporting
    inline rmt_channel_handle_t getRmtChannel() const { return _inst ? _inst->channel : nullptr; }
    inline rmt_encoder_handle_t getRmtEncoder() const { return _inst ? _inst->encoder : nullptr; }

    // IHardwareDriver ColorOrder interface
    void setColorOrder(ColorOrder order) override
    {
        if (_inst) _inst->colorOrder = order;
    }
    ColorOrder getColorOrder() const override { return _inst ? _inst->colorOrder : ColorOrder::RGB; }

    // Static resource detection methods (ESP32 variants)
    static uint32_t getAvailableRmtChannels(); // Count available RMT channels
    static uint32_t getTotalRmtChannels();     // Total RMT channels for current ESP32 target

  private:
    rmt_neopixel_serial_inst_t* _inst;

    void rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww = 0, uint8_t cw = 0);

  public:
    /**
     * Static mapping for callbacks (public for resource detection)
     */
    static RMT_NeoPixel_Serial* _instances[8]; // Max 8 RMT channels
    static void registerInstance(int channel, RMT_NeoPixel_Serial* instance);
    static RMT_NeoPixel_Serial* getInstance(int channel);
};

#endif // ARDUINO_ARCH_ESP32
