#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_spi.h"
    #include "../PhysicalStripConfig.h"
    #include "../SpiFrameMath.h"
    #include "OpenKNX.h"
    #include "pio_dma_shared.h"
    #include <Arduino.h>
    #include <hardware/gpio.h>

// =============================================================================
// PIO SPI PROGRAM FOR LED CONTROL
// =============================================================================
//
// APA102/SK9822 Protocol:
// - Start Frame: 32 bits LOW (0x00000000)
// - LED Data: 32 bits per LED [111bbbbb][BBBBBBBB][GGGGGGGG][RRRRRRRR]
//             brightness(5bit)  Blue      Green     Red
// - End Frame: 32 bits HIGH (0xFFFFFFFF) to latch data
//
// SPI Mode 0 (CPOL=0, CPHA=0):
// - Clock idles LOW
// - Data sampled on rising edge (CLK LOW→HIGH)
// - Data changed on falling edge (CLK HIGH→LOW)
//
// PIO Implementation:
// - Based on Raspberry Pi Pico SDK spi_cpha0_tx example
// - 2 instructions per bit (CLK toggle)
// - MSB-first transmission
// - Autopull every 32 bits from TX FIFO
//
// =============================================================================

// Static member initialization
PIO_NeoPixel_SPI* PIO_NeoPixel_SPI::_dmaHandlers[12] = {nullptr};

    #define pio_spi_wrap_target 0
    #define pio_spi_wrap 1

/**
 * PIO SPI TX Program (MSB-first, CPHA=0) - Based on Raspberry Pi APA102 reference
 *
 * Timing: 2 cycles per bit
 * - Instruction 0: Output 1 bit, CLK LOW
 * - Instruction 1: NOP, CLK HIGH (sample edge)
 *
 * Autopull pulls one FIFO word for each byte (threshold 8). The byte is held
 * in bits 31..24 of that word, so the PIO still emits it MSB-first while the
 * transfer can end on an exact byte boundary.
 * Sideset toggles CLK pin each instruction.
 * Reference: https://github.com/raspberrypi/pico-examples/tree/master/pio/spi
 */
static const uint16_t pio_spi_program_instructions[] = {
    //     .wrap_target
    0x6001, // 0: out pins, 1   side 0      ; Output 1 bit to MOSI, CLK LOW (stalls when FIFO empty)
    0xb042, // 1: nop           side 1      ; NOP (mov y,y), CLK HIGH (sample edge) - bit 12 set for side=1
    //     .wrap
};

static const pio_program_t pio_spi_program = {
    .instructions = pio_spi_program_instructions,
    .length = 2,
    .origin = -1,
};

/**
 * Initialize PIO SPI program
 *
 * @param pio PIO instance (pio0 or pio1)
 * @param sm State machine index (0-3)
 * @param offset Program offset in PIO memory
 * @param clk_pin GPIO pin for SCK (clock)
 * @param data_pin GPIO pin for MOSI (data out)
 * @param freq SPI frequency in Hz
 */
static inline void pio_spi_program_init(PIO pio, uint sm, uint offset, uint clk_pin, uint data_pin, float freq)
{
    // ===== GPIO OPTIMIZATION FOR HIGH-SPEED SPI (3-20 MHz) =====

    // 1. Set GPIO drive strength and slew rate FIRST (persist through pio_gpio_init)
    gpio_set_drive_strength(clk_pin, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(data_pin, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(clk_pin, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(data_pin, GPIO_SLEW_RATE_FAST);

    // 2. Transfer pins to PIO control (before setting state!)
    pio_gpio_init(pio, clk_pin);
    pio_gpio_init(pio, data_pin);

    // 3. Set pins as outputs under PIO control
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, true); // MOSI as output
    pio_sm_set_consecutive_pindirs(pio, sm, clk_pin, 1, true);  // CLK as output

    // Get default config
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + pio_spi_wrap_target, offset + pio_spi_wrap);

    // Configure sideset for CLK pin (1 bit, not optional, no pindirs)
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, clk_pin);

    // Configure out pins for MOSI
    sm_config_set_out_pins(&c, data_pin, 1);

    // Out shift: shift_right=FALSE for MSB-first. Autopull after eight bits
    // means every FIFO word represents exactly one wire byte. This keeps
    // APA102's 4-byte frames and WS2801/LPD8806's 3-byte frames distinct.
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Calculate clock divider
    // SPI frequency = System Clock / (cycles_per_bit * divider)
    // We have 2 instructions per bit (CLK LOW + CLK HIGH)
    // So: freq = clock_get_hz(clk_sys) / (2 * divider)
    // Therefore: divider = clock_get_hz(clk_sys) / (2 * freq)
    float sys_clk = (float)clock_get_hz(clk_sys);
    float div = sys_clk / (2.0f * freq);

    // CRITICAL: Enforce minimum divider (SDK requirement: divider >= 1.0)
    // Without this, low dividers cause incorrect frequencies!
    if (div < 1.0f) div = 1.0f;

    sm_config_set_clkdiv(&c, div);

    // 4. Set initial pin states to LOW (SPI Mode 0 idle: CLK=LOW, MOSI=LOW)
    //    CRITICAL: Must be BEFORE pio_sm_init() and AFTER pio_gpio_init()
    pio_sm_set_pins_with_mask(pio, sm, 0, (1u << clk_pin) | (1u << data_pin));

    // 5. Initialize and start state machine
    //    pio_sm_init() will use pin states set above
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    #ifdef OPENKNX_DEBUG
    float actual_freq = sys_clk / (2.0f * div);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "PIO SPI Init: MOSI=GPIO%u, SCK=GPIO%u, SM=%u",
                                          data_pin, clk_pin, sm);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "Requested: %.2fMHz, Divider: %.4f, Actual: %.2fMHz",
                                          freq / 1000000.0f, div, actual_freq / 1000000.0f);
    #endif
}

/**
 * Constructor for PIO-based SPI NeoPixel driver
 *
 * Creates a new instance and allocates resources:
 * - Configures GPIO pins for SPI (CLK, MOSI, CS)
 * - Allocates buffer for LED data
 * - Sets up protocol-specific parameters
 *
 * @param clkPin GPIO pin for SPI clock
 * @param mosiPin GPIO pin for MOSI/data
 * @param ledCount Number of LEDs in strip
 * @param protocol LED protocol (APA102, SK9822, WS2801, etc.)
 * @param frequency SPI frequency in Hz (default 10MHz)
 * @param csPin Chip select pin (optional, -1 if not used)
 * @param useDMA Enable DMA transfers (default: true)
 *
 * @note Call init() to configure hardware resources
 */
