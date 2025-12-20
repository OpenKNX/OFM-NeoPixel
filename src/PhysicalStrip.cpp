#include "PhysicalStrip.h"
#include "hal/DriverFactory.h"
#include "hal/HW_NeoPixel_SPI.h"
#include <Arduino.h>

#ifdef ARDUINO_ARCH_RP2040
    #include "pio/pio_neopixel_serial.h"
    #include "pio/pio_neopixel_spi.h"
#endif

/**
 * @brief Constructor with automatic driver selection
 *
 * Creates a PhysicalStrip and automatically selects the best driver based on:
 * - Platform (RP2040, RP2350, ESP32)
 * - Protocol (1-Wire vs SPI)
 *
 * @param pin GPIO pin for LED data/clock
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812B, SK6812, APA102, etc.)
 * @param driverType Driver type (AUTO = automatically selected)
 */
PhysicalStrip::PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, TimingMode timingMode)
    : _driver(nullptr),
      _dataPin(pin),
      _clockPin(0xFFFFFFFF),
      _ledCount(ledCount),
      _protocol(protocol),
      _initialized(false),
      _colorOrder(ColorOrder::NONE),
      _hasColorOrder(false),
      _config(nullptr),
      _timingMode(timingMode)
{
    createDriver(driverType);
}

/**
 * @brief Constructor with manual driver selection for SPI
 *
 * @param pin MOSI pin (or data pin for 1-Wire)
 * @param ledCount Number of LEDs
 * @param protocol LED protocol
 * @param sckPin Clock pin (SPI only)
 * @param driverType Driver type (AUTO = automatic)
 */
PhysicalStrip::PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, uint32_t sckPin, DriverType driverType, TimingMode timingMode)
    : _driver(nullptr),
      _dataPin(pin),
      _clockPin(sckPin),
      _ledCount(ledCount),
      _protocol(protocol),
      _initialized(false),
      _colorOrder(ColorOrder::NONE),
      _hasColorOrder(false),
      _config(nullptr),
      _timingMode(timingMode)
{
    createDriver(driverType);
}

/**
 * @brief Constructor for SPI strips with custom frequency
 * @param pin Data pin (MOSI)
 * @param ledCount Number of LEDs
 * @param protocol LED protocol
 * @param sckPin Clock pin
 * @param csPin Chip select pin (-1 if unused)
 * @param frequencyHz SPI frequency in Hz
 * @param driverType Driver type
 */
PhysicalStrip::PhysicalStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, uint32_t sckPin, int csPin, uint32_t frequencyHz, DriverType driverType)
    : _driver(nullptr),
      _dataPin(pin),
      _clockPin(sckPin),
      _ledCount(ledCount),
      _protocol(protocol),
      _initialized(false),
      _colorOrder(ColorOrder::NONE),
      _hasColorOrder(false),
      _config(nullptr),
      _timingMode(TimingMode::AUTO)
{
#ifdef ARDUINO_ARCH_RP2040
    // Set default ColorOrder based on protocol (if not already set)
    // Note: _colorOrder is always NONE here (set in initializer), but we keep this
    // check for consistency with createDriver() and future extensibility
    if (_colorOrder == ColorOrder::NONE)
    {
        switch (protocol)
        {
            case LedProtocol::APA102:
                setColorOrder(ColorOrder::BGR); // APA102: BGR
                break;
            case LedProtocol::SK9822:
                setColorOrder(ColorOrder::RGB); // SK9822 clones: RGB
                break;
            case LedProtocol::WS2801:
                setColorOrder(ColorOrder::RGB); // WS2801: RGB
                break;
            default:
                // Keep NONE - driver uses protocol default
                break;
        }
    }

    // Pass ColorOrder to driver (NONE = driver uses protocol default)
    _driver = new PIO_NeoPixel_SPI(sckPin, pin, ledCount, protocol, frequencyHz, csPin, true, _colorOrder);

    // Create default config from driver
    if (_driver)
    {
        _config = _driver->createDefaultConfig();
    }
#endif
}

/**
 * @brief Destructor - cleans up resources
 */
PhysicalStrip::~PhysicalStrip()
{
    if (_config)
    {
        delete _config;
        _config = nullptr;
    }
    if (_driver)
    {
        delete _driver;
        _driver = nullptr;
    }
}

/**
 * @brief Create driver via DriverFactory
 * @param driverType Type of driver to create
 * @return true if driver was created successfully
 */
