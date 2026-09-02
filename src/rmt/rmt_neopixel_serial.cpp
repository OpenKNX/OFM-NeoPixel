/**
 * @file rmt_neopixel_serial.cpp
 * @brief ESP32 RMT NeoPixel Driver Implementation
 */

#if defined(ARDUINO_ARCH_ESP32)

    #include "rmt_neopixel_serial.h"
#include "../SerialTimingProfile.h"
    #include "../PhysicalStripConfig.h"
    #include <Arduino.h>
    #include <esp_log.h>

    #define RMT_LED_STRIP_RESOLUTION_HZ 40000000 // 40 MHz = 25 ns per tick (matches NeoPixelBus)
    /// Duration of one RMT tick in ns, derived from the resolution above.
static constexpr uint32_t kRmtTickNs = 1000000000UL / RMT_LED_STRIP_RESOLUTION_HZ;

static constexpr uint8_t sm16825SettingsBytes(LedProtocol protocol)
{
    return protocol == LedProtocol::SM16825 ? 4 : 0;
}

static void writeSm16825Settings(rmt_neopixel_serial_inst_t* inst)
{
    if (!inst || !inst->buffer || inst->protocol != LedProtocol::SM16825) return;
    const size_t offset = (size_t)inst->prefixBytes + (size_t)inst->ledCount * inst->bytesPerLed;
    if (offset + 4 > inst->bufferSize) return;
    for (uint8_t i = 0; i < 4; ++i)
        inst->buffer[offset + i] = ProtocolHelper::sm16825FrameSettingsByte(i);
}

// Static instance mapping
RMT_NeoPixel_Serial* RMT_NeoPixel_Serial::_instances[8] = {nullptr};

// Completion is reported by the RMT ISR. Polling rmt_tx_wait_all_done(chan, 0) instead made
// the IDF log "flush timeout" on every poll of a still-running frame, and a loop that was
// held up elsewhere (effect switch, KNX, flash) came back past the deadline and tore down a
// channel that was transmitting perfectly well.
static bool rmtTxDoneCallback(rmt_channel_handle_t, const rmt_tx_done_event_data_t*, void* user_ctx)
{
    auto* inst = static_cast<rmt_neopixel_serial_inst_t*>(user_ctx);
    if (inst)
    {
        inst->lastTxEndUs = (uint32_t)micros();
        inst->waitingForReset = true;
        inst->busy = false;
    }
    return false; // no higher-priority task woken
}

/**
 * WS2812 Timing for RMT (10MHz Resolution)
 *
 * WS2812 Timing:
 * - 0-Bit: 0.4µs HIGH, 0.85µs LOW
 * - 1-Bit: 0.8µs HIGH, 0.45µs LOW
 * - Reset: >50µs LOW
 *
 * At 10MHz (100ns per tick):
 * - 0.4µs = 4 ticks
 * - 0.85µs = 8-9 ticks (round to 9)
 * - 0.8µs = 8 ticks
 * - 0.45µs = 4-5 ticks (round to 5)
 */
static const rmt_symbol_word_t ws2812_zero = {
    .duration0 = 4, // T0H = 0.4µs
    .level0 = 1,
    .duration1 = 9, // T0L = 0.9µs
    .level1 = 0,
};

static const rmt_symbol_word_t ws2812_one = {
    .duration0 = 8, // T1H = 0.8µs
    .level0 = 1,
    .duration1 = 5, // T1L = 0.5µs
    .level1 = 0,
};

// Reset Symbol (>50µs LOW)
static const rmt_symbol_word_t ws2812_reset = {
    .duration0 = 500, // 50µs
    .level0 = 0,
    .duration1 = 0,
    .level1 = 0,
};

