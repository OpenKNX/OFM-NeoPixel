#include "NeoPixelManager.h"
#include "OpenKNX/Log/Logger.h"
#include <Arduino.h>

// ============================================================================
// Constructor & Destructor
// ============================================================================
/**
 * @brief Construct a new NeoPixelManager object
 */
NeoPixelManager::NeoPixelManager()
    : _initialized(false),
      _lastUpdateTime(0),
      _updateCount(0),
      _errorCount(0),
      _powerManager(5000)  // Default 5A (5000mA)
{
    // Pre-allocate vectors based on configured limits to avoid reallocation overhead
    _strips.reserve(NEOPIXEL_MAX_PHYSICAL_STRIPS);
    _virtualStrips.reserve(NEOPIXEL_MAX_VIRTUAL_STRIPS);
    _segments.reserve(NEOPIXEL_MAX_SEGMENTS);

    // Set default LED profile (WS2812B)
    _powerManager.setLedProfile(LedProfiles::WS2812B);

    logDebugP("NeoPixelManager: Configured limits - Strips: %d, VirtualStrips: %d, Segments: %d (Enforcement: %s)",
              NEOPIXEL_MAX_PHYSICAL_STRIPS,
              NEOPIXEL_MAX_VIRTUAL_STRIPS,
              NEOPIXEL_MAX_SEGMENTS,
              NEOPIXEL_ENFORCE_LIMITS ? "ON" : "OFF");
}

/**
 * @brief Destroy the NeoPixelManager object
 */
NeoPixelManager::~NeoPixelManager()
{
    // Lösche alle Strips
    for (auto strip : _strips)
    {
        if (strip)
        {
            delete strip;
        }
    }
    _strips.clear();
}

// =====================================================================
// Strip Management
// =====================================================================
/**
 * Add a new PhysicalStrip
 * @return Pointer to PhysicalStrip, nullptr on error
 * @param pin GPIO pin
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812B, SK6812, APA102, etc.)
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of strips reached
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    // Check if resources are available
    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    // Create new strip
    PhysicalStrip* strip = new PhysicalStrip(pin, ledCount, protocol);
    if (!strip)
    {
        _errorCount++;
        return nullptr;
    }

    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added strip at pin %d with %d LEDs", pin, ledCount);

    return strip;
}

/**
 * Overload mit expliziter Driver-Auswahl
 * @return Pointer to PhysicalStrip, nullptr on error
 * @param pin GPIO pin
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812B, SK6812, APA102, etc.)
 * @param driverType Explicit driver selection (default: AUTO)
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of strips reached
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    // Check if resources are available
    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    // Validations for specific driver types
    if (driverType == DriverType::SPI_PIO)
    {
#if !defined(ARDUINO_ARCH_RP2040) && !defined(PICO_RP2350)
        logDebugP("ERROR: SPI_PIO only available on RP2040/RP2350!");
        _errorCount++;
        return nullptr;
#endif
    }

    // Create new strip with explicit driver selection
    PhysicalStrip* strip = new PhysicalStrip(pin, ledCount, protocol, driverType);
    if (!strip)
    {
        logDebugP("NeoPixelManager: Could not create strip (Pin %d, Driver %d)",
                  pin, (int)driverType);
        _errorCount++;
        return nullptr;
    }

    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added strip at pin %d with %d LEDs (Driver: %d)",
              pin, ledCount, (int)driverType);

    return strip;
}

/**
 * Add a new SPI PhysicalStrip
 * @return Pointer to PhysicalStrip, nullptr on error
 * @param mosiPin MOSI GPIO pin
 * @param sckPin SCK GPIO pin
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812B, SK6812, APA102, etc.)
 */
PhysicalStrip* NeoPixelManager::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of strips reached
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    // Check if resources are available
    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for SPI protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    // Create new SPI strip
    PhysicalStrip* strip = new PhysicalStrip(mosiPin, ledCount, protocol, sckPin);
    if (!strip)
    {
        _errorCount++;
        return nullptr;
    }

    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added SPI strip at MOSI=%d, SCK=%d with %d LEDs",
              mosiPin, sckPin, ledCount);

    return strip;
}

/**
 * Add a new SPI PhysicalStrip with explicit driver selection
 * @return Pointer to PhysicalStrip, nullptr on error
 * @param mosiPin MOSI GPIO pin
 * @param sckPin SCK GPIO pin
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812B, SK6812, APA102, etc.)
 * @param driverType Explicit driver selection (default: AUTO)
 */
