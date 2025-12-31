#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_serial.h"
    #include "../PhysicalStripConfig.h"
    #include "OpenKNX.h"
    #include "pio_dma_shared.h"
    #include <Arduino.h>

// =============================================================================
// WS2812B PIO PROGRAM (MSB FIRST!)
// =============================================================================
//
// WS2812B Protocol Timing:
// - 0-bit: 350ns HIGH (T0H) + 800ns LOW (T0L) = 1.15µs
// - 1-bit: 700ns HIGH (T1H) + 600ns LOW (T1L) = 1.30µs
//
// PIO Implementation @ 800kHz bit rate using 10 cycles per bit:
// - PIO clock = 125MHz / 15.625 = 8MHz (125ns per cycle)
//
// WS2812B MSB-first timing (starts with HIGH):
// - 0-bit: 3 cycles HIGH + 7 cycles LOW  = 375ns + 875ns ≈ 1.25µs
// - 1-bit: 6 cycles HIGH + 4 cycles LOW  = 750ns + 500ns ≈ 1.25µs
//
// PIO Assembly Program (adapted from Raspberry Pi Pico SDK):
//
// .wrap_target
// 0: out x, 1      side 1 [2]   ; Pull 1 bit, set HIGH, wait 2 (= 3 cycles HIGH)
// 1: jmp !x, do_0  side 1 [2]   ; If bit=0 jump, else 3 more HIGH (total 6)
// 2: jmp finish    side 0 [3]   ; 1-bit: Set LOW, wait 3 (total 4 cycles LOW)
// do_0:
// 3: nop           side 0 [6]   ; 0-bit: Set LOW, wait 6 (total 7 cycles LOW)
// finish:
// .wrap
//
// =============================================================================

    #define ws2812_wrap_target 0 // Wrap target instruction index
    #define ws2812_wrap 3        // Wrap instruction index

static const uint16_t ws2812b_program_instructions[] = {
    0x6221, //  0: out    x, 1            side 1 [2]  ; Pull bit, HIGH, wait 2
    0x1323, //  1: jmp    !x, 3           side 1 [2]  ; If 0, jump to 3, HIGH 2 more
    0x1003, //  2: jmp    0               side 0 [3]  ; 1-bit: LOW 4 cycles, loop
    0xa246, //  3: nop                    side 0 [6]  ; 0-bit: LOW 7 cycles, loop
};

static const pio_program_t neopixel_serial_program = {
    .instructions = ws2812b_program_instructions,
    .length = 4,
    .origin = -1,
};

// Static member initialization. Our DMA handlers array.
PIO_NeoPixel_Serial* PIO_NeoPixel_Serial::_dmaHandlers[12] = {nullptr};

/**
 * Initialize NeoPixel PIO program
 *
 * Works for all protocols (WS2812B @ 800kHz, WS2811 @ 400kHz, etc.)
 * Timing is controlled via clock divider parameter
 */
static inline void neopixel_serial_program_init(PIO pio, uint sm, uint offset, uint pin, float clkdiv, bool rgbw)
{
    // ===== GPIO OPTIMIZATION FOR HIGH-SPEED SERIAL (800 kHz - 1.25 MHz) =====

    // 1. Set GPIO drive strength and slew rate FIRST (persist through pio_gpio_init)
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);

    // 2. Transfer pin to PIO control (before setting state!)
    pio_gpio_init(pio, pin);

    // 3. Set pin as output under PIO control
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812_wrap_target, offset + ws2812_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);

    // Autopull: 24 bits for RGB, 32 bits for RGBW
    // WS2812B needs MSB-first! shift_right=FALSE (shift left = MSB first!)
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // Use full TX FIFO

    sm_config_set_clkdiv(&c, clkdiv);

    // 4. Set initial pin state to LOW (WS2812B reset state)
    //    CRITICAL: Must be BEFORE pio_sm_init() and AFTER pio_gpio_init()
    pio_sm_set_pins_with_mask(pio, sm, 0, (1u << pin));

    // 5. Initialize and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "PIO Init: pin=%u, SM=%u, offset=%u, clkdiv=%.2f, RGBW=%d",
                                          pin, sm, offset, clkdiv, rgbw);
    #endif
}

// ========== RESOURCE DETECTION (STATIC HELPERS) ==========

