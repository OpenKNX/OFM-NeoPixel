#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_serial.h"
    #include "../OneWireTimingMath.h"
    #include "../OneWireTimingProfile.h"
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

// Compact cadence variants for protocols whose HIGH pulse ratios differ from
// the canonical WS2812x/SK6812 3:7:6:4 waveform. Each program has the same
// control flow and four instructions, so it can use the common setup path.
static const uint16_t neopixel_three_step_program_instructions[] = {
    0x6021, // out x, 1 side 0 [0]  -> one LOW cycle
    0x1023, // jmp !x, 3 side 1 [0] -> one HIGH cycle
    0x1000, // jmp 0      side 1 [0] -> one bit: second HIGH cycle
    0xa042, // nop        side 0 [0] -> zero bit: second LOW cycle
};
static const pio_program_t neopixel_three_step_program = {
    .instructions = neopixel_three_step_program_instructions,
    .length = 4,
    .origin = -1,
};

static const uint16_t neopixel_four_step_program_instructions[] = {
    0x6021, // out x, 1 side 0 [0]  -> one LOW cycle
    0x1023, // jmp !x, 3 side 1 [0] -> one HIGH cycle
    0x1100, // jmp 0      side 1 [1] -> one bit: two more HIGH cycles
    0xa142, // nop        side 0 [1] -> zero bit: two more LOW cycles
};
static const pio_program_t neopixel_four_step_program = {
    .instructions = neopixel_four_step_program_instructions,
    .length = 4,
    .origin = -1,
};

static const uint16_t neopixel_six_step_program_instructions[] = {
    0x6221, // out x, 1 side 0 [2]  -> three LOW cycles
    0x1023, // jmp !x, 3 side 1 [0] -> one HIGH cycle
    0x1100, // jmp 0      side 1 [1] -> one bit: two more HIGH cycles
    0xa142, // nop        side 0 [1] -> zero bit: two more LOW cycles
};
static const pio_program_t neopixel_six_step_program = {
    .instructions = neopixel_six_step_program_instructions,
    .length = 4,
    .origin = -1,
};

struct PioCadenceConfig
{
    const pio_program_t* program;
    uint8_t cyclesPerBit;
    uint8_t oneHighCycles;
};

static PioCadenceConfig get_pio_cadence_config(OneWirePioCadence cadence)
{
    switch (cadence)
    {
        case OneWirePioCadence::THREE_STEP:
            return {&neopixel_three_step_program, 3, 2};
        case OneWirePioCadence::FOUR_STEP:
            return {&neopixel_four_step_program, 4, 3};
        case OneWirePioCadence::SIX_STEP:
            return {&neopixel_six_step_program, 6, 3};
        case OneWirePioCadence::CANONICAL_10:
        default:
            return {&neopixel_serial_program, 10, 6};
    }
}

// Static member initialization. Our DMA handlers array.
PIO_NeoPixel_Serial* PIO_NeoPixel_Serial::_dmaHandlers[12] = {nullptr};

/**
 * Initialize NeoPixel PIO program
 *
 * Works for all protocols (WS2812B @ 800kHz, WS2811 @ 400kHz, etc.)
 * Timing is controlled via clock divider parameter
 *
 * @param fifoWordBits For RGBCCT: autopull threshold (8) matching one valid payload byte
 *                     For RGB/RGBW: ignored (uses 24/32 respectively)
 */
