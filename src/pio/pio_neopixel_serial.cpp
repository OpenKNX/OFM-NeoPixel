#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_serial.h"
    #include "../PhysicalStripConfig.h"
    #include "OpenKNX.h"
    #include "pio_dma_shared.h"
    #include <Arduino.h>

// =============================================================================
// =============================================================================
// WS2812B PIO PROGRAM (MSB FIRST!) - 10 CYCLES PER BIT
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
// PIO Assembly Program = canonical Raspberry Pi Pico SDK ws2812.pio
// with T1=3, T2=3, T3=4 ("selected for broad compatibility with WS2812,
// WS2812B and SK6812"). Structure starts LOW (inter-bit gap), then pulses:
//
// .wrap_target
// bitloop:
// 0: out x, 1       side 0 [3]   ; LOW 4 cycles (T3), pull next bit into x
// 1: jmp !x, do_0   side 1 [2]   ; HIGH 3 cycles (T1); if bit==0 -> do_0 (LOW)
// 2: jmp bitloop    side 1 [2]   ; 1-bit: HIGH 3 more cycles (T2), wrap
// do_0:
// 3: nop            side 0 [2]   ; 0-bit: LOW 3 cycles (T2)
// .wrap
//
// => T0H:T0L:T1H:T1L = 3:7:6:4 (@800kHz true: 375/875/750/500 ns). SK6812-safe.
// The previous custom "start-HIGH" program emitted ~4-5 HIGH cycles with only
// ~1 cycle 0/1 separation -> T0H ~500ns (> SK6812 T0H_max ~450ns), so SK6812
// only worked when accidentally overclocked (old hardcoded-125MHz clkdiv).
//
// =============================================================================

    #define ws2812_wrap_target 0 // Wrap target instruction index
    #define ws2812_wrap 3        // Wrap instruction index

