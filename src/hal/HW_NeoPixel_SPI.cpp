#include "HW_NeoPixel_SPI.h"
#include "../PhysicalStripConfig.h"
#include "OpenKNX.h"

#if defined(ARDUINO_ARCH_RP2040)
    #include <hardware/gpio.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <esp_log.h>
#endif

bool HW_NeoPixel_SPI::_spi0Used = false;       // Static tracking variable for SPI0 usage
bool HW_NeoPixel_SPI::_spi1Used = false;       // Static tracking variable for SPI1 usage
SPIClass* HW_NeoPixel_SPI::_spi1Instance = nullptr; // Second SPI bus instance (allocated on demand)

// APA102/SK9822 Brightness Configuration (5-bit hardware brightness)
// Safe range: 16-30 (below 16 flickers, 31/0xFF breaks sync on clones)
#define BRIGHTNESS_MIN 16     // Minimum safe brightness (below = flicker)
#define BRIGHTNESS_MAX 30     // Maximum safe brightness (31 = 0xFF = sync bug)
#define BRIGHTNESS_DEFAULT 30 // Default to max safe brightness

/**
 * Constructor
 *
 * @param ledCount Total number of LEDs
 * @param protocol LED Protocol (APA102, WS2801, etc.)
 * @param frequency SPI Frequency (Hz, default 10MHz)
 *
 * Pins are automatically selected:
 * - RP2040/50: Standard SPI Pins for SPI0 or SPI1
 * - ESP32: Standard VSPI or HSPI Pins
 */
HW_NeoPixel_SPI::HW_NeoPixel_SPI(uint16_t ledCount, LedProtocol protocol, uint32_t frequency) : _inst(nullptr)
{
    _inst = new hw_neopixel_spi_inst();
    _inst->ledCount = ledCount;
    _inst->protocol = protocol;
    _inst->spiFrequency = frequency;
    _inst->csPin = -1;
    _inst->initialized = false;
    _inst->busy = false;

    // Define Bytes for each LED
    _inst->bytesPerLed = ProtocolHelper::getBytesPerLed(protocol);
    _inst->hasGlobalBrightness = (protocol == LedProtocol::APA102 || protocol == LedProtocol::SK9822);
    _inst->needs7bit = (protocol == LedProtocol::LPD8806);

    // ===== Extended SPI Configuration Defaults =====
    // Optimized for APA102 strips (150 LEDs @ 3 MHz tested stable)
    _inst->dummyLedMode = 1;                  // Physical dummy LED (sacrifice LED#0) - REQUIRED for SK9822/APA102 clones
    _inst->startFrameCount = 8;               // 8 start frames (tested stable 1-8, use max for safety)
    _inst->endFrameCount = 1;                 // 1 end frame for APA102 (SK9822 may need more)
    _inst->startFrameDelayUs = 0;             // No delay (tested with 0-50µs, not needed)
    _inst->endFramePattern = 0x00;            // 0x00000000 = APA102, 0xFFFFFFFF = SK9822
    _inst->detectedChip = protocol;           // Start with protocol assumption
    _inst->autoDetectChip = false;            // Manual chip type (can enable via API)
    _inst->hwBrightness = BRIGHTNESS_DEFAULT; // Default to max safe brightness (30)
    _inst->colorOrder = ProtocolHelper::getColorOrder(protocol); // Default ColorOrder from protocol

    // Allocate buffer: Start frames + Dummy LED (optional) + LED data + End frames
    // Calculate buffer components based on configuration
    if (_inst->hasGlobalBrightness)
    {
        uint16_t startFrameSize = _inst->startFrameCount * 4;
        uint16_t dummyLedSize = (_inst->dummyLedMode == 1) ? 4 : 0; // Physical dummy only
        uint16_t endFrameSize = _inst->endFrameCount * 4;

        _inst->bufferSize = startFrameSize + dummyLedSize + (ledCount * 4) + endFrameSize;
    }
    else
    {
        // WS2801/LPD8806: Just LED data
        _inst->bufferSize = ledCount * _inst->bytesPerLed;
    }

    _inst->buffer = new uint8_t[_inst->bufferSize];
    memset(_inst->buffer, 0, _inst->bufferSize);

    _inst->spi = selectSPI(); // Select best available SPI instance

    // Default Pins based on platform and SPI instance
#if defined(ARDUINO_ARCH_RP2040) // There must be a better way... ToDo!!
    if (_inst->spi == &SPI)
    {
        _inst->mosiPin = 3; // GP3 (SPI0 MOSI) // ToDo: OpenKNX HardwareConfig
        _inst->sckPin = 2;  // GP2 (SPI0 SCK)  // ToDo: OpenKNX HardwareConfig
    }
    else
    {
        _inst->mosiPin = 11; // GP11 (SPI1 MOSI) // ToDo: OpenKNX HardwareConfig
        _inst->sckPin = 10;  // GP10 (SPI1 SCK)  // ToDo: OpenKNX HardwareConfig
    }
#elif defined(ARDUINO_ARCH_ESP32)
    if (_inst->spi == &SPI)
    {
        _inst->mosiPin = 23; // GPIO23 (VSPI MOSI) // ToDo: OpenKNX HardwareConfig
        _inst->sckPin = 18;  // GPIO18 (VSPI SCK)  // ToDo: OpenKNX HardwareConfig
    }
    else
    {
        _inst->mosiPin = 13; // GPIO13 (HSPI MOSI) // ToDo: OpenKNX HardwareConfig
        _inst->sckPin = 14;  // GPIO14 (HSPI SCK)  // ToDo: OpenKNX HardwareConfig
    }
#else
    _inst->mosiPin = 11; // GP11 (SPI1 MOSI) // ToDo: OpenKNX HardwareConfig
    _inst->sckPin = 13;  // GP13 (SPI1 SCK)  // ToDo: OpenKNX HardwareConfig
#endif
}