PIO_NeoPixel_SPI::PIO_NeoPixel_SPI(uint clkPin,
                                   uint mosiPin,
                                   uint16_t ledCount,
                                   LedProtocol protocol,
                                   uint32_t frequency,
                                   int csPin,
                                   bool useDMA,
                                   ColorOrder colorOrder)
    : _inst(nullptr)
{
    _inst = (pio_neopixel_spi_inst_t*)malloc(sizeof(pio_neopixel_spi_inst_t));
    if (!_inst) return;

    memset(_inst, 0, sizeof(pio_neopixel_spi_inst_t));

    _inst->clkPin = clkPin;
    _inst->mosiPin = mosiPin;
    _inst->csPin = csPin;
    _inst->ledCount = ledCount;
    _inst->protocol = protocol;
    _inst->spiFrequency = frequency;
    _inst->dirty = true;
    _inst->dmaFinished = false;
    _inst->fifoDrainedAtUs = 0;

    // Set ColorOrder: Use provided value, or protocol-dependent default
    if (colorOrder == ColorOrder::NONE)
    {
        // Protocol defaults: APA102=BGR, APA102_CLONE=BGR, SK9822=RGB
        _inst->colorOrder = (protocol == LedProtocol::APA102 || protocol == LedProtocol::APA102_CLONE)
                                ? ColorOrder::BGR
                                : ColorOrder::RGB;
    }
    else
    {
        _inst->colorOrder = colorOrder;
    }

    // DMA configuration (claimed later in init() if available)
    _inst->useDMA = useDMA;
    _inst->dmaChannel = -1; // Not yet claimed
    _inst->dmaIrqNum = -1;  // Not yet assigned
    // ===== Extended SPI Configuration Defaults =====
    // These settings optimize APA102/SK9822 compatibility (adjustable via API)
    _inst->dummyLedMode = 1;        // Physical dummy LED: sacrifice LED#0 to fix first LED color
    _inst->startFrameCount = 8;     // 8 start frames (0x00000000) for data sync
    _inst->endFrameCount = 1;       // 1 end frame to latch data (varies by chip)
    _inst->startFrameDelayUs = 0;   // No delay after start frames (can add if needed)
    _inst->endFramePattern = 0x00;  // End frame pattern: 0x00 = APA102, 0xFF = SK9822
    _inst->detectedChip = protocol; // Start with user-provided protocol
    _inst->autoDetectChip = false;  // Chip auto-detection disabled by default

    // Protocol-specific defaults (safe ranges for clone chips)
    if (protocol == LedProtocol::APA102_CLONE)
    {
        // Clone chips: Safe ranges to prevent flicker and sync issues
        _inst->minRgbValue = 8;      // RGB < 8 doesn't update on clones
        _inst->maxRgbValue = 230;    // Upper limit to reduce power consumption (can via API if needed)
        _inst->hwBrightnessMin = 16; // Below 16 causes flicker on clones
        _inst->hwBrightnessMax = 30; // 31 breaks sync on some clones
        _inst->hwBrightness = 16;    // Default: minimum safe value (conservative)
    }
    else
    {
        // Original chips: Full range available
        _inst->minRgbValue = 0;      // No RGB clamping
        _inst->maxRgbValue = 255;    // No RGB clamping
        _inst->hwBrightnessMin = 0;  // Full range 0-31
        _inst->hwBrightnessMax = 31; // Full range 0-31
        _inst->hwBrightness = 16;    // Default: 50% brightness
    }

    // Store a canonical, protocol-native byte stream. A separate word buffer
    // feeds the PIO FIFO, preserving exact output framing without exposing a
    // host-endian/DMA representation to the rest of OFM.
    SpiFrameLayout layout = {};
    if (!spiMakeFrameLayout(protocol, ledCount, _inst->startFrameCount,
                            _inst->dummyLedMode, _inst->endFrameCount, layout))
        return;

    _inst->hasGlobalBrightness = layout.hasGlobalBrightness;
    _inst->bytesPerLed = layout.bytesPerLed;
    _inst->bufferSize = layout.bufferSize;
    _inst->bufferWordCount = _inst->bufferSize;
    _inst->buffer = (uint8_t*)calloc(_inst->bufferSize, sizeof(uint8_t));
    _inst->transferBuffer = (uint32_t*)calloc(_inst->bufferWordCount, sizeof(uint32_t));

    if (!_inst->buffer || !_inst->transferBuffer)
    {
        free(_inst->buffer);
        free(_inst->transferBuffer);
        _inst->buffer = nullptr;
        _inst->transferBuffer = nullptr;
        return;
    }

    if (_inst->hasGlobalBrightness)
    {
        const size_t firstLedOffset = layout.startFrameSize + layout.dummyLedSize;
        if (_inst->dummyLedMode == 1)
            _inst->buffer[layout.startFrameSize] = 0xE0;

        const uint8_t defaultBright = 0xE0 | 30;
        for (uint16_t i = 0; i < _inst->ledCount; ++i)
            _inst->buffer[firstLedOffset + (size_t)i * 4] = defaultBright;

        memset(_inst->buffer + firstLedOffset + layout.pixelDataSize, _inst->endFramePattern, layout.endFrameSize);
    }
}

/**
 * Destructor - cleanup resources
 *
 * Releases all hardware resources:
 * - Unregisters DMA handlers
 * - Disables PIO state machine
 * - Frees allocated memory
 */