static const uint16_t ws2812b_program_instructions[] = {
    0x6321, //  0: out    x, 1            side 0 [3]  ; LOW 4 (T3), pull bit into x
    0x1223, //  1: jmp    !x, 3           side 1 [2]  ; HIGH 3 (T1); if 0 -> do_0(3)
    0x1200, //  2: jmp    0               side 1 [2]  ; 1-bit: HIGH 3 more (T2), wrap
    0xa242, //  3: nop                    side 0 [2]  ; 0-bit: LOW 3 (T2)
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
 *
 * @param fifoWordBits For RGBCCT: autopull threshold (8/16/32) matching DMA transfer size
 *                     For RGB/RGBW: ignored (uses 24/32 respectively)
 */
static inline void neopixel_serial_program_init(PIO pio, uint sm, uint offset, uint pin, float clkdiv, bool rgbw, bool rgbcct, uint fifoWordBits = 32)
{
    // ===== GPIO OPTIMIZATION FOR HIGH-SPEED SERIAL (800 kHz - 1.25 MHz) =====

    // 1. Set GPIO drive strength and slew rate FIRST (persist through pio_gpio_init)
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
    // reduced aggressiveness:
    // gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    // gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);

    // 2. Transfer pin to PIO control (before setting state!)
    pio_gpio_init(pio, pin);

    // 3. Set pin as output under PIO control
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812_wrap_target, offset + ws2812_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);

    // Autopull configuration - WS2812B needs MSB-first! shift_right=FALSE (shift left = MSB first!)
    // - RGB (3 bytes): 24-bit autopull for efficient 1 word per LED
    // - RGBW (4 bytes): 32-bit autopull for efficient 1 word per LED
    // - RGBCCT (5 bytes): Dynamic autopull matching DMA transfer size (fifoWordBits)
    //   Data flows as continuous bit stream; bswap handles byte ordering
    uint autopull_bits;
    if (rgbcct)
    {
        autopull_bits = fifoWordBits; // Match DMA transfer size (8/16/32)
    }
    else
    {
        autopull_bits = rgbw ? 32 : 24; // RGBW=32, RGB=24
    }
    sm_config_set_out_shift(&c, false, true, autopull_bits);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // Use full TX FIFO

    sm_config_set_clkdiv(&c, clkdiv);

    // 4. Set initial pin state to LOW (WS2812B reset state)
    //    CRITICAL: Must be BEFORE pio_sm_init() and AFTER pio_gpio_init()
    pio_sm_set_pins_with_mask(pio, sm, 0, (1u << pin));

    // 5. Initialize and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "PIO Init: pin=%u, SM=%u, offset=%u, clkdiv=%.2f, RGBW=%d, RGBCCT=%d",
                                          pin, sm, offset, clkdiv, rgbw, rgbcct);
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
    _inst->fifoEmptyTime = 0;       // No previous transfer
    _inst->resetTimeUs = 0;         // Will be set in initPIO based on protocol
    _inst->waitingForReset = false; // Not waiting for reset yet

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
    // For DMA: Buffer handling depends on LED type
    //
    // RGB (3 bytes): 24-bit autopull, 1 uint32_t per LED (packed into 32-bit word)
    // RGBW (4 bytes): 32-bit autopull, 1 uint32_t per LED (perfect fit)
    // RGBCCT (5 bytes): NO separate packed buffer!
    //                   - Send byte buffer directly with bswap=true
    //                   - Always 32-bit transfers (buffer padded to 4-byte alignment)
    //                   - 32-bit autopull matches DMA transfer size
    if (useDMA)
    {
        // Calculate DMA buffer size based on LED type
        // CRITICAL: Cast to size_t to prevent overflow
        if (_inst->bytesPerLed == 5)
        {
            // RGBCCT: direct buffer DMA with bswap
            //
            // Send byte buffer directly via DMA - no separate packed buffer needed!
            // bswap=true handles byte reordering on-the-fly
            //
            // CRITICAL: Always use 32-bit transfers!
            // With 8-bit or 16-bit DMA transfers, the PIO OSR loads 32 bits but only
            // the transferred bits are valid. Since we shift MSB-first, the undefined
            // upper bits would be output as garbage (could be all 1s = white flash!)
            //
            // Solution: Pad buffer to 32-bit alignment and always use 32-bit transfers

            size_t bufSize = _inst->bufferSize;
            // Round up to next multiple of 4
            size_t paddedSize = (bufSize + 3) & ~3;
            _inst->fifoWordBits = 32;
            _inst->dmaBufferSize = paddedSize / 4; // Transfer count in 32-bit words

            // NO separate DMA buffer for RGBCCT - we use bufferSending instead
            _inst->dmaBuffer = nullptr;

            // RGBCCT: Allocate PADDED buffer for double-buffering
            // CRITICAL: Must be 4-byte aligned for DMA with bswap!
            // Extra bytes are zero-padded (ignored by LEDs)
            // Use aligned_alloc for guaranteed 4-byte alignment
            _inst->bufferSending = static_cast<uint8_t*>(aligned_alloc(4, paddedSize));
            if (_inst->bufferSending)
            {
                memset(_inst->bufferSending, 0, paddedSize); // Zero-pad including extra bytes
            }
        }
        else
        {
            // RGB/RGBW: 24/32-bit autopull, packed efficiently into uint32_t words
            _inst->fifoWordBits = _inst->bytesPerLed == 4 ? 32 : 24; // RGBW=32, RGB=24
            _inst->dmaBufferSize = (size_t)ledCount * (((_inst->bytesPerLed + 3) / 4));
            _inst->dmaBuffer = new uint32_t[_inst->dmaBufferSize];
            if (_inst->dmaBuffer)
            {
                memset(_inst->dmaBuffer, 0, _inst->dmaBufferSize * sizeof(uint32_t));
            }
            _inst->bufferSending = nullptr; // RGB/RGBW uses dmaBuffer instead
        }
    }
    else
    {
        _inst->dmaBuffer = nullptr;
        _inst->dmaBufferSize = 0;
        _inst->fifoWordBits = 0;
        _inst->bufferSending = nullptr;
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
                // Stop any in-flight transfer FIRST so the engine can't keep reading the
                // buffers we free below, and no completion IRQ fires into a freed/
                // unregistered handler (use-after-free → reboot).
                dma_channel_set_irq0_enabled(_inst->dmaChannel, false); // Serial strips use DMA_IRQ_0
                dma_channel_abort(_inst->dmaChannel);
                while (dma_channel_is_busy(_inst->dmaChannel)) { tight_loop_contents(); }
                unregisterDMAHandler(_inst->dmaChannel);
                dma_channel_unclaim(_inst->dmaChannel);
            }

            // Remove program and unclaim SM (matches pio_claim_free_sm_and_add_program)
            if (_inst->pio && _inst->sm < 4)
            {
                pio_remove_program_and_unclaim_sm(
                    &neopixel_serial_program,
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
        if (_inst->bufferSending)
        {
            free(_inst->bufferSending); // Use free() for aligned_alloc
            _inst->bufferSending = nullptr;
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

    // Apply ColorOrder (never let a NONE config stomp the live driver order)
    const ColorOrder cfgOrder = serialCfg->getColorOrder();
    if (cfgOrder != ColorOrder::NONE) _inst->colorOrder = cfgOrder;

    // Apply level-shifter GPIO optimizations
    _inst->levelShifterType = serialCfg->getLevelShifter();
    if (_inst->levelShifterType == LevelShifterType::TXS0108E)
    {
        // TXS0108E auto-direction requires no resistive load on A-side.
        // Drive strength (12mA + FAST) is already set in neopixel_serial_program_init().
        // Explicitly disable internal pull-up/down to avoid interfering with direction RC.
        gpio_set_pulls(_inst->pin, false, false);
        #ifdef OPENKNX_DEBUG
        openknx.logger.logWithValues("PIO NeoPixel Serial GPIO%u: TXS0108E mode - pull disabled", _inst->pin);
        #endif
    }
    else if (_inst->levelShifterType == LevelShifterType::SN74HCT125 ||
             _inst->levelShifterType == LevelShifterType::SN74AHCT125)
    {
        // 74HCT/AHCT: unidirectional transparent buffer; OE=GND, always enabled.
        // TTL input threshold VIH >= 2.0V is satisfied by 3.3V output; no GPIO changes needed.
        // Default 4mA drive + FAST slew (from pio_gpio_init) is sufficient for a logic input.
        #ifdef OPENKNX_DEBUG
        const char* lsN = (_inst->levelShifterType == LevelShifterType::SN74AHCT125) ? "74AHCT125" : "74HCT125";
        openknx.logger.logWithValues("PIO NeoPixel Serial GPIO%u: %s mode - no GPIO changes needed", _inst->pin, lsN);
        #endif
    }

    // Apply custom timing if T1H is set (CUSTOM mode)
    // PIO program has fixed 3:7:6:4 cycle ratio → only T1H determines the bit period.
    // Formula: clkdiv = sys_clk_hz * T1H_ns * 1e-9 / 6.0  (6 HIGH cycles per 10-cycle bit)
    uint16_t t1h = serialCfg->getT1H();
    if (t1h > 0 && _inst->initialized)
    {
        float sys_clk = (float)clock_get_hz(clk_sys);
        float clkdiv  = sys_clk * (float)t1h * 1e-9f / 6.0f;
        clkdiv = (clkdiv < 1.0f) ? 1.0f : clkdiv; // clkdiv < 1 not allowed

        pio_sm_set_clkdiv(_inst->pio, _inst->sm, clkdiv);
        pio_sm_clkdiv_restart(_inst->pio, _inst->sm); // apply immediately

        _inst->actual_clkdiv = clkdiv;
        _inst->actual_bitrate = sys_clk / clkdiv / 10.0f; // 10 cycles per bit
        _inst->timingMode = TimingMode::CUSTOM;

        // Update reset time if provided
        uint32_t resetUs = serialCfg->getResetTime();
        if (resetUs > 0) _inst->resetTimeUs = resetUs;
    }

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
 * - All protocols use 10 PIO cycles per bit (standard WS2812B program)
 * - PIO clock = system clock / divider
 * - Divider = system clock / (freq * cycles_per_bit)
 *
 * Examples:
 * - WS2812B @ 800kHz → 125MHz / (800kHz * 10) = 15.625
 * - WS2811 @ 400kHz → 125MHz / (400kHz * 10) = 31.25
 * - WS2805 @ 800kHz → 125MHz / (800kHz * 10) = 15.625 (uses WS2812B timing)
 *
 * @return true if PIO configured, false on error
 */
bool PIO_NeoPixel_Serial::initPIO()
{
    if (!_inst) return false;

    bool rgbw = (_inst->bytesPerLed == 4);
    bool rgbcct = (_inst->bytesPerLed == 5);

    // Detect WS2805 protocol (for reset time configuration)
    bool isWS2805 = (_inst->protocol == LedProtocol::WS2805_RGBCCT);

    // ALL protocols use 10 cycles per bit (standard WS2812B program)
    const uint8_t cycles_per_bit = 10;

    // Calculate clock divider based on frequency and timing mode
    // PIO clock = sys_clock / clkdiv
    // Need: frequency * cycles_per_bit = PIO clock
    // clkdiv = sys_clock / (frequency * cycles_per_bit)
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
            // WS2805 uses standard frequency (800kHz) like other protocols
            clkdiv = actual_sys_clk / (960000.0f * (float)cycles_per_bit);
            actual_bitrate = (actual_sys_clk / clkdiv) / (float)cycles_per_bit;
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
        clkdiv = actual_sys_clk / (target_bitrate * (float)cycles_per_bit);
        actual_bitrate = target_bitrate;
    }

    // Store actual values for debugging/info display
    _inst->actual_bitrate = actual_bitrate;
    _inst->actual_clkdiv = clkdiv;

    // Use standard WS2812B PIO program for ALL protocols
    const pio_program_t* selected_program = &neopixel_serial_program;

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "GPIO%u: sys_clk=%.0f MHz, clkdiv=%.3f, bitrate=%.0f kHz [%s], cycles=%d",
                                          _inst->pin, actual_sys_clk / 1e6f, clkdiv, actual_bitrate / 1000.0f, mode_name, cycles_per_bit);
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
        selected_program, // PIO program (WS2812B for all protocols)
        &pio_temp,        // Return PIO instance
        &sm_temp,         // Return state machine index
        &offset_temp,     // Return program offset
        gpio_base,        // Base GPIO pin
        gpio_count,       // Number of GPIOs needed
        true              // Allow setting GPIO base if needed
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
        selected_program, // PIO program (WS2812B for all protocols)
        &pio_temp,        // Return PIO instance
        &sm_temp,         // Return state machine index
        &offset_temp      // Return program offset
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
    neopixel_serial_program_init(_inst->pio, _inst->sm, _inst->offset, _inst->pin, clkdiv, rgbw, rgbcct, _inst->fifoWordBits);

    #ifdef OPENKNX_DEBUG
    const char* pioName = (_inst->pio == pio0) ? "PIO0" : (_inst->pio == pio1) ? "PIO1"
                                                                               : "PIO2";
    const char* channelType = rgbcct ? "RGBCCT" : (rgbw ? "RGBW" : "RGB");
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "NeoPixel PIO: %s/SM%d, GPIO%d, %.0fkHz, %s",
                                          pioName, _inst->sm, _inst->pin, _inst->actual_bitrate / 1000.0f, channelType);
    #endif

    // Set reset/latch time based on protocol
    // Intentionally longer than spec for compatibility
    if (isWS2805)
    {
        _inst->resetTimeUs = 300; // WS2805 spec is 280µs, use 300µs for margin
    }
    else
    {
        // WS2812B/SK6812: spec is 50µs, but use 300µs for compatibility
        // with older/clone chips that may need longer reset
        _inst->resetTimeUs = 300;
    }

    return true;
}