/**
 * @brief Get available PIO state machines count
 * @return Number of free state machines across all PIOs
 */
uint PIO_NeoPixel_Serial::getAvailableStateMachines()
{
    uint count = 0;
    PIO pios[] = {
        pio0,
        pio1,
    #ifdef PICO_RP2350
        pio2
    #endif
    };

    for (PIO pio : pios)
    {
        if (pio == nullptr) continue;
        for (uint sm = 0; sm < 4; sm++)
        {
            if (!pio_sm_is_claimed(pio, sm))
            {
                count++;
            }
        }
    }

    return count;
}

/**
 * @brief Get available DMA channels count
 * @return Number of free DMA channels
 */
uint PIO_NeoPixel_Serial::getAvailableDmaChannels()
{
    uint available = 0;
    for (uint ch = 0; ch < NUM_DMA_CHANNELS; ch++)
    {
        if (!dma_channel_is_claimed(ch))
        {
            available++;
        }
    }
    return available;
}

/**
 * @brief Creates a new PIO NeoPixel driver instance
 *
 * Configures a PIO-based NeoPixel driver with DMA support.
 * The driver supports various LED protocols like WS2812B and SK6812.
 *
 * Resources per strip:
 * - 1 PIO state machine for precise timing
 * - 1 DMA channel (optional) for CPU-free transfers
 * - Buffer for LED data (3-4 bytes per LED)
 *
 * @param pin GPIO pin for data line
 * @param ledCount Number of LEDs in strip
 * @param protocol LED protocol (WS2812, SK6812, etc.)
 * @param useDMA Enable DMA transfers (default: true)
 *
 * @note Constructor only allocates memory, call init() to
 *       configure hardware resources.
 */
PIO_NeoPixel_Serial::PIO_NeoPixel_Serial(uint pin, uint16_t ledCount, LedProtocol protocol, bool useDMA, TimingMode timingMode)
    : _inst(nullptr)
{
    _inst = new pio_neopixel_serial_inst_t();
    if (!_inst) return;

    memset(_inst, 0, sizeof(pio_neopixel_serial_inst_t));

    _inst->pin = pin;               // Set GPIO pin for data line
    _inst->ledCount = ledCount;     // Set number of LEDs
    _inst->protocol = protocol;     // Set LED protocol
    _inst->useDMA = useDMA;         // Set DMA usage
    _inst->timingMode = timingMode; // Set timing mode for clock divider calculation
    _inst->dmaChannel = -1;         // No DMA channel yet
    _inst->dmaIrqNum = -1;          // Will be claimed dynamically if DMA is used
    _inst->initialized = false;     // Set initialization state
    _inst->busy = false;            // Set busy state

    // Determine bytes per LED and color order
    _inst->bytesPerLed = ProtocolHelper::getBytesPerLed(protocol);
    _inst->colorOrder = ProtocolHelper::getColorOrder(protocol);
    _inst->frequency = ProtocolHelper::getDefaultFrequency(protocol);

    // Allocate buffers
    // CRITICAL: Cast to size_t to prevent overflow (e.g., 22000 * 3 = 66000 > uint16_t max 65535)
    _inst->bufferSize = (size_t)ledCount * _inst->bytesPerLed;
    _inst->buffer = new uint8_t[_inst->bufferSize];

    if (_inst->buffer)
    {
        memset(_inst->buffer, 0, _inst->bufferSize);
    }

    // DMA buffer (if needed)
    // For DMA: Each pixel needs ceil(bytesPerLed/4) uint32_t entries
    // RGB (3 bytes): 1 pixel = 1 uint32_t (packed)
    // RGBW (4 bytes): 1 pixel = 1 uint32_t (perfect fit)
    if (useDMA)
    {
        // Calculate DMA buffer size: ledCount pixels, rounded up for partial uint32_t
        // CRITICAL: Cast to size_t to prevent overflow
        _inst->dmaBufferSize = (size_t)ledCount * (((_inst->bytesPerLed + 3) / 4));
        _inst->dmaBuffer = new uint32_t[_inst->dmaBufferSize];
        if (_inst->dmaBuffer)
        {
            memset(_inst->dmaBuffer, 0, _inst->dmaBufferSize * sizeof(uint32_t));
        }
    }
    else
    {
        _inst->dmaBuffer = nullptr;
        _inst->dmaBufferSize = 0;
    }
}

