#if defined(ARDUINO_ARCH_RP2040)

    #include "pio_neopixel_spi.h"
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
    #define GLOBAL_DEFAULT_BRIGHTNESS 255 // 0 - 255: Global initial brightness (scaled to 5-bit 0-31 for APA102)

/**
 * PIO SPI TX Program (MSB-first, CPHA=0)
 *
 * Timing:
 * - Instruction 0: Output 1 bit, CLK LOW  (setup time)
 * - Instruction 1: Jump back, CLK HIGH    (sample time)
 *
 * Autopull pulls next 32-bit word from TX FIFO.
 * Sideset toggles CLK pin each instruction.
 */
static const uint16_t pio_spi_program_instructions[] = {
    //     .wrap_target
    0x6001, // 0: out pins, 1   side 0      ; Output 1 bit to MOSI, CLK LOW
    0x1000, // 1: jmp 0         side 1      ; Jump to 0, CLK HIGH (sample edge)
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
    // Initialize GPIO pins
    pio_gpio_init(pio, clk_pin);
    pio_gpio_init(pio, data_pin);

    // Set pins as outputs
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

    // Out shift: MSB-first (shift_right=false), autopull enabled, 32 bits per pull
    // We send 4 bytes packed as 32-bit words (MSB first)
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Calculate clock divider
    // SPI frequency = System Clock / (cycles_per_bit * divider)
    // We have 2 instructions per bit (CLK LOW + CLK HIGH)
    // So: freq = clock_get_hz(clk_sys) / (2 * divider)
    // Therefore: divider = clock_get_hz(clk_sys) / (2 * freq)
    float div = (float)clock_get_hz(clk_sys) / (2.0f * freq);
    sm_config_set_clkdiv(&c, div);

    // Initialize state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    #ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI",
                                          "PIO SPI Init: MOSI=GPIO%u, SCK=GPIO%u, SM=%u, Freq=%.2fMHz, Div=%.2f",
                                          data_pin, clk_pin, sm, freq / 1000000.0f, div);
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
                                   bool useDMA)
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
    _inst->useDMA = useDMA; // DMA re-enabled!
    _inst->dmaChannel = -1;
    _inst->dmaIrqNum = -1; // Will be claimed dynamically if DMA is used
    _inst->initialized = false;
    _inst->busy = false;

    // Determine bytes per LED and capabilities based on protocol
    _inst->bytesPerLed = 4; // APA102, SK9822: 4 bytes (brightness + RGB)
    _inst->hasGlobalBrightness = (protocol == LedProtocol::APA102 || protocol == LedProtocol::SK9822);

    // Allocate buffer: 4 bytes start frame + LED data + end frame
    // APA102 Protocol:
    // - Start Frame: 0x00000000 (32 bits LOW)
    // - LED Data: 4 bytes per LED [111xxxxx][BBBBBBBB][GGGGGGGG][RRRRRRRR]
    // - End Frame: 0xFFFFFFFF (32 bits HIGH) to latch data
    _inst->bufferSize = 4 + (ledCount * _inst->bytesPerLed) + 4;
    _inst->buffer = (uint8_t*)malloc(_inst->bufferSize);
    if (_inst->buffer)
    {
        memset(_inst->buffer, 0, _inst->bufferSize);
        // Start Frame: All zeros
        _inst->buffer[0] = 0x00;
        _inst->buffer[1] = 0x00;
        _inst->buffer[2] = 0x00;
        _inst->buffer[3] = 0x00;
        // End Frame: All ones (set in show() method after LED data)
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
        }

        free(_inst);
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
 * 1. Claims a free PIO and state machine
 * 2. Loads SPI timing program
 * 3. Calculates clock divider for desired frequency
 * 4. Configures GPIO pins and FIFO settings
 *
 * @return true if PIO configured, false on error
 */