bool PhysicalStrip::createDriver(DriverType driverType)
{
    if (_driver)
    {
        delete _driver;
        _driver = nullptr;
    }

    if (_config)
    {
        delete _config;
        _config = nullptr;
    }

    // Use DriverFactory for creation, pass timingMode parameter
    _driver = DriverFactory::create(
        _dataPin,
        _ledCount,
        _protocol,
        driverType,
        _dataPin,   // MOSI = dataPin
        _clockPin,  // SCK = clockPin
        _timingMode // Pass through
    );

    // Create default config from driver
    if (_driver)
    {
        _config = _driver->createDefaultConfig();
    }

    // Set default ColorOrder based on protocol (if not already set by user)
    // Driver will use these defaults via setColorOrder() call
    if (_driver != nullptr && _colorOrder == ColorOrder::NONE)
    {
        switch (_protocol)
        {
            case LedProtocol::APA102:
                setColorOrder(ColorOrder::BGR); // APA102: Official BGR
                break;

            case LedProtocol::SK9822:
                setColorOrder(ColorOrder::RGB); // SK9822: Clones use RGB
                break;

            case LedProtocol::WS2812B:
            case LedProtocol::SK6812:
                setColorOrder(ColorOrder::GRB); // WS2812B: GRB
                break;

            case LedProtocol::WS2801:
                setColorOrder(ColorOrder::RGB); // WS2801: RGB
                break;

            default:
                // Keep NONE - driver uses protocol default from ProtocolHelper
                break;
        }
    }

    // Apply ColorOrder to config
    if (_config && _colorOrder != ColorOrder::NONE)
    {
        _config->setColorOrder(_colorOrder);
        if (_driver)
        {
            _driver->applyConfig(_config);
        }
    }

    return _driver != nullptr;
}

/**
 * @brief Initialize the strip
 * Must be called before setPixel/show
 * @return true on success
 */
bool PhysicalStrip::init()
{
    if (!_driver)
    {
        Serial.println("PhysicalStrip: No driver available");
        return false;
    }

    if (_initialized) return true;

    // Create config if not already present (for strips loaded from storage)
    if (!_config && _driver)
    {
        _config = _driver->createDefaultConfig();
    }

    if (!_driver->init())
    {
        Serial.println("PhysicalStrip: Driver init failed");
        return false;
    }

    _initialized = true;
    return true;
}

/**
 * @brief Check if strip is initialized
 * @return true if initialized and driver is ready
 */
bool PhysicalStrip::isInitialized() const
{
    return _initialized && _driver && _driver->isInitialized();
}

/**
 * @brief Set a single RGB pixel
 * @param index LED index (0-based)
 * @param r Red (0-255, logical RGB)
 * @param g Green (0-255, logical RGB)
 * @param b Blue (0-255, logical RGB)
 * @return true on success
 *
 * Passes logical RGB values directly to driver.
 * Driver handles ColorOrder mapping and reads hwBrightness from config.
 */
bool PhysicalStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_driver || !isInitialized()) return false;

    // Pass RGB directly to driver
    // Driver handles ColorOrder mapping and brightness from config
    return _driver->setPixel(index, r, g, b);
}

/**
 * @brief Set a single RGBW pixel
 * @param index LED index
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param w White (0-255)
 * @return true on success
 */
bool PhysicalStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_driver || !isInitialized()) return false;

    // Pass RGB directly to driver - driver handles ColorOrder mapping
    // No conversion here to avoid double-mapping!
    return _driver->setPixel(index, r, g, b, w);
}

/**
 * @brief Set all LEDs to same color (RGB)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void PhysicalStrip::setAll(uint8_t r, uint8_t g, uint8_t b)
{
    if (!_driver || !isInitialized()) return;

    for (uint16_t i = 0; i < _ledCount; i++)
    {
        setPixel(i, r, g, b); // Use PhysicalStrip method (handles HardwareBrightness)
    }
}

/**
 * @brief Set all LEDs to same color (RGBW)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param w White (0-255)
 */
void PhysicalStrip::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_driver || !isInitialized()) return;

    for (uint16_t i = 0; i < _ledCount; i++)
    {
        setPixel(i, r, g, b, w); // Use PhysicalStrip method
    }
}

/**
 * @brief Turn off all LEDs (black)
 */
void PhysicalStrip::clear()
{
    if (!_driver || !isInitialized()) return;
    _driver->clear();
}

/**
 * @brief Update physical LEDs with buffer content
 * Can be blocking or non-blocking depending on driver
 * @return true on success
 */
bool PhysicalStrip::show()
{
    if (!_driver || !isInitialized()) return false;
    return _driver->show();
}

/**
 * @brief Wait until transfer is complete
 * @param timeoutMs Timeout in milliseconds (0 = unlimited)
 * @return true when finished, false on timeout
 */
bool PhysicalStrip::waitForTransfer(uint32_t timeoutMs)
{
    if (!_driver || !isInitialized()) return false;

    uint32_t startTime = millis();

    while (_driver->isBusy())
    {
        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs)
        {
            return false; // Timeout
        }
        delayMicroseconds(10);
    }

    return true;
}