PIO_NeoPixel_SPI::~PIO_NeoPixel_SPI()
{
    if (_inst)
    {
        if (_inst->initialized)
        {
            // Unregister DMA handlers (IRQ is shared, don't release it)
            if (_inst->dmaChannel >= 0)
            {
                // Stop any in-flight transfer FIRST: disable this channel's IRQ, abort
                // the DMA and wait for it to settle. Otherwise the engine keeps reading
                // _inst->buffer after we free it, and a completion IRQ can fire into a
                // freed/unregistered handler (use-after-free → reboot).
                dma_channel_set_irq1_enabled(_inst->dmaChannel, false); // SPI strips use DMA_IRQ_1
                dma_channel_abort(_inst->dmaChannel);
                while (dma_channel_is_busy(_inst->dmaChannel))
                {
                    tight_loop_contents();
                }
                unregisterDMAHandler(_inst->dmaChannel);
                dma_channel_unclaim(_inst->dmaChannel);
            }

            // Disable and unclaim PIO State Machine
            if (_inst->pio && _inst->sm < 4)
            {
                pio_sm_set_enabled(_inst->pio, _inst->sm, false);

                // Remove program and unclaim SM
                pio_remove_program(_inst->pio, &pio_spi_program, _inst->offset);
                pio_sm_unclaim(_inst->pio, _inst->sm);
            }
        }

        if (_inst->buffer)
        {
            free(_inst->buffer);
            _inst->buffer = nullptr;
        }

        if (_inst->transferBuffer)
        {
            free(_inst->transferBuffer);
            _inst->transferBuffer = nullptr;
        }

        free(_inst);
        _inst = nullptr;
    }
}

/**
 * Initializes the SPI NeoPixel driver
 *
 * Sets up hardware resources:
 * 1. Configures PIO state machine with SPI timing
 * 2. Initializes DMA if enabled (optional)
 * 3. Sets up chip select pin if provided
 *
 * @return true if initialization successful, false if resources unavailable
 *
 * @note Falls back to non-DMA mode on DMA failure
 */
bool PIO_NeoPixel_SPI::init()
{
    if (!_inst || !_inst->buffer || !_inst->transferBuffer) return false;
    if (_inst->initialized) return true;

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "PIO_SPI init(): CLK=%d, MOSI=%d, LEDs=%d, Freq=%dHz, DMA=%s",
                                          _inst->clkPin, _inst->mosiPin, _inst->ledCount,
                                          _inst->spiFrequency, _inst->useDMA ? "YES" : "NO");
    #endif
    // Initialize PIO program
    if (!initPIO())
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "PIO_NeoPixel_SPI: PIO init failed");
    #endif
        return false;
    }

    // Initialize DMA if requested
    if (_inst->useDMA)
    {
        if (!initDMA())
        {
    #ifdef OPENKNX_DEBUG
            openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                                  "PIO_NeoPixel_SPI: DMA init failed (continuing without DMA)");
    #endif

            _inst->useDMA = false;
        }
        else
        {
    #ifdef OPENKNX_DEBUG
            openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                                  "DMA initialized on channel %d",
                                                  _inst->dmaChannel);
    #endif
        }
    }

    // Setup Chip Select if provided
    if (_inst->csPin >= 0)
    {
        pinMode(_inst->csPin, OUTPUT);
        digitalWrite(_inst->csPin, HIGH); // CS active low
    }

    _inst->initialized = true;

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "PIO_NeoPixel_SPI init() complete");
    #endif

    return true;
}

/**
 * Initializes PIO state machine for SPI protocol
 *
 * Configures PIO hardware for SPI communication:
 * 1. Claims a free PIO and state machine (tries PIO1 first, then PIO0)
 * 2. Loads SPI timing program (2 instructions: CLK LOW + CLK HIGH)
 * 3. Calculates clock divider for desired frequency
 * 4. Configures GPIO pins and FIFO settings
 *
 * Resource strategy:
 * - Priority: PIO1 > PIO0 (avoid conflicts with other PIO users)
 * - Each PIO has 4 state machines (SM0-SM3), claims first available
 * - Program is 2 instructions, can share with other SPI strips
 *
 * @return true if PIO configured, false if no resources available
 */
bool PIO_NeoPixel_SPI::initPIO()
{
    if (!_inst) return false;

    // Try to claim a PIO and state machine
    // Priority: PIO0 first (dedicated for SPI, less conflicts with Serial strips)
    // RP2350: PIO0 > PIO2 > PIO1 (keep PIO1/PIO2 for Serial strips)
    PIO pios[] = {
        pio0, // First choice: PIO0 (dedicated for SPI)
    #ifdef PICO_RP2350
        pio2, // Second choice: PIO2 (if PIO0 busy)
    #endif
        pio1 // Last resort: PIO1 (avoid mixing with Serial)
    };

    bool success = false;

    for (PIO pio : pios)
    {
        // Try to claim a state machine from this PIO
        for (uint sm = 0; sm < 4; sm++)
        {
            // Check if we can add the program and if SM is free
            if (pio_can_add_program(pio, &pio_spi_program))
            {
                if (!pio_sm_is_claimed(pio, sm))
                {
                    // Claim this PIO and SM
                    _inst->pio = pio;
                    _inst->sm = sm;
                    pio_sm_claim(pio, sm);

                    // Add program to PIO memory
                    _inst->offset = pio_add_program(pio, &pio_spi_program);

                    // Calculate and store actual clkdiv
                    float sys_clk = (float)clock_get_hz(clk_sys);
                    _inst->clkdiv = sys_clk / (2.0f * (float)_inst->spiFrequency);
                    if (_inst->clkdiv < 1.0f) _inst->clkdiv = 1.0f;

                    // CRITICAL: Clear and reset state machine before init (prevents garbage after reboot!)
                    pio_sm_set_enabled(pio, sm, false); // Disable first
                    pio_sm_restart(pio, sm);            // Reset PC to start
                    pio_sm_clear_fifos(pio, sm);        // Clear any old data
                    pio_sm_clkdiv_restart(pio, sm);     // Reset clock divider

                    // Initialize SPI program
                    pio_spi_program_init(pio, sm, _inst->offset,
                                         _inst->clkPin, _inst->mosiPin,
                                         (float)_inst->spiFrequency);
                    success = true;
    #ifdef OPENKNX_DEBUG
                    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                                          "PIO SPI: Claimed PIO%d SM%d, offset=%d",
                                                          pio == pio0 ? 0 : 1, sm, _inst->offset);
    #endif
                    break;
                }
            }
        }
        if (success) break;
    }

    if (!success)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "ERROR: PIO_NeoPixel_SPI - No PIO/SM available!");
    #endif
    }

    return success;
}

