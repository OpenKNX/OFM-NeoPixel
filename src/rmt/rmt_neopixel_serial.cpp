/**
 * @file rmt_neopixel_serial.cpp
 * @brief ESP32 RMT NeoPixel Driver Implementation
 */

#if defined(ARDUINO_ARCH_ESP32)

    #include "rmt_neopixel_serial.h"
    #include "../PhysicalStripConfig.h"
    #include <Arduino.h>
    #include <esp_log.h>

    #define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz for RMT

// Static instance mapping
RMT_NeoPixel_Serial* RMT_NeoPixel_Serial::_instances[8] = {nullptr};

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
 * WS2811 Timing for RMT (10MHz Resolution)
 *
 * WS2811 operates at 400kHz (much slower than WS2812):
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

    // Configure TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)_inst->pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .mem_block_symbols = 64, // 64 symbols per block
        .trans_queue_depth = 4,  // 4 transaction queue
        .flags = {
            .invert_out = false,
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

    // Create encoder (bytes encoder for LED data)
    // Select timing based on protocol – WS2811 uses 400kHz, all others use 800kHz
    bool isWS2811 = (_inst->protocol == LedProtocol::WS2811);
    rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = isWS2811 ? ws2811_zero : ws2812_zero,
        .bit1 = isWS2811 ? ws2811_one  : ws2812_one,
        .flags = {
            .msb_first = 1 // MSB first for WS2812/WS2811
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
            .eot_level = 0, // LOW after transfer (reset)
            .queue_nonblocking = 0}};

    // Send data via RMT
    esp_err_t err = rmt_transmit(_inst->channel, _inst->encoder, _inst->buffer, _inst->bufferSize, &tx_config);
    if (err != ESP_OK)
    {
        _inst->busy = false;
        ESP_LOGE("RMT_NeoPixel", "RMT transmit failed: %d", err);
        return false;
    }

    // Wait for completion (blocking for now)
    rmt_tx_wait_all_done(_inst->channel, portMAX_DELAY);
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

    // Apply custom timing if T0H is set.
    // RMT encodes T0H/T0L and T1H/T1L independently via rmt_symbol_word_t.
    // Resolution: 10MHz = 100ns per tick  →  ticks = ns / 100
    uint16_t t0h = serialCfg->getT0H();
    uint16_t t0l = serialCfg->getT0L();
    uint16_t t1h = serialCfg->getT1H();
    uint16_t t1l = serialCfg->getT1L();
    if (t0h > 0 && t0l > 0 && t1h > 0 && t1l > 0 && _inst->initialized)
    {
        // Build custom symbol words (ticks at 10 MHz = 100 ns per tick)
        rmt_symbol_word_t custom_zero = {
            .duration0 = (uint16_t)(t0h / 100),
            .level0    = 1,
            .duration1 = (uint16_t)(t0l / 100),
            .level1    = 0,
        };
        rmt_symbol_word_t custom_one = {
            .duration0 = (uint16_t)(t1h / 100),
            .level0    = 1,
            .duration1 = (uint16_t)(t1l / 100),
            .level1    = 0,
        };

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

        // Update reset time if provided
        // (reset is handled in show() via delay, not via encoder)
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