/**
 * @brief Destroy the pio neopixel serial::pio neopixel serial object
 */
PIO_NeoPixel_Serial::~PIO_NeoPixel_Serial()
{
    if (_inst)
    {
        if (_inst->initialized)
        {
            // Disable state machine
            if (_inst->pio && _inst->sm < 4)
            {
                pio_sm_set_enabled(_inst->pio, _inst->sm, false);
            }

            // Free DMA (IRQ is shared, don't release it)
            if (_inst->dmaChannel >= 0)
            {
                unregisterDMAHandler(_inst->dmaChannel);
                dma_channel_unclaim(_inst->dmaChannel);
            }

            // Remove program and unclaim SM (matches pio_claim_free_sm_and_add_program)
            if (_inst->pio && _inst->sm < 4)
            {
                pio_remove_program_and_unclaim_sm(
                    &neopixel_serial_program, // Same program for all protocols
                    _inst->pio,
                    _inst->sm,
                    _inst->offset);
            }
        }

        // Free buffers
        if (_inst->dmaBuffer)
        {
            delete[] _inst->dmaBuffer;
            _inst->dmaBuffer = nullptr;
        }
        if (_inst->buffer)
        {
            delete[] _inst->buffer;
            _inst->buffer = nullptr;
        }

        delete _inst;
        _inst = nullptr;
    }
}

// =============================================================================
// Configuration Management (IHardwareDriver Interface)
// =============================================================================

/**
 * @brief Create default Serial strip configuration
 * @return New SerialStripConfig with driver-specific defaults
 */
PhysicalStripConfig* PIO_NeoPixel_Serial::createDefaultConfig() const
{
    SerialStripConfig* cfg = new SerialStripConfig();

    // Set defaults from current instance (if available)
    if (_inst)
    {
        cfg->setColorOrder(_inst->colorOrder);
        // TimingMode is in PIO instance, but SerialStripConfig uses custom timing
        // For now, just set default timing mode
        cfg->setTimingMode(TimingMode::AUTO);
    }

    return cfg;
}

/**
 * @brief Apply configuration to driver
 * @param config SerialStripConfig to apply
 * @return true if applied successfully
 */
bool PIO_NeoPixel_Serial::applyConfig(const PhysicalStripConfig* config)
{
    if (!_inst || !config) return false;

    // Type check - must be SerialStripConfig
    const SerialStripConfig* serialCfg = dynamic_cast<const SerialStripConfig*>(config);
    if (!serialCfg) return false;

    // Apply ColorOrder
    _inst->colorOrder = serialCfg->getColorOrder();

    // Timing parameters could be applied here if needed
    // For now, timing is set at init() based on TimingMode
    // Future: add custom timing support (t0h, t0l, t1h, t1l, resetTime)

    return true;
}

/**
 * @brief Initializes the NeoPixel driver
 *
 * Sets up hardware resources for the LED strip:
 * 1. Configures PIO state machine with correct timing
 * 2. Initializes DMA if enabled (optional)
 * 3. Sets GPIO pin as output
 *
 * @return true if initialization successful, false if resources unavailable
 *
 * @note Falls back to normal transfer mode on DMA failure
 */
bool PIO_NeoPixel_Serial::init()
{
    if (!_inst || !_inst->buffer) return false;
    if (_inst->initialized) return true;

    // Initialize PIO
    if (!initPIO())
    {
        return false;
    }

    // Initialize DMA if requested
    if (_inst->useDMA)
    {
        if (!initDMA())
        {
            // Fall back to non-DMA mode
            _inst->useDMA = false;
        }
    }

    _inst->initialized = true;
    return true;
}

/**
 * @brief Initializes PIO state machine for NeoPixel protocol
 *
 * Configures PIO hardware for the selected LED protocol:
 * 1. Loads WS2812B/SK6812 timing program
 * 2. Calculates clock divider for correct timing
 * 3. Configures GPIO and FIFO settings
 *
 * Timing calculation:
 * - 10 PIO cycles per bit
 * - PIO clock = system clock / divider
 * - Divider = system clock / (freq * 10)
 *
 * Examples:
 * - WS2812B @ 800kHz → 125MHz / (800kHz * 10) = 15.625
 * - WS2811 @ 400kHz → 125MHz / (400kHz * 10) = 31.25
 *
 * @return true if PIO configured, false on error
 */