/**
 * Configures DMA for SPI transfers
 *
 * Sets up DMA channel for automatic data transfer:
 * 1. Claims a free DMA channel (12 channels available on RP2040/RP2350)
 * 2. Configures for 32-bit word transfers to PIO FIFO
 * 3. Enables IRQ1 for transfer completion (shared across all SPI strips)
 *
 * DMA configuration details:
 * - Transfer size: 32-bit words (one encoded wire byte per FIFO word)
 * - Source: Buffer pointer (increments by 4 bytes per word)
 * - Destination: PIO TX FIFO (fixed address, auto-drains to PIO)
 * - Pacing: DREQ from PIO controls transfer timing (prevents FIFO overflow)
 * - IRQ: DMA_IRQ_1 shared by ALL SPI strips (IRQ_0 used by Serial strips)
 *
 * Why 32-bit words? The PIO TX FIFO is 32 bits wide. The PIO program consumes
 * only bits 31..24 from each word and auto-pulls after eight output bits.
 *
 * @note All SPI strips share IRQ1, Serial strips use IRQ0 (see pio_dma_shared.h)
 * @return true if DMA configured, false if no channels available
 */
bool PIO_NeoPixel_SPI::initDMA()
{
    if (!_inst) return false;
    // Note: Don't check _inst->initialized here! initDMA() is called BEFORE initialized=true!

    // Claim DMA channel
    int channel = dma_claim_unused_channel(false);
    if (channel < 0)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "ERROR - No DMA channel available!");
    #endif
        return false;
    }

    _inst->dmaChannel = channel;
    _inst->dmaIrqNum = 1; // ALL SPI strips use DMA_IRQ_1

    // Configure DMA channel
    // DMA feeds one 32-bit word per wire byte to the PIO TX FIFO.
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);                 // 32-bit word transfers (4 bytes per transfer)
    channel_config_set_read_increment(&c, true);                            // Increment read pointer by 4 bytes after each word
    channel_config_set_write_increment(&c, false);                          // Write to same FIFO address (PIO drains it)
    channel_config_set_dreq(&c, pio_get_dreq(_inst->pio, _inst->sm, true)); // PIO TX DREQ paces DMA (prevents FIFO overflow)

    dma_channel_configure(
        channel,
        &c,
        &_inst->pio->txf[_inst->sm], // Destination: PIO TX FIFO (32-bit register)
        _inst->transferBuffer,       // Source: PIO-ready wire bytes
        _inst->bufferWordCount,      // One 32-bit FIFO word per wire byte
        false                        // Don't start yet (triggered by show() → sendDataDMA())
    );

    // Register interrupt handler in global registry
    registerDMAHandler(channel, this);

    // ALL SPI strips use DMA_IRQ_1 (shared IRQ for all SPI types)
    dma_channel_set_irq1_enabled(channel, true);

    static bool irq1_initialized = false;
    if (!irq1_initialized)
    {
        irq_add_shared_handler(DMA_IRQ_1, unifiedDmaIRQHandler,
                               PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(DMA_IRQ_1, true);
        irq1_initialized = true;

    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "PIO_SPI: DMA_IRQ_1 initialized (unified handler for ALL SPI strips)");
    #endif
    }

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "PIO_SPI: DMA initialized - Channel=%d, IRQ=1 (shared)",
                                          channel);
    #endif

    return true;
}

/**
 * Sets RGB color values for an LED
 *
 * Stores RGB color values with default brightness for a specific LED.
 * Input is always logical RGB - driver handles ColorOrder mapping to hardware.
 * For APA102/SK9822, uses global default brightness (scaled to 0-31).
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255, logical RGB)
 * @param g Green component (0-255, logical RGB)
 * @param b Blue component (0-255, logical RGB)
 * @return true if successful, false if index invalid
 */
bool PIO_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || index >= _inst->ledCount) return false;

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    // This prevents flickering from partial buffer updates during DMA read
    if (_inst->busy) return false;

    // Uses global hardware brightness (adjustable via setHardwareBrightness())
    // Brightness range: 16-30 (below 16 flickers, 31/0xFF breaks sync)
    rgbToBuffer(index, r, g, b);
    _inst->dirty = true; // Mark buffer as changed
    return true;
}

/**
 * @brief RGBW not supported for SPI LEDs (APA102/SK9822 have no white channel)
 * @return false (not supported)
 */
bool PIO_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    (void)index;
    (void)r;
    (void)g;
    (void)b;
    (void)w; // Suppress unused parameter warnings
    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "ERROR: setPixel(r,g,b,w) not supported - SPI LEDs have no white channel!");
    #endif
    return false; // SPI LEDs don't support RGBW
}

/**
 * Stores color values in correct format in buffer
 *
 * Internal helper function that stores color values according to
 * ColorOrder setting. Input is always logical RGB, output is hardware format.
 *
 * APA102/SK9822 format: [111xxxxx][C1][C2][C3]
 * - Brightness byte: 0xE0 | (brightness & 0x1F)
 * - Color bytes: Mapped according to ColorOrder (RGB, BGR, GRB, etc.)
 *
 * Uses global hardware brightness from _inst->hwBrightness (adjustable via setHardwareBrightness())
 *
 * @param index LED index
 * @param r Red component (0-255, logical RGB)
 * @param g Green component (0-255, logical RGB)
 * @param b Blue component (0-255, logical RGB)
 */