/**
 * WS2811_400KHZ Timing for RMT (10MHz Resolution)
 *
 * The explicit legacy WS2811_400KHZ profile operates at 400kHz (much slower than WS2812):
 * - 0-Bit: 0.5µs HIGH, 2.0µs LOW
 * - 1-Bit: 1.2µs HIGH, 1.3µs LOW
 * - Reset: >50µs LOW
 *
 * At 10MHz (100ns per tick):
 * - 0.5µs = 5 ticks
 * - 2.0µs = 20 ticks
 * - 1.2µs = 12 ticks
 * - 1.3µs = 13 ticks
 */
static const rmt_symbol_word_t ws2811_zero = {
    .duration0 = 5,  // T0H = 0.5µs
    .level0 = 1,
    .duration1 = 20, // T0L = 2.0µs
    .level1 = 0,
};

static const rmt_symbol_word_t ws2811_one = {
    .duration0 = 12, // T1H = 1.2µs
    .level0 = 1,
    .duration1 = 13, // T1L = 1.3µs
    .level1 = 0,
};

// ============================================================================
// Static Resource Detection Helpers (ESP32)
// ============================================================================

/**
 * Get total number of RMT TX channels available on this ESP32 chip
 * ESP32 original:  8 TX channels
 * ESP32-S2/S3:     4 TX channels
 * ESP32-C3/C6:     2 TX channels
 * ESP32-C5:        4 TX channels
 */
static uint get_total_rmt_channels()
{
    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C5)
    return 4;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    return 2;
    #elif defined(CONFIG_IDF_TARGET_ESP32)
    return 8;
    #else
    return 4; // Safe default
    #endif
}

/**
 * WS2805 frames are five bytes per pixel and need substantially more RMT refill
 * bandwidth than the three-byte strips. On DMA-capable targets, reserve DMA for
 * that long frame only; ordinary strips continue to use one hardware memory block
 * each, so a multi-strip ESP32-S3 does not exhaust its RMT channels.
 */
static bool should_request_rmt_dma(LedProtocol protocol)
{
    #if defined(SOC_RMT_SUPPORT_DMA) && SOC_RMT_SUPPORT_DMA
    return protocol == LedProtocol::WS2805_RGBCCT;
    #else
    (void)protocol;
    return false;
    #endif
}

/**
 * In DMA mode mem_block_symbols is a symbol-buffer size, not a hardware block
 * count. Size it for a complete normal WS2805 frame when practical, with a cap
 * that bounds internal DMA RAM for unusually long strips.
 */
static size_t rmt_dma_buffer_symbols(const rmt_neopixel_serial_inst_t* inst)
{
    if (!inst) return 64;

    size_t symbols = inst->bufferSize * 8U;
    if (symbols < 64U) symbols = 64U;       // ESP-IDF documented minimum
    if (symbols > 4096U) symbols = 4096U;   // 16 KiB DMA buffer ceiling
    if (symbols & 1U) ++symbols;            // RMT requires an even size
    return symbols;
}

/**
 * Get number of available (unclaimed) RMT channels
 * Note: RMT driver doesn't expose channel claim state directly,
 * so we track via our static instance array
 */
static uint get_available_rmt_channels()
{
    uint total = get_total_rmt_channels();
    uint used = 0;

    // Count how many channels are in use (have instances)
    for (uint i = 0; i < total && i < 8; i++)
    {
        if (RMT_NeoPixel_Serial::_instances[i] != nullptr)
        {
            used++;
        }
    }

    return total - used;
}

// ============================================================================
// Instance Management
// ============================================================================

void RMT_NeoPixel_Serial::registerInstance(int channel, RMT_NeoPixel_Serial* instance)
{
    if (channel >= 0 && channel < 8)
    {
        _instances[channel] = instance;
    }
}

RMT_NeoPixel_Serial* RMT_NeoPixel_Serial::getInstance(int channel)
{
    if (channel >= 0 && channel < 8)
    {
        return _instances[channel];
    }
    return nullptr;
}