bool PIO_NeoPixel_Serial::initPIO()
{
    if (!_inst) return false;

    bool rgbw = (_inst->bytesPerLed == 4);

    // Calculate clock divider based on frequency and timing mode
    // Timing: 10 PIO cycles per bit (fixed in PIO program)
    // PIO clock = sys_clock / clkdiv
    // Need: frequency * 10 cycles = PIO clock
    // clkdiv = sys_clock / (frequency * 10)
    //
    // NOTE: RP2040 runs at 125 MHz, RP2350 at 150 MHz by default
    // The timing modes are overclock-safe: they work at any system clock speed
    //
    // AUTO_LEGACY: Optimized for WS2812C/D onboard LEDs
    // Targets 960 kHz bitrate which works better for newer LED chips that prefer
    // slightly faster timing than standard 800 kHz. Automatically calculates the
    // correct clkdiv for any CPU frequency to maintain consistent 960 kHz output.
    float actual_sys_clk = (float)clock_get_hz(clk_sys);
    float clkdiv = 1.0f;         // Initialize to safe default
    float actual_bitrate = 0.0f; // Initialize to safe default
    float bitrate_multiplier = 1.0f;
    const char* mode_name = "AUTO";

    switch (_inst->timingMode)
    {
        case TimingMode::AUTO_LEGACY:
            // Target 960 kHz for WS2812C/D (adapts to any CPU frequency)
            clkdiv = actual_sys_clk / (960000.0f * 10.0f);
            actual_bitrate = (actual_sys_clk / clkdiv) / 10.0f;
            mode_name = "AUTO_LEGACY";
            break;

        case TimingMode::SLOW_20PCT:
            bitrate_multiplier = 0.80f;
            mode_name = "SLOW_20PCT";
            break;
        case TimingMode::SLOW_15PCT:
            bitrate_multiplier = 0.85f;
            mode_name = "SLOW_15PCT";
            break;
        case TimingMode::SLOW_10PCT:
            bitrate_multiplier = 0.90f;
            mode_name = "SLOW_10PCT";
            break;
        case TimingMode::SLOW_5PCT:
            bitrate_multiplier = 0.95f;
            mode_name = "SLOW_5PCT";
            break;

        case TimingMode::FAST_5PCT:
            bitrate_multiplier = 1.05f;
            mode_name = "FAST_5PCT";
            break;
        case TimingMode::FAST_10PCT:
            bitrate_multiplier = 1.10f;
            mode_name = "FAST_10PCT";
            break;
        case TimingMode::FAST_15PCT:
            bitrate_multiplier = 1.15f;
            mode_name = "FAST_15PCT";
            break;
        case TimingMode::FAST_20PCT:
            bitrate_multiplier = 1.20f;
            mode_name = "FAST_20PCT";
            break;
        case TimingMode::FAST_25PCT:
            bitrate_multiplier = 1.25f;
            mode_name = "FAST_25PCT";
            break;

        case TimingMode::AUTO:
        default:
            bitrate_multiplier = 1.0f;
            mode_name = "AUTO";
            break;
    }

    // Calculate clkdiv for all modes except AUTO_LEGACY (which has custom calculation)
    if (_inst->timingMode != TimingMode::AUTO_LEGACY)
    {
        float target_bitrate = (float)_inst->frequency * bitrate_multiplier;
        clkdiv = actual_sys_clk / (target_bitrate * 10.0f);
        actual_bitrate = target_bitrate;
    }

    // Store actual values for debugging/info display
    _inst->actual_bitrate = actual_bitrate;
    _inst->actual_clkdiv = clkdiv;

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "GPIO%u: sys_clk=%.0f MHz, clkdiv=%.3f, bitrate=%.0f kHz [%s]",
                                          _inst->pin, actual_sys_clk / 1e6f, clkdiv, actual_bitrate / 1000.0f, mode_name);
    #endif

    #if PICO_PIO_USE_GPIO_BASE
    // RP2350B and later: Use GPIO base support for flexible pin assignment
    PIO pio_temp = nullptr;
    uint sm_temp = 0;
    uint offset_temp = 0;

    // For single-pin output, gpio_count = 1
    uint32_t gpio_base = _inst->pin;
    uint32_t gpio_count = 1;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &neopixel_serial_program, // PIO program (same for all protocols)
        &pio_temp,                // Return PIO instance
        &sm_temp,                 // Return state machine index
        &offset_temp,             // Return program offset
        gpio_base,                // Base GPIO pin
        gpio_count,               // Number of GPIOs needed
        true                      // Allow setting GPIO base if needed
    );

    if (!success)
    {
        openknx.logger.logWithPrefix("PIO NeoPixel Serial", "Failed to claim PIO SM (GPIO base method)");
        return false;
    }

    _inst->pio = pio_temp;
    _inst->sm = sm_temp;
    _inst->offset = offset_temp;

    #else
    // RP2040 and earlier: Use standard method
    PIO pio_temp = nullptr;
    uint sm_temp = 0;
    uint offset_temp = 0;

    bool success = pio_claim_free_sm_and_add_program(
        &neopixel_serial_program, // PIO program (same for all protocols)
        &pio_temp,                // Return PIO instance
        &sm_temp,                 // Return state machine index
        &offset_temp              // Return program offset
    );

    if (!success)
    {
        openknx.logger.logWithPrefix("PIO NeoPixel Serial", "Failed to claim PIO SM");
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "Available SMs: %u", getAvailableStateMachines());
        return false;
    }

    _inst->pio = pio_temp;
    _inst->sm = sm_temp;
    _inst->offset = offset_temp;

    #endif

    // CRITICAL: Clear and reset state machine before init (prevents garbage after reboot!)
    pio_sm_set_enabled(_inst->pio, _inst->sm, false); // Disable first
    pio_sm_restart(_inst->pio, _inst->sm);            // Reset PC to start
    pio_sm_clear_fifos(_inst->pio, _inst->sm);        // Clear any old data
    pio_sm_clkdiv_restart(_inst->pio, _inst->sm);     // Reset clock divider

    // Initialize the PIO program with calculated clock divider
    neopixel_serial_program_init(_inst->pio, _inst->sm, _inst->offset, _inst->pin, clkdiv, rgbw);

    #ifdef OPENKNX_DEBUG
    const char* pioName = (_inst->pio == pio0) ? "PIO0" : (_inst->pio == pio1) ? "PIO1"
                                                                               : "PIO2";
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "NeoPixel PIO: %s/SM%d, GPIO%d, %.0fkHz, %s",
                                          pioName, _inst->sm, _inst->pin, (float)_inst->frequency / 1000.0f, rgbw ? "RGBW" : "RGB");
    #endif

    return true;
}

