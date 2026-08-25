#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_spi.h"
    #include "../PhysicalStripConfig.h"
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
 * Autopull pulls next 32-bit word from TX FIFO (threshold 32).
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

    // Out shift: shift_right=FALSE for MSB-first
    // false = shift left = output from MSB (bit 31 first)
    // Autopull enabled, 32 bits per pull
    sm_config_set_out_shift(&c, false, true, 32);
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
    _inst->useDMA = true;   // Enable DMA by default (falls back to PIO if unavailable)
    _inst->dmaChannel = -1; // Not yet claimed
    _inst->dmaIrqNum = -1;  // Not yet assigned
    // ===== Extended SPI Configuration Defaults =====
    // These settings optimize APA102/SK9822 compatibility (adjustable via API)
    _inst->dummyLedMode = 1;        // Physical dummy LED: sacrifice LED#0 to fix first LED color
    _inst->startFrameCount = 8;     // 8 start frames (0x00000000) for data sync
    // APA102 needs one clock edge per two LEDs after the data, i.e. ledCount/16 bytes.
    // A fixed single 4-byte frame only carries 64 LEDs; longer chains never latch the tail.
    _inst->endFrameCount = (uint32_t)((ledCount + 63) / 64);
    if (_inst->endFrameCount < 1) _inst->endFrameCount = 1;
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

    // Frame width and framing per protocol. The APA102 family keeps the layout it always
    // had; the others differ in width, start frames and how the data is latched.
    _inst->bytesPerLed = ProtocolHelper::getBytesPerLed(protocol);
    _inst->hasGlobalBrightness = (protocol == LedProtocol::APA102 || protocol == LedProtocol::APA102_CLONE || protocol == LedProtocol::SK9822);

    switch (protocol)
    {
        case LedProtocol::WS2801:
            // Raw RGB, no framing. The chip latches on an idle gap between frames.
            _inst->startFrameCount = 0;
            _inst->endFrameCount = 0;
            _inst->dummyLedMode = 0;
            break;

        case LedProtocol::LPD8806:
            // No start frame; one zero byte per 32 LEDs latches the chain.
            _inst->startFrameCount = 0;
            _inst->endFrameCount = (uint32_t)((((ledCount + 31) / 32) + 3) / 4);
            if (_inst->endFrameCount < 1) _inst->endFrameCount = 1;
            _inst->dummyLedMode = 0;
            break;

        case LedProtocol::LPD6803:
        case LedProtocol::P9813:
            // One 4-byte zero frame on each side.
            _inst->startFrameCount = 1;
            _inst->endFrameCount = 1;
            _inst->dummyLedMode = 0;
            break;

        default:
            break; // APA102 family keeps the defaults set above
    }

    // ===== Buffer Allocation =====
    // Buffer layout: [START_FRAMES] [DUMMY_LED] [LED_DATA] [END_FRAMES]
    // This structure ensures proper APA102/SK9822 protocol framing:
    // - Start Frames: N * 4 bytes (0x00000000) - synchronize data stream
    // - Dummy LED: 4 bytes if Mode 1 (0xE0 + Brightness + RGB) - fixes first LED color bug
    // - LED Data: ledCount * 4 bytes each [111bbbbb][C1][C2][C3] - actual LED colors
    // - End Frames: M * 4 bytes (pattern 0x00 or 0xFF) - latch data to LEDs

    // CRITICAL: Use size_t for intermediate calculations to prevent overflow!
    // With uint16_t: ledCount=20000 * 4 = 80000 > 65535 → OVERFLOW!
    size_t startFrameSize = (size_t)_inst->startFrameCount * 4; // Typically 8 frames = 32 bytes
    size_t dummyLedSize = (_inst->dummyLedMode == 1) ? 4 : 0;   // 4 bytes if physical dummy
    size_t endFrameSize = (size_t)_inst->endFrameCount * 4;     // Typically 1 frame = 4 bytes

    _inst->bufferSize = startFrameSize + dummyLedSize + ((size_t)ledCount * _inst->bytesPerLed) + endFrameSize;

    // CRITICAL: Allocate as uint32_t aligned (DMA and atomic writes require this)
    // Buffer must be 32-bit aligned for direct word writes (no byte-by-byte access)
    // Round UP to nearest word boundary to prevent buffer underallocation
    _inst->bufferWordCount = (_inst->bufferSize + 3) / 4; // Round up: (size + 3) / 4
    uint32_t* wordBuffer = (uint32_t*)malloc(_inst->bufferWordCount * sizeof(uint32_t));
    _inst->buffer = (uint8_t*)wordBuffer; // Keep uint8_t* for compatibility

    if (_inst->buffer)
    {
        // Clear the ENTIRE allocated buffer (use word count, not logical byte size)
        memset(_inst->buffer, 0, _inst->bufferWordCount * 4);

        // Re-cast for word-based initialization
        uint32_t* words = (uint32_t*)_inst->buffer;

        // Initialize start frames (all zeros per APA102/SK9822 spec)
        for (uint16_t i = 0; i < _inst->startFrameCount; i++)
        {
            words[i] = 0x00000000;
        }

        // Initialize dummy LED (Mode 1: Physical)
        // Dummy LED at brightness 0 (OFF) - matches test_sk9822_clean.cpp line 183: 0xE0000000
        if (_inst->dummyLedMode == 1)
        {
            uint32_t dummyIdx = _inst->startFrameCount;
            words[dummyIdx] = 0xE0000000; // [0xE0 = 111 00000 = brightness 0][B=0][G=0][R=0]
        }

        // Pre-set the brightness byte so rgbToBuffer can write a whole word without a
        // read-modify-write. Only the APA102 family carries that byte.
        if (_inst->hasGlobalBrightness)
        {
            uint32_t firstLedIdx = _inst->startFrameCount + ((_inst->dummyLedMode == 1) ? 1 : 0);
            uint8_t defaultBright = 0xE0 | 30;
            uint32_t defaultLedWord = ((uint32_t)defaultBright << 24) | 0x000000;

            for (uint16_t i = 0; i < _inst->ledCount; i++)
            {
                words[firstLedIdx + i] = defaultLedWord;
            }

            uint32_t endFrameStartIdx = firstLedIdx + _inst->ledCount;
            uint32_t endFrameValue = (_inst->endFramePattern == 0xFF) ? 0xFFFFFFFF : 0x00000000;
            for (uint32_t i = 0; i < _inst->endFrameCount; i++)
            {
                words[endFrameStartIdx + i] = endFrameValue;
            }
        }
        else if (_inst->protocol == LedProtocol::LPD6803)
        {
            // LPD6803 words must keep their leading 1 bit even while black.
            const size_t dataOffset = (size_t)_inst->startFrameCount * 4;
            for (uint16_t i = 0; i < _inst->ledCount; i++)
            {
                _inst->buffer[dataOffset + (size_t)i * 2] = 0x80;
                _inst->buffer[dataOffset + (size_t)i * 2 + 1] = 0x00;
            }
        }
        else if (_inst->protocol == LedProtocol::LPD8806)
        {
            // LPD8806 idles at 0x80: bit 7 set marks a data byte, value 0.
            const size_t dataOffset = (size_t)_inst->startFrameCount * 4;
            memset(_inst->buffer + dataOffset, 0x80, (size_t)_inst->ledCount * 3);
        }
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
                while (dma_channel_is_busy(_inst->dmaChannel)) { tight_loop_contents(); }
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
    if (!_inst || !_inst->buffer) return false;
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
 * - Transfer size: 32-bit words (matches PIO autopull=32 setting)
 * - Source: Buffer pointer (increments by 4 bytes per word)
 * - Destination: PIO TX FIFO (fixed address, auto-drains to PIO)
 * - Pacing: DREQ from PIO controls transfer timing (prevents FIFO overflow)
 * - IRQ: DMA_IRQ_1 shared by ALL SPI strips (IRQ_0 used by Serial strips)
 *
 * Why 32-bit words? PIO configured with autopull=32 expects full 32-bit values.
 * Buffer must be 4-byte aligned and size must be multiple of 4.
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
    // CRITICAL: We use autopull=32 in PIO, so DMA MUST transfer 32-bit words!
    // Buffer size must be multiple of 4 bytes (e.g., 408 bytes = 102 words)
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);                 // 32-bit word transfers (4 bytes per transfer)
    channel_config_set_read_increment(&c, true);                            // Increment read pointer by 4 bytes after each word
    channel_config_set_write_increment(&c, false);                          // Write to same FIFO address (PIO drains it)
    channel_config_set_dreq(&c, pio_get_dreq(_inst->pio, _inst->sm, true)); // PIO TX DREQ paces DMA (prevents FIFO overflow)

    dma_channel_configure(
        channel,
        &c,
        &_inst->pio->txf[_inst->sm], // Destination: PIO TX FIFO (32-bit register)
        _inst->buffer,               // Source: Main buffer (direct transfer, no intermediate copy)
        _inst->bufferWordCount,      // Transfer count in 32-bit words (stored during allocation)
        false                        // Don't start yet (triggered by show() → sendDataDMA())
    );

    // Register interrupt handler in global registry
    registerDMAHandler(channel, this);

    // ALL SPI strips use DMA_IRQ_1 (shared IRQ for all SPI types)
    dma_channel_set_irq1_enabled(channel, true);

    static bool irq1_initialized = false;
    if (!irq1_initialized)
    {
        irq_set_exclusive_handler(DMA_IRQ_1, unifiedDmaIRQHandler);
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

    // Buffer layout: [START_FRAMES(N words)] [DUMMY_LED(1 word if mode=1)] [LED_DATA(N words)] [END_FRAMES(M words)]
    // Calculate word index for this LED:
    // - Skip start frames (typically 8 words)
    // - Skip dummy LED if mode=1 (1 word)
    // - Add LED index to reach target LED position
    uint32_t startFrameWords = _inst->startFrameCount;         // Typically 8
    uint32_t dummyWords = (_inst->dummyLedMode == 1) ? 1 : 0;  // 1 if physical dummy, 0 otherwise
    uint32_t wordIndex = startFrameWords + dummyWords + index; // Final word position in buffer

    uint32_t* wordBuffer = (uint32_t*)_inst->buffer;

    #ifdef OPENKNX_NEOPIXEL_TRACE1
    if (index < 1) // Only debug first LED
    {
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                              "rgbToBuffer[%d]: R=%d G=%d B=%d Bright=%d, WordIdx=%d",
                                              index, r, g, b, _inst->hwBrightness, wordIndex);
    }
    #endif

    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822: Pack as 32-bit word
        // Format: [Byte3: Brightness | Byte2: Color2 | Byte1: Color1 | Byte0: Color0]
        uint8_t brightByte = 0xE0 | (_inst->hwBrightness & 0x1F); // 111xxxxx (5-bit brightness)

        // Build RGB value (24-bit) according to ColorOrder
        // Each case maps logical RGB → hardware byte positions
        uint32_t rgbValue;
        switch (_inst->colorOrder)
        {
            case ColorOrder::RGB: // [R][G][B]
                rgbValue = (r << 16) | (g << 8) | b;
                break;
            case ColorOrder::RBG: // [R][B][G]
                rgbValue = (r << 16) | (b << 8) | g;
                break;
            case ColorOrder::GRB: // [G][R][B]
                rgbValue = (g << 16) | (r << 8) | b;
                break;
            case ColorOrder::GBR: // [G][B][R]
                rgbValue = (g << 16) | (b << 8) | r;
                break;
            case ColorOrder::BRG: // [B][R][G]
                rgbValue = (b << 16) | (r << 8) | g;
                break;
            case ColorOrder::BGR: // [B][G][R] (APA102 standard)
            default:
                rgbValue = (b << 16) | (g << 8) | r;
                break;
        }

        // Write complete 32-bit word (atomic operation, DMA-safe)
        // Result: [Brightness << 24 | Byte2 << 16 | Byte1 << 8 | Byte0]

        // CRITICAL: Memory barrier BEFORE write to ensure previous operations complete
        __dmb();

        wordBuffer[wordIndex] = ((uint32_t)brightByte << 24) | (rgbValue & 0x00FFFFFF);

        // CRITICAL: Memory barrier AFTER write to ensure write is visible before busy check
        __dmb();

        // CRITICAL: Double-check busy flag AFTER write to detect race with show()
        // If DMA started between our initial check and write, invalidate this write
        if (_inst->busy)
        {
            // DMA started during our write - mark buffer dirty so next show() will resend
            _inst->dirty = true;
        }

    #ifdef OPENKNX_NEOPIXEL_TRACE1
        if (index < 1)
        {
            openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                                  "  ColorOrder=%d, Written word[%d]=0x%08X [Bright=0x%02X][RGB=0x%06X]",
                                                  (int)_inst->colorOrder, wordIndex, wordBuffer[wordIndex],
                                                  brightByte, rgbValue & 0x00FFFFFF);
        }
    #endif
    }
    else
    {
        // Protocols without a brightness byte. Frame widths differ (2, 3 or 4 bytes), so
        // these write bytes rather than a whole word.
        uint8_t ch[6] = {0};
        ProtocolHelper::orderChannels(_inst->colorOrder, r, g, b, 0, 0, ch);

        const size_t dataOffset = (size_t)_inst->startFrameCount * 4;
        const size_t off = dataOffset + (size_t)index * _inst->bytesPerLed;
        if (off + _inst->bytesPerLed > _inst->bufferSize) return;

        __dmb();

        switch (_inst->protocol)
        {
            case LedProtocol::LPD8806:
                // 7-bit channels, bit 7 marks a data byte.
                _inst->buffer[off]     = (uint8_t)(0x80 | (ch[0] >> 1));
                _inst->buffer[off + 1] = (uint8_t)(0x80 | (ch[1] >> 1));
                _inst->buffer[off + 2] = (uint8_t)(0x80 | (ch[2] >> 1));
                break;

            case LedProtocol::LPD6803:
            {
                // 1 + 5R + 5G + 5B, MSB first.
                const uint16_t w = (uint16_t)(0x8000 |
                                              ((uint16_t)(ch[0] >> 3) << 10) |
                                              ((uint16_t)(ch[1] >> 3) << 5) |
                                              (uint16_t)(ch[2] >> 3));
                _inst->buffer[off]     = (uint8_t)(w >> 8);
                _inst->buffer[off + 1] = (uint8_t)(w & 0xFF);
                break;
            }

            case LedProtocol::P9813:
            {
                // Flag byte carries the inverted top two bits of each channel.
                const uint8_t flag = (uint8_t)(0xC0 |
                                               ((uint8_t)(~ch[2] >> 6) & 0x03) << 4 |
                                               ((uint8_t)(~ch[1] >> 6) & 0x03) << 2 |
                                               ((uint8_t)(~ch[0] >> 6) & 0x03));
                _inst->buffer[off]     = flag;
                _inst->buffer[off + 1] = ch[2];
                _inst->buffer[off + 2] = ch[1];
                _inst->buffer[off + 3] = ch[0];
                break;
            }

            default: // WS2801 and anything else: raw bytes in colour order
                _inst->buffer[off]     = ch[0];
                _inst->buffer[off + 1] = ch[1];
                _inst->buffer[off + 2] = ch[2];
                break;
        }

        __dmb();

        // Double-check busy flag to detect race with show()
        if (_inst->busy)
        {
            _inst->dirty = true; // Mark for resend
        }
    }
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
    if (!_inst || !_inst->initialized || !_inst->buffer) return false;

    // Skip frame if previous transfer still in progress (busy flag)
    if (_inst->busy) return false;

    // Skip if buffer unchanged since last show() (dirty-flag optimization)
    // This prevents redundant sends which can cause visible flicker on SPI strips
    if (!_inst->dirty) return true;

    _inst->busy = true;
    _inst->dirty = false;

    if (_inst->useDMA && _inst->dmaChannel >= 0)
    {
        sendDataDMA();
    }
    else
    {
        sendDataPIO();
        _inst->busy = false;
    }

    return true;
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
void PIO_NeoPixel_SPI::sendDataPIO()
{
    if (!_inst) return;

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to PIO!
    __dmb(); // Data Memory Barrier

    // Use stored word count (already calculated during allocation)
    // CRITICAL: Must use bufferWordCount, NOT bufferSize/4 (can cause rounding errors)
    uint32_t* wordBuffer = (uint32_t*)_inst->buffer;

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
                _inst->busy = false;
                return; // abandon the frame; next show() re-arms
            }
        }
        pio_sm_put(_inst->pio, _inst->sm, wordBuffer[i]);
    }

    if (_inst->csPin >= 0)
    {
        digitalWrite(_inst->csPin, HIGH); // CS inactive
    }

    _inst->busy = false;
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
void PIO_NeoPixel_SPI::sendDataDMA()
{
    if (!_inst) return; // can't clear busy on a null instance — must check first
    if (_inst->dmaChannel < 0)
    {
        _inst->busy = false;
        return;
    }

    // CRITICAL: Memory barrier to ensure all buffer writes are visible to DMA!
    // Without this, the CPU might reorder buffer writes AFTER the DMA starts
    __dmb(); // Data Memory Barrier (ARM Cortex-M0+ instruction)

    // CRITICAL: re-set BOTH read address AND transfer count every frame. After a completed DMA the
    // trans_count register is 0 (NORMAL mode, no auto-reload), so re-triggering with only
    // set_read_addr would transfer 0 words -> strip frozen on frame 1. Mirror the Serial driver,
    // which documents+fixes the same hazard: set count, then start.
    dma_channel_set_read_addr(_inst->dmaChannel, _inst->buffer, false);
    dma_channel_set_trans_count(_inst->dmaChannel, _inst->bufferWordCount, false);
    dma_channel_start(_inst->dmaChannel);
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
    _inst->busy = false; // Transfer complete, ready for next frame
}