PhysicalStrip* NeoPixelManager::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of strips reached
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    // Prüfe ob Ressourcen verfügbar sind
    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for SPI protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    // Validierungen für SPI_PIO
    if (driverType == DriverType::SPI_PIO)
    {
#if !defined(ARDUINO_ARCH_RP2040) && !defined(PICO_RP2350)
        logDebugP("ERROR: SPI_PIO nur auf RP2040/RP2350 verfügbar!");
        _errorCount++;
        return nullptr;
#endif
    }

    // Erstelle neuen SPI-Strip mit expliziter Driver-Auswahl
    PhysicalStrip* strip = new PhysicalStrip(mosiPin, ledCount, protocol, sckPin, driverType);
    if (!strip)
    {
        logDebugP("NeoPixelManager: Konnte SPI-Strip nicht erstellen (MOSI=%d, SCK=%d, Driver=%d)",
                  mosiPin, sckPin, (int)driverType);
        _errorCount++;
        return nullptr;
    }

    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added SPI strip at MOSI=%d, SCK=%d with %d LEDs (Driver: %d)",
              mosiPin, sckPin, ledCount, (int)driverType);

    return strip;
}

/**
 * Add PhysicalStrip with ColorOrder
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder)
{
    PhysicalStrip* strip = addStrip(pin, ledCount, protocol);
    if (strip)
    {
        strip->setColorOrder(colorOrder);
        logDebugP("NeoPixelManager: Set ColorOrder %d for strip at pin %d", (int)colorOrder, pin);
    }
    return strip;
}

/**
 * Add PhysicalStrip with ColorOrder and DriverType
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder)
{
    PhysicalStrip* strip = addStrip(pin, ledCount, protocol, driverType);
    if (strip)
    {
        strip->setColorOrder(colorOrder);
        logDebugP("NeoPixelManager: Set ColorOrder %d for strip at pin %d", (int)colorOrder, pin);
    }
    return strip;
}

/**
 * Add SPI PhysicalStrip with ColorOrder
 */
PhysicalStrip* NeoPixelManager::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder)
{
    PhysicalStrip* strip = addSpiStrip(mosiPin, sckPin, ledCount, protocol);
    if (strip)
    {
        strip->setColorOrder(colorOrder);
        logDebugP("NeoPixelManager: Set ColorOrder %d for SPI strip at MOSI=%d", (int)colorOrder, mosiPin);
    }
    return strip;
}

/**
 * Add SPI PhysicalStrip with ColorOrder and DriverType
 */
PhysicalStrip* NeoPixelManager::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder)
{
    PhysicalStrip* strip = addSpiStrip(mosiPin, sckPin, ledCount, protocol, driverType);
    if (strip)
    {
        strip->setColorOrder(colorOrder);
        logDebugP("NeoPixelManager: Set ColorOrder %d for SPI strip at MOSI=%d", (int)colorOrder, mosiPin);
    }
    return strip;
}

/**
 * @brief Remove a PhysicalStrip
 * @param strip Pointer to the PhysicalStrip to remove
 * @return true if the strip was found and removed
 * @return false if the strip was not found
 */
bool NeoPixelManager::removeStrip(PhysicalStrip* strip)
{
    if (!strip) return false;

    for (auto it = _strips.begin(); it != _strips.end(); ++it)
    {
        if (*it == strip)
        {
            delete *it;
            _strips.erase(it);
            return true;
        }
    }

    return false;
}

/**
 * @brief Get a PhysicalStrip by index
 * @param index Index of the strip
 * @return PhysicalStrip* Pointer to the PhysicalStrip, or nullptr if index is out of range
 */
PhysicalStrip* NeoPixelManager::getStrip(uint32_t index)
{
    if (index >= _strips.size()) return nullptr;
    return _strips[index];
}

/**
 * @brief Find a PhysicalStrip by its data pin
 * @param pin GPIO pin number
 * @return PhysicalStrip* Pointer to the PhysicalStrip, or nullptr if not found
 */
PhysicalStrip* NeoPixelManager::findStripByPin(uint32_t pin)
{
    for (auto strip : _strips)
    {
        if (strip && strip->getDataPin() == pin)
        {
            return strip;
        }
    }
    return nullptr;
}

