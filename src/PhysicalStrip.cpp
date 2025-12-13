#include "PhysicalStrip.h"
#include "hal/DriverFactory.h"

#ifdef ARDUINO_ARCH_RP2040
#include "pio/pio_neopixel_serial.h"
#endif

#include <Arduino.h>

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
      _colorOrder(ColorOrder::RGB),
      _hasColorOrder(false),
      _hardwareBrightness(255),
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
      _colorOrder(ColorOrder::RGB),
      _hasColorOrder(false),
      _hardwareBrightness(255),
      _timingMode(timingMode)
{
    createDriver(driverType);
}

/**
 * @brief Destructor - cleans up resources
 */
PhysicalStrip::~PhysicalStrip()
{
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

    // Use DriverFactory for creation, pass timingMode parameter
    _driver = DriverFactory::create(
        _dataPin,
        _ledCount,
        _protocol,
        driverType,
        _dataPin, // MOSI = dataPin
        _clockPin, // SCK = clockPin
        _timingMode // Pass through
    );

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
 * @param r Red (0-255, logical RGB color)
 * @param g Green (0-255, logical RGB color)
 * @param b Blue (0-255, logical RGB color)
 * @return true on success
 *
 * This method converts logical RGB to hardware ColorOrder:
 * - If PhysicalStrip has ColorOrder: Convert RGB → hardware order (e.g., GRB, BGR)
 * - If no ColorOrder: Pass RGB directly to driver (backward compatibility)
 * - For APA102/SK9822: Uses _hardwareBrightness for global brightness
 *
 * Examples:
 *   ColorOrder::GRB: RGB(50,0,0) → Driver gets [0,50,0] (G,R,B)
 *   ColorOrder::BGR: RGB(50,0,0) → Driver gets [0,0,50] (B,G,R)
 */
bool PhysicalStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_driver || !isInitialized()) return false;

    // If PhysicalStrip has ColorOrder, convert RGB to hardware order
    if (_hasColorOrder)
    {
        uint8_t byte0, byte1, byte2;
        switch (_colorOrder)
        {
            case ColorOrder::RGB:
                byte0 = r;
                byte1 = g;
                byte2 = b;
                break;
            case ColorOrder::RBG:
                byte0 = r;
                byte1 = b;
                byte2 = g;
                break;
            case ColorOrder::GRB:
                byte0 = g;
                byte1 = r;
                byte2 = b;
                break;
            case ColorOrder::GBR:
                byte0 = g;
                byte1 = b;
                byte2 = r;
                break;
            case ColorOrder::BGR:
                byte0 = b;
                byte1 = g;
                byte2 = r;
                break;
            case ColorOrder::BRG:
                byte0 = b;
                byte1 = r;
                byte2 = g;
                break;
            default:
                byte0 = r;
                byte1 = g;
                byte2 = b;
                break;
        }

        // For APA102/SK9822: Use 4-parameter setPixel with hardware brightness
        if (supportsHardwareBrightness())
        {
            return _driver->setPixel(index, byte0, byte1, byte2, _hardwareBrightness);
        }

        return _driver->setPixel(index, byte0, byte1, byte2);
    }

    // No ColorOrder: Pass RGB directly (backward compatibility)
    if (supportsHardwareBrightness())
    {
        return _driver->setPixel(index, r, g, b, _hardwareBrightness);
    }

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

    // If PhysicalStrip has ColorOrder, convert RGB to hardware order
    if (_hasColorOrder)
    {
        uint8_t byte0, byte1, byte2;
        switch (_colorOrder)
        {
            case ColorOrder::RGB:
            case ColorOrder::RGBW:
                byte0 = r;
                byte1 = g;
                byte2 = b;
                break;
            case ColorOrder::RBG:
                byte0 = r;
                byte1 = b;
                byte2 = g;
                break;
            case ColorOrder::GRB:
            case ColorOrder::GRBW:
                byte0 = g;
                byte1 = r;
                byte2 = b;
                break;
            case ColorOrder::GBR:
                byte0 = g;
                byte1 = b;
                byte2 = r;
                break;
            case ColorOrder::BGR:
                byte0 = b;
                byte1 = g;
                byte2 = r;
                break;
            case ColorOrder::BRG:
                byte0 = b;
                byte1 = r;
                byte2 = g;
                break;
            default:
                byte0 = r;
                byte1 = g;
                byte2 = b;
                break;
        }
        return _driver->setPixel(index, byte0, byte1, byte2, w);
    }

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
        _driver->setPixel(i, r, g, b);
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
        _driver->setPixel(i, r, g, b, w);
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

/**
 * @brief Set hardware brightness for APA102/SK9822 (0-255)
 * Only effective for SPI protocols with global brightness support
 * Silently ignored for WS2812B, SK6812, etc.
 *
 * @param brightness Brightness value (0-255, will be scaled to 5-bit: 0-31 for APA102)
 */
void PhysicalStrip::setHardwareBrightness(uint8_t brightness)
{
    _hardwareBrightness = brightness;
}

/**
 * @brief Check if this strip supports hardware brightness
 * @return true for APA102/SK9822, false for WS2812B/SK6812/etc.
 */
bool PhysicalStrip::supportsHardwareBrightness() const
{
    return (_protocol == LedProtocol::APA102 || _protocol == LedProtocol::SK9822);
}