void PIO_NeoPixel_SPI::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || !_inst->buffer) return;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (index >= _inst->ledCount)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "ERROR: Index %d out of bounds (max %d)",
                                              index, _inst->ledCount - 1);
    #endif
        return;
    }

    // CRITICAL FIX: APA102/SK9822 clone chips have bugs:
    // 1. RGB values below threshold (~6-8) don't update → retain previous color
    // 2. High brightness values (31) can break sync on some clones
    // Solution: Clamp RGB to safe range from config (minRgbValue, maxRgbValue)
    // Note: Values are configurable per strip (0=disabled)
    uint8_t minRgbValue = _inst->minRgbValue;
    uint8_t maxRgbValue = _inst->maxRgbValue;

    if (minRgbValue > 0)
    {
        if (r > 0 && r < minRgbValue) r = minRgbValue;
        if (g > 0 && g < minRgbValue) g = minRgbValue;
        if (b > 0 && b < minRgbValue) b = minRgbValue;
    }

    if (maxRgbValue < 255)
    {
        if (r > maxRgbValue) r = maxRgbValue;
        if (g > maxRgbValue) g = maxRgbValue;
        if (b > maxRgbValue) b = maxRgbValue;
    }

    uint8_t channels[3] = {r, g, b};
    switch (_inst->colorOrder)
    {
        case ColorOrder::RBG:
            channels[1] = b;
            channels[2] = g;
            break;
        case ColorOrder::GRB:
            channels[0] = g;
            channels[1] = r;
            break;
        case ColorOrder::GBR:
            channels[0] = g;
            channels[1] = b;
            channels[2] = r;
            break;
        case ColorOrder::BRG:
            channels[0] = b;
            channels[1] = r;
            channels[2] = g;
            break;
        case ColorOrder::BGR:
            channels[0] = b;
            channels[1] = g;
            channels[2] = r;
            break;
        case ColorOrder::RGB:
        default: break;
    }

    const size_t dataOffset = _inst->hasGlobalBrightness
                                  ? (size_t)_inst->startFrameCount * 4 +
                                        (_inst->dummyLedMode == 1 ? 4 : 0) + (size_t)index * 4
                                  : (size_t)index * _inst->bytesPerLed;
    if (dataOffset + 3 > _inst->bufferSize) return;

    __dmb();
    if (_inst->hasGlobalBrightness)
    {
        if (dataOffset + 4 > _inst->bufferSize) return;
        _inst->buffer[dataOffset] = 0xE0 | (_inst->hwBrightness & 0x1F);
        _inst->buffer[dataOffset + 1] = channels[0];
        _inst->buffer[dataOffset + 2] = channels[1];
        _inst->buffer[dataOffset + 3] = channels[2];
    }
    else if (_inst->protocol == LedProtocol::LPD8806)
    {
        _inst->buffer[dataOffset] = spiEncodeLpd8806Channel(channels[0]);
        _inst->buffer[dataOffset + 1] = spiEncodeLpd8806Channel(channels[1]);
        _inst->buffer[dataOffset + 2] = spiEncodeLpd8806Channel(channels[2]);
    }
    else
    {
        _inst->buffer[dataOffset] = channels[0];
        _inst->buffer[dataOffset + 1] = channels[1];
        _inst->buffer[dataOffset + 2] = channels[2];
    }
    __dmb();

    if (_inst->busy) _inst->dirty = true;
}

/**
 * Sends color data to LED strip
 *
 * Initiates transfer of LED data from buffer to hardware:
 * - Non-blocking: Returns immediately, transfer continues in background (if DMA)
 * - Dirty-flag optimized: Skips transfer if buffer unchanged (prevents flicker)
 * - Busy-flag protected: Skips if previous transfer still in progress
 *
 * Note: APA102/SK9822 don't need CS (Chip Select) pin - data latches automatically!
 *
 * @return true if transfer started or buffer clean, false if error or busy
 */
bool PIO_NeoPixel_SPI::show()
{
    if (!_inst || !_inst->initialized || !_inst->buffer || !_inst->transferBuffer) return false;

    // Skip frame if previous transfer still in progress (busy flag)
    if (_inst->busy) return false;

    // Skip if buffer unchanged since last show() (dirty-flag optimization)
    // This prevents redundant sends which can cause visible flicker on SPI strips
    if (!_inst->dirty) return true;

    prepareTransferBuffer();
    _inst->busy = true;
    _inst->dmaFinished = false;
    _inst->fifoDrainedAtUs = 0;
    if (_inst->csPin >= 0) digitalWrite(_inst->csPin, LOW);

    bool started = false;
    if (_inst->useDMA && _inst->dmaChannel >= 0)
    {
        started = sendDataDMA();
    }
    else
    {
        started = sendDataPIO();
    }

    if (!started)
    {
        _inst->busy = false;
        _inst->dirty = true;
        if (_inst->csPin >= 0) digitalWrite(_inst->csPin, HIGH);
        return false;
    }

    _inst->dirty = false;
    return true;
}

/**
 * Convert the canonical on-wire byte stream into PIO FIFO words. The PIO
 * state machine transmits only the high byte of each word, so no padding byte
 * can become an accidental LED value on a 3-byte protocol.
 */
void PIO_NeoPixel_SPI::prepareTransferBuffer()
{
    if (!_inst || !_inst->buffer || !_inst->transferBuffer) return;

    for (size_t i = 0; i < _inst->bufferSize; ++i)
        _inst->transferBuffer[i] = spiPioWordForByte(_inst->buffer[i]);

    __dmb();
}

/**
 * Sends data directly via PIO (BLOCKING)
 *
 * Fallback method when DMA is unavailable or disabled.
 * Transfers data word-by-word to PIO TX FIFO using CPU.
 *
 * Process:
 * - Iterates through buffer as 32-bit words
 * - pio_sm_put_blocking() waits for FIFO space (blocks if FIFO full)
 * - PIO drains FIFO and outputs to SPI at configured frequency
 *
 * Note: BLOCKING - function returns only after ALL data sent.
 * Use DMA for non-blocking transfers in production.
 */
bool PIO_NeoPixel_SPI::sendDataPIO()
{
    if (!_inst) return false;

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to PIO!
    __dmb(); // Data Memory Barrier

    // One PIO FIFO word represents one wire byte.
    const uint32_t* wordBuffer = _inst->transferBuffer;

    // Send data as 32-bit words to PIO TX FIFO. Use a bounded wait for FIFO space
    // instead of pio_sm_put_blocking(): if the SM ever wedges (FIFO never drains) an
    // unbounded spin here would starve the main loop → 16 s watchdog reboot. This is
    // the no-DMA fallback path only (DMA strips never get here).
    for (size_t i = 0; i < _inst->bufferWordCount; i++)
    {
        const uint32_t startUs = micros();
        while (pio_sm_is_tx_fifo_full(_inst->pio, _inst->sm))
        {
            if ((uint32_t)(micros() - startUs) > 20000) // 20 ms >> any real FIFO wait
            {
                if (_inst->csPin >= 0) digitalWrite(_inst->csPin, HIGH);
                return false;
            }
        }
        pio_sm_put(_inst->pio, _inst->sm, wordBuffer[i]);
    }

    const uint32_t drainStart = micros();
    while (!pio_sm_is_tx_fifo_empty(_inst->pio, _inst->sm))
    {
        if ((uint32_t)(micros() - drainStart) > 20000) return false;
    }
    // The FIFO becoming empty still leaves one byte in the output shifter.
    delayMicroseconds((8000000U + _inst->spiFrequency - 1U) / _inst->spiFrequency + 1U);
    if (_inst->csPin >= 0) digitalWrite(_inst->csPin, HIGH);
    _inst->busy = false;
    return true;
}