RMT_NeoPixel_Serial::RMT_NeoPixel_Serial(uint32_t pin, uint16_t ledCount, LedProtocol protocol)
    : _inst(nullptr)
{
    _inst = (rmt_neopixel_serial_inst_t*)malloc(sizeof(rmt_neopixel_serial_inst_t));
    if (!_inst) return;

    memset(_inst, 0, sizeof(rmt_neopixel_serial_inst_t));

    _inst->pin = pin;
    _inst->ledCount = ledCount;
    _inst->protocol = protocol;
    _inst->bytesPerLed = ProtocolHelper::getBytesPerLed(protocol);
    _inst->colorOrder = ProtocolHelper::getColorOrder(protocol);
    _inst->channel = nullptr;
    _inst->encoder = nullptr;
    _inst->initialized = false;
    _inst->busy = false;

    const SerialTiming::Profile profile = SerialTiming::profileFor(protocol);
    _inst->resetTimeUs = profile.resetUs;
    _inst->bitPeriodNs = ((uint32_t)profile.t0h + profile.t0l + profile.t1h + profile.t1l + 1U) / 2U;

    // TM1814 expects C1 C2 D1..Dn: two 4-byte constant-current commands, the second the
    // bit complement of the first.
    _inst->prefixBytes = (protocol == LedProtocol::TM1814) ? 8 : 0;

    _inst->bufferSize = (size_t)_inst->prefixBytes + (size_t)ledCount * _inst->bytesPerLed +
                        sm16825SettingsBytes(protocol);
    _inst->buffer = (uint8_t*)malloc(_inst->bufferSize);
    if (_inst->buffer)
    {
        memset(_inst->buffer, 0, _inst->bufferSize);

        if (_inst->prefixBytes == 8)
        {
            // Current level in bits [5:0]; bits 7 and 6 stay 0. Order is W R G B.
            const uint8_t level = ProtocolHelper::tm1814CurrentLevel(180); // 18.0 mA
            for (uint8_t i = 0; i < 4; ++i)
            {
                _inst->buffer[i] = level;
                _inst->buffer[4 + i] = (uint8_t)~level;
            }
        }
        writeSm16825Settings(_inst);
    }
}

RMT_NeoPixel_Serial::~RMT_NeoPixel_Serial()
{
    if (_inst)
    {
        // Release outside the initialized guard: a half-built instance owns handles too.
        for (int i = 0; i < 8; i++)
        {
            if (_instances[i] == this) _instances[i] = nullptr;
        }

        if (_inst->encoder)
        {
            rmt_del_encoder(_inst->encoder);
            _inst->encoder = nullptr;
        }
        if (_inst->channel)
        {
            // rmt_del_channel() refuses an enabled channel; without this disable its
            // memory blocks and GPIO routing stay claimed until reboot.
            esp_err_t derr = rmt_disable(_inst->channel);
            if (derr != ESP_OK && derr != ESP_ERR_INVALID_STATE)
            {
                ESP_LOGW("RMT_NeoPixel", "rmt_disable failed (err=%d)", derr);
            }
            derr = rmt_del_channel(_inst->channel);
            if (derr != ESP_OK)
            {
                ESP_LOGE("RMT_NeoPixel", "rmt_del_channel failed (err=%d) - channel leaked", derr);
            }
            _inst->channel = nullptr;
        }

        if (_inst->buffer)
        {
            free(_inst->buffer);
        }

        free(_inst);
    }
}