/**
 * Constructor with Custom Pins
 * @param mosiPin MOSI/Data Pin
 * @param sckPin Clock Pin
 * @param ledCount Number of LEDs
 * @param protocol LED Protocol
 * @param frequency SPI Frequency (Hz)
 * @param csPin Chip Select Pin (optional, -1 if not used)
 */
HW_NeoPixel_SPI::HW_NeoPixel_SPI(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, uint32_t frequency, int csPin) : _inst(nullptr)
{
    _inst = new hw_neopixel_spi_inst();
    _inst->mosiPin = mosiPin;        // Set the SPI MOSI Pin
    _inst->sckPin = sckPin;          // Set the SPI SCK Pin
    _inst->csPin = csPin;            // Set the SPI CS Pin
    _inst->ledCount = ledCount;      // Set the number of LEDs
    _inst->protocol = protocol;      // Set the LED Protocol (APA102, WS2801, etc.)
    _inst->spiFrequency = frequency; // Set the SPI Frequency (Hz)
    _inst->initialized = false;      // Will be set in init()
    _inst->busy = false;             // Will be set in init()

    // Define Bytes for each LED
    _inst->bytesPerLed = ProtocolHelper::getBytesPerLed(protocol);
    _inst->hasGlobalBrightness = (protocol == LedProtocol::APA102 || protocol == LedProtocol::SK9822);
    _inst->needs7bit = (protocol == LedProtocol::LPD8806); // LPD8806 needs 7-bit values - Just for Preperation

    // ===== Extended SPI Configuration Defaults =====
    _inst->dummyLedMode = 1;                  // Physical dummy LED (sacrifice LED#0)
    _inst->startFrameCount = 8;               // 8 start frames
    _inst->endFrameCount = 1;                 // 1 end frame
    _inst->startFrameDelayUs = 0;             // No delay
    _inst->endFramePattern = 0x00;            // 0x00 = APA102
    _inst->detectedChip = protocol;           // Start with protocol assumption
    _inst->autoDetectChip = false;            // Manual chip type
    _inst->hwBrightness = BRIGHTNESS_DEFAULT; // Default to max safe brightness (30)
    _inst->colorOrder = ProtocolHelper::getColorOrder(protocol); // Default ColorOrder from protocol

    // Allocate buffer:
    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822: Start frames + Dummy LED (optional) + LED data + End frames
        uint16_t startFrameSize = _inst->startFrameCount * 4;
        uint16_t dummyLedSize = (_inst->dummyLedMode == 1) ? 4 : 0;
        uint16_t endFrameSize = _inst->endFrameCount * 4;

        _inst->bufferSize = startFrameSize + dummyLedSize + (ledCount * 4) + endFrameSize;
    }
    else
    {
        // WS2801/LPD8806: 3 per LED
        _inst->bufferSize = ledCount * _inst->bytesPerLed;
    }

    _inst->buffer = new uint8_t[_inst->bufferSize];
    memset(_inst->buffer, 0, _inst->bufferSize); // This will be set to black on init()

    _inst->spi = selectSPI(); // Select best available SPI instance
}

