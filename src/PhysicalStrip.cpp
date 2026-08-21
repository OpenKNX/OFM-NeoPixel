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
      _timingMode(timingMode),
      _voltage(5),
      _dirty(true), _frameInFlight(false), _sentFrameCount(0), _skippedFrameCount(0), _timeoutCount(0), _transportFailureCount(0), _configurationFailureCount(0), _lastError(PhysicalStripError::NONE)
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
      _timingMode(timingMode),
      _voltage(5),
      _dirty(true), _frameInFlight(false), _sentFrameCount(0), _skippedFrameCount(0), _timeoutCount(0), _transportFailureCount(0), _configurationFailureCount(0), _lastError(PhysicalStripError::NONE)
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
      _timingMode(TimingMode::AUTO),
      _voltage(5),
      _dirty(true), _frameInFlight(false), _sentFrameCount(0), _skippedFrameCount(0), _timeoutCount(0), _transportFailureCount(0), _configurationFailureCount(0), _lastError(PhysicalStripError::NONE)
{
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

#ifdef ARDUINO_ARCH_RP2040
    // Pass ColorOrder to driver (NONE = driver uses protocol default)
    _driver = new PIO_NeoPixel_SPI(sckPin, pin, ledCount, protocol, frequencyHz, csPin, true, _colorOrder);
#elif defined(ARDUINO_ARCH_ESP32)
    // Hardware SPI with explicit pins/frequency (DriverFactory has no frequency parameter, so bypass it here)
    _driver = new HW_NeoPixel_SPI(pin, sckPin, ledCount, protocol, frequencyHz, csPin);
    if (_driver) _driver->setColorOrder(_colorOrder);
#endif

    // Create default config from driver
    if (_driver)
    {
        _config = _driver->createDefaultConfig();
    }
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

void PhysicalStrip::setColorOrder(ColorOrder order)
{
    const ColorOrder normalized = ProtocolHelper::normalizeColorOrder(_protocol, order);
    if (normalized != order)
    {
        Serial.printf("PhysicalStrip: ColorOrder %u (%u channels) exceeds protocol %u (%u channels); using %u\n",
                      (unsigned)order, (unsigned)ProtocolHelper::getChannelCount(order),
                      (unsigned)_protocol, (unsigned)ProtocolHelper::getBytesPerLed(_protocol),
                      (unsigned)normalized);
    }

    _colorOrder = normalized;
    if (_config) _config->setColorOrder(normalized);
    if (_driver) _driver->setColorOrder(normalized);
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
            case LedProtocol::APA102_CLONE:
                setColorOrder(ColorOrder::BGR); // APA102: Official BGR
                break;

            case LedProtocol::SK9822:
                setColorOrder(ColorOrder::RGB); // SK9822: Clones use RGB
                break;

            case LedProtocol::WS2812B:
                setColorOrder(ColorOrder::GRB); // WS2812B: GRB
                break;

            case LedProtocol::SK6812:
                setColorOrder(ColorOrder::GRBW); // SK6812 RGBW: GRBW
                break;

            case LedProtocol::WS2801:
                setColorOrder(ColorOrder::RGB); // WS2801: RGB
                break;

            default:
                setColorOrder(ProtocolHelper::getColorOrder(_protocol));
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
        _transportFailureCount++;
        _lastError = PhysicalStripError::RESOURCE_UNAVAILABLE;
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
        _transportFailureCount++;
        _lastError = PhysicalStripError::HARDWARE_ERROR;
        return false;
    }

    // Apply config now that the driver is up (CUSTOM-timing in applyConfig is gated on
    // initialized; a setCustomTiming() before init() only lands in _config until here).
    if (_config && !_driver->applyConfig(_config))
    {
        Serial.println("PhysicalStrip: Driver configuration failed");
        _configurationFailureCount++;
        _lastError = PhysicalStripError::INVALID_CONFIG;
        return false;
    }

    _initialized = true;
    _lastError = PhysicalStripError::NONE;
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

    // Check if this LED should be skipped (forced to black)
    if (_config)
    {
        // Fast path: First N LEDs (O(1), ~2 cycles)
        if (index < _config->getSkipFirstLeds())
        {
            r = g = b = 0;
        }
        // Flexible path: Bitset (O(1), ~3 cycles, only if mask initialized)
        else if (_config->isLedSkipped(index))
        {
            r = g = b = 0;
        }
        // Apply gamma correction if enabled (O(1) lookup table)
        else if (_config->isGammaCorrectionEnabled())
        {
            // Remember which channels had color before gamma
            bool hadR = (r > 0), hadG = (g > 0), hadB = (b > 0);

            r = _config->getGammaCorrectedValue(r);
            g = _config->getGammaCorrectedValue(g);
            b = _config->getGammaCorrectedValue(b);

            // Minimum value protection: if a channel had color, ensure it doesn't go to 0
            // This prevents LEDs from turning off completely at low brightness
            if (hadR && r == 0) r = 1;
            if (hadG && g == 0) g = 1;
            if (hadB && b == 0) b = 1;
        }

        // Apply white balance correction if enabled
        if (_config->isWhiteBalanceEnabled())
        {
            _config->applyWhiteBalance(r, g, b);
        }
    }

    // Pass RGB directly to driver
    // Driver handles ColorOrder mapping and brightness from config
    const bool updated = _driver->setPixel(index, r, g, b);
    if (updated) _dirty = true;
    return updated;
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

    // Check if this LED should be skipped (forced to black)
    if (_config)
    {
        // Fast path: First N LEDs (O(1), ~2 cycles)
        if (index < _config->getSkipFirstLeds())
        {
            r = g = b = w = 0;
        }
        // Flexible path: Bitset (O(1), ~3 cycles, only if mask initialized)
        else if (_config->isLedSkipped(index))
        {
            r = g = b = w = 0;
        }
        // Apply gamma correction if enabled (O(1) lookup table)
        else if (_config->isGammaCorrectionEnabled())
        {
            // Remember which channels had color before gamma
            bool hadR = (r > 0), hadG = (g > 0), hadB = (b > 0), hadW = (w > 0);

            r = _config->getGammaCorrectedValue(r);
            g = _config->getGammaCorrectedValue(g);
            b = _config->getGammaCorrectedValue(b);
            w = _config->getGammaCorrectedValue(w);

            // Minimum value protection: if a channel had color, ensure it doesn't go to 0
            // This prevents LEDs from turning off completely at low brightness
            if (hadR && r == 0) r = 1;
            if (hadG && g == 0) g = 1;
            if (hadB && b == 0) b = 1;
            if (hadW && w == 0) w = 1;
        }

        // Apply white balance correction if enabled (RGB only, not W)
        if (_config->isWhiteBalanceEnabled())
        {
            _config->applyWhiteBalance(r, g, b);
        }
    }

    // Pass RGB directly to driver - driver handles ColorOrder mapping
    // No conversion here to avoid double-mapping!
    const bool updated = _driver->setPixel(index, r, g, b, w);
    if (updated) _dirty = true;
    return updated;
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
 * @brief Set a single RGBCCT pixel (5-channel: RGB + Warm White + Cool White)
 * @param index LED index
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param ww Warm white (0-255, ~2700-3000K)
 * @param cw Cool white (0-255, ~5000-6500K)
 * @return true on success
 */
bool PhysicalStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_driver || !isInitialized()) return false;

    // Check if this LED should be skipped (forced to black)
    if (_config)
    {
        // Fast path: First N LEDs (O(1), ~2 cycles)
        if (index < _config->getSkipFirstLeds())
        {
            r = g = b = ww = cw = 0;
        }
        // Flexible path: Bitset (O(1), ~3 cycles, only if mask initialized)
        else if (_config->isLedSkipped(index))
        {
            r = g = b = ww = cw = 0;
        }
        // Apply gamma correction if enabled (O(1) lookup table)
        else if (_config->isGammaCorrectionEnabled())
        {
            // Remember which channels had color before gamma
            bool hadR = (r > 0), hadG = (g > 0), hadB = (b > 0);
            bool hadWW = (ww > 0), hadCW = (cw > 0);

            r = _config->getGammaCorrectedValue(r);
            g = _config->getGammaCorrectedValue(g);
            b = _config->getGammaCorrectedValue(b);
            ww = _config->getGammaCorrectedValue(ww);
            cw = _config->getGammaCorrectedValue(cw);

            // Minimum value protection: if a channel had color, ensure it doesn't go to 0
            if (hadR && r == 0) r = 1;
            if (hadG && g == 0) g = 1;
            if (hadB && b == 0) b = 1;
            if (hadWW && ww == 0) ww = 1;
            if (hadCW && cw == 0) cw = 1;
        }

        // Apply white balance correction if enabled (RGB only, not WW/CW)
        if (_config->isWhiteBalanceEnabled())
        {
            _config->applyWhiteBalance(r, g, b);
        }
    }

    // Pass RGBCCT directly to driver - driver handles ColorOrder mapping
    const bool updated = _driver->setPixel(index, r, g, b, ww, cw);
    if (updated) _dirty = true;
    return updated;
}