bool RMT_NeoPixel_Serial::init()
{
    if (!_inst || !_inst->buffer) return false;
    if (_inst->initialized) return true;

    // Find a free slot in our instance tracking array (0..maxChannels-1)
    int channel = -1;

    // Count available channels depending on variant
    int maxChannels = 8; // ESP32 Original

    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C5)
    maxChannels = 4;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    maxChannels = 2;
    #endif

    // Search for a free software slot in our instance tracking array
    // Note: rmt_new_tx_channel() allocates HW channels automatically (new ESP-IDF 5 API).
    // _instances[] is only used to track how many strips are active and enforce the per-chip limit.
    for (int i = 0; i < maxChannels; i++)
    {
        if (_instances[i] == nullptr)
        {
            channel = i;
            break;
        }
    }

    if (channel < 0)
    {
        ESP_LOGE("RMT_NeoPixel", "No free RMT channels available!");
        return false;
    }

    const bool requestDma = should_request_rmt_dma(_inst->protocol);

    // Configure TX channel. In normal mode this value represents one hardware
    // RMT memory block; in DMA mode it is the DMA symbol-buffer size.
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)_inst->pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        // Normal strips use one hardware block. More than SOC_RMT_MEM_WORDS_PER_CHANNEL
        // in non-DMA mode makes the IDF take the neighbouring channel too.
        .mem_block_symbols = requestDma ? rmt_dma_buffer_symbols(_inst)
                                        : SOC_RMT_MEM_WORDS_PER_CHANNEL,
        .trans_queue_depth = 4,  // 4 transaction queue
        .flags = {
            // Fixed when the channel is created, so PhysicalStrip sets the override first.
            .invert_out = (_inst->polarityOverride == 1) ? false
                        : (_inst->polarityOverride == 2) ? true
                                                         : SerialTiming::profileFor(_inst->protocol).inverted,
            .with_dma = requestDma,
        }};

    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &_inst->channel);
    if (err != ESP_OK && tx_chan_config.flags.with_dma)
    {
        ESP_LOGW("RMT_NeoPixel", "RMT DMA unavailable on GPIO %lu, retrying without DMA", (unsigned long)_inst->pin);
        tx_chan_config.flags.with_dma = false;
        tx_chan_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
        err = rmt_new_tx_channel(&tx_chan_config, &_inst->channel);
    }

    if (err != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to create RMT TX channel (err=%d, dma=%s)", err,
                 tx_chan_config.flags.with_dma ? "on" : "off");
        return false;
    }

    // Encoder symbols come from the protocol profile, the same source the PIO backend
    // uses, so a chip produces one waveform regardless of which backend drives it.
    SerialTiming::Profile prof = SerialTiming::profileFor(_inst->protocol);
    if (prof.t1h == 0) prof = SerialTiming::profileFor(LedProtocol::WS2812B);
    _inst->resetTimeUs = prof.resetUs;

    const SerialTiming::Ticks tk = SerialTiming::toTicks(prof, kRmtTickNs);
    _inst->realizedT0hNs = (uint16_t)(tk.t0h * kRmtTickNs);
    _inst->realizedT0lNs = (uint16_t)(tk.t0l * kRmtTickNs);
    _inst->realizedT1hNs = (uint16_t)(tk.t1h * kRmtTickNs);
    _inst->realizedT1lNs = (uint16_t)(tk.t1l * kRmtTickNs);
    _inst->bitPeriodNs = ((uint32_t)_inst->realizedT0hNs + _inst->realizedT0lNs +
                          _inst->realizedT1hNs + _inst->realizedT1lNs + 1U) / 2U;
    rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = { .duration0 = tk.t0h, .level0 = 1, .duration1 = tk.t0l, .level1 = 0 },
        .bit1 = { .duration0 = tk.t1h, .level0 = 1, .duration1 = tk.t1l, .level1 = 0 },
        .flags = {
            .msb_first = 1
        }};

    if (rmt_new_bytes_encoder(&encoder_config, &_inst->encoder) != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to create RMT encoder");
        rmt_del_channel(_inst->channel);
        return false;
    }

    // Must be registered while the channel is still in the init state.
    rmt_tx_event_callbacks_t tx_callbacks = {.on_trans_done = rmtTxDoneCallback};
    if (rmt_tx_register_event_callbacks(_inst->channel, &tx_callbacks, _inst) != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to register RMT TX callbacks");
        rmt_del_encoder(_inst->encoder);
        rmt_del_channel(_inst->channel);
        return false;
    }

    // Enable RMT
    if (rmt_enable(_inst->channel) != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to enable RMT channel");
        rmt_del_encoder(_inst->encoder);
        rmt_del_channel(_inst->channel);
        return false;
    }

    // Register Instanz
    registerInstance(channel, this);

    _inst->initialized = true;
    return true;
}