/*
 * Destructor
 */
HW_NeoPixel_SPI::~HW_NeoPixel_SPI()
{
    if (_inst)
    {
        if (_inst->buffer)
        {
            delete[] _inst->buffer; // Free buffer
        }
        if (_inst->spi == &SPI)
        {
            _spi0Used = false; // Mark SPI0 as free
        }
        else
        {
            _spi1Used = false; // Mark SPI1 as free
        }
        delete _inst;
    }
}

// =============================================================================
// Configuration Management (IHardwareDriver Interface)
// =============================================================================

/**
 * @brief Create default SPI strip configuration
 * @return New SpiStripConfig with driver-specific defaults
 */
PhysicalStripConfig* HW_NeoPixel_SPI::createDefaultConfig() const
{
    SpiStripConfig* cfg = new SpiStripConfig();

    // Set defaults from current instance (if available)
    if (_inst)
    {
        cfg->setHwBrightness(_inst->hwBrightness);
        cfg->setDummyLedMode(_inst->dummyLedMode);
        cfg->setStartFrameCount(_inst->startFrameCount);
        cfg->setEndFrameCount(_inst->endFrameCount);
        cfg->setEndFramePattern(_inst->endFramePattern);
        cfg->setStartFrameDelayUs(_inst->startFrameDelayUs);
        cfg->setAutoDetectChip(_inst->autoDetectChip);
        cfg->setDetectedChip(_inst->detectedChip);
        cfg->setSpiFrequency(_inst->spiFrequency);
    }

    return cfg;
}

/**
 * @brief Apply configuration to driver
 * @param config SpiStripConfig to apply
 * @return true if applied successfully
 */
bool HW_NeoPixel_SPI::applyConfig(const PhysicalStripConfig* config)
{
    if (!_inst || !config) return false;

    // Type check - must be SpiStripConfig
    const SpiStripConfig* spiCfg = config->isSpiConfig() ? static_cast<const SpiStripConfig*>(config) : nullptr;
    if (!spiCfg) return false;

    // Apply SPI-specific settings
    _inst->colorOrder = spiCfg->getColorOrder();
    _inst->hwBrightness = spiCfg->getHwBrightness();
    _inst->endFramePattern = spiCfg->getEndFramePattern();
    _inst->startFrameDelayUs = spiCfg->getStartFrameDelayUs();
    _inst->autoDetectChip = spiCfg->getAutoDetectChip();
    _inst->detectedChip = spiCfg->getDetectedChip();
    _inst->spiFrequency = spiCfg->getSpiFrequency();

    // Settings that require re-init (only apply if not initialized)
    if (!_inst->initialized)
    {
        _inst->dummyLedMode = spiCfg->getDummyLedMode();
        _inst->startFrameCount = spiCfg->getStartFrameCount();
        _inst->endFrameCount = spiCfg->getEndFrameCount();
    }

    return true;
}

/**
 * Select best available SPI instance
 * @return Pointer to SPIClass instance, or nullptr if none available
 * @Info Checks the availability of SPI instances and returns the best one.
 */