static inline void neopixel_serial_program_init(PIO pio, uint sm, uint offset, uint pin, float clkdiv,
                                                bool rgbw, bool rgbcct, bool inverted,
                                                uint fifoWordBits = 32)
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
    gpio_set_outover(pin, inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);

    // 3. Set pin as output under PIO control
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812_wrap_target, offset + ws2812_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);

    // Autopull configuration - WS2812B needs MSB-first! shift_right=FALSE (shift left = MSB first!)
    // - RGB (3 bytes): 24-bit autopull for efficient 1 word per LED
    // - RGBW (4 bytes): 32-bit autopull for efficient 1 word per LED
    // - RGBCCT (5 bytes): 8-bit autopull. Each payload byte is MSB-aligned in
    //   its own DMA/FIFO word, so the stream has exactly 40 bits per LED.
    uint autopull_bits;
    if (rgbcct)
    {
        autopull_bits = fifoWordBits; // One valid payload byte per FIFO word
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
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "PIO Init: pin=%u, SM=%u, offset=%u, clkdiv=%.2f, RGBW=%d, RGBCCT=%d, inverted=%d",
                                          pin, sm, offset, clkdiv, rgbw, rgbcct, inverted);
    #endif
}

static void write_frame_settings(pio_neopixel_serial_inst_t* inst)
{
    if (!inst || !inst->buffer || inst->protocol != LedProtocol::SM16825 ||
        inst->frameSettingsBytes != 4) return;
    oneWireWriteSm16825Settings(inst->buffer + (size_t)inst->ledCount * inst->bytesPerLed);
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
    _inst->program = nullptr;
    _inst->cyclesPerBit = 0;
    _inst->oneHighCycles = 0;
    _inst->outputInverted = false;

    // Protocol selection owns timing, channel count and byte order together.
    const OneWireTimingProfile& profile = getOneWireTimingProfile(protocol);
    _inst->channelCount = profile.channelCount;
    _inst->bytesPerChannel = profile.bytesPerChannel;
    _inst->bytesPerLed = profile.channelCount * profile.bytesPerChannel;
    _inst->frameSettingsBytes = profile.frameSettingsBytes;
    _inst->colorOrder = profile.defaultColorOrder;
    _inst->frequency = profile.bitRateHz;

    // Allocate buffers
    // CRITICAL: Cast to size_t to prevent overflow (e.g., 22000 * 3 = 66000 > uint16_t max 65535)
    _inst->bufferSize = (size_t)ledCount * _inst->bytesPerLed + _inst->frameSettingsBytes;
    _inst->buffer = new uint8_t[_inst->bufferSize];

    if (_inst->buffer)
    {
        memset(_inst->buffer, 0, _inst->bufferSize);
        write_frame_settings(_inst);
    }

    // DMA buffer (if needed)
    // For DMA: Buffer handling depends on LED type
    //
    // RGB (3 bytes): 24-bit autopull, 1 uint32_t per LED (packed into 32-bit word)
    // RGBW (4 bytes): 32-bit autopull, 1 uint32_t per LED (perfect fit)
    // RGBCCT (5 bytes): expand every payload byte into one 32-bit DMA word
    //                    with an 8-bit autopull threshold. This avoids ever
    //                    emitting alignment padding after the final LED.
    if (useDMA)
    {
        // Calculate DMA buffer size based on LED type
        // CRITICAL: Cast to size_t to prevent overflow
        if (_inst->bytesPerLed != 3 && _inst->bytesPerLed != 4)
        {
            // Each word contains one byte at bits 31..24. With 8-bit
            // autopull the PIO emits exactly that byte and immediately pulls
            // the next word; no zero padding becomes LED data.
            _inst->fifoWordBits = 8;
            _inst->dmaBufferSize = _inst->bufferSize;
            _inst->dmaBuffer = new uint32_t[_inst->dmaBufferSize];
            if (_inst->dmaBuffer)
            {
                memset(_inst->dmaBuffer, 0, _inst->dmaBufferSize * sizeof(uint32_t));
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
        }
    }
    else
    {
        _inst->dmaBuffer = nullptr;
        _inst->dmaBufferSize = 0;
        _inst->fifoWordBits = (_inst->bytesPerLed != 3 && _inst->bytesPerLed != 4) ? 8 : 0;
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
                gpio_set_outover(_inst->pin, GPIO_OVERRIDE_NORMAL);
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
                    _inst->program ? _inst->program : &neopixel_serial_program,
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

    const uint16_t t0h = serialCfg->getT0H();
    const uint16_t t0l = serialCfg->getT0L();
    const uint16_t t1h = serialCfg->getT1H();
    const uint16_t t1l = serialCfg->getT1L();
    const bool anyCustomTiming = t0h || t0l || t1h || t1l;
    const bool completeCustomTiming = t0h && t0l && t1h && t1l;
    if (anyCustomTiming && !completeCustomTiming) return false;

    // Do not alter a divider or GPIO configuration while the state machine is
    // emitting a frame. The caller can retry after the driver-derived deadline.
    if (_inst->initialized && isBusy()) return false;

    const OneWireTimingProfile& profile = getOneWireTimingProfile(_inst->protocol);
    const uint8_t oneHighCycles = _inst->oneHighCycles ? _inst->oneHighCycles : 6;
    const uint8_t cyclesPerBit = _inst->cyclesPerBit ? _inst->cyclesPerBit : 10;
    const uint32_t sysClk = clock_get_hz(clk_sys);
    const TimingMode requestedMode = completeCustomTiming
                                         ? TimingMode::CUSTOM
                                         : serialCfg->getTimingMode();
    if (requestedMode == TimingMode::CUSTOM && !completeCustomTiming) return false;

    float targetBitrate = (float)profile.bitRateHz;
    if (completeCustomTiming)
    {
        targetBitrate = 1000000000.0f / ((float)t1h * (float)cyclesPerBit / (float)oneHighCycles);
    }
    else
    {
        switch (requestedMode)
        {
            case TimingMode::AUTO_LEGACY: targetBitrate = 960000.0f; break;
            case TimingMode::SLOW_20PCT: targetBitrate *= 0.80f; break;
            case TimingMode::SLOW_15PCT: targetBitrate *= 0.85f; break;
            case TimingMode::SLOW_10PCT: targetBitrate *= 0.90f; break;
            case TimingMode::SLOW_5PCT: targetBitrate *= 0.95f; break;
            case TimingMode::FAST_5PCT: targetBitrate *= 1.05f; break;
            case TimingMode::FAST_10PCT: targetBitrate *= 1.10f; break;
            case TimingMode::FAST_15PCT: targetBitrate *= 1.15f; break;
            case TimingMode::FAST_20PCT: targetBitrate *= 1.20f; break;
            case TimingMode::FAST_25PCT: targetBitrate *= 1.25f; break;
            case TimingMode::AUTO:
            default: break;
        }
    }
    float targetClkdiv = 0.0f;
    float actualBitrate = 0.0f;
    if (!oneWireMakePioClockDivider(sysClk, targetBitrate, cyclesPerBit,
                                    targetClkdiv, actualBitrate))
        return false;

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

    // A PIO cadence keeps a fixed pulse ratio, so T1H selects the divider.
    // With no complete override, restore the profile divider and latch time;
    // this makes clearCustomTiming() change the actual hardware as well as
    // the stored configuration.
    if (_inst->initialized)
    {
        pio_sm_set_clkdiv(_inst->pio, _inst->sm, targetClkdiv);
        pio_sm_clkdiv_restart(_inst->pio, _inst->sm);
    }

    _inst->actual_clkdiv = targetClkdiv;
    _inst->actual_bitrate = actualBitrate;
    _inst->timingMode = requestedMode;
    _inst->resetTimeUs = serialCfg->getResetTime() > 0
                              ? serialCfg->getResetTime()
                              : profile.resetTimeUs;

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
    _inst->framePending = false;
    _inst->busy = false;
    _inst->waitingForReset = false;
    return true;
}

/**
 * @brief Initializes PIO state machine for NeoPixel protocol
 *
 * Configures PIO hardware for the selected LED protocol:
 * 1. Loads the protocol's cadence program
 * 2. Calculates clock divider for the selected profile
 * 3. Configures GPIO and FIFO settings
 *
 * Timing calculation:
 * - All protocols use 10 PIO cycles per bit (standard WS2812B program)
 * - PIO clock = system clock / divider
 * - Divider = system clock / (freq * cycles_per_bit)
 *
 * Examples:
 * - WS2812B @ 800kHz → 125MHz / (800kHz * 10) = 15.625
 * - WS2811 @ 400kHz with six cycles/bit → 125MHz / (400kHz * 6) = 52.08
 * - WS2805 @ 917kHz with four cycles/bit → 125MHz / (917kHz * 4) = 34.06
 *
 * @return true if PIO configured, false on error
 */
bool PIO_NeoPixel_Serial::initPIO()
{
    if (!_inst) return false;

    bool rgbw = (_inst->channelCount == 4);
    bool byteStream = (_inst->bytesPerLed != 3 && _inst->bytesPerLed != 4);

    const OneWireTimingProfile& timing = getOneWireTimingProfile(_inst->protocol);
    const PioCadenceConfig cadence = get_pio_cadence_config(timing.pioCadence);
    const uint8_t cycles_per_bit = cadence.cyclesPerBit;
    _inst->frequency = timing.bitRateHz;
    _inst->program = cadence.program;
    _inst->cyclesPerBit = cadence.cyclesPerBit;
    _inst->oneHighCycles = cadence.oneHighCycles;
    _inst->outputInverted = timing.inverted;

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
    const uint32_t actual_sys_clk = clock_get_hz(clk_sys);
    float clkdiv = 1.0f;
    float actual_bitrate = 0.0f;
    float target_bitrate = (float)_inst->frequency;
    float bitrate_multiplier = 1.0f;
    const char* mode_name = "AUTO";

    switch (_inst->timingMode)
    {
        case TimingMode::AUTO_LEGACY:
            // Expert compatibility override for WS2812C/D-style strips.
            target_bitrate = 960000.0f;
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

    if (_inst->timingMode != TimingMode::AUTO_LEGACY)
        target_bitrate *= bitrate_multiplier;

    if (!oneWireMakePioClockDivider(actual_sys_clk, target_bitrate, cycles_per_bit,
                                    clkdiv, actual_bitrate))
        return false;

    // Store actual values for debugging/info display
    _inst->actual_bitrate = actual_bitrate;
    _inst->actual_clkdiv = clkdiv;

    const pio_program_t* selected_program = cadence.program;

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "GPIO%u: %s, sys_clk=%.0f MHz, clkdiv=%.3f, bitrate=%.0f kHz [%s], cycles=%d, inverted=%d",
                                          _inst->pin, timing.name, actual_sys_clk / 1e6f, clkdiv,
                                          actual_bitrate / 1000.0f, mode_name, cycles_per_bit, timing.inverted);
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
        selected_program, // PIO program selected by protocol cadence
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
        selected_program, // PIO program selected by protocol cadence
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
    neopixel_serial_program_init(_inst->pio, _inst->sm, _inst->offset, _inst->pin, clkdiv,
                                 rgbw, byteStream, timing.inverted, _inst->fifoWordBits);

    #ifdef OPENKNX_DEBUG
    const char* pioName = (_inst->pio == pio0) ? "PIO0" : (_inst->pio == pio1) ? "PIO1"
                                                                               : "PIO2";
    const char* channelType = byteStream ? (ProtocolHelper::is16Bit(_inst->protocol) ? "SM16825" : "RGBCCT") : (rgbw ? "RGBW" : "RGB");
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "NeoPixel PIO: %s/SM%d, GPIO%d, %.0fkHz, %s",
                                          pioName, _inst->sm, _inst->pin, _inst->actual_bitrate / 1000.0f, channelType);
    #endif

    _inst->resetTimeUs = timing.resetTimeUs;

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
 * - RGBCCT (5-byte): one MSB-aligned DMA word per payload byte, with an
 *   8-bit autopull threshold. This transmits the exact 40-bit LED payload.
 * - Source pointer increments (buffer)
 * - Destination pointer fixed (PIO FIFO)
 * - DREQ from PIO controls transfer timing
 *
 * @note All serial strips share IRQ0
 * @return true if DMA configured, false if resources unavailable
 */
bool PIO_NeoPixel_Serial::initDMA()
{
    // All modes use packed, 32-bit DMA words. RGBCCT uses one word per byte
    // because its 40-bit pixel width is not divisible by a FIFO word.
    if (!_inst) return false;
    if (!_inst->dmaBuffer) return false;

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

    if (_inst->bytesPerLed != 3 && _inst->bytesPerLed != 4)
    {
        // RGBCCT: one valid byte in bits 31..24 of each word.
        // The PIO's 8-bit autopull emits the top byte and immediately pulls
        // the next DMA word.
        // DMA still uses 32-bit writes; only the top eight bits of every
        // word are valid serial data, so there is no trailing padding.
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

        // Data is already positioned for MSB-first output.
        channel_config_set_bswap(&c, false);

        srcBuffer = _inst->dmaBuffer;
        transferCount = _inst->dmaBufferSize;

    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial", "RGBCCT DMA: exact 8-bit stream, %u payload bytes",
                                              (uint32_t)transferCount);
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
        irq_add_shared_handler(DMA_IRQ_0, unifiedDmaIRQHandler,
                               PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
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
    if (_inst->channelCount < 4) return false; // Not RGBW

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
    if (_inst->channelCount < 5) return false; // Not RGBCCT

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

    // A narrower logical order is allowed on wider hardware. Clear channels
    // omitted by that order so an earlier white value cannot leak into a frame.
    memset(_inst->buffer + offset, 0, _inst->bytesPerLed);

    const auto setChannel = [&](uint8_t channel, uint8_t value) {
        oneWireStoreChannel(_inst->buffer + offset, _inst->bytesPerChannel, channel, value);
    };

    uint8_t first = 0, second = 0, third = 0;
    if (ProtocolHelper::mapRgbChannels(_inst->colorOrder, r, g, b, first, second, third))
    {
        setChannel(0, first); setChannel(1, second); setChannel(2, third);
        return;
    }

    switch (_inst->colorOrder)
    {
        case ColorOrder::RGBW:
            setChannel(0, r); setChannel(1, g); setChannel(2, b); setChannel(3, ww);
            break;

        case ColorOrder::GRBW:
            setChannel(0, g); setChannel(1, r); setChannel(2, b); setChannel(3, ww);
            break;

        // 5-channel color orders (RGBCCT)
        case ColorOrder::RGBCCT:
            setChannel(0, r); setChannel(1, g); setChannel(2, b); setChannel(3, ww); setChannel(4, cw);
            break;

        case ColorOrder::GRBCCT:
            setChannel(0, g); setChannel(1, r); setChannel(2, b); setChannel(3, ww); setChannel(4, cw);
            break;

        case ColorOrder::RGBCTW:
            setChannel(0, r); setChannel(1, g); setChannel(2, b); setChannel(3, cw); setChannel(4, ww);
            break;

        case ColorOrder::GRBCTW:
            setChannel(0, g); setChannel(1, r); setChannel(2, b); setChannel(3, cw); setChannel(4, ww);
            break;

        default:
            break;
    }
}

static void recoverDirectPioTransfer(pio_neopixel_serial_inst_t* inst);

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
    const uint32_t showWaitTimeoutUs = getTransferTimeoutUs();
    const uint32_t waitStartUs = micros();
    while (isBusy())
    {
        if ((uint32_t)(micros() - waitStartUs) > showWaitTimeoutUs)
        {
            // Wedge recovery: abort any in-flight DMA and clear the transfer
            // state so the next show() can re-arm cleanly.
            if (_inst->useDMA && _inst->dmaChannel >= 0)
                dma_channel_abort(_inst->dmaChannel);
            _inst->recoveryCount++;
            _inst->busy = false;
            _inst->waitingForReset = false;
            _inst->framePending = false;

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
                    (unsigned long)((showWaitTimeoutUs + 999U) / 1000U));
            }
            break;
        }
    }

    // Reset state for new transfer
    _inst->busy = true;
    _inst->waitingForReset = false;
    _inst->framePending = true;

    if (_inst->useDMA && _inst->dmaChannel >= 0)
    {
        if (!sendDataDMA())
        {
            _inst->busy = false;
            _inst->waitingForReset = false;
            _inst->framePending = false;
            return false;
        }
    }
    else
    {
        if (!sendDataPIO())
        {
            recoverDirectPioTransfer(_inst);
            _inst->busy = false;
            _inst->waitingForReset = false;
            _inst->framePending = false;
            return false;
        }

        // Direct FIFO writes are blocking only while the FIFO is full. The
        // final words can still be queued or held in the OSR, so keep the
        // instance busy until the same FIFO-drain and latch check used by DMA
        // reports the line safe for a following frame.
        const uint32_t completionStartUs = micros();
        const uint32_t completionTimeoutUs = getTransferTimeoutUs();
        while (isBusy())
        {
            if ((uint32_t)(micros() - completionStartUs) > completionTimeoutUs)
            {
                recoverDirectPioTransfer(_inst);
                _inst->busy = false;
                _inst->waitingForReset = false;
                _inst->framePending = false;
                openknx.logger.logWithPrefixAndValues("PIO NeoPixel Serial",
                    "direct PIO transfer timed out on GPIO%u SM%u after %lums",
                    _inst->pin, _inst->sm,
                    (unsigned long)((completionTimeoutUs + 999U) / 1000U));
                return false;
            }
        }
        _inst->busy = false;
    }

    return true;
}