// =====================================================================
// Initialization & Control
// =====================================================================
/**
 * Initialisiere alle Strips
 * @return true wenn alle erfolgreich initialisiert
 */
bool NeoPixelManager::init()
{
    // REMOVED: if (_initialized) return true;
    // Allow re-initialization when strips are added dynamically!

    // logDebugP("NeoPixelManager: Initializing %d strips...", _strips.size());
    logDebugP("Initializing NeoPixelManager with %d strips", _strips.size());

    int successCount = 0;
    for (auto strip : _strips)
    {
        if (strip)
        {
            // Re-init is safe, strips check if already initialized
            if (strip->init())
            {
                successCount++;
            }
            else
            {
                logDebugP("NeoPixelManager: Failed to init strip at pin %d", strip->getDataPin());
                _errorCount++;
            }
        }
    }

    if (successCount == _strips.size())
    {
        _initialized = true;
        logDebugP("NeoPixelManager: All strips initialized successfully");
        return true;
    }
    else
    {
        logDebugP("NeoPixelManager: Only %d/%d strips initialized", successCount, _strips.size());
        return false;
    }
}

/**
 * Reset all strips
 */
void NeoPixelManager::reset()
{
    for (auto strip : _strips)
    {
        if (strip)
        {
            strip->clear();
        }
    }
    _errorCount = 0;
    _updateCount = 0;
}

/**
 * Update all strips (blocking)
 * @return true if all strips were successfully updated
 */
bool NeoPixelManager::updateAll()
{
    if (!_initialized)
    {
        _errorCount++;
        return false;
    }

    uint32_t startTime = millis();
    bool allSuccess = true;

    // Starte alle Transfers
    for (auto strip : _strips)
    {
        if (strip)
        {
            if (!strip->show())
            {
                allSuccess = false;
                _errorCount++;
            }
        }
    }

    _lastUpdateTime = millis() - startTime;
    if (allSuccess) _updateCount++;

    return allSuccess;
}

/**
 * Update all strips (non-blocking)
 * @param deltaTime Time since last update in milliseconds
 */
void NeoPixelManager::update(uint32_t deltaTime)
{
    if (!_initialized)
    {
        _errorCount++;
        return;
    }

    // ========== PHASE 1: UPDATE EFFECTS ==========
    // Für jedes Segment: rufe effect->update() auf
    for (auto segment : _segments)
    {
        if (segment)
        {
            segment->update(deltaTime);
        }
    }

    // ========== PHASE 2: GLOBAL POWER MANAGEMENT ==========
    // Apply global current limiting BEFORE sync to physical
    // This ensures one power limit for ALL LEDs (not per VirtualStrip)
    if (_powerManager.isEnabled())
    {
        // Step 1: Calculate total current across ALL VirtualStrips
        uint32_t totalRequestedCurrent = 0;
        for (auto vstrip : _virtualStrips)
        {
            if (vstrip && vstrip->getBuffer())
            {
                uint16_t ledCount = vstrip->getLedCount();
                uint8_t bytesPerPixel = vstrip->getBytesPerLed();
                uint8_t hardwareBrightness = vstrip->getHardwareBrightness();
                const uint8_t* buffer = vstrip->getBuffer();

                uint32_t stripCurrent = _powerManager.calculateTotalCurrent(
                    buffer, ledCount, bytesPerPixel, hardwareBrightness);
                totalRequestedCurrent += stripCurrent;
            }
        }

        // Step 2: Check if limiting is needed
        if (totalRequestedCurrent > _powerManager.getMaxCurrent())
        {
            // Calculate global scale factor
            float globalScale = (float)_powerManager.getMaxCurrent() / (float)totalRequestedCurrent;

            // Step 3: Apply same scale to ALL VirtualStrip buffers
            for (auto vstrip : _virtualStrips)
            {
                if (vstrip && vstrip->getBuffer())
                {
                    uint16_t ledCount = vstrip->getLedCount();
                    uint8_t bytesPerPixel = vstrip->getBytesPerLed();
                    uint8_t* buffer = vstrip->getBuffer();

                    // Scale all pixels in this VirtualStrip
                    for (uint16_t i = 0; i < ledCount; i++)
                    {
                        uint16_t offset = i * bytesPerPixel;
                        buffer[offset] = (uint8_t)(buffer[offset] * globalScale);         // R
                        buffer[offset + 1] = (uint8_t)(buffer[offset + 1] * globalScale); // G
                        buffer[offset + 2] = (uint8_t)(buffer[offset + 2] * globalScale); // B
                        if (bytesPerPixel == 4)
                        {
                            buffer[offset + 3] = (uint8_t)(buffer[offset + 3] * globalScale); // W
                        }
                    }
                }
            }

            // Update PowerManager statistics for correct reporting
            _powerManager.setCachedCurrentValues(
                totalRequestedCurrent,           // What was requested
                _powerManager.getMaxCurrent()    // What is actually flowing (capped at limit)
            );
        }
        else
        {
            // No limiting needed - actual = requested
            _powerManager.setCachedCurrentValues(totalRequestedCurrent, totalRequestedCurrent);
        }
    }

    // ========== PHASE 3: SYNC VIRTUAL TO PHYSICAL ==========
    // Synchronisiere alle Virtual→Physical Buffer mit Hardware-Brightness
    // (AFTER power limiting has been applied to VirtualStrip buffers)
    for (auto segment : _segments)
    {
        if (segment && segment->getVirtualStrip())
        {
            VirtualStrip* vstrip = segment->getVirtualStrip();
            if (vstrip->isDirty())
            {
                // Propagiere Hardware-Brightness vom Segment zu PhysicalStrips
                uint8_t hwBrightness = segment->getHardwareBrightness();
                vstrip->syncToPhysical(hwBrightness);
            }
        }
    }

    // ========== PHASE 4: SEND TO HARDWARE ==========
    // Starte DMA zu allen PhysicalStrips (non-blocking!)
    for (auto strip : _strips)
    {
        if (strip)
        {
            strip->show();
        }
    }

    _lastUpdateTime = 0; // Reset timing
    _updateCount++;
}