/**
 * @brief Set all LEDs to same color (RGBCCT - 5 channel)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param ww Warm white (0-255)
 * @param cw Cool white (0-255)
 */
void PhysicalStrip::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_driver || !isInitialized()) return;

    for (uint16_t i = 0; i < _ledCount; i++)
    {
        setPixel(i, r, g, b, ww, cw);
    }
}

/**
 * @brief Turn off all LEDs (black)
 */
void PhysicalStrip::clear()
{
    if (!_driver || !isInitialized()) return;
    _driver->clear();
    _dirty = true;
}

/**
 * @brief Update physical LEDs with buffer content
 * Can be blocking or non-blocking depending on driver
 * @return true on success
 */
bool PhysicalStrip::show()
{
    if (!_driver || !isInitialized())
    {
        _transportFailureCount++;
        _lastError = PhysicalStripError::NOT_INITIALIZED;
        return false;
    }
    if (_driver->isBusy())
    {
        _transportFailureCount++;
        _lastError = PhysicalStripError::BUSY;
        return false;
    }
    if (!_driver->show())
    {
        _transportFailureCount++;
        _lastError = PhysicalStripError::HARDWARE_ERROR;
        return false;
    }
    _lastError = PhysicalStripError::NONE;
    return true;
}

/**
 * @brief Wait until transfer is complete
 * @param timeoutMs Timeout in milliseconds (0 = driver-derived deadline)
 * @return true when finished, false on timeout
 */