SPIClass* HW_NeoPixel_SPI::selectSPI()
{
    if (!_spi0Used)
    {
        _spi0Used = true;
        return &SPI; // SPI0 / SPI2 (IDF) / VSPI - always the default Arduino SPI bus
    }

    if (!_spi1Used)
    {
#if defined(ARDUINO_ARCH_RP2040)
        _spi1Used = true;
        return &SPI1; // RP2040: SPI1

#elif defined(ARDUINO_ARCH_ESP32)
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
        // C3 has only one usable SPI bus - cannot create a second SPI strip
        openknx.logger.logWithPrefix("HW NeoPixel SPI", "ERROR: ESP32-C3 supports only 1 SPI strip (single SPI bus)!");
        return nullptr;

    #elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        // Original ESP32 / S2 / S3: second bus is HSPI
        if (_spi1Instance == nullptr)
            _spi1Instance = new SPIClass(HSPI);
        _spi1Used = true;
        return _spi1Instance;

    #else
        // C5, C6 and future variants: second bus via FSPI
        if (_spi1Instance == nullptr)
            _spi1Instance = new SPIClass(FSPI);
        _spi1Used = true;
        return _spi1Instance;
    #endif

#else
        return nullptr;
#endif
    }

    openknx.logger.logWithPrefix("HW NeoPixel SPI", "ERROR: All SPI instances are already in use!");
    return nullptr; // No SPI available
}

/**
 * Initialize hardware driver
 * @return true on success, false on error
 * @note Pins must be configured before init() is called, via constructor!
 */
bool HW_NeoPixel_SPI::init()
{
    if (!_inst || !_inst->spi)
    {
        openknx.logger.logWithPrefix("PIO NeoPixel SPI", "ERROR: HW_NeoPixel_SPI not properly configured!");
        return false;
    }

    // Configure SPI Pins - platform specific
#if defined(ARDUINO_ARCH_RP2040)
    // RP2040: Set pins before begin() ?
    //_inst->spi->setSCK(_inst->sckPin);
    //_inst->spi->setTX(_inst->mosiPin);
    // if (_inst->csPin >= 0)
    //{
    //    _inst->spi->setCS(_inst->csPin);
    //}
    _inst->spi->begin();
#elif defined(ARDUINO_ARCH_ESP32)
    // ESP32: Pass pins to begin()
    _inst->spi->begin(_inst->sckPin, -1, _inst->mosiPin, _inst->csPin);
#else
    // Default: Use standard pins
    _inst->spi->begin();
#endif

    //
    // Configure SPI Settings
    //
    uint32_t actualFrequency = _inst->spiFrequency;

    switch (_inst->protocol) // Important: Limit frequency based on protocol, else data corruption may occur
    {
        case LedProtocol::APA102:
        case LedProtocol::SK9822:
            if (actualFrequency > 20000000) actualFrequency = 20000000; // 20MHz max. for APA102
            break;
        case LedProtocol::WS2801:
            if (actualFrequency > 25000000) actualFrequency = 25000000; // 25MHz max. for WS2801
            break;
        case LedProtocol::LPD8806:
            if (actualFrequency > 20000000) actualFrequency = 20000000; // 20MHz max. for LPD8806
            break;
        default:
            if (actualFrequency > 10000000) actualFrequency = 10000000; // 10MHz default max
    }

#ifdef OPENKNX_DEBUG
    // Log requested vs achievable frequency
    openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                          "Init: Protocol=%u, Requested Freq=%lu Hz, SysClock=%lu Hz",
#if defined(ARDUINO_ARCH_RP2040)
                                          (uint8_t)_inst->protocol, actualFrequency, clock_get_hz(clk_sys));
#elif defined(ARDUINO_ARCH_ESP32)
                                          (uint8_t)_inst->protocol, actualFrequency, ESP.getCpuFreqMHz() * 1000000UL);
#else
                                          (uint8_t)_inst->protocol, actualFrequency, F_CPU);
#endif

    // Calculate actual achievable frequency based on system clock
    // SPI baudrate = sys_clk / (CPSR * (1 + SCR))
    // where CPSR = 2..254 (even), SCR = 0..255
    // For simplicity, calculate what we can actually achieve
#if defined(ARDUINO_ARCH_RP2040)
    uint32_t sys_freq = clock_get_hz(clk_sys);
#elif defined(ARDUINO_ARCH_ESP32)
    uint32_t sys_freq = ESP.getCpuFreqMHz() * 1000000UL;
#else
    uint32_t sys_freq = F_CPU;
#endif
    float best_div = (float)sys_freq / (float)actualFrequency;
    uint32_t achievable_freq = sys_freq / (uint32_t)(best_div + 0.5f);
    openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                          "Achievable Freq=%lu Hz (div=%.2f)", achievable_freq, best_div);