/**
 * Wait for all strips (blocking)
 * @param timeoutMs Timeout in milliseconds
 * @return true if all strips are ready, false on timeout
 */
bool NeoPixelManager::waitForAll(uint32_t timeoutMs)
{
    uint32_t startTime = millis();

    while (isAnyBusy())
    {
        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs)
        {
            _errorCount++;
            return false; // Timeout
        }
        delayMicroseconds(10);
    }

    return true;
}

/**
 * Wait for a specific strip (blocking)
 * @param strip Pointer to the PhysicalStrip
 * @param timeoutMs Timeout in milliseconds
 * @return true if the strip is ready, false on timeout
 */
bool NeoPixelManager::waitForStrip(PhysicalStrip* strip, uint32_t timeoutMs)
{
    if (!strip) return false;

    uint32_t startTime = millis();

    while (strip->isBusy())
    {
        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs)
        {
            _errorCount++;
            return false;
        }
        delayMicroseconds(10);
    }

    return true;
}

/**
 * Check if any strip is busy
 * @return true if any strip is busy, false otherwise
 */
bool NeoPixelManager::isAnyBusy() const
{
    for (const auto strip : _strips)
    {
        if (strip && strip->isBusy())
        {
            return true;
        }
    }
    return false;
}

/**
 * Check if all strips are ready
 * @return true if all strips are ready, false if any is busy
 */
bool NeoPixelManager::areAllReady() const
{
    return !isAnyBusy();
}

/**
 * Set all strips to the specified RGB color
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
void NeoPixelManager::setAllRGB(uint8_t r, uint8_t g, uint8_t b)
{
    for (auto strip : _strips)
    {
        if (strip)
        {
            strip->setAll(r, g, b);
        }
    }
}

/**
 * Set all strips to the specified RGBW color
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param w White component
 */
void NeoPixelManager::setAllRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    for (auto strip : _strips)
    {
        if (strip)
        {
            strip->setAll(r, g, b, w);
        }
    }
}

/**
 * Clear all strips
 */
void NeoPixelManager::clearAll()
{
    for (auto strip : _strips)
    {
        if (strip)
        {
            strip->clear();
        }
    }
}

/**
 * Set all strips to blackout (Switch to off) - (clear + update: blocking)
 */
void NeoPixelManager::blackout()
{
    clearAll();
    updateAll();
}

/**
 * Set all strips to blackout (Switch to off) - (clear + update: non-blocking)
 */