/**
 * Checks if a transfer is in progress
 */
bool PIO_NeoPixel_SPI::isBusy()
{
    return _inst ? _inst->busy : false;
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

    uint32_t* wordBuffer = (uint32_t*)_inst->buffer;

    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822: Clear all LEDs to black
        uint32_t startFrameWords = _inst->startFrameCount;
        uint32_t dummyWords = (_inst->dummyLedMode == 1) ? 1 : 0;
        uint32_t firstLedWordIdx = startFrameWords + dummyWords;

        uint8_t brightByte = 0xE0 | 30;
        uint32_t blackWord = ((uint32_t)brightByte << 24) | 0x00000000;

        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            wordBuffer[firstLedWordIdx + i] = blackWord;
        }
    }
    else
    {
        // Other SPI protocols: Clear to zero
        uint32_t startFrameWords = _inst->startFrameCount;
        uint32_t dummyWords = (_inst->dummyLedMode == 1) ? 1 : 0;
        uint32_t firstLedWordIdx = startFrameWords + dummyWords;

        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            wordBuffer[firstLedWordIdx + i] = 0x00000000;
        }
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
    DriverCapabilities caps;
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
    if (channel >= 0 && channel < MAX_DMA_CHANNELS)
    {
        g_spiHandlers[channel] = instance; // Use global registry
    }
}