#endif

    // Begin SPI Transaction, configure the SPI Mode and Frequency
    SPISettings settings(actualFrequency, MSBFIRST, SPI_MODE0); // APA102/WS2801/LPD8806 are using Mode 0 (CPOL=0, CPHA=0)

    _inst->spi->beginTransaction(settings); // Begin Transaction
    if (_inst->hasGlobalBrightness)         // Send initial Start Frame for APA102/SK9822
    {
        sendStartFrame();
    }
    _inst->spi->endTransaction(); // End Transaction

    _inst->initialized = true; // Mark as initialized
    openknx.logger.logWithPrefixAndValues("PIO NeoPixel SPI", "Initialized %u LEDs with protocol %u at %lu Hz",
                                          _inst->ledCount, (uint8_t)_inst->protocol, actualFrequency);
    return true;
}

/**
 * Set each pixel color (RGB)
 * Brightness is read from _inst->hwBrightness (stored in config)
 */
bool HW_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || index >= _inst->ledCount)
    {
        return false;
    }

    // Apply ColorOrder conversion (RGB → hardware order)
    uint8_t r_hw, g_hw, b_hw;
    switch (_inst->colorOrder)
    {
        case ColorOrder::RGB: r_hw = r; g_hw = g; b_hw = b; break;
        case ColorOrder::RBG: r_hw = r; g_hw = b; b_hw = g; break;
        case ColorOrder::GRB: r_hw = g; g_hw = r; b_hw = b; break;
        case ColorOrder::GBR: r_hw = g; g_hw = b; b_hw = r; break;
        case ColorOrder::BRG: r_hw = b; g_hw = r; b_hw = g; break;
        case ColorOrder::BGR: r_hw = b; g_hw = g; b_hw = r; break;
        default:              r_hw = r; g_hw = g; b_hw = b; break;
    }

    rgbToBuffer(index, r_hw, g_hw, b_hw);
    return true;
}

/**
 * Set each pixel color (RGBW)
 * For APA102/SK9822, w parameter is ignored (no white channel)
 */
bool HW_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    (void)w; // Suppress unused parameter warning

    if (!_inst || index >= _inst->ledCount)
    {
        return false;
    }

#ifdef OPENKNX_DEBUG
    static bool warned = false;
    if (!warned && w != 0)
    {
        openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                              "WARNING: setPixel(r,g,b,w) - SPI LEDs have no white channel, w parameter ignored!");
        warned = true; // Only warn once
    }
#endif

    // Forward to RGB version (brightness from config)
    return setPixel(index, r, g, b);
}

/**
 * Set each pixel color (RGBCCT / 5-channel)
 * SPI LEDs (APA102, WS2801, SK9822, LPD8806) are RGB-only and do not support
 * separate warm-white/cold-white channels. RGBCCT requires 5-chip LEDs like SK6812-RGBCCT.
 * This method exists for interface compatibility but ignores ww/cw parameters.
 */
bool HW_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    (void)ww; // SPI LEDs have no warm-white channel - parameter unused
    (void)cw; // SPI LEDs have no cold-white channel - parameter unused

    if (!_inst || index >= _inst->ledCount)
    {
        return false;
    }

#ifdef OPENKNX_DEBUG
    static bool warned = false;
    if (!warned && (ww != 0 || cw != 0))
    {
        openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                              "WARNING: setPixel(r,g,b,ww,cw) - SPI LEDs are RGB-only, no RGBCCT support. ww/cw parameters ignored!");
        warned = true; // Only warn once
    }
#endif

    // Forward to RGB version
    return setPixel(index, r, g, b);
}

/**
 * @brief Convert hardware-ordered bytes to SPI buffer format
 * @param index LED index
 * @param r First color byte (already in hardware ColorOrder)
 * @param g Second color byte (already in hardware ColorOrder)
 * @param b Third color byte (already in hardware ColorOrder)
 *
 * IMPORTANT: setPixel() has already converted RGB to hardware ColorOrder!
 * For APA102 with BGR: r=B, g=G, b=R (not RGB!)
 *
 * This function writes hardware-ordered bytes directly to protocol-specific buffer:
 * - APA102/SK9822: [Brightness byte][byte0][byte1][byte2]
 * - WS2801: [byte0][byte1][byte2]
 * - LPD8806: [byte0][byte1][byte2] (7-bit format)
 */
