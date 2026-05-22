#include "NeoPixel.h"
#include "test/PerformanceTracker.h" // Performance tracking (always available)

#ifdef OPENKNX_NEOPIXEL_TESTS // Test systems (optional - animation/simple tests only)
    #include "test/AnimationTest.h"
    #include "test/SimpleTest.h"
#endif

NeoPixel neoPixelModule;          // Module instance
PerformanceTracker g_perfTracker; // Global performance tracker instance, always available

// =============================================================================
// NeoPixel Module Implementation
// =============================================================================
/**
 * @brief Construct a new NeoPixel module
 */
NeoPixel::NeoPixel()
    : _manager(nullptr),
      _initialized(false),
      _lastUpdateTime(0),
      _updateInterval(static_cast<uint32_t>(UpdateSpeed::NORMAL)), // Default: 20 FPS
      _autoUpdate(false),
      _fpsCounter(0),
      _fpsLastMeasure(0),
      _measuredFps(0.0f)
{
    // Initialize performance tracking (always available)
    g_perfTracker.reset();
}

/**
 * @brief Destroy the NeoPixel module
 */
NeoPixel::~NeoPixel()
{
    if (_manager)
    {
        delete _manager;
        _manager = nullptr;
    }
}

// =============================================================================
// NeoPixel OpenKNX Module Methods
// =============================================================================
/**
 * @brief Initialize the NeoPixel module
 */
void NeoPixel::init()
{
    logDebugP("Initializing NeoPixel module");

    _manager = new NeoPixelManager(); // Create manager instance
    if (!_manager)
    {
        logErrorP("Failed to create NeoPixel manager!");
        return;
    }

    logInfoP("NeoPixel module initialized");
}

/**
 * @brief Setup the NeoPixel module
 * @param configured true if OpenKNX is configured
 */
void NeoPixel::setup(bool configured)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return;
    }

    logDebugP("Setting up NeoPixel module");

    // Initialize the manager
    if (!_manager->init())
    {
        logErrorP("Failed to initialize NeoPixel manager!");
        return;
    }

    _initialized = true;
    logInfoP("NeoPixel module setup complete");

    // Print debug info
    auto stats = _manager->getStats();
    logDebugP("NeoPixel Stats: %d strips, %d LEDs total",
              stats.totalStrips, stats.totalLeds);

#ifdef OPENKNX_NEOPIXEL_AUTO_TEST
    // Auto-start AnimationTest if flag is set
    logInfoP("Auto-starting AnimationTest...");
    AnimationTest::instance().init(_manager);
#endif
}

/**
 * @brief Loop for the NeoPixel module
 * @param configured true if OpenKNX is configured
 */
void NeoPixel::loop(bool configured)
{
    if (!_initialized || !_manager)
    {
        return;
    }

#ifdef OPENKNX_NEOPIXEL_TESTS
    // Run AnimationTest if active
    if (AnimationTest::instance().isRunning())
    {
        AnimationTest::instance().loop();
        return; // Don't run normal loop when test is active
    }

    // Run SimpleTest if active
    if (SimpleTest::instance().isRunning())
    {
        SimpleTest::instance().loop();
        return;
    }
#endif

    // Auto-update mode - Render effects + send to hardware
    if (_autoUpdate && openknx.freeLoopTime()) // Only update if there's free loop time
    {
        uint32_t now = millis();
        if ((now - _lastUpdateTime) >= _updateInterval)
        {
            uint32_t deltaTime = now - _lastUpdateTime;
            _lastUpdateTime = now;

            // ALWAYS render effects + send to hardware
            uint32_t updateStart = micros();
            _manager->update(deltaTime);
            uint32_t updateTime = micros() - updateStart;

            // Record timing for performance tracking
            g_perfTracker.recordUpdate(updateTime);

            // FPS measurement (update every 1000ms)
            _fpsCounter++;
            if (now - _fpsLastMeasure >= 1000)
            {
                _measuredFps = _fpsCounter * 1000.0f / (now - _fpsLastMeasure);
                _fpsCounter = 0;
                _fpsLastMeasure = now;
            }
        }
    }
}