/**
 * @brief Sends data directly via PIO
 *
 * Transfers color data blocking via PIO FIFO.
 * Data is packed identically to the DMA path. The PIO is configured for
 * shift-left/MSB-first output, therefore Byte0 belongs in bits 31..24.
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

static void recoverDirectPioTransfer(pio_neopixel_serial_inst_t* inst)
{
    if (!inst) return;

    // A FIFO timeout leaves only a partial frame queued. Restart at an
    // idle-low boundary so a later show() cannot continue that bit stream.
    pio_sm_set_enabled(inst->pio, inst->sm, false);
    pio_sm_clear_fifos(inst->pio, inst->sm);
    pio_sm_restart(inst->pio, inst->sm);
    pio_sm_clkdiv_restart(inst->pio, inst->sm);
    pio_sm_set_pins_with_mask(inst->pio, inst->sm, 0, 1u << inst->pin);
    pio_sm_set_enabled(inst->pio, inst->sm, true);
    delayMicroseconds(inst->resetTimeUs);
    inst->recoveryCount++;
}

bool PIO_NeoPixel_Serial::sendDataPIO()
{
    if (!_inst) return false;

    const uint32_t putTimeoutUs = getTransferTimeoutUs();

    // Send data to PIO FIFO in 32-bit chunks
    // The PIO shift register emits MSB first (shift_right=false), matching the
    // packed DMA words below.

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to PIO!
    __dmb(); // Data Memory Barrier

    const size_t wordCount = oneWirePackedWordCount(_inst->bufferSize, _inst->bytesPerLed);
    if (wordCount == 0) return false;
    for (size_t i = 0; i < wordCount; ++i)
    {
        const uint32_t value = oneWirePackedWordAt(_inst->buffer, _inst->bytesPerLed, i);
        if (!neoSerialPutGuarded(_inst->pio, _inst->sm, value, putTimeoutUs)) return false;
    }

    return true;
}

/**
 * @brief Starts asynchronous DMA transfer
 *
 * Prepares data for DMA and starts the transfer:
 * 1. For RGB/RGBW: Packs color data into DMA buffer, then starts DMA
 * 2. For RGBCCT: Expands each byte to an MSB-aligned DMA word for exact 8-bit output
 *
 * The busy flag is cleared by the DMA IRQ handler.
 */
