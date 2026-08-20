/**
 * @file rmt_neopixel_serial.cpp
 * @brief ESP32 RMT NeoPixel Driver Implementation
 */

#if defined(ARDUINO_ARCH_ESP32)

    #include "rmt_neopixel_serial.h"
    #include "../OneWireTimingProfile.h"
    #include "../PhysicalStripConfig.h"
    #include <Arduino.h>
    #include <esp_log.h>

    #define RMT_LED_STRIP_RESOLUTION_HZ 40000000UL // 40MHz = 25ns RMT ticks
    #define RMT_MAX_DURATION_TICKS 32767U

// Static instance mapping
RMT_NeoPixel_Serial* RMT_NeoPixel_Serial::_instances[8] = {nullptr};

/**
 * Convert a duration to an RMT tick count with round-to-nearest semantics.
 * RMT cannot emit a zero-duration half-symbol, and this driver only supports
 * symbols that fit in the hardware duration field.
 */
static bool rmt_ns_to_ticks(uint32_t durationNs, uint16_t& ticks)
{
    const uint64_t scaled = (uint64_t)durationNs * RMT_LED_STRIP_RESOLUTION_HZ;
    uint64_t rounded = (scaled + 500000000ULL) / 1000000000ULL;
    if (rounded == 0) rounded = 1;
    if (rounded > RMT_MAX_DURATION_TICKS) return false;
    ticks = (uint16_t)rounded;
    return true;
}

/**
 * Quantise custom symbols without allowing the zero and one bit cells to drift
 * apart. The current ETS frequency controls one serial clock, therefore the
 * two symbols must have the same total duration. Keep the requested HIGH pulse
 * as closely as possible and derive the LOW pulse from one rounded target cell.
 */
static bool make_balanced_rmt_symbols(uint16_t t0h, uint16_t t0l,
                                      uint16_t t1h, uint16_t t1l,
                                      rmt_symbol_word_t& zero,
                                      rmt_symbol_word_t& one)
{
    const uint32_t zeroPeriodNs = (uint32_t)t0h + t0l;
    const uint32_t onePeriodNs = (uint32_t)t1h + t1l;
    const uint32_t targetPeriodNs = (zeroPeriodNs + onePeriodNs + 1U) / 2U;

    uint16_t periodTicks = 0;
    uint16_t zeroHighTicks = 0;
    uint16_t oneHighTicks = 0;
    if (!rmt_ns_to_ticks(targetPeriodNs, periodTicks) ||
        !rmt_ns_to_ticks(t0h, zeroHighTicks) ||
        !rmt_ns_to_ticks(t1h, oneHighTicks) ||
        zeroHighTicks >= periodTicks || oneHighTicks >= periodTicks)
    {
        return false;
    }

    zero = {
        .duration0 = zeroHighTicks,
        .level0 = 1,
        .duration1 = (uint16_t)(periodTicks - zeroHighTicks),
        .level1 = 0,
    };
    one = {
        .duration0 = oneHighTicks,
        .level0 = 1,
        .duration1 = (uint16_t)(periodTicks - oneHighTicks),
        .level1 = 0,
    };
    return true;
}

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
 * Check whether RMT DMA should be requested on this target.
 *
 * The classic ESP32 does not support RMT DMA. Other variants/framework
 * combinations may or may not support it, so we still keep a runtime fallback.
 */
static bool should_request_rmt_dma()
{
    #if defined(CONFIG_IDF_TARGET_ESP32)
    return false;
    #else
    return true;
    #endif
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
    _inst->resetTimeUs = getOneWireTimingProfile(protocol).resetTimeUs;

    // Allocate buffer
    _inst->bufferSize = ledCount * _inst->bytesPerLed;
    _inst->buffer = (uint8_t*)malloc(_inst->bufferSize);
    if (_inst->buffer)
    {
        memset(_inst->buffer, 0, _inst->bufferSize);
    }
}