/**
 * @brief Configures DMA for NeoPixel transfers
 *
 * Sets up DMA channel for automatic data transfer:
 * 1. Claims a free DMA channel
 * 2. Configures transfer size based on LED type
 * 3. Enables IRQ0 for transfer completion
 *
 * DMA configuration:
 * - RGB/RGBW: 32-bit word transfers, manual packing, no bswap
 * - RGBCCT (5-byte): direct buffer transfer
 *   - Send byte buffer directly (no separate packed buffer)
 *   - bswap=true for on-the-fly byte reordering
 *   - Dynamic transfer size (8/16/32-bit) based on buffer alignment
 *   - Autopull threshold matches DMA transfer size
 * - Source pointer increments (buffer)
 * - Destination pointer fixed (PIO FIFO)
 * - DREQ from PIO controls transfer timing
 *
 * @note All serial strips share IRQ0
 * @return true if DMA configured, false if resources unavailable
 */
bool PIO_NeoPixel_Serial::initDMA()
{
    // For RGBCCT: use byte buffer directly with bswap
    // For RGB/RGBW: Use packed DMA buffer (bswap=false)
    bool isRGBCCT = (_inst->bytesPerLed == 5);

    if (!_inst) return false;
    if (!isRGBCCT && !_inst->dmaBuffer) return false; // RGB/RGBW needs DMA buffer
    if (isRGBCCT && !_inst->buffer) return false;     // RGBCCT needs byte buffer (sent directly)

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

    // Configure DMA based on LED type
    dma_channel_config c = dma_channel_get_default_config(channel);

    void* srcBuffer;
    size_t transferCount;

    if (isRGBCCT)
    {
        // RGBCCT: direct buffer DMA with bswap
        // Send byte buffer directly - no separate packed buffer!
        //
        // bswap=true for MSB-first transmission:
        // - Memory: [A,B,C,D] at addresses 0,1,2,3
        // - Normal 32-bit read (little-endian): 0xDDCCBBAA
        // - With bswap: 0xAABBCCDD (byte A at MSB position)
        // - PIO shifts MSB first: outputs bytes in order A,B,C,D
        //
        // Always use 32-bit transfers to avoid OSR undefined bit issues

        if (!_inst->buffer) return false;

        // Always 32-bit transfers for RGBCCT (buffer is padded to 32-bit alignment)
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

        // bswap=true: DMA reorders bytes for MSB-first transmission
        channel_config_set_bswap(&c, true);

        srcBuffer = _inst->buffer;            // Will be replaced with bufferSending in sendDataDMA
        transferCount = _inst->dmaBufferSize; // Transfer count in 32-bit words (padded)

    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "RGBCCT DMA: 32-bit bswap=true, count=%u (padded from %u bytes)",
                                              (uint32_t)transferCount, (uint32_t)_inst->bufferSize);
    #endif
    }
    else
    {
        // RGB/RGBW: 32-bit transfers with manual packing, no bswap
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_bswap(&c, false); // Data is pre-packed in correct order

        srcBuffer = _inst->dmaBuffer;
        transferCount = _inst->dmaBufferSize; // Already in 32-bit words
    }

    channel_config_set_read_increment(&c, true);                            // Increment read address
    channel_config_set_write_increment(&c, false);                          // Write always to same FIFO
    channel_config_set_dreq(&c, pio_get_dreq(_inst->pio, _inst->sm, true)); // PIO TX DREQ

    dma_channel_configure(
        channel,
        &c,
        &_inst->pio->txf[_inst->sm], // Write to PIO TX FIFO
        srcBuffer,                   // Read from DMA buffer
        transferCount,               // Transfer count
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
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "DMA initialized - Channel=%d, IRQ=0 (shared), Count=%u, Target=PIO%d SM%d",
                                          channel, (uint32_t)transferCount, _inst->pio == pio0 ? 0 : 1, _inst->sm);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "Latch timing: resetTime=%uµs (FIFO check via hardware)",
                                          _inst->resetTimeUs);
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

    // Wait for previous transfer to complete including latch time.
    // busy-wait ensures proper reset pulse; isBusy() checks DMA in progress +
    // final OSR drain time + reset time.
    //
    // SAFETY: bound the wait with a timeout. If a DMA/PIO transfer ever wedges
    // (channel stuck busy / SM stalled), an unbounded spin here would block the
    // main loop, starve openknx.watchdog.loop() and trigger a 16 s watchdog
    // reboot. Instead we force-recover and log, so the device keeps running and
    // the wedge becomes diagnosable (USB stays connected because we don't reboot).
    static constexpr uint32_t kShowWaitTimeoutUs = 50000; // 50 ms (>> any real frame)
    const uint32_t waitStartUs = micros();
    while (isBusy())
    {
        if ((uint32_t)(micros() - waitStartUs) > kShowWaitTimeoutUs)
        {
            // Wedge recovery: abort any in-flight DMA and clear the transfer
            // state so the next show() can re-arm cleanly.
            if (_inst->useDMA && _inst->dmaChannel >= 0)
                dma_channel_abort(_inst->dmaChannel);
            _inst->busy = false;
            _inst->waitingForReset = false;

            // Throttle the log (shared across strips) to ~1/s so a persistent
            // wedge doesn't flood the console/bus.
            static uint32_t lastWedgeLogMs = 0;
            const uint32_t nowMs = millis();
            if (nowMs - lastWedgeLogMs > 1000)
            {
                lastWedgeLogMs = nowMs;
                openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial",
                    "show() WEDGE recovered: GPIO%u SM%u DMA%d stuck >%lums (no reboot)",
                    _inst->pin, _inst->sm, _inst->dmaChannel,
                    (unsigned long)(kShowWaitTimeoutUs / 1000));
            }
            break;
        }
    }

    // Reset state for new transfer
    _inst->busy = true;
    _inst->waitingForReset = false;

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
// Bounded replacement for pio_sm_put_blocking(): waits for TX-FIFO space but gives up
// after timeoutUs. A wedged SM (FIFO never drains) would otherwise spin forever here,
// starve the main loop and trigger the 16 s watchdog reboot. Returns false on timeout
// so the caller abandons the frame instead of hanging. (No-DMA fallback path only.)
static inline bool neoSerialPutGuarded(PIO pio, uint sm, uint32_t value, uint32_t timeoutUs)
{
    const uint32_t start = micros();
    while (pio_sm_is_tx_fifo_full(pio, sm))
    {
        if ((uint32_t)(micros() - start) > timeoutUs) return false;
    }
    pio_sm_put(pio, sm, value);
    return true;
}

