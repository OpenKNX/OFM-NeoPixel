#include "NeoPixel.h"
#include "effects/EffectPool.h"      // Effect Pool for LED effects
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
      _autoUpdate(false)
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
    logInfoP("Update speed set to %d ms (%d FPS)", _updateInterval, 1000 / _updateInterval);
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
        logInfoP("Auto-update enabled @ %d FPS", 1000 / _updateInterval);
    }
    else
    {
        logInfoP("Auto-update disabled");
    }
}