RMT_NeoPixel_Serial::~RMT_NeoPixel_Serial()
{
    if (_inst)
    {
        if (_inst->initialized)
        {
            // Unregister callback
            for (int i = 0; i < 8; i++)
            {
                if (_instances[i] == this)
                {
                    _instances[i] = nullptr;
                }
            }

            // Cleanup RMT resources
            if (_inst->encoder)
            {
                rmt_del_encoder(_inst->encoder);
            }
            if (_inst->channel)
            {
                rmt_del_channel(_inst->channel);
            }
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

    const OneWireTimingProfile& timing = getOneWireTimingProfile(_inst->protocol);

    // Configure TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)_inst->pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .mem_block_symbols = 64, // 64 symbols per block
        .trans_queue_depth = 4,  // 4 transaction queue
        .flags = {
            .invert_out = timing.inverted,
            .with_dma = should_request_rmt_dma(),
        }};

    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &_inst->channel);
    if (err != ESP_OK && tx_chan_config.flags.with_dma)
    {
        ESP_LOGW("RMT_NeoPixel", "RMT DMA unsupported on GPIO %lu, retrying without DMA", (unsigned long)_inst->pin);
        tx_chan_config.flags.with_dma = false;
        err = rmt_new_tx_channel(&tx_chan_config, &_inst->channel);
    }

    if (err != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to create RMT TX channel (err=%d, dma=%s)", err,
                 tx_chan_config.flags.with_dma ? "on" : "off");
        return false;
    }

    // Create an encoder from the selected protocol profile. Its two symbols
    // are quantised together so a payload's bit pattern cannot change its
    // effective serial clock.
    rmt_symbol_word_t zero = {};
    rmt_symbol_word_t one = {};
    if (!make_balanced_rmt_symbols(timing.t0hNs, timing.t0lNs,
                                   timing.t1hNs, timing.t1lNs,
                                   zero, one))
    {
        ESP_LOGE("RMT_NeoPixel", "Invalid built-in timing profile %s", timing.name);
        rmt_del_channel(_inst->channel);
        _inst->channel = nullptr;
        return false;
    }
    rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = zero,
        .bit1 = one,
        .flags = {
            .msb_first = 1 // All supported clockless profiles are MSB first
        }};

    if (rmt_new_bytes_encoder(&encoder_config, &_inst->encoder) != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to create RMT encoder");
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

    uint32_t offset = index * _inst->bytesPerLed;

    switch (_inst->colorOrder)
    {
        case ColorOrder::RGB:
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            break;

        case ColorOrder::GRB:
            _inst->buffer[offset] = g;
            _inst->buffer[offset + 1] = r;
            _inst->buffer[offset + 2] = b;
            break;

        case ColorOrder::BGR:
            _inst->buffer[offset] = b;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = r;
            break;

        case ColorOrder::RGBW:
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = ww; // Use ww as single white
            break;

        case ColorOrder::GRBW:
            _inst->buffer[offset] = g;
            _inst->buffer[offset + 1] = r;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = ww; // Use ww as single white
            break;

        // 5-channel color orders (RGBCCT)
        case ColorOrder::RGBCCT:
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = ww;
            _inst->buffer[offset + 4] = cw;
            break;

        case ColorOrder::GRBCCT:
            _inst->buffer[offset] = g;
            _inst->buffer[offset + 1] = r;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = ww;
            _inst->buffer[offset + 4] = cw;
            break;

        case ColorOrder::RGBCTW:
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = cw; // Cool white first
            _inst->buffer[offset + 4] = ww; // Warm white second
            break;

        case ColorOrder::GRBCTW:
            _inst->buffer[offset] = g;
            _inst->buffer[offset + 1] = r;
            _inst->buffer[offset + 2] = b;
            _inst->buffer[offset + 3] = cw; // Cool white first
            _inst->buffer[offset + 4] = ww; // Warm white second
            break;

        default:
            break;
    }
}