/**
 * Starts asynchronous DMA transfer from main buffer
 *
 * Non-blocking: Returns immediately, transfer continues in background.
 * DMA IRQ handler calls onDmaComplete() when finished.
 *
 * Process:
 * 1. Set DMA read address to buffer start
 * 2. DMA auto-transfers configured word count (set in initDMA())
 * 3. DREQ pacing from PIO prevents FIFO overflow
 * 4. IRQ fires on completion → onDmaComplete() clears busy flag
 */
bool PIO_NeoPixel_SPI::sendDataDMA()
{
    if (!_inst) return false;
    if (_inst->dmaChannel < 0)
    {
        return false;
    }

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to DMA!
    // Without this, the CPU might reorder buffer writes AFTER the DMA starts
    __dmb(); // Data Memory Barrier (ARM Cortex-M0+ instruction)

    // Trigger DMA transfer (last parameter true = start immediately)
    // DMA reads PIO-ready words and writes to the PIO TX FIFO.
    dma_channel_set_read_addr(_inst->dmaChannel, _inst->transferBuffer, true);
    return true;
}

/**
 * DMA completion callback
 *
 * Called by unified DMA IRQ handler (pio_dma_shared.h) when transfer completes.
 * Clears busy flag to allow next show() call.
 */
void PIO_NeoPixel_SPI::onDmaComplete()
{
    if (!_inst) return;
    // DMA completion only says that the FIFO was fed.  isBusy() completes the
    // transfer after FIFO and output-shifter drain in normal foreground code.
    _inst->dmaFinished = true;
    _inst->fifoDrainedAtUs = 0;
}

/**
 * Checks if a transfer is in progress
 */
bool PIO_NeoPixel_SPI::isBusy()
{
    if (!_inst || !_inst->busy) return false;
    if (!_inst->dmaFinished) return true;
    if (!pio_sm_is_tx_fifo_empty(_inst->pio, _inst->sm)) return true;

    const uint32_t now = micros();
    if (_inst->fifoDrainedAtUs == 0)
    {
        _inst->fifoDrainedAtUs = now;
        return true;
    }
    const uint32_t finalByteUs = (8000000U + _inst->spiFrequency - 1U) / _inst->spiFrequency + 1U;
    if ((uint32_t)(now - _inst->fifoDrainedAtUs) < finalByteUs) return true;

    _inst->busy = false;
    if (_inst->csPin >= 0) digitalWrite(_inst->csPin, HIGH);
    return false;
}

/**
 * Clears all LED colors to black
 */
void PIO_NeoPixel_SPI::clear()
{
    if (!_inst || !_inst->buffer) return;

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    // Wait for transfer to complete before clearing
    if (_inst->busy) return;

    // CRITICAL: Memory barrier before buffer access
    __dmb();

    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822: Clear all LEDs to black
        const size_t firstLedOffset = (size_t)_inst->startFrameCount * 4 +
                                      (_inst->dummyLedMode == 1 ? 4 : 0);

        uint8_t brightByte = 0xE0 | 30;

        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            const size_t offset = firstLedOffset + (size_t)i * 4;
            _inst->buffer[offset] = brightByte;
            _inst->buffer[offset + 1] = 0;
            _inst->buffer[offset + 2] = 0;
            _inst->buffer[offset + 3] = 0;
        }
    }
    else
    {
        // WS2801 uses zero bytes. LPD8806 requires the update marker even
        // for black, otherwise the chips retain their previous colour.
        memset(_inst->buffer, 0, _inst->bufferSize);
        if (_inst->protocol == LedProtocol::LPD8806)
            memset(_inst->buffer, 0x80, _inst->bufferSize);
    }

    // CRITICAL: Memory barrier after buffer writes
    __dmb();

    _inst->dirty = true;
}

/**
 * Returns the driver's capabilities
 */
DriverCapabilities PIO_NeoPixel_SPI::getCapabilities() const
{
    DriverCapabilities caps = {};
    caps.supportsRGBW = false;
    caps.supportsDMA = (_inst && _inst->useDMA);
    caps.supportsAsync = caps.supportsDMA;
    caps.maxFrequency = _inst ? _inst->spiFrequency : 10000000;
    caps.maxLeds = 2000;
    return caps;
}

/**
 * Register a DMA handler for a specific channel
 * @param channel DMA channel number
 * @param instance Pointer to the PIO_NeoPixel_SPI instance
 */
void PIO_NeoPixel_SPI::registerDMAHandler(int channel, PIO_NeoPixel_SPI* instance)
{
    setSPIDMAHandler(channel, instance);
}

/**
 * Unregister a DMA handler for a specific channel
 * @param channel DMA channel number
 */
void PIO_NeoPixel_SPI::unregisterDMAHandler(int channel)
{
    setSPIDMAHandler(channel, nullptr);
}

/*
 * Legacy DMA IRQ handler - delegates to unified handler
 */
void PIO_NeoPixel_SPI::dmaIRQHandler()
{
    unifiedDmaIRQHandler();
}

// =============================================================================
// Configuration Management (IHardwareDriver Interface)
// =============================================================================

/**
 * @brief Create default SPI strip configuration
 * @return New SpiStripConfig with driver-specific defaults
 */
PhysicalStripConfig* PIO_NeoPixel_SPI::createDefaultConfig() const
{
    SpiStripConfig* cfg = new SpiStripConfig();

    // Set defaults from current instance (if available)
    if (_inst)
    {
        cfg->setColorOrder(_inst->colorOrder);
        cfg->setHwBrightness(_inst->hwBrightness);
        cfg->setDummyLedMode(_inst->dummyLedMode);
        cfg->setStartFrameCount(_inst->startFrameCount);
        cfg->setEndFrameCount(_inst->endFrameCount);
        cfg->setEndFramePattern(_inst->endFramePattern);
        cfg->setStartFrameDelayUs(_inst->startFrameDelayUs);
        cfg->setAutoDetectChip(_inst->autoDetectChip);
        cfg->setDetectedChip(_inst->detectedChip);
        cfg->setSpiFrequency(_inst->spiFrequency);
        cfg->setMinRgbValue(_inst->minRgbValue);                                   // Clone chip workaround
        cfg->setMaxRgbValue(_inst->maxRgbValue);                                   // Power limiting / clone workaround
        cfg->setHwBrightnessRange(_inst->hwBrightnessMin, _inst->hwBrightnessMax); // Clone chip safe range
    }

    return cfg;
}