/**
 * @brief Check if a transfer is currently in progress
 * @return true if busy
 */
bool PhysicalStrip::isBusy() const
{
    if (!_driver) return false;
    return _driver->isBusy();
}

/**
 * @brief Direct buffer access for advanced applications
 *        Format depends on protocol (GRB, GRBW, etc.)
 * @return Pointer to buffer, nullptr if not available
 * @warning Direct buffer access bypasses all validations!
 *          Use only if you know exactly what you're doing.
 */
uint8_t* PhysicalStrip::getBuffer()
{
    if (!_driver) return nullptr;
    return _driver->getBuffer();
}

/**
 * @brief Get buffer size in bytes
 * @return Buffer size
 */
size_t PhysicalStrip::getBufferSize() const
{
    if (!_driver) return 0;
    return _driver->getBufferSize();
}

/**
 * @brief Get number of LEDs in this strip
 * @return LED count
 */
uint16_t PhysicalStrip::getLedCount() const
{
    return _ledCount;
}

/**
 * @brief Get LED protocol
 * @return Protocol type
 */
LedProtocol PhysicalStrip::getProtocol() const
{
    return _protocol;
}

/**
 * @brief Get driver capabilities
 * @return Capabilities structure
 */
DriverCapabilities PhysicalStrip::getCapabilities() const
{
    if (!_driver)
    {
        DriverCapabilities empty = {false, false, false, 0, 0};
        return empty;
    }
    return _driver->getCapabilities();
}

/**
 * @brief Get driver name for debugging
 * @return Driver name string
 */
const char* PhysicalStrip::getDriverName() const
{
    if (!_driver) return "None";

    // Get driver type from driver implementation
    DriverImplementation driverType = _driver->getDriverType();

    switch (driverType)
    {
        case DriverImplementation::PIO_SERIAL:
            return "PIO Serial";
        case DriverImplementation::PIO_SPI:
            return "PIO SPI";
        case DriverImplementation::RMT_SERIAL:
            return "RMT Serial";
        case DriverImplementation::HARDWARE_SPI:
            return "HW SPI";
        case DriverImplementation::NATIVE:
            return "Native";
        default:
            return "Unknown";
    }
}

/**
 * @brief Change update frequency in Hz
 * Only implemented for certain drivers
 * @param frequencyHz New frequency
 * @return true if change was successful
 */
bool PhysicalStrip::setUpdateFrequency(uint32_t frequencyHz)
{
    if (!_driver || !isInitialized()) return false;

    // This function would need driver-specific implementations
    // For now: Return false (not supported)
    (void)frequencyHz;
    return false;
}

/**
 * @brief Set timing mode and reinitialize PIO (RP2040/RP2350 only)
 * @param mode New timing mode
 * @return true if mode was changed and driver reinitialized successfully
 */
bool PhysicalStrip::setTimingMode(TimingMode mode)
{
    if (_timingMode == mode)
    {
        return true; // Already set, no change needed
    }

    _timingMode = mode;

    // Reinitialize driver with new timing mode
    if (!_driver)
    {
        return false;
    }

#ifdef ARDUINO_ARCH_RP2040
    // For PIO Serial driver, we need to recreate it
    auto pioDriver = dynamic_cast<PIO_NeoPixel_Serial*>(_driver);
    if (pioDriver)
    {
        // Save current state
        bool wasBusy = pioDriver->isBusy();
        if (wasBusy)
        {
            // Wait for current transfer to complete
            uint32_t timeout = millis() + 100;
            while (pioDriver->isBusy() && millis() < timeout)
            {
                delay(1);
            }
        }

        // Delete old driver and create new one with new timing mode
        delete _driver;
        _driver = DriverFactory::create(
            _dataPin,
            _ledCount,
            _protocol,
            DriverType::SERIAL_1WIRE,
            _dataPin,
            _clockPin,
            _timingMode);

        if (!_driver)
        {
            _initialized = false;
            return false;
        }

        // Reinitialize
        if (!_driver->init())
        {
            _initialized = false;
            return false;
        }

        _initialized = true;
        return true;
    }
#endif

    // For non-PIO drivers or other platforms, timing mode has no effect
    return false;
}

// ============================================================================
// Hardware Configuration (NEW API)
// ============================================================================

bool PhysicalStrip::isSpiStrip() const
{
    return dynamic_cast<const SpiStripConfig*>(_config) != nullptr;
}

bool PhysicalStrip::isSerialStrip() const
{
    return dynamic_cast<const SerialStripConfig*>(_config) != nullptr;
}

bool PhysicalStrip::applyConfig()
{
    if (!_driver || !_config) return false;
    return _driver->applyConfig(_config);
}
