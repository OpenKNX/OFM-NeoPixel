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

// ============================================================================
// Static Resource Detection Helpers (ESP32)
// ============================================================================

/**
 * Get total number of RMT channels available on this ESP32 chip
 */
static uint get_total_rmt_channels()
{
    return 4; // ESP32-S3 has 4 RMT channels
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

    // Find free RMT channel (1-7, 0 is reserved for library)
    int channel = -1;

    // Count available channels depending on variant
    int maxChannels = 8; // ESP32 Original

    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    maxChannels = 4;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    maxChannels = 2;
    #endif

    // Search for free channel (starting from 1, because 0 is reserved)
    for (int i = 1; i < maxChannels; i++)
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
            .with_dma = true, // DMA enabled
        }};

    if (rmt_new_tx_channel(&tx_chan_config, &_inst->channel) != ESP_OK)
    {
        ESP_LOGE("RMT_NeoPixel", "Failed to create RMT TX channel");
        return false;
    }

    // Create encoder (bytes encoder for LED data)
    rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = ws2812_zero,
        .bit1 = ws2812_one,
        .flags = {
            .msb_first = 1 // MSB first for WS2812
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
    caps.supportsAsync = true;
    caps.maxFrequency = 400; // ~400Hz update rate
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

    // RMT has no other runtime-configurable settings for serial strips
    // Timing and protocol are fixed at initialization
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