/**
 * Unregister a DMA handler for a specific channel
 * @param channel DMA channel number
 */
void PIO_NeoPixel_SPI::unregisterDMAHandler(int channel)
{
    if (channel >= 0 && channel < MAX_DMA_CHANNELS)
    {
        g_spiHandlers[channel] = nullptr; // Use global registry
    }
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

    // Apply ColorOrder
    // Never let a NONE config stomp the live driver order.
    const ColorOrder spiCfgOrder = spiCfg->getColorOrder();
    if (spiCfgOrder != ColorOrder::NONE) _inst->colorOrder = spiCfgOrder;

    // Apply SPI-specific settings
    _inst->hwBrightness = spiCfg->getHwBrightness();
    _inst->endFramePattern = spiCfg->getEndFramePattern();
    _inst->startFrameDelayUs = spiCfg->getStartFrameDelayUs();
    _inst->autoDetectChip = spiCfg->getAutoDetectChip();
    _inst->detectedChip = spiCfg->getDetectedChip();
    _inst->minRgbValue = spiCfg->getMinRgbValue();         // Clone chip workaround
    _inst->maxRgbValue = spiCfg->getMaxRgbValue();         // Power limiting / clone workaround
    _inst->hwBrightnessMin = spiCfg->getHwBrightnessMin(); // Clone chip safe range
    _inst->hwBrightnessMax = spiCfg->getHwBrightnessMax(); // Clone chip safe range

    // Apply SPI frequency (can be changed live if initialized)
    uint32_t newFreq = spiCfg->getSpiFrequency();
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

    _inst->dummyLedMode = mode;
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

    _inst->startFrameCount = count;
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

    _inst->endFrameCount = count;
}