void PIO_NeoPixel_Serial::sendDataPIO()
{
    if (!_inst) return;

    static constexpr uint32_t kPutTimeoutUs = 20000; // 20 ms >> any real FIFO wait

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
        // This is different from packDataToDMABuffer because direct PIO writes
        // go through a different hardware path than DMA transfers
        for (uint32_t i = 0; i < numTransfers; i++)
        {
            uint32_t idx = i * 3;
            uint32_t value = ((uint32_t)buf[idx + 2] << 24) | // Byte2 → bits 31-24
                             ((uint32_t)buf[idx + 1] << 16) | // Byte1 → bits 23-16
                             ((uint32_t)buf[idx] << 8);       // Byte0 → bits 15-8 (sent 1st!)
            if (!neoSerialPutGuarded(_inst->pio, _inst->sm, value, kPutTimeoutUs)) return;
        }
    }
    else if (bytesPerTransfer == 4)
    {
        // 4-byte buffer (RGBW): Pack REVERSED
        for (uint32_t i = 0; i < numTransfers; i++)
        {
            uint32_t idx = i * 4;
            uint32_t value = ((uint32_t)buf[idx + 3] << 24) | // Byte3 → bits 31-24
                             ((uint32_t)buf[idx + 2] << 16) | // Byte2 → bits 23-16
                             ((uint32_t)buf[idx + 1] << 8) |  // Byte1 → bits 15-8
                             (uint32_t)buf[idx];              // Byte0 → bits 7-0 (sent 1st!)
            if (!neoSerialPutGuarded(_inst->pio, _inst->sm, value, kPutTimeoutUs)) return;
        }
    }
    else if (bytesPerTransfer == 5)
    {
        // 5-byte buffer (RGBCCT): streaming approach
        // Use 32-bit autopull, pack bytes in MSB-first order (same as bswap does for DMA)
        // Data flows as continuous bit stream across LED boundaries
        // Pack 4 bytes per word, handle all bytes as continuous stream
        size_t totalBytes = _inst->bufferSize;
        size_t fullWords = totalBytes / 4;
        size_t remainingBytes = totalBytes % 4;

        // Send full 32-bit words
        for (size_t w = 0; w < fullWords; w++)
        {
            size_t idx = w * 4;
            uint32_t value = ((uint32_t)buf[idx] << 24) | // Byte at MSB
                             ((uint32_t)buf[idx + 1] << 16) |
                             ((uint32_t)buf[idx + 2] << 8) |
                             (uint32_t)buf[idx + 3];
            if (!neoSerialPutGuarded(_inst->pio, _inst->sm, value, kPutTimeoutUs)) return;
        }

        // Send remaining bytes (1-3 bytes) padded to 32-bit
        if (remainingBytes > 0)
        {
            size_t idx = fullWords * 4;
            uint32_t value = 0;
            if (remainingBytes >= 1) value |= ((uint32_t)buf[idx] << 24);
            if (remainingBytes >= 2) value |= ((uint32_t)buf[idx + 1] << 16);
            if (remainingBytes >= 3) value |= ((uint32_t)buf[idx + 2] << 8);
            if (!neoSerialPutGuarded(_inst->pio, _inst->sm, value, kPutTimeoutUs)) return;
        }
    }
}