bool PIO_NeoPixel_SPI::initPIO()
{
    if (!_inst) return false;

    // Try to claim a PIO and state machine
    // Priority: PIO1 > PIO0 (avoid conflicts with other PIO users)
    PIO pios[] = {
        pio1,
        pio0};

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
 * 1. Claims a free DMA channel
 * 2. Configures for 32-bit word transfers to PIO FIFO
 * 3. Enables IRQ1 for transfer completion
 *
 * DMA configuration:
 * - 32-bit word transfers for efficient transfer
 * - Source pointer increments (buffer)
 * - Destination pointer fixed (PIO FIFO)
 * - DREQ from PIO controls transfer timing
 *
 * @note All SPI strips share IRQ1
 * @return true if DMA configured, false if resources unavailable
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

    // Configure DMA
    // We use autopull=32, so DMA must transfer 32-bit words!
    // Buffer size must be multiple of 4 bytes (408 bytes = 102 words)
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);                 // 32-bit word transfers
    channel_config_set_read_increment(&c, true);                            // Increment read pointer by 4 bytes
    channel_config_set_write_increment(&c, false);                          // Write to same FIFO address
    channel_config_set_dreq(&c, pio_get_dreq(_inst->pio, _inst->sm, true)); // PIO TX DREQ

    dma_channel_configure(
        channel,
        &c,
        &_inst->pio->txf[_inst->sm], // Write to PIO TX FIFO (32-bit register)
        _inst->buffer,               // Read from buffer
        _inst->bufferSize / 4,       // Transfer size in 32-bit words (408/4 = 102)
        false                        // Don't start yet
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
 * For APA102/SK9822, uses global default brightness (scaled to 0-31).
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return true if successful, false if index invalid
 */
bool PIO_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;

    // Use max brightness (255, scaled to 31) for APA102/SK9822
    uint8_t brightness_5bit = (GLOBAL_DEFAULT_BRIGHTNESS * 31 + 127) / 255;
    rgbToBuffer(index, r, g, b, brightness_5bit);
    return true;
}

/**
 * Sets RGBW color values for an LED
 *
 * For SPI LEDs (APA102/SK9822), the 'w' parameter is interpreted
 * as brightness value (0-255, scaled to 0-31) instead of white channel.
 *
 * @param index LED index (0-based)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w Brightness value (0-255, scaled to 0-31) for APA102/SK9822
 * @return true if successful, false if index invalid
 */
bool PIO_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_inst || !_inst->buffer || index >= _inst->ledCount) return false;

    // w parameter is interpreted as brightness (0-255) for APA102
    // Scale from 0-255 to 0-31 (5-bit brightness)
    uint8_t brightness_5bit = (w * 31 + 127) / 255;
    rgbToBuffer(index, r, g, b, brightness_5bit);
    return true;
}

/**
 * Stores color values in correct format in buffer
 *
 * Internal helper function that stores color values according to
 * LED protocol format (APA102, WS2801, etc.) in the buffer.
 *
 * APA102/SK9822 format: [111xxxxx][B][G][R]
 * WS2801 format: [R][G][B]
 *
 * @param index LED index
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param brightness Brightness value (0-31) for APA102/SK9822
 */
void PIO_NeoPixel_SPI::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    if (!_inst || !_inst->buffer) return;

    // Buffer layout: [START_FRAME(4)] [LED_DATA...] [END_FRAME(4)]
    uint32_t offset = 4 + (index * _inst->bytesPerLed); // Skip start frame

    #ifdef OPENKNX_TRACE1
    if (index < 1) // Only debug first 8 LEDs
    {
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                              "rgbToBuffer[%d]: R=%d G=%d B=%d Bright=%d, Offset=%d",
                                              index, r, g, b, brightness, offset);
    }
    #endif
      
    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822 format: 111xxxxx (brightness 5 bits) + BGR
        // Brightness byte: 0xE0 | (brightness & 0x1F)
        // According to protocol, first byte is brightness/gray level byte, then B, G, R
        // The highest 3 bits are '111' followed by 5 bits of brightness
        // Example: brightness=31 -> 0xFF(11111111) (full brightness), brightness=0 -> 0xE0(11100000) (off)
        _inst->buffer[offset] = 0xE0 | (brightness & 0x1F); // 111xxxxx
        _inst->buffer[offset + 1] = b;
        _inst->buffer[offset + 2] = g;
        _inst->buffer[offset + 3] = r;

    #ifdef OPENKNX_TRACE1
        if (index < 1)
        {
            openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                                  "  Written: [%d]=%02X [%d]=%02X [%d]=%02X [%d]=%02X",
                                                  offset, 0xE0 | (brightness & 0x1F),
                                                  offset + 1, b,
                                                  offset + 2, g,
                                                  offset + 3, r);
        }
    #endif
    }
    else
    {
        // WS2801 format: just RGB
        _inst->buffer[offset] = r;
        _inst->buffer[offset + 1] = g;
        _inst->buffer[offset + 2] = b;
        if (_inst->bytesPerLed == 4)
        {
            _inst->buffer[offset + 3] = 0; // Padding
        }
    }
}