bool RMT_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;

    rgbToBuffer(index, r, g, b, 0, 0);
    return true;
}

bool RMT_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;
    if (_inst->bytesPerLed < 4) return false; // Not RGBW

    rgbToBuffer(index, r, g, b, w, 0);
    return true;
}

/**
 * @brief Sets RGBCCT color values for an LED (5-channel)
 *
 * Stores RGBCCT color values for a specific LED in the internal buffer.
 * Only available for LED types with dual white channels (warm + cool white).
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param ww Warm White component (0-255)
 * @param cw Cool White component (0-255)
 * @return true if successful, false if index invalid or not an RGBCCT strip
 */
bool RMT_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;
    if (_inst->bytesPerLed < 5) return false; // Not RGBCCT

    rgbToBuffer(index, r, g, b, ww, cw);
    return true;
}

void RMT_NeoPixel_Serial::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_inst || !_inst->buffer) return;

    const size_t offset = (size_t)_inst->prefixBytes + (size_t)index * _inst->bytesPerLed;
    if (offset + _inst->bytesPerLed > _inst->bufferSize) return; // the PIO twin bounds-checks too

    // Swap runs on the logical components, before the colour order is applied.
    ProtocolHelper::applyChannelSwap(_inst->channelSwap, r, g, b, ww, cw);

    uint8_t ch[6] = {0};
    uint8_t count = ProtocolHelper::orderChannels(_inst->colorOrder, r, g, b, ww, cw, ch);

    const uint8_t bits = ProtocolHelper::getBitsPerChannel(_inst->protocol);
    const uint8_t chBytes = (uint8_t)(bits / 8);
    if (count * chBytes > _inst->bytesPerLed) count = (uint8_t)(_inst->bytesPerLed / chBytes);

    if (bits == 16)
    {
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint16_t v = (uint16_t)(ch[i] * 257);
            _inst->buffer[offset + i * 2] = (uint8_t)(v >> 8);
            _inst->buffer[offset + i * 2 + 1] = (uint8_t)(v & 0xFF);
        }
    }
    else
    {
        for (uint8_t i = 0; i < count; ++i)
            _inst->buffer[offset + i] = ch[i];
    }
}

bool RMT_NeoPixel_Serial::show()
{
    if (!_inst || !_inst->initialized || !_inst->buffer) return false;
    if (_inst->busy || _inst->waitingForReset) return false; // Still transmitting or latching

    _inst->busy = true;
    _inst->transferStartedUs = micros();

    // TX configuration
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // No loop
        .flags = {
            .eot_level = 0, // LOW after transfer (reset)
            .queue_nonblocking = 0}};

    // Send data via RMT. The call queues the transfer and returns; waiting happens in
    // isBusy() so several strips overlap instead of running one after another.
    esp_err_t err = rmt_transmit(_inst->channel, _inst->encoder, _inst->buffer, _inst->bufferSize, &tx_config);
    if (err != ESP_OK)
    {
        _inst->busy = false;
        ESP_LOGE("RMT_NeoPixel", "RMT transmit failed: %d", err);
        return false;
    }

    return true;
}

bool RMT_NeoPixel_Serial::isBusy()
{
    if (!_inst || !_inst->initialized) return false;

    if (_inst->busy)
    {
        // rmtTxDoneCallback() clears busy, so still being set past the deadline means the
        // channel really is stuck rather than merely slow.
        if ((uint32_t)(micros() - _inst->transferStartedUs) < getTransferTimeoutUs()) return true;

        // Recover a stalled peripheral without changing the asynchronous
        // model. The active transaction is terminated, the data line is
        // held LOW for a real reset interval, then the channel is made
        // available for the next frame.
        const esp_err_t disableErr = rmt_disable(_inst->channel);
        gpio_set_level((gpio_num_t)_inst->pin, 0);
        delayMicroseconds(_inst->resetTimeUs);
        const esp_err_t enableErr = rmt_enable(_inst->channel);
        _inst->busy = false;
        _inst->waitingForReset = false;
        if (enableErr != ESP_OK) _inst->initialized = false;
        ESP_LOGE("RMT_NeoPixel", "RMT transfer stalled (disable=%d, enable=%d)",
                 disableErr, enableErr);
        return false;
    }

    if (_inst->waitingForReset)
    {
        if ((uint32_t)(micros() - _inst->lastTxEndUs) < _inst->resetTimeUs) return true;
        _inst->waitingForReset = false;
    }

    return false;
}