bool RMT_NeoPixel_Serial::show()
{
    if (!_inst || !_inst->initialized || !_inst->buffer) return false;
    if (_inst->busy) return false; // Already transmitting

    _inst->busy = true;

    // TX configuration
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // No loop
        .flags = {
            .eot_level = 0, // Hold LOW after the final data symbol
            .queue_nonblocking = 0}};

    // Send data via RMT
    esp_err_t err = rmt_transmit(_inst->channel, _inst->encoder, _inst->buffer, _inst->bufferSize, &tx_config);
    if (err != ESP_OK)
    {
        _inst->busy = false;
        ESP_LOGE("RMT_NeoPixel", "RMT transmit failed: %d", err);
        return false;
    }

    // Wait for the final data symbol, then hold the line LOW for the complete
    // protocol latch interval. eot_level alone selects the idle level; it does
    // not create elapsed reset time.
    rmt_tx_wait_all_done(_inst->channel, portMAX_DELAY);
    delayMicroseconds(_inst->resetTimeUs);
    _inst->busy = false;

    return true;
}

bool RMT_NeoPixel_Serial::isBusy()
{
    return _inst ? _inst->busy : false;
}

void RMT_NeoPixel_Serial::clear()
{
    if (!_inst || !_inst->buffer) return;
    memset(_inst->buffer, 0, _inst->bufferSize);
}

DriverCapabilities RMT_NeoPixel_Serial::getCapabilities() const
{
    DriverCapabilities caps;
    caps.supportsRGBW = (_inst && _inst->bytesPerLed == 4);
    caps.supportsDMA = true;
    caps.supportsAsync = false; // show() is blocking (rmt_tx_wait_all_done)
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

    // Apply ColorOrder
    _inst->colorOrder = serialCfg->getColorOrder();

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

    // Reset/latch time is independent of the bit symbols. Apply it even when
    // this configuration uses the protocol's default waveform.
    const uint32_t resetUs = serialCfg->getResetTime();
    if (resetUs > 0) _inst->resetTimeUs = resetUs;

    // Apply custom timing if T0H is set.
    // RMT encodes T0H/T0L and T1H/T1L independently via rmt_symbol_word_t.
    // Quantisation uses 40MHz/25ns ticks and preserves one common bit period
    // for zero and one symbols.
    uint16_t t0h = serialCfg->getT0H();
    uint16_t t0l = serialCfg->getT0L();
    uint16_t t1h = serialCfg->getT1H();
    uint16_t t1l = serialCfg->getT1L();
    if (t0h > 0 && t0l > 0 && t1h > 0 && t1l > 0 && _inst->initialized)
    {
        rmt_symbol_word_t custom_zero = {};
        rmt_symbol_word_t custom_one = {};
        if (!make_balanced_rmt_symbols(t0h, t0l, t1h, t1l, custom_zero, custom_one))
        {
            ESP_LOGE("RMT_NeoPixel", "Invalid custom timing %u/%u/%u/%u ns for %luHz RMT",
                     t0h, t0l, t1h, t1l, (unsigned long)RMT_LED_STRIP_RESOLUTION_HZ);
            return false;
        }

        // Recreate the bytes encoder with the new symbols
        if (_inst->encoder)
        {
            rmt_del_encoder(_inst->encoder);
            _inst->encoder = nullptr;
        }
        rmt_bytes_encoder_config_t encoder_config = {
            .bit0  = custom_zero,
            .bit1  = custom_one,
            .flags = { .msb_first = 1 },
        };
        if (rmt_new_bytes_encoder(&encoder_config, &_inst->encoder) != ESP_OK)
        {
            ESP_LOGE("RMT_NeoPixel", "Failed to recreate encoder for custom timing");
            return false;
        }

        // Store in inst for display
        _inst->customT0H = t0h;
        _inst->customT0L = t0l;
        _inst->customT1H = t1h;
        _inst->customT1L = t1l;

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