/**
 * Sends color data to LED strip
 *
 * Starts the transmission of color data from internal buffer:
 * 1. Sets end frame for APA102 protocol
 * 2. Activates chip select if available
 * 3. Transfers data via DMA or direct PIO
 *
 * - DMA: Asynchronous background transfer
 * - PIO: Blocking transfer (waits for completion)
 *
 * @return true if transfer started, false on error or active transfer
 */
bool PIO_NeoPixel_SPI::show()
{
    if (!_inst || !_inst->initialized || !_inst->buffer) return false;
    if (_inst->busy) return false; // Already transmitting

    // Trace: Print first LED data and buffer info
    #ifdef OPENKNX_TRACE1
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                          "PIO_SPI show(): BufferSize=%d, DMA=%s, CH=%d",
                                          _inst->bufferSize,
                                          _inst->useDMA ? "YES" : "NO",
                                          _inst->dmaChannel);
    // Print Start Frame + First LED
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                          "  Start: %02X %02X %02X %02X",
                                          _inst->buffer[0], _inst->buffer[1], _inst->buffer[2], _inst->buffer[3]);
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                          "  LED0:  %02X %02X %02X %02X",
                                          _inst->buffer[4], _inst->buffer[5], _inst->buffer[6], _inst->buffer[7]);
    #endif

    // Set End Frame (last 4 bytes of buffer)
    // APA102 needs 0xFFFFFFFF to latch all LED data
    size_t endFrameOffset = _inst->bufferSize - 4;
    _inst->buffer[endFrameOffset] = 0xFF;
    _inst->buffer[endFrameOffset + 1] = 0xFF;
    _inst->buffer[endFrameOffset + 2] = 0xFF;
    _inst->buffer[endFrameOffset + 3] = 0xFF;

    _inst->busy = true;

    if (_inst->csPin >= 0)
    {
        digitalWrite(_inst->csPin, LOW); // CS active
    }

    if (_inst->useDMA && _inst->dmaChannel >= 0)
    {
    #ifdef OPENKNX_TRACE1
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                              "  Starting DMA transfer on channel %d",
                                              _inst->dmaChannel);
    #endif

        sendDataDMA(); // Start non-blocking DMA transfer
    }
    else
    {
    #ifdef OPENKNX_TRACE1
        openknx.logger.logWithPrefixAndValues("PIO NeoPixel Spi",
                                              "  Starting PIO blocking transfer");
    #endif
        sendDataPIO(); // Blocking PIO transfer
    }

    return true;
}

/**
 * Sends data directly via PIO
 *
 * Transfers color data blocking via PIO FIFO.
 * Data is packed into 32-bit words (MSB first):
 *
 * Word format:
 * - Byte 0 → bits[31:24] (sent first)
 * - Byte 1 → bits[23:16]
 * - Byte 2 → bits[15:8]
 * - Byte 3 → bits[7:0] (sent last)
 *
 * Waits for transmission to complete before returning.
 */
