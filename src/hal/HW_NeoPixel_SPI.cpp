#include "HW_NeoPixel_SPI.h"
#include "OpenKNX.h"

#if defined(ARDUINO_ARCH_RP2040)
    #include <hardware/gpio.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <esp_log.h>
#endif

bool HW_NeoPixel_SPI::_spi0Used = false; // Static tracking variable for SPI0 usage
bool HW_NeoPixel_SPI::_spi1Used = false; // Static tracking variable for SPI1 usage

#define GLOBAL_DEFAULT_BRIGHTNESS 255 // 0 - 255: Max brightness for APA102 (scaled to 5-bit 0-31). Range: 0 (off) to 255 (max)

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

    // Allocate buffer for:
    // APA102/SK9822: 4 Start Frame + 4 per LED + 4 End Frame
    // WS2801/LPD8806: 3 per LED
    if (_inst->hasGlobalBrightness)
    {
        _inst->bufferSize = 4 + (ledCount * 4) + 4; // Start + Data + End
    }
    else
    {
        _inst->bufferSize = ledCount * _inst->bytesPerLed; // Just Data
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
HW_NeoPixel_SPI::HW_NeoPixel_SPI(uint mosiPin, uint sckPin, uint16_t ledCount, LedProtocol protocol, uint32_t frequency, int csPin) : _inst(nullptr)
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

    // Allocate buffer for:
    if (_inst->hasGlobalBrightness)
    {
        // APA102/SK9822: 4 Start Frame + 4 per LED + 4 End Frame
        _inst->bufferSize = 4 + (ledCount * 4) + 4;
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
        return &SPI;
    }

    if (!_spi1Used)
    {
        _spi1Used = true;
#if defined(ARDUINO_ARCH_RP2040)
        return &SPI1; // On RP2040, SPI1 is available
#elif defined(ARDUINO_ARCH_ESP32)
        // On ESP32 is SPI1 the HSPI
        return &SPI; // Fallback, normally SPIClass HSPI would be used
#endif
    };
    openknx.logger.logWithPrefix("PIO NeoPixel SPI", "ERROR: All SPI instances are already in use!");
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
 */
bool HW_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_inst || index >= _inst->ledCount)
    {
        return false;
    }

    // Use max brightness (255, scaled to 31) for APA102/SK9822
    uint8_t brightness_5bit = (GLOBAL_DEFAULT_BRIGHTNESS * 31 + 127) / 255;
    rgbToBuffer(index, r, g, b, brightness_5bit);
    return true;
}

/**
 * Set each pixel color (RGBW)
 * For APA102/SK9822, w parameter is interpreted as brightness (0-255, scaled to 0-31)
 */
bool HW_NeoPixel_SPI::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_inst || index >= _inst->ledCount)
    {
        return false;
    }

    // w parameter is interpreted as brightness (0-255) for APA102/SK9822
    // Scale from 0-255 to 0-31 (5-bit brightness)
    uint8_t brightness_5bit = (w * 31 + 127) / 255;
    rgbToBuffer(index, r, g, b, brightness_5bit);
    return true;
}

/**
 * Convert RGB to buffer format
 */
void HW_NeoPixel_SPI::rgbToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    if (!_inst || !_inst->buffer) return;

    switch (_inst->protocol)
    {
        case LedProtocol::APA102:
        case LedProtocol::SK9822:
        {
            // Info - Format: 0b11111BBB RRRRGGGG BBBBWWWW for APA102
            // Start Frame:   4x 0x00
            // End Frame:     max. 15 Bits at the end
            uint32_t offset = 4 + (index * 4);                  // Skip Start Frame
            _inst->buffer[offset] = 0xE0 | (brightness & 0x1F); // Global Brightness
            _inst->buffer[offset + 1] = b;                      // Blue
            _inst->buffer[offset + 2] = r;                      // Red
            _inst->buffer[offset + 3] = g;                      // Green
            break;
        }

        case LedProtocol::WS2801:
        {
            // Format: RGB simple, 8-Bit per color
            uint32_t offset = index * 3;
            _inst->buffer[offset] = r;
            _inst->buffer[offset + 1] = g;
            _inst->buffer[offset + 2] = b;
            break;
        }

        case LedProtocol::LPD8806:
        {
            // Format: 7-Bit RGB (MSB is used as "Update" bit) - This is a special format. Need to shift right by 1
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

    // APA102 End Frame: Min. 4 Byte 0xFF (floor(n/2) Bits "1")
    for (int i = 0; i < 4; i++)
    {
        _inst->spi->transfer(0xFF); // Protocol requires at least 4 bytes of 0xFF
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
    DriverCapabilities caps;
    caps.supportsRGBW = false;    // SPI protocols do not support RGBW
    caps.supportsDMA = false;     // Hardware SPI is simple, no DMA needed
    caps.supportsAsync = false;   // Synchronous transmission
    caps.maxLeds = 10000;         // Practically limited by memory. 10k should be enough for SPI with RP2040/ESP32
    caps.maxFrequency = 25000000; // WS2801 max frequency
    return caps;
}