bool PIO_NeoPixel_SPI::rebuildFrameBuffers()
{
    if (!_inst || _inst->initialized) return false;

    SpiFrameLayout layout = {};
    if (!spiMakeFrameLayout(_inst->protocol, _inst->ledCount, _inst->startFrameCount,
                            _inst->dummyLedMode, _inst->endFrameCount, layout))
        return false;

    uint8_t* replacement = static_cast<uint8_t*>(calloc(layout.bufferSize, sizeof(uint8_t)));
    uint32_t* replacementWords = static_cast<uint32_t*>(calloc(layout.bufferSize, sizeof(uint32_t)));
    if (!replacement || !replacementWords)
    {
        free(replacement);
        free(replacementWords);
        return false;
    }

    if (layout.hasGlobalBrightness)
    {
        const size_t firstLedOffset = layout.startFrameSize + layout.dummyLedSize;
        if (_inst->dummyLedMode == 1) replacement[layout.startFrameSize] = 0xE0;
        const uint8_t bright = 0xE0 | (_inst->hwBrightness & 0x1F);
        for (uint16_t i = 0; i < _inst->ledCount; ++i)
            replacement[firstLedOffset + static_cast<size_t>(i) * 4] = bright;
        memset(replacement + firstLedOffset + layout.pixelDataSize,
               _inst->endFramePattern, layout.endFrameSize);
    }
    else if (_inst->protocol == LedProtocol::LPD8806)
    {
        memset(replacement, 0x80, layout.bufferSize);
    }

    free(_inst->buffer);
    free(_inst->transferBuffer);
    _inst->buffer = replacement;
    _inst->transferBuffer = replacementWords;
    _inst->bufferSize = layout.bufferSize;
    _inst->bufferWordCount = layout.bufferSize;
    _inst->bytesPerLed = layout.bytesPerLed;
    _inst->hasGlobalBrightness = layout.hasGlobalBrightness;
    _inst->dirty = true;
    return true;
}

/**
 * @brief Apply configuration to driver
 * @param config SpiStripConfig to apply
 * @return true if applied successfully
 */
bool PIO_NeoPixel_SPI::applyConfig(const PhysicalStripConfig* config)
{
    if (!_inst || !config) return false;

    // Type check - must be SpiStripConfig
    const SpiStripConfig* spiCfg = dynamic_cast<const SpiStripConfig*>(config);
    if (!spiCfg) return false;

    const bool layoutChanged = _inst->dummyLedMode != spiCfg->getDummyLedMode() ||
                               _inst->startFrameCount != spiCfg->getStartFrameCount() ||
                               _inst->endFrameCount != spiCfg->getEndFrameCount() ||
                               _inst->endFramePattern != spiCfg->getEndFramePattern();
    if (spiCfg->getStartFrameDelayUs() != 0) return false;
    if (_inst->initialized && layoutChanged) return false;

    // Apply ColorOrder
    _inst->colorOrder = spiCfg->getColorOrder();

    // Apply SPI-specific settings
    _inst->hwBrightness = spiCfg->getHwBrightness();
    _inst->endFramePattern = spiCfg->getEndFramePattern();
    _inst->startFrameDelayUs = 0;
    _inst->autoDetectChip = spiCfg->getAutoDetectChip();
    _inst->detectedChip = spiCfg->getDetectedChip();
    _inst->minRgbValue = spiCfg->getMinRgbValue();         // Clone chip workaround
    _inst->maxRgbValue = spiCfg->getMaxRgbValue();         // Power limiting / clone workaround
    _inst->hwBrightnessMin = spiCfg->getHwBrightnessMin(); // Clone chip safe range
    _inst->hwBrightnessMax = spiCfg->getHwBrightnessMax(); // Clone chip safe range

    // Apply SPI frequency (can be changed live if initialized)
    uint32_t newFreq = spiCfg->getSpiFrequency();
    if (newFreq == 0) return false;
    if (_inst->spiFrequency != newFreq)
    {
        _inst->spiFrequency = newFreq;

        // If already initialized, update PIO clock divider immediately
        if (_inst->initialized && _inst->pio && _inst->sm < 4)
        {
            float sys_clk = (float)clock_get_hz(clk_sys);
            float div = sys_clk / (2.0f * (float)newFreq);
            if (div < 1.0f) div = 1.0f;

            // Update stored clkdiv and apply to PIO
            _inst->clkdiv = div;
            pio_sm_set_clkdiv(_inst->pio, _inst->sm, div);
        }
    }

    // Settings that require re-init (only apply if not initialized)
    if (!_inst->initialized)
    {
        _inst->dummyLedMode = spiCfg->getDummyLedMode();
        _inst->startFrameCount = spiCfg->getStartFrameCount();
        _inst->endFrameCount = spiCfg->getEndFrameCount();
        if (layoutChanged && !rebuildFrameBuffers()) return false;
    }

    return true;
}

// =============================================================================
// Extended SPI Configuration API Implementation
// =============================================================================

/**
 * @brief Set dummy LED mode (requires buffer re-allocation, call before init!)
 * @param mode 0=none (first LED wrong color), 1=physical (sacrifice LED#0), 2=virtual (LED#0 forced black)
 */
void PIO_NeoPixel_SPI::setDummyLedMode(uint8_t mode)
{
    if (!_inst) return;
    if (mode > 2) mode = 1; // Clamp to valid range

    if (_inst->initialized)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "WARNING: Cannot change dummyLedMode after init() - requires re-init!");
    #endif
        return;
    }

    const uint8_t previous = _inst->dummyLedMode;
    _inst->dummyLedMode = mode;
    if (!rebuildFrameBuffers()) _inst->dummyLedMode = previous;
}

/**
 * @brief Set start frame count (1-8, default: 8)
 */
void PIO_NeoPixel_SPI::setStartFrameCount(uint8_t count)
{
    if (!_inst) return;
    if (count < 1) count = 1;
    if (count > 8) count = 8;

    if (_inst->initialized)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "WARNING: Cannot change startFrameCount after init() - requires re-init!");
    #endif
        return;
    }

    const uint8_t previous = _inst->startFrameCount;
    _inst->startFrameCount = count;
    if (!rebuildFrameBuffers()) _inst->startFrameCount = previous;
}

/**
 * @brief Set end frame count (1-80, default: 1)
 * WARNING: Cannot be changed after init() - requires buffer re-allocation!
 */