ManagerStats NeoPixelManager::getStats() const
{
    ManagerStats stats = {0};
    stats.totalStrips = _strips.size();
    stats.lastUpdateTime = _lastUpdateTime;
    stats.updateCount = _updateCount;
    stats.errorCount = _errorCount;

    for (const auto strip : _strips)
    {
        if (strip && strip->isInitialized())
        {
            stats.activeStrips++;
            stats.totalLeds += strip->getLedCount();
        }
    }

    return stats;
}

/**
 * Print debug information about the manager and all strips
 */
void NeoPixelManager::printDebugInfo()
{
    ManagerStats stats = getStats();

    logDebugP("========================================");
    logDebugP("NeoPixel Manager - Debug Info");
    logDebugP("========================================");
    logDebugP("Initialized:     %s", _initialized ? "Yes" : "No");
    logDebugP("Total Strips:    %d", stats.totalStrips);
    logDebugP("Active Strips:   %d", stats.activeStrips);
    logDebugP("Total LEDs:      %d", stats.totalLeds);
    logDebugP("Updates:         %d", stats.updateCount);
    logDebugP("Errors:          %d", stats.errorCount);
    logDebugP("Last Update:     %d ms", stats.lastUpdateTime);
    logDebugP("Memory Usage:    %d bytes", getTotalMemoryUsage());
    logDebugP("----------------------------------------");

    for (uint32_t i = 0; i < _strips.size(); i++)
    {
        auto strip = _strips[i];
        if (strip)
        {
            logDebugP("Strip %d:", i);
            logDebugP("  Pin:     %d", strip->getDataPin());
            logDebugP("  LEDs:    %d", strip->getLedCount());
            logDebugP("  Driver:  %s", strip->getDriverName());
            logDebugP("  Init:    %s", strip->isInitialized() ? "Yes" : "No");
            logDebugP("  Busy:    %s", strip->isBusy() ? "Yes" : "No");

            auto caps = strip->getCapabilities();
            logDebugP("  Caps:    RGBW=%s DMA=%s Async=%s",
                      caps.supportsRGBW ? "Y" : "N",
                      caps.supportsDMA ? "Y" : "N",
                      caps.supportsAsync ? "Y" : "N");
        }
    }

    logDebugP("========================================");
}

/**
 * Get total memory usage of all strips
 * @return Total memory usage in bytes
 */
uint32_t NeoPixelManager::getTotalMemoryUsage() const
{
    uint32_t total = 0;
    for (const auto strip : _strips)
    {
        if (strip)
        {
            total += strip->getBufferSize();
        }
    }
    return total;
}

/**
 * Get total LED count of all strips
 * @return Total LED count
 */
uint32_t NeoPixelManager::getTotalLedCount() const
{
    uint32_t total = 0;
    for (const auto strip : _strips)
    {
        if (strip)
        {
            total += strip->getLedCount();
        }
    }
    return total;
}

/**
 * Get maximum number of supported strips based on platform
 * @return Maximum number of strips
 */
uint32_t NeoPixelManager::getMaxStrips()
{
#if defined(ARDUINO_ARCH_RP2040)
    #ifdef PICO_RP2350
    return 13; // RP2350: 11 PIO + 2 SPI
    #else
    return 9; // RP2040: 7 PIO + 2 SPI
    #endif
#elif defined(ARDUINO_ARCH_ESP32)
    return 9; // ESP32: 7 RMT + 2 SPI
#else
    return 2; // Default/Other
#endif
}

/**
 * Check if resources are available for a new strip with the given protocol
 * @param protocol LED protocol
 * @return true if resources are available, false otherwise
 */
bool NeoPixelManager::checkResourcesAvailable(LedProtocol protocol)
{
    uint32_t pioUsed = 0, spiUsed = 0, rmtUsed = 0;
    countResourceUsage(pioUsed, spiUsed, rmtUsed);

    bool is1Wire = ProtocolHelper::is1Wire(protocol);
    bool isSpi = ProtocolHelper::isSPI(protocol);

#if defined(ARDUINO_ARCH_RP2040)
    if (is1Wire)
    {
    #ifdef PICO_RP2350
        return pioUsed < 11; // RP2350: max 11
    #else
        return pioUsed < 7; // RP2040: max 7
    #endif
    }
    else if (isSpi)
    {
        return spiUsed < 2;
    }
#elif defined(ARDUINO_ARCH_ESP32)
    if (is1Wire)
    {
        return rmtUsed < 7; // RMT channels 1-7 (0 reserved)
    }
    else if (isSpi)
    {
        return spiUsed < 2;
    }
#endif

    return true;
}