uint32_t RMT_NeoPixel_Serial::getTransferTimeoutUs() const
{
    if (!_inst || _inst->bufferSize == 0) return 1000000U;

    const uint32_t bitPeriodNs = _inst->bitPeriodNs ? _inst->bitPeriodNs : 1250U;
    const uint64_t payloadUs = ((uint64_t)_inst->bufferSize * 8ULL * bitPeriodNs + 999ULL) / 1000ULL;
    const uint64_t deadlineUs = payloadUs + _inst->resetTimeUs + 2000ULL;
    return deadlineUs > UINT32_MAX ? UINT32_MAX : (uint32_t)deadlineUs;
}

void RMT_NeoPixel_Serial::clear()
{
    if (!_inst || !_inst->buffer) return;
    memset(_inst->buffer, 0, _inst->bufferSize);
    writeSm16825Settings(_inst);
}

DriverCapabilities RMT_NeoPixel_Serial::getCapabilities() const
{
    DriverCapabilities caps;
    caps.supportsRGBW = (_inst && _inst->bytesPerLed >= 4);
    caps.supportsRGBCCT = (_inst && ProtocolHelper::isRGBCCT(_inst->protocol));
    caps.supportsDMA = true;
    caps.supportsAsync = true; // show() queues; isBusy() polls completion and latch time
    caps.maxFrequency = 400;    // ~400Hz update rate
    caps.maxLeds = 2000;
    return caps;
}

// ============================================================================
// Configuration Interface
// ============================================================================

PhysicalStripConfig* RMT_NeoPixel_Serial::createDefaultConfig() const
{
    if (!_inst) return nullptr;
    return new SerialStripConfig();
}