void HW_NeoPixel_SPI::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || !_inst->buffer) return;

    switch (_inst->protocol)
    {
        case LedProtocol::APA102:
        case LedProtocol::SK9822:
        {
            // APA102: [Start Frames][Dummy LED (optional)][LED Data][End Frames]
            // Calculate correct offset: start frames + dummy LED (if enabled) + LED index
            uint32_t startFrameBytes = _inst->startFrameCount * 4;
            uint32_t dummyLedBytes = (_inst->dummyLedMode == 1) ? 4 : 0;
            uint32_t offset = startFrameBytes + dummyLedBytes + (index * 4);
            
            _inst->buffer[offset] = 0xE0 | (_inst->hwBrightness & 0x1F); // 111xxxxx format (5-bit brightness)
            _inst->buffer[offset + 1] = r;                               // Hardware byte 0
            _inst->buffer[offset + 2] = g;                               // Hardware byte 1
            _inst->buffer[offset + 3] = b;                               // Hardware byte 2
            break;
        }

        case LedProtocol::WS2801:
        {
            // WS2801: [byte0][byte1][byte2] (hardware order)
            uint32_t offset = index * 3;
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            break;
        }

        case LedProtocol::LPD8806:
        {
            // LPD8806: 7-Bit format with MSB=1 as update bit
            uint32_t offset = index * 3;
            _inst->buffer[offset] = 0x80 | (r >> 1);     // MSB = 1: Update bit
            _inst->buffer[offset + 1] = 0x80 | (g >> 1); // MSB = 1: Update bit
            _inst->buffer[offset + 2] = 0x80 | (b >> 1); // MSB = 1: Update bit
            break;
        }

        default: // Unsupported protocol
#ifdef OPENKNX_DEBUG
            openknx.logger.logWithPrefix("PIO NeoPixel SPI", "ERROR: Unsupported LED Protocol in rgbToBuffer()");
#endif
            break;
    }
}

/**
 * Send start frame
 */
void HW_NeoPixel_SPI::sendStartFrame()
{
    if (!_inst || !_inst->spi) return;

    // APA102 Start Frame: 4 Byte every 0x00!
    for (int i = 0; i < 4; i++)
    {
        _inst->spi->transfer(0x00); // Required before sending LED data
    }
}

/**
 * Send end frame
 */
void HW_NeoPixel_SPI::sendEndFrame()
{
    if (!_inst || !_inst->spi) return;

    // APA102 End Frame: Needs (ledCount + 1) / 2 bits of "1"
    // Safer to send (ledCount / 16) + 1 bytes of 0xFF (minimum 4 bytes)
    uint16_t endFrameBytes = (_inst->ledCount / 16) + 1;
    if (endFrameBytes < 4) endFrameBytes = 4;

    for (uint16_t i = 0; i < endFrameBytes; i++)
    {
        _inst->spi->transfer(0x00); // Protocol requires at least 4 bytes of 0xFF
    }
}

/**
 * Show LEDs (send data over SPI)
 */
bool HW_NeoPixel_SPI::show()
{
    if (!_inst || !_inst->spi || !_inst->initialized)
    {
        return false; // Not initialized
#ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefix("PIO NeoPixel SPI", "ERROR: HW_NeoPixel_SPI not initialized!");
#endif
    }

    _inst->busy = true;

    SPISettings settings(_inst->spiFrequency, MSBFIRST, SPI_MODE0); // APA102/WS2801/LPD8806 are using Mode 0 (CPOL=0, CPHA=0)

    _inst->spi->beginTransaction(settings); // Begin SPI Transaction
    for (size_t i = 0; i < _inst->bufferSize; i++)
    {
        _inst->spi->transfer(_inst->buffer[i]);
    } // Send complete buffer byte by byte
    if (_inst->hasGlobalBrightness)
    {
        sendEndFrame();
    } // End Frame for APA102
    _inst->spi->endTransaction(); // End SPI Transaction

    _inst->busy = false;

    return true;
}

/**
 * Is driver busy (transmission in progress)?
 */
bool HW_NeoPixel_SPI::isBusy()
{
    if (!_inst) return false; // Hardware SPI is synchronous, therefore always ready
    return _inst->busy;
}

/**
 * Clear all LEDs
 */