/**
 * @brief Set delay after start frames in microseconds (0-1000, default: 0)
 */
void PIO_NeoPixel_SPI::setStartFrameDelayUs(uint32_t delayUs)
{
    if (!_inst) return;
    if (delayUs > 1000) delayUs = 1000;

    _inst->startFrameDelayUs = delayUs;
}

/**
 * @brief Set end frame pattern (0x00 for APA102, 0xFF for SK9822)
 */
void PIO_NeoPixel_SPI::setEndFramePattern(uint8_t pattern)
{
    if (!_inst) return;
    _inst->endFramePattern = pattern;
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

    // Guess from chain length. Nothing is measured: the chip gives no readback on this
    // bus, so telling APA102 from SK9822 needs the end-frame behaviour observed optically.
    // A previous version flashed the strip and waited 400 ms in delay() before applying
    // this same rule, which blocked loop() and changed nothing about the outcome.

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
                                          "Chip guess from chain length: %s (LED count: %d, end frame: 0x%02X)",
                                          detected == LedProtocol::APA102 ? "APA102" : "SK9822",
                                          _inst->ledCount,
                                          _inst->endFramePattern);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "NOTE: not a measurement - set the chip explicitly if the guess is wrong.");
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
    if (brightness < _inst->hwBrightnessMin) brightness = _inst->hwBrightnessMin;
    if (brightness > _inst->hwBrightnessMax) brightness = _inst->hwBrightnessMax;

    _inst->hwBrightness = brightness; // Update global brightness

    // CRITICAL: Update ALL existing pixels in buffer with new brightness!
    // Only change brightness byte (byte 0), preserve RGB values (bytes 1-3)
    if (_inst->hasGlobalBrightness && _inst->buffer)
    {
        uint32_t* wordBuffer = (uint32_t*)_inst->buffer;
        uint32_t startFrameWords = _inst->startFrameCount;
        uint32_t dummyWords = (_inst->dummyLedMode == 1) ? 1 : 0;
        uint32_t firstLedWordIdx = startFrameWords + dummyWords;

        uint8_t brightByte = 0xE0 | (brightness & 0x1F); // New brightness byte

        for (uint16_t i = 0; i < _inst->ledCount; i++)
        {
            uint32_t oldWord = wordBuffer[firstLedWordIdx + i];
            uint32_t rgbValue = oldWord & 0x00FFFFFF;                                  // Preserve RGB (bottom 24 bits)
            wordBuffer[firstLedWordIdx + i] = ((uint32_t)brightByte << 24) | rgbValue; // New brightness + old RGB
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