bool PIO_NeoPixel_Serial::sendDataDMA()
{
    if (!_inst || _inst->dmaChannel < 0) return false;

    bool byteStream = (_inst->bytesPerLed != 3 && _inst->bytesPerLed != 4);
    void* srcBuffer;

    if (byteStream)
    {
        // Expand each payload byte to the MSB of its own FIFO word. Combined
        // with 8-bit autopull this preserves the exact 40-bit pixel stream.
        if (!_inst->buffer || !_inst->dmaBuffer) return false;
        packDataToDMABuffer();
        srcBuffer = _inst->dmaBuffer;
    }
    else
    {
        // RGB/RGBW: Pack data into 32-bit words (bswap=false, manual packing)
        if (!_inst->dmaBuffer) return false;
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
    return true;
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
 * RGBCCT: one MSB-aligned word per payload byte, emitted with 8-bit autopull.
 *
 * NOTE: Buffer order is determined by ColorOrder setting (set via rgbToBuffer).
 *       This function is ColorOrder-agnostic and just packs bytes in buffer order.
 */
void PIO_NeoPixel_Serial::packDataToDMABuffer()
{
    if (!_inst || !_inst->buffer || !_inst->dmaBuffer) return;

    uint32_t* dst = _inst->dmaBuffer;
    const size_t wordCount = oneWirePackedWordCount(_inst->bufferSize, _inst->bytesPerLed);
    for (size_t i = 0; i < wordCount; ++i)
        dst[i] = oneWirePackedWordAt(_inst->buffer, _inst->bytesPerLed, i);
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

    // The drain/latch window belongs to a queued frame. Without this gate every
    // poll of an idle strip would re-arm the window below and report busy, which
    // made applyConfig() fail after init() and show() reject the next frame.
    if (!_inst->framePending) return false;

    // Direct PIO writes set busy while they drain the FIFO, final OSR word and
    // reset interval. An idle no-DMA strip must still report ready immediately.
    if (!_inst->useDMA && !_inst->busy) return false;

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
    _inst->framePending = false;
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
    write_frame_settings(_inst);
}

uint32_t PIO_NeoPixel_Serial::getTransferTimeoutUs() const
{
    if (!_inst || _inst->bufferSize == 0) return 1000000U;

    // Use the realised PIO bitrate, not the nominal protocol rate. A frame is
    // followed by one final OSR word and the protocol reset interval; add a
    // small scheduler/IRQ margin while retaining a finite recovery deadline.
    const uint32_t bitrate = _inst->actual_bitrate > 1.0f
                                 ? (uint32_t)_inst->actual_bitrate
                                 : getOneWireTimingProfile(_inst->protocol).bitRateHz;
    uint32_t finalWordBits = _inst->fifoWordBits;
    if (_inst->bytesPerLed == 3) finalWordBits = 24;
    else if (_inst->bytesPerLed == 4) finalWordBits = 32;
    if (finalWordBits == 0) finalWordBits = 32;

    return oneWireTransferDeadlineUs(_inst->bufferSize, bitrate, finalWordBits, _inst->resetTimeUs);
}

uint32_t PIO_NeoPixel_Serial::getFinalWordDrainUs() const
{
    if (!_inst || _inst->fifoWordBits == 0 || _inst->actual_bitrate <= 1.0f) return 0;
    return oneWireFinalWordDrainUs(_inst->fifoWordBits, (uint32_t)_inst->actual_bitrate);
}

uint32_t PIO_NeoPixel_Serial::getReadyAtUs() const
{
    if (!_inst || !_inst->waitingForReset) return 0;
    return _inst->fifoEmptyTime + getFinalWordDrainUs() + _inst->resetTimeUs;
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
    DriverCapabilities caps = {};
    caps.supportsRGBW = (_inst && _inst->channelCount >= 4);
    caps.supportsRGBCCT = (_inst && _inst->channelCount == 5);
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
    setSerialDMAHandler(channel, instance);
}

/**
 * @brief Unregister a DMA handler for a specific channel
 * @param channel DMA channel number
 */
void PIO_NeoPixel_Serial::unregisterDMAHandler(int channel)
{
    setSerialDMAHandler(channel, nullptr);
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