/**
 * Count current resource usage
 * @param pioUsed Reference to store used PIO count
 * @param spiUsed Reference to store used SPI count
 * @param rmtUsed Reference to store used RMT count
 */
void NeoPixelManager::countResourceUsage(uint32_t& pioUsed, uint32_t& spiUsed, uint32_t& rmtUsed)
{
    pioUsed = 0;
    spiUsed = 0;
    rmtUsed = 0;

    for (const auto strip : _strips)
    {
        if (strip && strip->isInitialized())
        {
            bool is1Wire = ProtocolHelper::is1Wire(strip->getProtocol());
            bool isSpi = ProtocolHelper::isSPI(strip->getProtocol());

            if (is1Wire)
            {
#if defined(ARDUINO_ARCH_RP2040)
                pioUsed++;
#elif defined(ARDUINO_ARCH_ESP32)
                rmtUsed++;
#endif
            }
            else if (isSpi)
            {
                spiUsed++;
            }
        }
    }
}

// ============================================================================
// Virtual Strip Management
// ============================================================================
/**
 * @brief Add a new virtual strip
 * @param totalLeds Number of LEDs in the virtual strip
 * @param colorOrder Color order for the virtual strip
 * @return VirtualStrip* Pointer to the newly created virtual strip, or nullptr on failure
 */
VirtualStrip* NeoPixelManager::addVirtualStrip(uint16_t totalLeds, ColorOrder colorOrder)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of virtual strips reached
    if (_virtualStrips.size() >= NEOPIXEL_MAX_VIRTUAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum virtual strips limit reached (%d/%d)",
                  _virtualStrips.size(), NEOPIXEL_MAX_VIRTUAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    VirtualStrip* vstrip = new VirtualStrip(totalLeds, colorOrder);
    if (!vstrip)
    {
        _errorCount++;
        return nullptr;
    }

    // Attach power manager for automatic current limiting
    vstrip->setPowerManager(&_powerManager);

    _virtualStrips.push_back(vstrip);
    logDebugP("NeoPixelManager: VirtualStrip added (%u LEDs)", totalLeds);
    return vstrip;
}

/**
 * @brief Attach a physical strip to a virtual strip at a given offset
 * @param vstrip Pointer to the virtual strip
 * @param pstrip Pointer to the physical strip
 * @param offset Offset within the virtual strip to attach the physical strip
 * @return true if attachment was successful
 * @return false if attachment failed
 */
bool NeoPixelManager::attachPhysicalToVirtual(VirtualStrip* vstrip, PhysicalStrip* pstrip, uint16_t offset)
{
    if (!vstrip || !pstrip)
    {
        _errorCount++;
        return false;
    }

    return vstrip->attachPhysicalStrip(pstrip, offset);
}

/**
 * @brief Get a virtual strip by index
 * @param index Index of the virtual strip
 * @return VirtualStrip* Pointer to the virtual strip, or nullptr if index is out of range
 */
VirtualStrip* NeoPixelManager::getVirtualStrip(uint32_t index)
{
    if (index >= _virtualStrips.size()) return nullptr;
    return _virtualStrips[index];
}

// ============================================================================
// Segment Management
// ============================================================================
/**
 * @brief Add a new segment
 * @param vstrip Pointer to the virtual strip
 * @param startLed Start LED index within the virtual strip
 * @param endLed End LED index within the virtual strip
 * @return Segment* Pointer to the newly created segment, or nullptr on failure
 */
Segment* NeoPixelManager::addSegment(VirtualStrip* vstrip, uint16_t startLed, uint16_t endLed)
{
#if NEOPIXEL_ENFORCE_LIMITS
    // Check if maximum number of segments reached
    if (_segments.size() >= NEOPIXEL_MAX_SEGMENTS)
    {
        logDebugP("NeoPixelManager: Maximum segments limit reached (%d/%d)",
                  _segments.size(), NEOPIXEL_MAX_SEGMENTS);
        _errorCount++;
        return nullptr;
    }
#endif

    if (!vstrip)
    {
        _errorCount++;
        return nullptr;
    }

    Segment* segment = new Segment(vstrip, startLed, endLed);
    if (!segment)
    {
        _errorCount++;
        return nullptr;
    }

    _segments.push_back(segment);
    logDebugP("NeoPixelManager: Segment added (%u-%u)", startLed, endLed);
    return segment;
}