/**
 * @brief Process GroupObjects
 * @param ko GroupObject to process
 */
void NeoPixel::processInputKo(GroupObject& ko)
{
    // TODO: Implement KO processing for LED control
    // Example: KO for strip on/off, brightness, color, etc.
}

// =============================================================================
// NeoPixel Strip Management
// =============================================================================
/**
 * @brief Add a new LED strip
 */
PhysicalStrip* NeoPixel::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addStrip(pin, ledCount, protocol);
}

PhysicalStrip* NeoPixel::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addStrip(pin, ledCount, protocol, driverType);
}

PhysicalStrip* NeoPixel::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addStrip(pin, ledCount, protocol, colorOrder);
}

PhysicalStrip* NeoPixel::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addStrip(pin, ledCount, protocol, driverType, colorOrder);
}

/**
 * @brief Add a new SPI LED strip
 * @param mosiPin MOSI pin number (Data pin of the strip)
 * @param sckPin SCK pin number (Clock pin of the strip)
 * @param ledCount Number of LEDs in the strip
 * @param protocol LED protocol type (Optional, default is APA102)
 */
PhysicalStrip* NeoPixel::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addSpiStrip(mosiPin, sckPin, ledCount, protocol);
}

PhysicalStrip* NeoPixel::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addSpiStrip(mosiPin, sckPin, ledCount, protocol, driverType);
}

PhysicalStrip* NeoPixel::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addSpiStrip(mosiPin, sckPin, ledCount, protocol, colorOrder);
}

PhysicalStrip* NeoPixel::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addSpiStrip(mosiPin, sckPin, ledCount, protocol, driverType, colorOrder);
}

/**
 * @brief Add a virtual strip
 */
VirtualStrip* NeoPixel::addVirtualStrip(uint16_t totalLeds, ColorOrder colorOrder)
{
    if (!_manager)
    {
        logErrorP("NeoPixel manager not initialized!");
        return nullptr;
    }

    return _manager->addVirtualStrip(totalLeds, colorOrder);
}

// =============================================================================
// NeoPixel Update Control
// =============================================================================
/**
 * @brief Update all strips
 */
void NeoPixel::updateAll()
{
    if (_manager)
    {
        _manager->updateAll();
    }
}

/**
 * @brief Clear all strips
 */
void NeoPixel::clearAll()
{
    if (_manager)
    {
        _manager->clearAll();
    }
}

/**
 * @brief Set update speed for auto-update mode
 * @param speed UpdateSpeed preset (SLOW, NORMAL, FAST, MAX, LUDICROUS, FTL)
 */
void NeoPixel::setUpdateSpeed(UpdateSpeed speed)
{
    _updateInterval = static_cast<uint32_t>(speed);
    if (_updateInterval > 0)
    {
        logInfoP("Update speed set to %d ms (%d FPS)", _updateInterval, 1000 / _updateInterval);
    }
    else
    {
        logInfoP("Update speed set to %d ms (FTL mode - measuring actual FPS...)", _updateInterval);
    }
}

/**
 * @brief Enable or disable auto-update mode
 * @param enabled true to enable auto-update
 */
void NeoPixel::setAutoUpdate(bool enabled)
{
    _autoUpdate = enabled;
    if (enabled)
    {
        _lastUpdateTime = millis();
        _fpsCounter = 0;
        _fpsLastMeasure = millis();
        _measuredFps = 0.0f;
        if (_updateInterval > 0)
        {
            logInfoP("Auto-update enabled @ %d FPS", 1000 / _updateInterval);
        }
        else
        {
            logInfoP("Auto-update enabled @ FTL mode (unlimited)");
        }
    }
    else
    {
        logInfoP("Auto-update disabled");
    }
}

/**
 * @brief Get actual measured FPS
 * @return Measured FPS (0.0 if not yet measured)
 */
float NeoPixel::getActualFps() const
{
    return _measuredFps;
}