/**
 * @brief Configures DMA for NeoPixel transfers
 *
 * Sets up DMA channel for automatic data transfer:
 * 1. Claims a free DMA channel
 * 2. Configures for 32-bit word transfers to PIO FIFO
 * 3. Enables IRQ0 for transfer completion
 *
 * DMA configuration:
 * - 32-bit word transfers for efficient transfer
 * - Source pointer increments (buffer)
 * - Destination pointer fixed (PIO FIFO)
 * - DREQ from PIO controls transfer timing
 *
 * @note All serial strips share IRQ0
 * @return true if DMA configured, false if resources unavailable
 */
bool PIO_NeoPixel_Serial::initDMA()
{
    if (!_inst || !_inst->dmaBuffer) return false; // Need DMA buffer!

    // Claim DMA channel
    int channel = dma_claim_unused_channel(false);
    if (channel < 0)
    {
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "ERROR: No DMA channel available!");
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "Available DMA channels: %u", getAvailableDmaChannels());
        return false;
    }

    _inst->dmaChannel = channel;
    _inst->dmaIrqNum = 0; // ALL Serial strips use DMA_IRQ_0

    // Configure DMA for 32-bit word transfers!
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);                 // 32-bit transfers!
    channel_config_set_read_increment(&c, true);                            // Increment read address
    channel_config_set_write_increment(&c, false);                          // Write always to same FIFO
    channel_config_set_dreq(&c, pio_get_dreq(_inst->pio, _inst->sm, true)); // PIO TX DREQ

    dma_channel_configure(
        channel,
        &c,
        &_inst->pio->txf[_inst->sm], // Write to PIO TX FIFO (32-bit)
        _inst->dmaBuffer,            // Read from 32-bit word buffer
        _inst->dmaBufferSize,        // Transfer count in 32-bit words
        false                        // Don't start yet
    );

    // Register interrupt handler in global registry
    registerDMAHandler(channel, this);

    // ALL Serial strips use DMA_IRQ_0 (shared IRQ for all Serial types)
    dma_channel_set_irq0_enabled(channel, true);

    static bool irq0_initialized = false;
    if (!irq0_initialized)
    {
        irq_set_exclusive_handler(DMA_IRQ_0, unifiedDmaIRQHandler);
        irq_set_enabled(DMA_IRQ_0, true);
        irq0_initialized = true;
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefix("PIO NeoPixel Serial", "DMA_IRQ_0 initialized (unified handler for ALL Serial strips)");
    #endif
    }

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "DMA initialized - Channel=%d, IRQ=0 (shared), Words=%u, Target=PIO%d SM%d",
                                          channel, (uint32_t)_inst->dmaBufferSize, _inst->pio == pio0 ? 0 : 1, _inst->sm);
    #endif

    return true;
}