void PIO_NeoPixel_SPI::setEndFrameCount(uint8_t count)
{
    if (!_inst) return;
    if (count < 1) count = 1;
    if (count > 80) count = 80;

    if (_inst->initialized)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "WARNING: Cannot change endFrameCount after init() - requires re-init!");
    #endif
        return;
    }

    const uint8_t previous = _inst->endFrameCount;
    _inst->endFrameCount = count;
    if (!rebuildFrameBuffers()) _inst->endFrameCount = previous;
}

/**
 * @brief Set delay after start frames in microseconds (0-1000, default: 0)
 */
void PIO_NeoPixel_SPI::setStartFrameDelayUs(uint32_t delayUs)
{
    if (!_inst) return;
    if (delayUs != 0)
    {
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "WARNING: startFrameDelayUs is unsupported");
        return;
    }
    _inst->startFrameDelayUs = 0;
}

/**
 * @brief Set end frame pattern (0x00 for APA102, 0xFF for SK9822)
 */
void PIO_NeoPixel_SPI::setEndFramePattern(uint8_t pattern)
{
    if (!_inst) return;
    if (_inst->initialized) return;
    const uint8_t previous = _inst->endFramePattern;
    _inst->endFramePattern = pattern;
    if (!rebuildFrameBuffers()) _inst->endFramePattern = previous;
}

/**
 * @brief Enable/disable chip auto-detection
 */
void PIO_NeoPixel_SPI::setAutoDetectChip(bool enable)
{
    if (!_inst) return;
    _inst->autoDetectChip = enable;
}

/**
 * @brief Run chip auto-detection (APA102 vs SK9822)
 *
 * Detection Method:
 * 1. Send test pattern with end frame 0x00000000
 * 2. If LEDs work correctly → APA102
 * 3. Send test pattern with end frame 0xFFFFFFFF
 * 4. If LEDs work correctly → SK9822
 *
 * @return Detected chip type (APA102 or SK9822)
 *
 * @note This is a destructive test - LEDs will flash during detection
 * @note Detection accuracy depends on LED count and chain stability
 */
LedProtocol PIO_NeoPixel_SPI::detectChipType()
{
    if (!_inst || !_inst->initialized)
    {
    #ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                              "ERROR: Cannot auto-detect chip - driver not initialized!");
    #endif
        return _inst ? _inst->protocol : LedProtocol::APA102;
    }

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "Auto-detecting chip type (APA102 vs SK9822)...");
    #endif

    // Test 1: End frame 0x00000000 (APA102 standard)
    _inst->endFramePattern = 0x00;
    clear();
    setPixel(0, 255, 0, 0); // Red on first LED
    show();
    delay(200); // Give time to stabilize

    // Test 2: End frame 0xFFFFFFFF (SK9822 standard)
    _inst->endFramePattern = 0xFF;
    clear();
    setPixel(0, 0, 255, 0); // Green on first LED
    show();
    delay(200);

    // Heuristic: APA102 works with both patterns, SK9822 prefers 0xFF
    // For practical use: Check LED count - longer chains often indicate APA102
    // Real detection would require visual confirmation or photo sensor

    LedProtocol detected;
    if (_inst->ledCount > 100)
    {
        // Long chains (>100 LEDs) typically use genuine APA102
        detected = LedProtocol::APA102;
        _inst->endFramePattern = 0x00;
    }
    else
    {
        // Shorter chains often use SK9822 clones
        detected = LedProtocol::SK9822;
        _inst->endFramePattern = 0xFF;
    }

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "Chip detection: %s (LED count: %d, end frame: 0x%02X)",
                                          detected == LedProtocol::APA102 ? "APA102" : "SK9822",
                                          _inst->ledCount,
                                          _inst->endFramePattern);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "NOTE: Auto-detection is heuristic-based. Use setSpi...() for manual override.");
    #endif

    clear();
    show();

    _inst->detectedChip = detected;
    return detected;
}

/**
 * Set hardware brightness for all LEDs
 *
 * @param brightness Hardware brightness (16-30 safe range, clamps to valid range)
 * @note Must call show() after this to update LEDs
 * @note Brightness 16-30 is safe range (below 16 flickers, 31/0xFF breaks sync on clones)
 * @note UPDATES ALL EXISTING PIXELS in buffer with new brightness!
 */
void PIO_NeoPixel_SPI::setHardwareBrightness(uint8_t brightness)
{
    if (!_inst) return;

    // CRITICAL: Don't modify buffer while DMA transfer is in progress!
    // This prevents flickering from partial buffer updates during DMA read
    if (_inst->busy) return;

    // Clamp to safe range 16-30
    // if (brightness < 16) brightness = 16;
    // if (brightness > 30) brightness = 30;

    _inst->hwBrightness = brightness; // Update global brightness

    // CRITICAL: Update ALL existing pixels in buffer with new brightness!
    // Only change brightness byte (byte 0), preserve RGB values (bytes 1-3)
    if (_inst->hasGlobalBrightness && _inst->buffer)
    {
        const size_t firstLedOffset = (size_t)_inst->startFrameCount * 4 +
                                      (_inst->dummyLedMode == 1 ? 4 : 0);

        uint8_t brightByte = 0xE0 | (brightness & 0x1F); // New brightness byte

        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            _inst->buffer[firstLedOffset + (size_t)i * 4] = brightByte;
        }
    }

    _inst->dirty = true; // Mark buffer as dirty to force update

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "Set hardware brightness: %d (safe range 16-30), updated %d LEDs",
                                          brightness, _inst->ledCount);
    #endif
}

/**
 * Get actual SPI clock frequency from PIO divider
 *
 * @return Actual SPI frequency in Hz (or 0 if not initialized)
 * @note Useful for debugging clock divider calculation
 */
float PIO_NeoPixel_SPI::getActualSpiFrequency() const
{
    if (!_inst || !_inst->initialized) return 0.0f;

    // Read clock divider directly from PIO SM registers
    // Clock divider is in SM CLKDIV register (16.16 fixed point format)
    uint32_t clkdiv_reg = _inst->pio->sm[_inst->sm].clkdiv;
    float divider = (float)clkdiv_reg / (1 << 16);

    // Calculate actual frequency:
    // SM clock = System clock / divider
    // SPI clock = SM clock / 2 (2 cycles per bit in PIO program)
    uint32_t sys_clk = clock_get_hz(clk_sys);
    float sm_freq = (float)sys_clk / divider;
    float spi_freq = sm_freq / 2.0f;

    return spi_freq;
}

#endif // ARDUINO_ARCH_RP2040