/**
 * @brief Starts asynchronous DMA transfer
 *
 * Prepares data for DMA and starts the transfer:
 * 1. For RGB/RGBW: Packs color data into DMA buffer, then starts DMA
 * 2. For RGBCCT: Copies byte buffer to dmaBuffer (with padding), bswap handles byte order
 *
 * The busy flag is cleared by the DMA IRQ handler.
 */
void PIO_NeoPixel_Serial::sendDataDMA()
{
    if (!_inst || _inst->dmaChannel < 0) return;

    bool isRGBCCT = (_inst->bytesPerLed == 5);
    void* srcBuffer;

    if (isRGBCCT)
    {
        // RGBCCT: Use double-buffering to prevent DMA reading corrupted data
        // Copy current buffer to sending buffer, then DMA from sending buffer
        // user can safely modify buffer while DMA is sending the copy
        if (!_inst->buffer || !_inst->bufferSending) return;

        // Copy editing buffer to sending buffer
        memcpy(_inst->bufferSending, _inst->buffer, _inst->bufferSize);
        srcBuffer = _inst->bufferSending;
    }
    else
    {
        // RGB/RGBW: Pack data into 32-bit words (bswap=false, manual packing)
        if (!_inst->dmaBuffer) return;
        packDataToDMABuffer();
        srcBuffer = _inst->dmaBuffer;
    }

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to DMA!
    __dmb(); // Data Memory Barrier

    // SAFETY: Ensure DMA channel is fully stopped before reconfiguring
    // (isBusy() should have waited)
    dma_channel_abort(_inst->dmaChannel);

    // CRITICAL: Must set BOTH read address AND transfer count before each transfer!
    // After DMA completes, trans_count becomes 0. If we only set read_addr and start,
    // the DMA will transfer 0 bytes (or use stale count), causing timing issues!
    dma_channel_set_read_addr(_inst->dmaChannel, srcBuffer, false);
    dma_channel_set_trans_count(_inst->dmaChannel, _inst->dmaBufferSize, false);
    dma_channel_start(_inst->dmaChannel);

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
 * RGBCCT: NOT packed here! Uses raw buffer with bswap.
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
    // RGBCCT (bytesPerLed == 5): Not packed here!
    // Uses raw buffer sent directly with bswap=true
}