/**
 * @brief Sets RGB color values for an LED
 *
 * Stores RGB color values for a specific LED in the internal buffer.
 * The order is adjusted according to the LED protocol.
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return true if successful, false if index invalid
 */
bool PIO_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    if (_inst->busy) return false;

    rgbToBuffer(index, r, g, b, 0, 0);
    return true;
}

/**
 * @brief Sets RGBW color values for an LED
 *
 * Stores RGBW color values for a specific LED in the internal buffer.
 * Only available for LED types with additional white channel.
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 * @return true if successful, false if index invalid or not an RGBW strip
 */
bool PIO_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;
    if (_inst->bytesPerLed < 4) return false; // Not RGBW

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    if (_inst->busy) return false;

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
bool PIO_NeoPixel_Serial::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;
    if (_inst->bytesPerLed < 5) return false; // Not RGBCCT

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    if (_inst->busy) return false;

    rgbToBuffer(index, r, g, b, ww, cw);
    return true;
}

/**
 * @brief Stores color values in correct format in buffer
 *
 * Internal helper function that stores color values according to
 * LED color order (RGB, GRB, BGR etc.) in the buffer.
 *
 * @param index LED index
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param ww Warm White component (optional, 0 for RGB/RGBW)
 * @param cw Cool White component (optional, 0 for RGB/RGBW)
 */
void PIO_NeoPixel_Serial::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_inst || !_inst->buffer) return;

    // CRITICAL: Cast to size_t to prevent overflow (index * bytesPerLed can exceed uint16_t)
    // Example: index=22000, bytesPerLed=3 → 66000 > 65535 → OVERFLOW!
    size_t offset = (size_t)index * _inst->bytesPerLed;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (offset + _inst->bytesPerLed > _inst->bufferSize)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial",
                                              "ERROR: Buffer overflow prevented! Index %d, Offset %u, BufferSize %u",
                                              index, (uint32_t)offset, (uint32_t)_inst->bufferSize);
    #endif
        return;
    }

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

/**
 * @brief Sends color data to LED strip
 *
 * Starts the transmission of color data from internal buffer.
 * Uses either DMA or direct PIO transfer depending on configuration.
 *
 * - DMA: Asynchronous background transfer
 * - PIO: Blocking transfer (waits for completion)
 *
 * @return true if transfer started, false on error or active transfer
 */
bool PIO_NeoPixel_Serial::show()
{
    if (!_inst || !_inst->initialized || !_inst->buffer) return false;
    if (_inst->busy) return false; // Already transmitting

    _inst->busy = true;

    if (_inst->useDMA && _inst->dmaChannel >= 0)
    {
        sendDataDMA();
    }
    else
    {
        sendDataPIO();
        _inst->busy = false; // PIO mode is blocking
    }

    return true;
}