/**
 * @brief Get a segment by index
 * @param index Index of the segment
 * @return Segment* Pointer to the segment, or nullptr if index is out of range
 */
Segment* NeoPixelManager::getSegment(uint32_t index)
{
    if (index >= _segments.size()) return nullptr;
    return _segments[index];
}

/**
 * @brief Attach an effect to a segment
 * @param segment Pointer to the segment
 * @param effect Pointer to the effect
 * @return true if attachment was successful
 * @return false if attachment failed
 */
bool NeoPixelManager::attachEffect(Segment* segment, Effect* effect)
{
    if (!segment || !effect)
    {
        _errorCount++;
        return false;
    }

    segment->setEffect(effect);
    return true;
}

// ============================================================================
// VirtualStrip Removal
// ============================================================================
/**
 * @brief Remove a VirtualStrip and all its associated Segments
 * @param vstrip Pointer to the VirtualStrip to remove
 * @return true if the VirtualStrip was found and removed
 * @return false if the VirtualStrip was not found
 */
bool NeoPixelManager::removeVirtualStrip(VirtualStrip* vstrip)
{
    if (!vstrip) return false;

    // First, remove all segments that belong to this VirtualStrip
    auto segIt = _segments.begin();
    while (segIt != _segments.end())
    {
        if (*segIt && (*segIt)->getVirtualStrip() == vstrip)
        {
            delete *segIt;
            segIt = _segments.erase(segIt);
        }
        else
        {
            ++segIt;
        }
    }

    // Now remove the VirtualStrip itself
    for (auto it = _virtualStrips.begin(); it != _virtualStrips.end(); ++it)
    {
        if (*it == vstrip)
        {
            delete *it;
            _virtualStrips.erase(it);
            logDebugP("NeoPixelManager: VirtualStrip removed");
            return true;
        }
    }

    return false;
}

/**
 * @brief Remove a VirtualStrip by index
 * @param index Index of the VirtualStrip to remove
 * @return true if the VirtualStrip was found and removed
 * @return false if the VirtualStrip was not found
 */
bool NeoPixelManager::removeVirtualStrip(uint32_t index)
{
    if (index >= _virtualStrips.size()) return false;

    VirtualStrip* vstrip = _virtualStrips[index];
    return removeVirtualStrip(vstrip);
}

// ============================================================================
// Segment Removal
// ============================================================================
/**
 * @brief Remove a Segment
 * @param segment Pointer to the Segment to remove
 * @return true if the Segment was found and removed
 * @return false if the Segment was not found
 */
bool NeoPixelManager::removeSegment(Segment* segment)
{
    if (!segment) return false;

    for (auto it = _segments.begin(); it != _segments.end(); ++it)
    {
        if (*it == segment)
        {
            delete *it;
            _segments.erase(it);
            logDebugP("NeoPixelManager: Segment removed");
            return true;
        }
    }

    return false;
}

/**
 * @brief Remove a Segment by index
 * @param index Index of the Segment to remove
 * @return true if the Segment was found and removed
 * @return false if the Segment was not found
 */
bool NeoPixelManager::removeSegment(uint32_t index)
{
    if (index >= _segments.size()) return false;

    Segment* segment = _segments[index];
    return removeSegment(segment);
}

// ============================================================================
// Power Management
// ============================================================================

/**
 * @brief Get estimated total power consumption
 * @return Power in Watts
 */
float NeoPixelManager::getTotalPowerWatts() const
{
    float totalPower = 0.0f;

    for (auto vstrip : _virtualStrips)
    {
        if (vstrip && vstrip->getBuffer())
        {
            uint16_t ledCount = vstrip->getLedCount();
            uint8_t bytesPerPixel = vstrip->getBytesPerLed();
            const uint8_t* buffer = vstrip->getBuffer();
            uint8_t hardwareBrightness = vstrip->getHardwareBrightness();

            totalPower += _powerManager.calculatePowerWatts(buffer, ledCount, bytesPerPixel, hardwareBrightness);
        }
    }

    return totalPower;
}