/**
 * @brief Checks if strip is ready for new transfer
 *
 * Returns true if:
 * - DMA transfer is still in progress, OR
 * - PIO TX FIFO is not empty (still transmitting), OR
 * - The final output-shift-register word has not drained, OR
 * - Reset/latch time hasn't passed after the final output bit
 *
 * This prevents starting a new transfer too early, which would
 * cause visual glitches (flashing) on the LED strip.
 *
 * @return true if busy (not ready for new transfer)
 */
bool PIO_NeoPixel_Serial::isBusy()
{
    if (!_inst) return false;

    // No DMA = not busy (non-DMA mode is blocking)
    if (!_inst->useDMA) return false;

    // Check hardware directly: Is DMA channel still active?
    // This is more reliable than relying on IRQ callback
    if (_inst->dmaChannel >= 0 && dma_channel_is_busy(_inst->dmaChannel))
    {
        return true; // DMA still transferring
    }

    // DMA done - now check hardware: Is PIO TX FIFO actually empty?
    if (!pio_sm_is_tx_fifo_empty(_inst->pio, _inst->sm))
    {
        return true; // FIFO still has data
    }

    // FIFO just became empty - record the time if we haven't already.
    //
    // IMPORTANT: FIFO empty is not the end of the frame. The state machine may have
    // just pulled the final FIFO word into the OSR and can still emit a complete
    // 24/32-bit word. The reset interval begins only after that final bit's falling
    // edge, so it must not be used to cover the OSR drain time.
    if (!_inst->waitingForReset)
    {
        _inst->fifoEmptyTime = micros();
        _inst->waitingForReset = true;
    }

    // Calculate a conservative bound for the final OSR word. RGB uses a 24-bit
    // autopull threshold, RGBW uses 32-bit, and RGBCCT follows fifoWordBits.
    // Add one microsecond for PIO instruction/observation jitter. actual_bitrate is
    // the requested divider result; the small positive margin also covers the PIO
    // divider's hardware quantisation.
    uint32_t finalWordBits = _inst->fifoWordBits;
    if (_inst->bytesPerLed == 3) finalWordBits = 24;
    else if (_inst->bytesPerLed == 4) finalWordBits = 32;
    if (finalWordBits == 0) finalWordBits = 32;

    const float bitrate = _inst->actual_bitrate;
    const uint32_t drainUs = bitrate > 0.0f
                                 ? (uint32_t)(((float)finalWordBits * 1000000.0f) / bitrate + 0.999f) + 1u
                                 : 0u;
    const uint32_t requiredLowUs = drainUs + _inst->resetTimeUs;

    const uint32_t elapsed = micros() - _inst->fifoEmptyTime;
    if (elapsed < requiredLowUs)
    {
        return true; // Still draining the final word and/or waiting for reset pulse
    }

    // Ready for next transfer
    _inst->waitingForReset = false;
    return false;
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
 *         - supportsRGBCCT: true for RGBCCT LEDs (5 bytes/LED)
 */
DriverCapabilities PIO_NeoPixel_Serial::getCapabilities() const
{
    DriverCapabilities caps;
    caps.supportsRGBW = (_inst && _inst->bytesPerLed >= 4);
    caps.supportsRGBCCT = (_inst && _inst->bytesPerLed == 5);
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
    if (channel >= 0 && channel < MAX_DMA_CHANNELS)
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
    if (channel >= 0 && channel < MAX_DMA_CHANNELS)
    {
        g_serialHandlers[channel] = nullptr; // Use global registry
    }
}

/**
 * DMA completion callback - called from unified IRQ handler
 *
 * Records the time when DMA finished so isBusy() can enforce
 * proper reset/latch timing before allowing next transfer.
 */
void PIO_NeoPixel_Serial::onDmaComplete()
{
    if (!_inst) return;

    // Clear DMA busy flag
    // isBusy() will still return true until FIFO empties + reset time passes
    _inst->busy = false;
    _inst->waitingForReset = false; // Will be set when FIFO actually empties
}

// Old per-class handler - now replaced by unifiedDmaIRQHandler in pio_dma_shared.h
// Keeping this for backward compatibility but it's no longer used
void PIO_NeoPixel_Serial::dmaIRQHandler()
{
    unifiedDmaIRQHandler(); // Delegate to unified handler
}
#endif // ARDUINO_ARCH_RP2040