/**
 * @brief Sends data directly via PIO
 *
 * Transfers color data blocking via PIO FIFO.
 * Data is packed into 32-bit words:
 *
 * RGB format (3 bytes per LED):
 * - Pack as: Byte2<<24 | Byte1<<16 | Byte0<<8
 * - LSB (Byte0) sent first
 *
 * RGBW format (4 bytes per LED):
 * - Pack as: Byte3<<24 | Byte2<<16 | Byte1<<8 | Byte0
 * - LSB (Byte0) sent first
 *
 * NOTE: Buffer order is determined by ColorOrder setting (via rgbToBuffer).
 *       This function is ColorOrder-agnostic and just sends bytes in buffer order.
 */
void PIO_NeoPixel_Serial::sendDataPIO()
{
    if (!_inst) return;

    // Send data to PIO FIFO in 32-bit chunks
    // PIO autopull LSB-first (shift_right=false means shift LEFT, output from LSB!)
    // Pack REVERSED: Byte2<<24 | Byte1<<16 | Byte0<<8 so LSB (Byte0) is sent first!

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to PIO!
    __dmb(); // Data Memory Barrier

    uint32_t bytesPerTransfer = _inst->bytesPerLed;
    uint32_t numTransfers = _inst->bufferSize / bytesPerTransfer;
    uint8_t* buf = _inst->buffer;

    if (bytesPerTransfer == 3)
    {
        // 3-byte buffer: Pack REVERSED as Byte2<<24 | Byte1<<16 | Byte0<<8
        for (uint32_t i = 0; i < numTransfers; i++)
        {
            uint32_t idx = i * 3;
            uint32_t value = ((uint32_t)buf[idx + 2] << 24) | // Byte2 → bits 31-24
                             ((uint32_t)buf[idx + 1] << 16) | // Byte1 → bits 23-16
                             ((uint32_t)buf[idx] << 8);       // Byte0 → bits 15-8 (sent 1st!)
            pio_sm_put_blocking(_inst->pio, _inst->sm, value);
        }
    }
    else
    {
        // 4-byte buffer (RGBW): Pack REVERSED
        for (uint32_t i = 0; i < numTransfers; i++)
        {
            uint32_t idx = i * 4;
            uint32_t value = ((uint32_t)buf[idx + 3] << 24) | // Byte3 → bits 31-24
                             ((uint32_t)buf[idx + 2] << 16) | // Byte2 → bits 23-16
                             ((uint32_t)buf[idx + 1] << 8) |  // Byte1 → bits 15-8
                             (uint32_t)buf[idx];              // Byte0 → bits 7-0 (sent 1st!)
            pio_sm_put_blocking(_inst->pio, _inst->sm, value);
        }
    }
}

/**
 * @brief Starts asynchronous DMA transfer
 *
 * Prepares data for DMA and starts the transfer:
 * 1. Packs color data into DMA buffer
 * 2. Configures DMA channel
 * 3. Starts transfer in background
 *
 * The busy flag is cleared by the DMA IRQ handler.
 */
void PIO_NeoPixel_Serial::sendDataDMA()
{
    if (!_inst || _inst->dmaChannel < 0 || !_inst->dmaBuffer) return;

    // Pack RGB/RGBW bytes → 32-bit words
    packDataToDMABuffer();

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to DMA!
    __dmb(); // Data Memory Barrier

    // Start DMA transfer (non-blocking!)
    dma_channel_set_read_addr(_inst->dmaChannel, _inst->dmaBuffer, true);

    // busy flag is cleared by DMA IRQ handler
}

/**
 * @brief Packs color data into DMA buffer
 * Converts byte color values into 32-bit words for DMA:
 *
 * RGB format (3 bytes → 1 word):
 * - Input: [Byte0, Byte1, Byte2] as bytes
 * - Output: Byte0<<24 | Byte1<<16 | Byte2<<8 (MSB first)
 *
 * RGBW format (4 bytes → 1 word):
 * - Input: [Byte0, Byte1, Byte2, Byte3] as bytes
 * - Output: Byte0<<24 | Byte1<<16 | Byte2<<8 | Byte3 (MSB first)
 *
 * NOTE: Buffer order is determined by ColorOrder setting (set via rgbToBuffer).
 *       This function is ColorOrder-agnostic and just packs bytes in buffer order.
 */