void PIO_NeoPixel_SPI::sendDataPIO()
{
    if (!_inst) return;

    // Send data to PIO FIFO (blocking)
    // With autopull=32, PIO automatically pulls 32 bits at a time
    // Pack 4 bytes per FIFO write (MSB-first: byte0 in bits[31:24])

    size_t i = 0;
    while (i < _inst->bufferSize)
    {
        uint32_t word = 0;

        // Pack 4 bytes into 32-bit word (MSB first)
        // Byte 0 → bits[31:24] (sent first)
        // Byte 1 → bits[23:16]
        // Byte 2 → bits[15:8]
        // Byte 3 → bits[7:0] (sent last)
        if (i < _inst->bufferSize)
            word |= ((uint32_t)_inst->buffer[i++] << 24);
        if (i < _inst->bufferSize)
            word |= ((uint32_t)_inst->buffer[i++] << 16);
        if (i < _inst->bufferSize)
            word |= ((uint32_t)_inst->buffer[i++] << 8);
        if (i < _inst->bufferSize)
            word |= ((uint32_t)_inst->buffer[i++]);

        // Send to PIO FIFO
        // PIO will shift out MSB-first: bits[31] → [30] → ... → [0]
        pio_sm_put_blocking(_inst->pio, _inst->sm, word);
    }

    // Wait for PIO to finish transmitting
    // Check if TX FIFO is empty (all data sent)
    while (!pio_sm_is_tx_fifo_empty(_inst->pio, _inst->sm))
    {
        tight_loop_contents(); // Busy-wait efficiently
    }

    if (_inst->csPin >= 0)
    {
        digitalWrite(_inst->csPin, HIGH); // CS inactive
    }

    _inst->busy = false;
}

/**
 * Starts asynchronous DMA transfer
 *
 * Initiates DMA transfer in background:
 * 1. Activates chip select pin
 * 2. Starts DMA channel
 * 3. Returns immediately (non-blocking)
 *
 * The busy flag is cleared by the DMA IRQ handler.
 * Chip select is deactivated in completion callback.
 */
void PIO_NeoPixel_SPI::sendDataDMA()
{
    if (!_inst || _inst->dmaChannel < 0)
    {
        _inst->busy = false;
        return;
    }

    // Activate chip select (LOW = active)
    if (_inst->csPin >= 0)
    {
        digitalWrite(_inst->csPin, LOW);
    }

    // Start DMA transfer
    // DMA will trigger interrupt when done, which will set busy=false and CS=HIGH
    dma_channel_set_read_addr(_inst->dmaChannel, _inst->buffer, true);

    // Note: _inst->busy stays true until DMA IRQ handler clears it!
}

/**
 * Checks if a transfer is in progress
 * @return true if DMA or PIO is actively transferring
 */
bool PIO_NeoPixel_SPI::isBusy()
{
    return _inst ? _inst->busy : false;
}

/**
 * Clears all LED colors (sets to black)
 *
 * Sets LED data portion of buffer to 0.
 * Preserves start frame.
 * Call show() to apply the change.
 */
void PIO_NeoPixel_SPI::clear()
{
    if (!_inst || !_inst->buffer) return;

    // Clear LED data, but keep start frame
    memset(_inst->buffer + 4, 0, _inst->bufferSize - 4);
}

/**
 * Returns the driver's capabilities
 *
 * @return DriverCapabilities with supported features:
 *         - supportsRGBW: false (SPI LEDs don't support RGBW)
 *         - supportsDMA: true if DMA is enabled
 *         - supportsAsync: true if DMA is enabled
 */
DriverCapabilities PIO_NeoPixel_SPI::getCapabilities() const
{
    DriverCapabilities caps;
    caps.supportsRGBW = false; // SPI LEDs typically don't support RGBW
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
    if (channel >= 0 && channel < 12)
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
    if (channel >= 0 && channel < 12)
    {
        g_spiHandlers[channel] = nullptr; // Use global registry
    }
}

/*
 * DMA completion callback - called from unified IRQ handler
 *
 * Deactivates chip select and clears busy flag when transfer completes.
 */
void PIO_NeoPixel_SPI::onDmaComplete()
{
    if (!_inst) return;

    // Set CS pin HIGH if used
    if (_inst->csPin >= 0)
    {
        digitalWrite(_inst->csPin, HIGH);
    }

    // Clear busy flag
    _inst->busy = false;
}

/*
 * Legacy DMA IRQ handler - now replaced by unifiedDmaIRQHandler
 * Kept for backward compatibility but no longer used
 */
void PIO_NeoPixel_SPI::dmaIRQHandler()
{
    unifiedDmaIRQHandler(); // Delegate to unified handler
}


#endif // ARDUINO_ARCH_RP2040