bool PhysicalStrip::waitForTransfer(uint32_t timeoutMs)
{
    if (!_driver || !isInitialized())
    {
        _transportFailureCount++;
        _lastError = PhysicalStripError::NOT_INITIALIZED;
        return false;
    }

    // A caller which does not specify a deadline uses the driver's calculated
    // frame time. This is still bounded, so a wedged peripheral cannot spin
    // forever, but long valid strips are not rejected by an arbitrary cap.
    if (timeoutMs == 0) timeoutMs = getTransferTimeoutMs();
    uint32_t startTime = millis();

    while (_driver->isBusy())
    {
        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs)
        {
            _timeoutCount++;
            _lastError = PhysicalStripError::TIMEOUT;
            return false; // Timeout
        }
        delayMicroseconds(10);
    }

    _lastError = PhysicalStripError::NONE;
    return true;
}

uint32_t PhysicalStrip::getTransferTimeoutMs() const
{
    if (!_driver) return 1000U;

    const uint32_t timeoutUs = _driver->getTransferTimeoutUs();
    // Round up: a 1,001 us deadline must never become a 1 ms deadline.
    return (timeoutUs + 999U) / 1000U;
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
        return {};
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
bool PhysicalStrip::setCustomTiming(uint16_t t0h, uint16_t t0l, uint16_t t1h, uint16_t t1l, uint32_t resetUs)
{
    if (!_config || !_config->isSerialConfig()) return false;
    auto* sCfg = static_cast<SerialStripConfig*>(_config);
    const SerialStripConfig previousConfig = *sCfg;
    const TimingMode previousTimingMode = _timingMode;
    sCfg->setTiming(t0h, t0l, t1h, t1l);
    if (resetUs > 0) sCfg->setResetTime(resetUs);
    sCfg->setTimingMode(TimingMode::CUSTOM);
    _timingMode = TimingMode::CUSTOM;

    if (applyConfig()) return true;

    // A failed hardware change must not leave a configuration object claiming
    // timing which was never applied. Restore the previous known-good state.
    *sCfg = previousConfig;
    _timingMode = previousTimingMode;
    return false;
}

bool PhysicalStrip::clearCustomTiming()
{
    if (!_config || !_config->isSerialConfig()) return false;
    auto* sCfg = static_cast<SerialStripConfig*>(_config);
    const SerialStripConfig previousConfig = *sCfg;
    const TimingMode previousTimingMode = _timingMode;
    sCfg->setTiming(0, 0, 0, 0);
    sCfg->setResetTime(0);
    sCfg->setTimingMode(TimingMode::AUTO);
    _timingMode = TimingMode::AUTO;
    if (applyConfig()) return true;

    *sCfg = previousConfig;
    _timingMode = previousTimingMode;
    return false;
}

bool PhysicalStrip::setTimingMode(TimingMode mode)
{
    if (_timingMode == mode)
    {
        return true; // Already set, no change needed
    }

    if (!_config || !_config->isSerialConfig()) return false;
    auto* sCfg = static_cast<SerialStripConfig*>(_config);
    const SerialStripConfig previousConfig = *sCfg;
    const TimingMode previousTimingMode = _timingMode;

    // A named timing mode and a four-value override are mutually exclusive.
    // Do not recreate a driver: that loses buffers, DMA ownership and the
    // last known-good configuration if initialization later fails.
    if (mode != TimingMode::CUSTOM) sCfg->setTiming(0, 0, 0, 0);
    sCfg->setTimingMode(mode);
    _timingMode = mode;
    if (applyConfig()) return true;

    *sCfg = previousConfig;
    _timingMode = previousTimingMode;
    return false;
}

// ============================================================================
// Hardware Configuration (NEW API)
// ============================================================================

bool PhysicalStrip::isSpiStrip() const
{
    return _config && _config->isSpiConfig();
}

bool PhysicalStrip::isSerialStrip() const
{
    return _config && _config->isSerialConfig();
}

bool PhysicalStrip::applyConfig()
{
    if (!_driver)
    {
        _configurationFailureCount++;
        _lastError = PhysicalStripError::RESOURCE_UNAVAILABLE;
        return false;
    }
    if (!_config)
    {
        _configurationFailureCount++;
        _lastError = PhysicalStripError::INVALID_CONFIG;
        return false;
    }
    if (_config->getColorOrder() != ColorOrder::NONE)
    {
        setColorOrder(_config->getColorOrder());
    }
    if (isInitialized() && !waitForTransfer()) return false;
    const bool applied = _driver->applyConfig(_config);
    if (applied)
    {
        _dirty = true;
        _lastError = PhysicalStripError::NONE;
    }
    else
    {
        _configurationFailureCount++;
        _lastError = PhysicalStripError::INVALID_CONFIG;
    }
    return applied;
}