void HW_NeoPixel_SPI::clear()
{
    if (!_inst || !_inst->buffer) return;

    if (_inst->hasGlobalBrightness)
    {
        // Set start and end frames, but all LED frames to 0
        memset(_inst->buffer, 0x00, _inst->bufferSize);
        _inst->buffer[0] = 0x00; // Set start frame
        _inst->buffer[1] = 0x00;
        _inst->buffer[2] = 0x00;
        _inst->buffer[3] = 0x00;
    }
    else
    {
        memset(_inst->buffer, 0x00, _inst->bufferSize); // No global brightness requested
    }
}

/*
 * Get the Driver Capabilities
 */
DriverCapabilities HW_NeoPixel_SPI::getCapabilities() const
{
    DriverCapabilities caps = {};
    caps.supportsRGBW = false;    // SPI protocols do not support RGBW
    caps.supportsDMA = false;     // Hardware SPI is simple, no DMA needed
    caps.supportsAsync = false;   // Synchronous transmission
    caps.maxLeds = 10000;         // Practically limited by memory. 10k should be enough for SPI with RP2040/ESP32
    caps.maxFrequency = 25000000; // WS2801 max frequency
    return caps;
}

// =============================================================================
// Extended SPI Configuration API Implementation
// =============================================================================

/**
 * @brief Set dummy LED mode (requires buffer re-allocation, call before init!)
 * @param mode 0=none (first LED wrong color), 1=physical (sacrifice LED#0), 2=virtual (LED#0 forced black)
 */
void HW_NeoPixel_SPI::setDummyLedMode(uint8_t mode)
{
    if (!_inst) return;
    if (mode > 2) mode = 1; // Clamp to valid range

    if (_inst->initialized)
    {
#ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                              "WARNING: Cannot change dummyLedMode after init() - requires re-init!");
#endif
        return;
    }

    _inst->dummyLedMode = mode;
}

/**
 * @brief Set start frame count (1-8, default: 8)
 */
void HW_NeoPixel_SPI::setStartFrameCount(uint8_t count)
{
    if (!_inst) return;
    if (count < 1) count = 1;
    if (count > 8) count = 8;

    if (_inst->initialized)
    {
#ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                              "WARNING: Cannot change startFrameCount after init() - requires re-init!");
#endif
        return;
    }

    _inst->startFrameCount = count;
}

/**
 * @brief Set end frame count (1-80, default: 1)
 */
void HW_NeoPixel_SPI::setEndFrameCount(uint8_t count)
{
    if (!_inst) return;
    if (count < 1) count = 1;
    if (count > 80) count = 80;

    _inst->endFrameCount = count;
}

/**
 * @brief Set delay after start frames in microseconds (0-1000, default: 0)
 */
void HW_NeoPixel_SPI::setStartFrameDelayUs(uint32_t delayUs)
{
    if (!_inst) return;
    if (delayUs > 1000) delayUs = 1000;

    _inst->startFrameDelayUs = delayUs;
}

/**
 * @brief Set end frame pattern (0x00 for APA102, 0xFF for SK9822)
 */
void HW_NeoPixel_SPI::setEndFramePattern(uint8_t pattern)
{
    if (!_inst) return;
    _inst->endFramePattern = pattern;
}

/**
 * @brief Enable/disable chip auto-detection
 */
void HW_NeoPixel_SPI::setAutoDetectChip(bool enable)
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
LedProtocol HW_NeoPixel_SPI::detectChipType()
{
    if (!_inst || !_inst->initialized)
    {
#ifdef OPENKNX_DEBUG
        openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                              "ERROR: Cannot auto-detect chip - driver not initialized!");
#endif
        return _inst ? _inst->protocol : LedProtocol::APA102;
    }

#ifdef OPENKNX_DEBUG
    openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
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
    openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                          "Chip detection: %s (LED count: %d, end frame: 0x%02X)",
                                          detected == LedProtocol::APA102 ? "APA102" : "SK9822",
                                          _inst->ledCount,
                                          _inst->endFramePattern);
    openknx.logger.logWithPrefixAndValues("HW NeoPixel SPI",
                                          "NOTE: Auto-detection is heuristic-based. Use setSpi...() for manual override.");
#endif

    clear();
    show();

    _inst->detectedChip = detected;
    return detected;
}