void PIO_NeoPixel_Serial::packDataToDMABuffer()
{
    if (!_inst || !_inst->buffer || !_inst->dmaBuffer) return;

    uint8_t* src = _inst->buffer;
    uint32_t* dst = _inst->dmaBuffer;
    uint32_t bytesPerLed = _inst->bytesPerLed;

    if (bytesPerLed == 3)
    {
        // 3-byte buffer: Pack for MSB-first transmission (PIO shifts left, sends bit 31 first!)
        // Buffer layout: [Byte0, Byte1, Byte2] → Pack as Byte0<<24 | Byte1<<16 | Byte2<<8
        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            uint32_t idx = i * 3;
            dst[i] = ((uint32_t)src[idx] << 24) |     // Byte0 → bits 31-24 (sent 1st!)
                     ((uint32_t)src[idx + 1] << 16) | // Byte1 → bits 23-16
                     ((uint32_t)src[idx + 2] << 8);   // Byte2 → bits 15-8
        }
    }
    else if (bytesPerLed == 4)
    {
        // 4-byte buffer (RGBW): Pack for MSB-first transmission (PIO shifts left, sends bit 31 first!)
        // Buffer layout: [Byte0, Byte1, Byte2, Byte3] → Pack as Byte0<<24 | Byte1<<16 | Byte2<<8 | Byte3<<0
        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            uint32_t idx = i * 4;
            dst[i] = ((uint32_t)src[idx] << 24) |     // Byte0 → bits 31-24 (sent 1st!)
                     ((uint32_t)src[idx + 1] << 16) | // Byte1 → bits 23-16
                     ((uint32_t)src[idx + 2] << 8) |  // Byte2 → bits 15-8
                     (uint32_t)src[idx + 3];          // Byte3 → bits 7-0 (sent last!)
        }
    }
}

/**
 * @brief Checks if a transfer is in progress
 * @return true if DMA or PIO is actively transferring
 */
bool PIO_NeoPixel_Serial::isBusy()
{
    return _inst ? _inst->busy : false;
}

/**
 * @brief Clears all LED colors (sets to black)
 * Sets the entire internal color buffer to 0.
 * Call show() to apply the change.
 */
void PIO_NeoPixel_Serial::clear()
{
    if (!_inst || !_inst->buffer) return;

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    if (_inst->busy) return;

    memset(_inst->buffer, 0, _inst->bufferSize);
}

/**
 * @brief Returns the driver's capabilities
 *
 * @return DriverCapabilities with supported features:
 *         - supportsRGBW: true for RGBW LEDs (4 bytes/LED)
 */
DriverCapabilities PIO_NeoPixel_Serial::getCapabilities() const
{
    DriverCapabilities caps;
    caps.supportsRGBW = (_inst && _inst->bytesPerLed == 4);
    caps.supportsDMA = (_inst && _inst->useDMA);
    caps.supportsAsync = caps.supportsDMA;
    caps.maxFrequency = _inst ? _inst->frequency : 800000;
    caps.maxLeds = 2000; // Practical limit based on RAM
    return caps;
}

// ========== DMA HANDLER REGISTRATION (GLOBAL REGISTRY) ==========
/**
 * @brief Register a DMA handler for a specific channel
 * @param channel DMA channel number
 * @param instance Pointer to the PIO_NeoPixel_Serial instance
 */
void PIO_NeoPixel_Serial::registerDMAHandler(int channel, PIO_NeoPixel_Serial* instance)
{
    if (channel >= 0 && channel < 12)
    {
        g_serialHandlers[channel] = instance; // Use global registry
    }
}

/**
 * @brief Unregister a DMA handler for a specific channel
 * @param channel DMA channel number
 */
void PIO_NeoPixel_Serial::unregisterDMAHandler(int channel)
{
    if (channel >= 0 && channel < 12)
    {
        g_serialHandlers[channel] = nullptr; // Use global registry
    }
}

/**
 * DMA completion callback - called from unified IRQ handler
 */
void PIO_NeoPixel_Serial::onDmaComplete()
{
    if (!_inst) return;

    // Clear busy flag
    _inst->busy = false;
}

// Old per-class handler - now replaced by unifiedDmaIRQHandler in pio_dma_shared.h
// Keeping this for backward compatibility but it's no longer used
void PIO_NeoPixel_Serial::dmaIRQHandler()
{
    unifiedDmaIRQHandler(); // Delegate to unified handler
}
#endif // ARDUINO_ARCH_RP2040