bool RMT_NeoPixel_Serial::applyConfig(const PhysicalStripConfig* config)
{
    if (!_inst || !config) return false;

    // Type check - must be SerialStripConfig
    const SerialStripConfig* serialCfg = config->isSerialConfig() ? static_cast<const SerialStripConfig*>(config) : nullptr;
    if (!serialCfg) return false;

    // Never let a NONE config stomp the live driver order: that turns the strip into
    // wrong colours rather than leaving it alone.
    _inst->channelSwap = serialCfg->getChannelSwap();

    // The RMT channel fixes invert_out at creation; a later change needs a restart.
    if (serialCfg->getSignalPolarity() != _inst->polarityOverride)
    {
        _inst->polarityOverride = serialCfg->getSignalPolarity();
        ESP_LOGW("RMT_NeoPixel", "GPIO%lu: polarity change takes effect after restart",
                 (unsigned long)_inst->pin);
    }
    const ColorOrder cfgOrder = serialCfg->getColorOrder();
    if (cfgOrder != ColorOrder::NONE) _inst->colorOrder = cfgOrder;

    // Apply level-shifter GPIO optimizations
    _inst->levelShifterType = serialCfg->getLevelShifter();
    if (_inst->levelShifterType == LevelShifterType::TXS0108E)
    {
        // TXS0108E auto-direction requires strong A-side edges and no resistive load.
        // Boost drive capability to 40 mA for sharpest rising/falling edges.
        gpio_set_drive_capability((gpio_num_t)_inst->pin, GPIO_DRIVE_CAP_3);
        // Disable internal pull-up/down to avoid interfering with direction-detect RC.
        gpio_set_pull_mode((gpio_num_t)_inst->pin, GPIO_FLOATING);
        ESP_LOGI("RMT_NeoPixel", "GPIO%lu: TXS0108E mode - drive=40mA, pull=FLOAT", (unsigned long)_inst->pin);
    }
    else if (_inst->levelShifterType == LevelShifterType::SN74HCT125 ||
             _inst->levelShifterType == LevelShifterType::SN74AHCT125)
    {
        // 74HCT/AHCT: unidirectional transparent buffer; OE=GND, always enabled.
        // TTL input threshold VIH >= 2.0V is satisfied by 3.3V output; no GPIO changes needed.
        // Default 20mA drive (GPIO_DRIVE_CAP_2) is more than sufficient for a logic input.
        ESP_LOGI("RMT_NeoPixel", "GPIO%lu: %s mode - no GPIO changes needed",
                 (unsigned long)_inst->pin,
                 (_inst->levelShifterType == LevelShifterType::SN74AHCT125) ? "74AHCT125" : "74HCT125");
    }

    // Reset/latch time is independent of the bit timing, so apply it either way.
    const uint32_t cfgResetUs = serialCfg->getResetTime();
    if (cfgResetUs > 0) _inst->resetTimeUs = cfgResetUs;

    // Ticks derive from ONE rounded bit period, so 0-bit and 1-bit last equally long;
    // rounding the four values separately lets the halves land on different periods.
    const uint16_t t0h = serialCfg->getT0H();
    const uint16_t t0l = serialCfg->getT0L();
    const uint16_t t1h = serialCfg->getT1H();
    const uint16_t t1l = serialCfg->getT1L();
    if (t0h > 0 && t0l > 0 && t1h > 0 && t1l > 0 && _inst->initialized)
    {
        SerialTiming::Profile custom = { t0h, t0l, t1h, t1l,
                                         cfgResetUs ? cfgResetUs : _inst->resetTimeUs,
                                         SerialTiming::profileFor(_inst->protocol).inverted };
        const SerialTiming::Ticks tk = SerialTiming::toTicks(custom, kRmtTickNs);

        rmt_bytes_encoder_config_t encoder_config = {
            .bit0  = { .duration0 = tk.t0h, .level0 = 1, .duration1 = tk.t0l, .level1 = 0 },
            .bit1  = { .duration0 = tk.t1h, .level0 = 1, .duration1 = tk.t1l, .level1 = 0 },
            .flags = { .msb_first = 1 },
        };

        // Build the replacement before dropping the old one: a failed rebuild used to
        // leave the encoder NULL while initialized stayed true.
        rmt_encoder_handle_t rebuilt = nullptr;
        if (rmt_new_bytes_encoder(&encoder_config, &rebuilt) != ESP_OK)
        {
            ESP_LOGE("RMT_NeoPixel", "Failed to recreate encoder for custom timing, keeping previous");
            return false;
        }
        if (_inst->encoder) rmt_del_encoder(_inst->encoder);
        _inst->encoder = rebuilt;

        _inst->customT0H = t0h;
        _inst->customT0L = t0l;
        _inst->customT1H = t1h;
        _inst->customT1L = t1l;

        _inst->realizedT0hNs = (uint16_t)(tk.t0h * kRmtTickNs);
        _inst->realizedT0lNs = (uint16_t)(tk.t0l * kRmtTickNs);
        _inst->realizedT1hNs = (uint16_t)(tk.t1h * kRmtTickNs);
        _inst->realizedT1lNs = (uint16_t)(tk.t1l * kRmtTickNs);
        _inst->bitPeriodNs = ((uint32_t)_inst->realizedT0hNs + _inst->realizedT0lNs +
                              _inst->realizedT1hNs + _inst->realizedT1lNs + 1U) / 2U;
    }

    return true;
}

// ============================================================================
// Static Resource Detection Methods (Public API)
// ============================================================================

uint32_t RMT_NeoPixel_Serial::getAvailableRmtChannels()
{
    return get_available_rmt_channels();
}

uint32_t RMT_NeoPixel_Serial::getTotalRmtChannels()
{
    return get_total_rmt_channels();
}
#endif // ARDUINO_ARCH_ESP32
