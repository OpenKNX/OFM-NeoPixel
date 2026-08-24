#include "NeoPixelManager.h"
#include "OpenKNX/Log/Logger.h"
#include <Arduino.h>
#include <algorithm>
#include <new>

namespace
{
template <typename T>
bool managerOwns(const std::vector<T*>& entries, const T* value)
{
    return value && std::find(entries.begin(), entries.end(), value) != entries.end();
}

uint8_t loadPercent(uint32_t currentMa, uint32_t limitMa)
{
    if (limitMa == 0) return 0;
    const uint64_t percent = (static_cast<uint64_t>(currentMa) * 100U) / limitMa;
    return percent > 255U ? 255U : static_cast<uint8_t>(percent);
}
}

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
      _lastTransferErrorLogTime(0),
      _lastTransferError(PhysicalStripError::NONE),
      _powerManager(5000), // Default 5A (5000mA)
      _mappingDirty(true)  // Needs initial build
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
    clearTopology();
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
    if (ledCount == 0)
    {
        _errorCount++;
        return nullptr;
    }
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
    if (!strip || !strip->isValid())
    {
        delete strip;
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
    if (ledCount == 0)
    {
        _errorCount++;
        return nullptr;
    }
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
    if (!strip || !strip->isValid())
    {
        delete strip;
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
    if (ledCount == 0 || mosiPin == sckPin)
    {
        _errorCount++;
        return nullptr;
    }
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
    if (!strip || !strip->isValid())
    {
        delete strip;
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
    if (ledCount == 0 || mosiPin == sckPin)
    {
        _errorCount++;
        return nullptr;
    }
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
    if (!strip || !strip->isValid())
    {
        delete strip;
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
 * Add PhysicalStrip with ColorOrder and TimingMode
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder, TimingMode timingMode)
{
    if (ledCount == 0)
    {
        _errorCount++;
        return nullptr;
    }
#if NEOPIXEL_ENFORCE_LIMITS
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    PhysicalStrip* strip = new PhysicalStrip(pin, ledCount, protocol, DriverType::AUTO, timingMode);
    if (!strip || !strip->isValid())
    {
        delete strip;
        _errorCount++;
        return nullptr;
    }

    strip->setColorOrder(colorOrder);
    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added strip at pin %d with %d LEDs (ColorOrder: %d, TimingMode: %d)",
              pin, ledCount, (int)colorOrder, (int)timingMode);
    return strip;
}

/**
 * Add PhysicalStrip with ColorOrder, DriverType and TimingMode
 */
PhysicalStrip* NeoPixelManager::addStrip(uint32_t pin, uint16_t ledCount, LedProtocol protocol, DriverType driverType, ColorOrder colorOrder, TimingMode timingMode)
{
    if (ledCount == 0)
    {
        _errorCount++;
        return nullptr;
    }
#if NEOPIXEL_ENFORCE_LIMITS
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    PhysicalStrip* strip = new PhysicalStrip(pin, ledCount, protocol, driverType, timingMode);
    if (!strip || !strip->isValid())
    {
        delete strip;
        logDebugP("NeoPixelManager: Could not create strip (Pin %d, Driver %d)",
                  pin, (int)driverType);
        _errorCount++;
        return nullptr;
    }

    strip->setColorOrder(colorOrder);
    _strips.push_back(strip);
    _mappingDirty = true; // Mapping needs rebuild
    logDebugP("NeoPixelManager: Added strip at pin %d with %d LEDs (Driver: %d, ColorOrder: %d, TimingMode: %d)",
              pin, ledCount, (int)driverType, (int)colorOrder, (int)timingMode);
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
 * Add SPI PhysicalStrip with ColorOrder and custom frequency
 */
PhysicalStrip* NeoPixelManager::addSpiStrip(uint32_t mosiPin, uint32_t sckPin, uint16_t ledCount, LedProtocol protocol, ColorOrder colorOrder, uint32_t frequencyHz)
{
    if (ledCount == 0 || mosiPin == sckPin || frequencyHz == 0)
    {
        _errorCount++;
        return nullptr;
    }
#if NEOPIXEL_ENFORCE_LIMITS
    if (_strips.size() >= NEOPIXEL_MAX_PHYSICAL_STRIPS)
    {
        logDebugP("NeoPixelManager: Maximum physical strips limit reached (%d/%d)",
                  _strips.size(), NEOPIXEL_MAX_PHYSICAL_STRIPS);
        _errorCount++;
        return nullptr;
    }
#endif

    if (!checkResourcesAvailable(protocol))
    {
        logDebugP("NeoPixelManager: No resources available for SPI protocol %d", (int)protocol);
        _errorCount++;
        return nullptr;
    }

    PhysicalStrip* strip = new PhysicalStrip(mosiPin, ledCount, protocol, sckPin, -1, frequencyHz);
    if (!strip || !strip->isValid())
    {
        delete strip;
        _errorCount++;
        return nullptr;
    }

    strip->setColorOrder(colorOrder);
    _strips.push_back(strip);
    logDebugP("NeoPixelManager: Added SPI strip at MOSI=%d, SCK=%d with %d LEDs, %d Hz",
              mosiPin, sckPin, ledCount, frequencyHz);
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
            // Detach from every consumer BEFORE freeing, or their cached raw pointers
            // dangle and the next show()/syncToPhysical()/power-stat read dereferences
            // freed heap (silent reboot on RP2350).
            for (VirtualStrip* vs : _virtualStrips)
                if (vs) vs->detachPhysicalStrip(strip);
            _stripPowerCache.erase(strip);
            _physToVirtualMap.erase(strip);
            _mappingDirty = true; // force phys→virtual map rebuild on next sync

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

    // A previous successful configuration must not remain operational after a
    // later reconfiguration fails.
    _initialized = false;
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

    if (!_strips.empty() && successCount == static_cast<int>(_strips.size()))
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
 * @brief Remove all manager-owned strips, virtual strips, and segments.
 *
 * VirtualStrip mappings and Segment instances reference PhysicalStrip
 * instances, therefore deleting physical strips first leaves dangling pointers
 * that can be reached by a subsequent render or power-management pass.
 */
void NeoPixelManager::clearTopology()
{
    for (auto segment : _segments)
    {
        delete segment;
    }
    _segments.clear();

    for (auto vstrip : _virtualStrips)
    {
        delete vstrip;
    }
    _virtualStrips.clear();

    for (auto strip : _strips)
    {
        delete strip;
    }
    _strips.clear();

    _stripPowerCache.clear();
    _physToVirtualMap.clear();
    _mappingDirty = true;
    _initialized = false;
}

/**
 * Apply global power limiting to all PhysicalStrip buffers
 * Phase 3: Must be called AFTER syncAll() (works on PhysicalStrip buffers)
 * @public
 */
void NeoPixelManager::applyPowerLimit()
{
    if (!_powerManager.isEnabled())
    {
        // Do not leave stale limited-frame statistics behind when callers
        // explicitly disable ABL. More importantly, do not modify the physical
        // buffer in this mode.
        _stripPowerCache.clear();
        _globalCurrentMa = 0;
        _globalLimitMa = 0;
        _globalLoadPercent = 0;
        return;
    }

    // Rebuild mapping if needed (for LED count calculations)
    if (_mappingDirty)
    {
        rebuildPhysToVirtualMapping();
    }

    PowerLimitMode mode = _powerManager.getPowerLimitMode();

    if (mode == PowerLimitMode::GLOBAL)
    {
        // GLOBAL MODE: All strips using global power limit share one budget
        // Calculate total requested current from ALL "UseGlobal" strips

        uint32_t totalRequestedCurrent = 0;

        // Step 1: Sum up requested current from all PhysicalStrips with "UseGlobal" mode
        for (auto phys : _strips)
        {
            if (!phys) continue;

            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();
            if (stripPowerMode != 1)
            {
                continue; // Skip if not using global
            }

            // Calculate current from PhysicalStrip buffer (after syncAll())
            uint32_t stripCurrent = calculatePhysicalStripCurrent(phys);
            totalRequestedCurrent += stripCurrent;
        }

        // Step 2: Check if limiting is needed
        uint32_t effectiveLimit = _powerManager.getMaxCurrent();

        if (totalRequestedCurrent > effectiveLimit)
        {
            // Apply hysteresis: Target 95% of limit to prevent oscillation
            uint32_t targetCurrent = (effectiveLimit * 95) / 100;

            // Calculate global scale factor
            float globalScale = (float)targetCurrent / (float)totalRequestedCurrent;

            // Step 3: Apply same scale to ALL PhysicalStrip buffers with UseGlobal mode
            for (auto phys : _strips)
            {
                if (!phys) continue;
                auto* cfg = phys->getConfig();
                if (!cfg) continue;

                uint8_t stripPowerMode = cfg->getPowerLimitMode();
                if (stripPowerMode == 1 && phys->getBuffer())
                {
                    applyScaleToPhysicalBuffer(phys, globalScale);
                }
            }

            _powerManager.setCachedCurrentValues(totalRequestedCurrent, targetCurrent);

            // Cache global stats
            _globalCurrentMa = targetCurrent;
            _globalLimitMa = effectiveLimit;
            _globalLoadPercent = loadPercent(targetCurrent, effectiveLimit);
        }
        else
        {
            // No limiting needed
            _powerManager.setCachedCurrentValues(totalRequestedCurrent, totalRequestedCurrent);

            _globalCurrentMa = totalRequestedCurrent;
            _globalLimitMa = effectiveLimit;
            _globalLoadPercent = loadPercent(totalRequestedCurrent, effectiveLimit);
        }

        // Cache per-strip stats for ALL strips (for monitoring)
        _stripPowerCache.clear();

        for (auto phys : _strips)
        {
            if (!phys) continue;
            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();

            // Calculate current from PhysicalStrip buffer
            uint16_t ledCount = phys->getLedCount();
            uint32_t stripCurrent = calculatePhysicalStripCurrent(phys);

            // Determine effective limit based on mode
            uint32_t stripLimit = 0;

            if (stripPowerMode == 0)
            {
                stripLimit = 0; // Disabled
            }
            else if (stripPowerMode == 1)
            {
                stripLimit = effectiveLimit; // UseGlobal
            }
            else if (stripPowerMode == 2)
            {
                stripLimit = cfg->getMaxCurrentMa(); // FixedValue
            }
            else if (stripPowerMode == 3)
            {
                // PerLED: Use physical LED count (not VirtualStrip sum)
                stripLimit = ledCount * cfg->getCurrentPerLedMa();
            }

            StripPowerStats stats;
            stats.currentMa = stripCurrent;
            stats.limitMa = stripLimit;
            stats.loadPercent = loadPercent(stripCurrent, stripLimit);
            _stripPowerCache[phys] = stats;
        }

        // IMPORTANT: Apply individual limits to strips that don't use global
        // Even in GLOBAL mode, strips with Individual limits should be respected
        for (auto phys : _strips)
        {
            if (!phys) continue;

            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();

            // Only process strips with individual limits
            if (stripPowerMode == 0 || stripPowerMode == 1) continue;

            uint32_t stripLimit = 0;

            if (stripPowerMode == 2)
            {
                stripLimit = cfg->getMaxCurrentMa();
            }
            else if (stripPowerMode == 3)
            {
                // PerLED: Use physical LED count (not VirtualStrip sum)
                stripLimit = phys->getLedCount() * cfg->getCurrentPerLedMa();
            }

            if (stripLimit == 0) continue;

            // Calculate current from PhysicalStrip buffer
            uint32_t stripRequestedCurrent = calculatePhysicalStripCurrent(phys);

            // Get ABL parameters
            uint8_t autoBrLimit = cfg->getAutoBrightnessLimit();
            uint8_t threshold = cfg->getPowerLimitThreshold();
            uint32_t thresholdCurrent = (stripLimit * threshold) / 100;

            // Apply ABL if needed
            uint32_t actualCurrent = stripRequestedCurrent;
            if (stripRequestedCurrent > thresholdCurrent)
            {
                float stripScale = 1.0f;

                if (stripRequestedCurrent > stripLimit)
                {
                    uint32_t targetCurrent = (stripLimit * 95) / 100;
                    stripScale = (float)targetCurrent / (float)stripRequestedCurrent;
                    actualCurrent = targetCurrent;
                }
                else if (stripLimit > thresholdCurrent)
                {
                    float exceedRatio = (float)(stripRequestedCurrent - thresholdCurrent) /
                                        (float)(stripLimit - thresholdCurrent);
                    float targetBrightness = 100.0f - (exceedRatio * (100.0f - autoBrLimit));
                    stripScale = targetBrightness / 100.0f;
                    actualCurrent = (uint32_t)(stripRequestedCurrent * stripScale);
                }
                else
                {
                    stripScale = (float)stripLimit / (float)stripRequestedCurrent;
                    actualCurrent = stripLimit;
                }

                applyScaleToPhysicalBuffer(phys, stripScale);
            }

            // Update cache
            StripPowerStats stats;
            stats.currentMa = actualCurrent;
            stats.limitMa = stripLimit;
            stats.loadPercent = loadPercent(actualCurrent, stripLimit);
            _stripPowerCache[phys] = stats;
        }
    }
    else if (mode == PowerLimitMode::PER_CHANNEL)
    {
        // PER_CHANNEL MODE: Each PhysicalStrip gets its own limit

        _stripPowerCache.clear();

        // FIRST PASS: Calculate and cache current consumption for ALL strips
        for (auto phys : _strips)
        {
            if (!phys) continue;
            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();

            // Calculate current from PhysicalStrip buffer
            uint16_t ledCount = phys->getLedCount();
            uint32_t stripRequestedCurrent = calculatePhysicalStripCurrent(phys);

            // Determine strip limit
            uint32_t stripLimit = 0;

            if (stripPowerMode == 0)
            {
                stripLimit = 0; // Disabled
            }
            else if (stripPowerMode == 1)
            {
                stripLimit = _powerManager.getMaxCurrent(); // Use global
            }
            else if (stripPowerMode == 2)
            {
                stripLimit = cfg->getMaxCurrentMa(); // Fixed
            }
            else if (stripPowerMode == 3)
            {
                // PerLED: Use physical LED count (not VirtualStrip sum)
                stripLimit = ledCount * cfg->getCurrentPerLedMa();
            }

            // Cache stats
            StripPowerStats stats;
            stats.currentMa = stripRequestedCurrent;
            stats.limitMa = stripLimit;
            stats.loadPercent = loadPercent(stripRequestedCurrent, stripLimit);
            _stripPowerCache[phys] = stats;
        }

        // SECOND PASS: Apply power limiting to active strips
        for (auto phys : _strips)
        {
            if (!phys) continue;

            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();

            // 0=Disabled - skip limiting (already cached above)
            if (stripPowerMode == 0) continue;

            // Get cached values
            auto cacheIt = _stripPowerCache.find(phys);
            if (cacheIt == _stripPowerCache.end()) continue;

            uint32_t stripRequestedCurrent = cacheIt->second.currentMa;
            uint32_t stripLimit = cacheIt->second.limitMa;

            if (stripLimit == 0) continue; // No limit set

            // Get ABL parameters for this strip
            uint8_t autoBrLimit = 100;
            uint8_t threshold = 80;

            if (stripPowerMode == 1)
            {
                // Use global ABL settings
                autoBrLimit = _powerManager.getMaxBrightnessPercent();
                threshold = _powerManager.getThresholdPercent();
            }
            else if (stripPowerMode > 1)
            {
                // Use per-strip ABL settings
                autoBrLimit = cfg->getAutoBrightnessLimit();
                threshold = cfg->getPowerLimitThreshold();
            }

            // Calculate threshold current (when ABL starts dimming)
            uint32_t thresholdCurrent = (stripLimit * threshold) / 100;

            // Apply ABL-based limiting if needed
            uint32_t actualCurrent = stripRequestedCurrent;
            if (stripRequestedCurrent > thresholdCurrent)
            {
                float stripScale = 1.0f;

                if (stripRequestedCurrent > stripLimit)
                {
                    uint32_t targetCurrent = (stripLimit * 95) / 100;
                    stripScale = (float)targetCurrent / (float)stripRequestedCurrent;
                }
                else if (stripLimit > thresholdCurrent)
                {
                    float exceedRatio = (float)(stripRequestedCurrent - thresholdCurrent) /
                                        (float)(stripLimit - thresholdCurrent);
                    float targetBrightness = 100.0f - (exceedRatio * (100.0f - autoBrLimit));
                    stripScale = targetBrightness / 100.0f;
                }
                else
                {
                    stripScale = (float)stripLimit / (float)stripRequestedCurrent;
                }

                applyScaleToPhysicalBuffer(phys, stripScale);

                actualCurrent = (uint32_t)(stripRequestedCurrent * stripScale);
                if (actualCurrent > stripLimit) actualCurrent = stripLimit;

                cacheIt->second.currentMa = actualCurrent;
                cacheIt->second.loadPercent = loadPercent(actualCurrent, stripLimit);
            }
        }

        // Calculate global totals
        _globalCurrentMa = 0;
        _globalLimitMa = 0;
        for (const auto& entry : _stripPowerCache)
        {
            _globalCurrentMa += entry.second.currentMa;
            _globalLimitMa += entry.second.limitMa;
        }
        _globalLoadPercent = loadPercent(_globalCurrentMa, _globalLimitMa);
    }
    else if (mode == PowerLimitMode::PER_LED)
    {
        // PER_LED MODE: Limit calculated from LED count * mA per LED

        uint8_t globalCurrentPerLed = _powerManager.getMaxCurrentPerLed();

        _stripPowerCache.clear();

        // FIRST PASS: Calculate and cache current consumption
        for (auto phys : _strips)
        {
            if (!phys) continue;
            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();

            // Calculate current from PhysicalStrip buffer
            uint16_t ledCount = phys->getLedCount();
            uint32_t stripRequestedCurrent = calculatePhysicalStripCurrent(phys);

            // Determine strip limit
            uint32_t stripLimit = 0;

            if (stripPowerMode == 0)
            {
                stripLimit = 0; // Disabled
            }
            else if (stripPowerMode == 1)
            {
                // UseGlobal: Use physical LED count (not VirtualStrip sum)
                stripLimit = ledCount * globalCurrentPerLed;
            }
            else if (stripPowerMode == 2)
            {
                stripLimit = cfg->getMaxCurrentMa(); // Fixed
            }
            else if (stripPowerMode == 3)
            {
                // PerLED: Use physical LED count (not VirtualStrip sum)
                stripLimit = ledCount * cfg->getCurrentPerLedMa();
            }

            // Cache stats
            StripPowerStats stats;
            stats.currentMa = stripRequestedCurrent;
            stats.limitMa = stripLimit;
            stats.loadPercent = loadPercent(stripRequestedCurrent, stripLimit);
            _stripPowerCache[phys] = stats;
        }

        // SECOND PASS: Apply power limiting
        for (auto phys : _strips)
        {
            if (!phys) continue;
            auto* cfg = phys->getConfig();
            if (!cfg) continue;

            uint8_t stripPowerMode = cfg->getPowerLimitMode();
            if (stripPowerMode == 0) continue; // Skip disabled

            auto cacheIt = _stripPowerCache.find(phys);
            if (cacheIt == _stripPowerCache.end()) continue;

            uint32_t stripRequestedCurrent = cacheIt->second.currentMa;
            uint32_t stripLimit = cacheIt->second.limitMa;

            if (stripLimit == 0) continue;

            // Get ABL parameters
            uint8_t autoBrLimit = 100;
            uint8_t threshold = 80;

            if (stripPowerMode == 1)
            {
                autoBrLimit = _powerManager.getMaxBrightnessPercent();
                threshold = _powerManager.getThresholdPercent();
            }
            else if (stripPowerMode > 1)
            {
                autoBrLimit = cfg->getAutoBrightnessLimit();
                threshold = cfg->getPowerLimitThreshold();
            }

            uint32_t thresholdCurrent = (stripLimit * threshold) / 100;

            // Apply ABL if needed
            uint32_t actualCurrent = stripRequestedCurrent;
            if (stripRequestedCurrent > thresholdCurrent)
            {
                float stripScale = 1.0f;

                if (stripRequestedCurrent > stripLimit)
                {
                    uint32_t targetCurrent = (stripLimit * 95) / 100;
                    stripScale = (float)targetCurrent / (float)stripRequestedCurrent;
                }
                else if (stripLimit > thresholdCurrent)
                {
                    float exceedRatio = (float)(stripRequestedCurrent - thresholdCurrent) /
                                        (float)(stripLimit - thresholdCurrent);
                    float targetBrightness = 100.0f - (exceedRatio * (100.0f - autoBrLimit));
                    stripScale = targetBrightness / 100.0f;
                }
                else
                {
                    stripScale = (float)stripLimit / (float)stripRequestedCurrent;
                }

                applyScaleToPhysicalBuffer(phys, stripScale);

                actualCurrent = (uint32_t)(stripRequestedCurrent * stripScale);
                if (actualCurrent > stripLimit) actualCurrent = stripLimit;

                cacheIt->second.currentMa = actualCurrent;
                cacheIt->second.loadPercent = loadPercent(actualCurrent, stripLimit);
            }
        }

        // Calculate global totals
        _globalCurrentMa = 0;
        _globalLimitMa = 0;
        for (const auto& entry : _stripPowerCache)
        {
            _globalCurrentMa += entry.second.currentMa;
            _globalLimitMa += entry.second.limitMa;
        }
        _globalLoadPercent = loadPercent(_globalCurrentMa, _globalLimitMa);
    }
}

/**
 * Helper: Apply brightness scale to VirtualStrip buffer
 * @private
 */
void NeoPixelManager::applyScaleToBuffer(VirtualStrip* vstrip, float scale)
{
    if (!vstrip || !vstrip->getBuffer()) return;

    uint16_t ledCount = vstrip->getLedCount();
    uint8_t bytesPerPixel = vstrip->getBytesPerLed();
    uint8_t* buffer = vstrip->getBuffer();

    // Scale all pixels in this VirtualStrip
    for (uint16_t i = 0; i < ledCount; i++)
    {
        // Use size_t for offset calculation to prevent arithmetic overflow
        // (i * bytesPerPixel can exceed uint16_t range for large LED counts)
        size_t offset = (size_t)i * bytesPerPixel;

        // Validate buffer bounds before access
        size_t bufferSize = vstrip->getBufferSize();
        if (offset + bytesPerPixel > bufferSize)
        {
            logErrorP("NeoPixelManager: Power limiting buffer overflow! LED %u, offset %u, bufferSize %u",
                      i, (uint32_t)offset, (uint32_t)bufferSize);
            break; // Skip remaining LEDs for this strip
        }

        buffer[offset] = (uint8_t)(buffer[offset] * scale);         // R
        buffer[offset + 1] = (uint8_t)(buffer[offset + 1] * scale); // G
        buffer[offset + 2] = (uint8_t)(buffer[offset + 2] * scale); // B
        if (bytesPerPixel >= 4)
        {
            buffer[offset + 3] = (uint8_t)(buffer[offset + 3] * scale); // W
        }
        if (bytesPerPixel >= 5)
        {
            buffer[offset + 4] = (uint8_t)(buffer[offset + 4] * scale); // CW
        }
    }
}

/**
 * Helper: Apply brightness scale to PhysicalStrip buffer
 * Used by applyPowerLimit() when working on PhysicalStrip buffers (after syncAll())
 * @private
 */
/**
 * @brief Calculate requested current (mA) for a PhysicalStrip's current buffer content
 * @note SPI strips (APA102/SK9822) use a framed buffer (start frames + optional dummy LED +
 *       [brightness,R,G,B] per LED + end frames), not a flat RGB(W) array from offset 0 - read
 *       the correct per-LED offset and fold in that LED's hardware brightness byte.
 */
uint32_t NeoPixelManager::calculatePhysicalStripCurrent(PhysicalStrip* phys)
{
    if (!phys) return 0;
    const uint8_t* buffer = phys->getBuffer();
    if (!buffer) return 0;

    uint16_t ledCount = phys->getLedCount();

    const LedProtocol protocol = phys->getProtocol();

    if (protocol == LedProtocol::APA102 || protocol == LedProtocol::APA102_CLONE ||
        protocol == LedProtocol::SK9822)
    {
        auto* cfg = phys->getConfig();
        SpiStripConfig* spiCfg = cfg && cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
        if (!spiCfg) return 0;

        size_t bufferSize = phys->getBufferSize();
        size_t dataOffset = (size_t)spiCfg->getStartFrameCount() * 4 + (spiCfg->getDummyLedMode() == 1 ? 4 : 0);
        uint32_t totalCurrent = 0;

        for (uint16_t i = 0; i < ledCount; i++)
        {
            size_t offset = dataOffset + (size_t)i * 4;
            if (offset + 4 > bufferSize) break;

            uint8_t hwBrightness255 = (uint8_t)(((uint16_t)(buffer[offset] & 0x1F) * 255) / 31);
            totalCurrent += _powerManager.calculateTotalCurrent(&buffer[offset + 1], 1, 3, hwBrightness255);
        }
        return totalCurrent;
    }

    if (protocol == LedProtocol::SM16825)
    {
        constexpr size_t kBytesPerChannel = 2;
        constexpr size_t kChannels = 5;
        constexpr size_t kBytesPerPixel = kBytesPerChannel * kChannels;
        const size_t pixelDataSize = (size_t)ledCount * kBytesPerPixel;
        const size_t bufferSize = phys->getBufferSize();
        uint32_t totalCurrent = 0;

        for (uint16_t i = 0; i < ledCount; ++i)
        {
            const size_t offset = (size_t)i * kBytesPerPixel;
            if (offset + kBytesPerPixel > bufferSize || offset + kBytesPerPixel > pixelDataSize)
                break;

            // SM16825 channels are 16-bit MSB-first values. OFM expands an
            // 8-bit logical component as v*257, so the high byte is the exact
            // logical component used for current estimation.
            const uint8_t logicalChannels[kChannels] = {
                buffer[offset], buffer[offset + 2], buffer[offset + 4],
                buffer[offset + 6], buffer[offset + 8]};
            totalCurrent += _powerManager.calculateTotalCurrent(logicalChannels, 1, kChannels, 255);
        }
        return totalCurrent;
    }

    if (protocol == LedProtocol::LPD8806)
    {
        uint32_t totalCurrent = 0;
        const size_t bufferSize = phys->getBufferSize();
        for (uint16_t i = 0; i < ledCount; ++i)
        {
            const size_t offset = (size_t)i * 3;
            if (offset + 3 > bufferSize) break;

            // LPD8806 transports 7-bit components with bit 7 set as the
            // protocol update marker. Estimate using expanded 8-bit values.
            const uint8_t channels[3] = {
                (uint8_t)((((uint16_t)buffer[offset] & 0x7F) * 255U + 63U) / 127U),
                (uint8_t)((((uint16_t)buffer[offset + 1] & 0x7F) * 255U + 63U) / 127U),
                (uint8_t)((((uint16_t)buffer[offset + 2] & 0x7F) * 255U + 63U) / 127U)};
            totalCurrent += _powerManager.calculateTotalCurrent(channels, 1, 3, 255);
        }
        return totalCurrent;
    }

    // A physical buffer uses its protocol's native frame width. ColorOrder is
    // only a channel permutation and must not shrink that stride.
    uint8_t bytesPerPixel = ProtocolHelper::getBytesPerLed(protocol);
    return _powerManager.calculateTotalCurrent(buffer, ledCount, bytesPerPixel, 255);
}

void NeoPixelManager::applyScaleToPhysicalBuffer(PhysicalStrip* phys, float scale)
{
    if (!phys || !phys->getBuffer()) return;

    uint16_t ledCount = phys->getLedCount();
    uint8_t* buffer = phys->getBuffer();
    size_t bufferSize = phys->getBufferSize();

    const LedProtocol protocol = phys->getProtocol();

    if (protocol == LedProtocol::APA102 || protocol == LedProtocol::APA102_CLONE ||
        protocol == LedProtocol::SK9822)
    {
        // SPI buffers are framed as [start frames][optional dummy LED][brightness,R,G,B]*N[end
        // frames], not a flat RGB(W) array from offset 0 - the generic path below would scale
        // the wrong bytes. Scale the 8-bit RGB bytes at the correct offsets; scaling the 5-bit
        // brightness byte instead would quantise to 0 (strip goes fully dark) at low scales.
        auto* cfg = phys->getConfig();
        SpiStripConfig* spiCfg = cfg && cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
        if (!spiCfg) return;

        size_t dataOffset = (size_t)spiCfg->getStartFrameCount() * 4 + (spiCfg->getDummyLedMode() == 1 ? 4 : 0);

        for (uint16_t i = 0; i < ledCount; i++)
        {
            size_t offset = dataOffset + (size_t)i * 4;
            if (offset + 4 > bufferSize) break;

            buffer[offset + 1] = (uint8_t)(buffer[offset + 1] * scale);
            buffer[offset + 2] = (uint8_t)(buffer[offset + 2] * scale);
            buffer[offset + 3] = (uint8_t)(buffer[offset + 3] * scale);
        }
        return;
    }

    if (protocol == LedProtocol::SM16825)
    {
        constexpr size_t kBytesPerChannel = 2;
        constexpr size_t kChannels = 5;
        constexpr size_t kBytesPerPixel = kBytesPerChannel * kChannels;
        const size_t pixelDataSize = (size_t)ledCount * kBytesPerPixel;

        for (uint16_t i = 0; i < ledCount; ++i)
        {
            const size_t offset = (size_t)i * kBytesPerPixel;
            if (offset + kBytesPerPixel > bufferSize || offset + kBytesPerPixel > pixelDataSize)
                break;

            // Scale a complete 16-bit value and leave the four-byte SM16825
            // settings trailer immediately after pixelDataSize untouched.
            for (size_t channel = 0; channel < kChannels; ++channel)
            {
                uint8_t* value = buffer + offset + channel * kBytesPerChannel;
                const uint16_t original = ((uint16_t)value[0] << 8) | value[1];
                const uint16_t scaled = (uint16_t)((float)original * scale);
                value[0] = (uint8_t)(scaled >> 8);
                value[1] = (uint8_t)scaled;
            }
        }
        return;
    }

    if (protocol == LedProtocol::LPD8806)
    {
        for (uint16_t i = 0; i < ledCount; ++i)
        {
            const size_t offset = (size_t)i * 3;
            if (offset + 3 > bufferSize) break;

            // Preserve the mandatory update marker while scaling the 7-bit
            // payload; scaling the raw byte would clear that marker.
            for (size_t channel = 0; channel < 3; ++channel)
            {
                const uint8_t level = buffer[offset + channel] & 0x7F;
                buffer[offset + channel] = 0x80 | (uint8_t)((float)level * scale);
            }
        }
        return;
    }

    uint8_t bytesPerPixel = ProtocolHelper::getBytesPerLed(protocol);

    // Scale all pixels in this PhysicalStrip
    for (uint16_t i = 0; i < ledCount; i++)
    {
        size_t offset = (size_t)i * bytesPerPixel;

        // Validate buffer bounds before access
        if (offset + bytesPerPixel > bufferSize)
        {
            logErrorP("NeoPixelManager: Power limiting buffer overflow on PhysicalStrip! LED %u, offset %u, bufferSize %u",
                      i, (uint32_t)offset, (uint32_t)bufferSize);
            break; // Skip remaining LEDs for this strip
        }

        buffer[offset] = (uint8_t)(buffer[offset] * scale);         // R
        buffer[offset + 1] = (uint8_t)(buffer[offset + 1] * scale); // G
        buffer[offset + 2] = (uint8_t)(buffer[offset + 2] * scale); // B
        if (bytesPerPixel >= 4)
        {
            buffer[offset + 3] = (uint8_t)(buffer[offset + 3] * scale); // W
        }
        if (bytesPerPixel >= 5)
        {
            buffer[offset + 4] = (uint8_t)(buffer[offset + 4] * scale); // CW
        }
    }
}

/**
 * @brief Rebuild Physical→Virtual mapping table (performance optimization)
 *
 * Pre-computes which VirtualStrips use which PhysicalStrips to avoid O(n²) nested loops
 * in power limiting. Called automatically when mapping becomes dirty (after addStrip/addVirtualStrip).
 * @private
 */
void NeoPixelManager::rebuildPhysToVirtualMapping()
{
    _physToVirtualMap.clear();

    // Build mapping: PhysicalStrip → [VirtualStrip1, VirtualStrip2, ...]
    for (auto vstrip : _virtualStrips)
    {
        if (!vstrip) continue;

        // Check all physical strips attached to this virtual strip
        for (uint16_t i = 0; i < vstrip->getPhysicalStripCount(); i++)
        {
            PhysicalStrip* phys = vstrip->getPhysicalStrip(i);
            if (phys)
            {
                _physToVirtualMap[phys].push_back(vstrip);
            }
        }
    }

    _mappingDirty = false;
    logDebugP("NeoPixelManager: Rebuilt Phys→Virtual mapping (%u physical, %u virtual strips)",
              _physToVirtualMap.size(), _virtualStrips.size());
}

/**
 * Sync all VirtualStrips to their PhysicalStrips
 * Handles hardware brightness and dirty flag checking
 * Call this BEFORE applyPowerLimit() and before showAll()
 */
bool NeoPixelManager::syncAll()
{
    if (!_initialized) return false;

    bool allSuccess = true;

    // Virtual strips are valid without segments. Rebuild every physical frame
    // from its logical virtual buffer before power limiting touches it. ABL
    // scales physical buffers in place, so syncing only dirty virtual strips
    // would compound a previous limit on static content.
    for (auto vstrip : _virtualStrips)
    {
        if (vstrip)
        {
            // Hardware brightness is managed through PhysicalStripConfig.
            if (!vstrip->syncToPhysical())
            {
                allSuccess = false;
                _errorCount++;
            }
        }
    }

    return allSuccess;
}

/**
 * Send all PhysicalStrips to hardware (blocking)
 * @return true if all strips were successfully updated
 */
bool NeoPixelManager::showAll()
{
    if (!_initialized)
    {
        _errorCount++;
        return false;
    }

    uint32_t startTime = millis();
    bool allSuccess = true;

    // Push all strips CONCURRENTLY. Every PhysicalStrip owns its own PIO state machine,
    // DMA channel and buffer, and the unified DMA IRQ dispatches per channel — so the
    // transfers run in parallel with no shared state to corrupt. Phase 1 kicks off every
    // DMA; phase 2 waits for them all. Frame push time then ≈ the single LONGEST strip
    // instead of the SUM of all strips (≈13 ms vs ≈22 ms for this 6-strip config).
    // (A PIO-fallback strip without DMA blocks inside show() and is simply done by phase 2.)
    for (uint32_t stripIndex = 0; stripIndex < _strips.size(); stripIndex++)
    {
        auto strip = _strips[stripIndex];
        if (!strip) continue;
        if (!strip->isDirty())
        {
            strip->markFrameSkipped();
            continue;
        }
        if (!strip->show())
        {
            reportTransferFailure(stripIndex, strip, "start");
            allSuccess = false;
            _errorCount++;
        }
        else
        {
            strip->markFrameStarted();
        }
    }
    // Phase 2: wait for every in-flight transfer to finish. The deadline is
    // derived by the driver from its actual serial rate and frame size, rather
    // than imposing a fixed maximum strip length.
    for (uint32_t stripIndex = 0; stripIndex < _strips.size(); stripIndex++)
    {
        auto strip = _strips[stripIndex];
        // Only transfers started in phase 1 need completion tracking.  A failed
        // wait deliberately leaves the strip dirty so its complete frame is
        // retried on the next update instead of being silently dropped.
        if (!strip || !strip->hasFrameInFlight()) continue;

        if (!strip->waitForTransfer(strip->getTransferTimeoutMs()))
        {
            strip->markFrameFailed();
            reportTransferFailure(stripIndex, strip, "complete");
            allSuccess = false;
            _errorCount++;
        }
        else
        {
            strip->markFrameSent();
        }
    }

    _lastUpdateTime = millis() - startTime;
    if (allSuccess) _updateCount++;

    return allSuccess;
}

void NeoPixelManager::reportTransferFailure(uint32_t stripIndex, const PhysicalStrip* strip, const char* operation)
{
    if (!strip) return;

    const PhysicalStripError error = strip->getLastError();
    const uint32_t now = millis();
    if (error != _lastTransferError || (uint32_t)(now - _lastTransferErrorLogTime) >= 1000U)
    {
        logErrorP("NeoPixelManager: strip %u %s failed: %s", stripIndex, operation,
                  physicalStripErrorName(error));
        _lastTransferError = error;
        _lastTransferErrorLogTime = now;
    }
}

/**
 * Convenience method: PowerLimit + Sync + Show all strips
 * @return true if all strips were successfully updated
 *
 * Pipeline:
 * 1. applyPowerLimit() - ABL on VirtualStrip buffers
 * 2. syncAll() - VirtualStrip → PhysicalStrip
 * 3. showAll() - PhysicalStrip → Hardware
 */
bool NeoPixelManager::updateAll()
{
    if (!syncAll()) return false; // Never show a stale physical frame.
    applyPowerLimit(); // Phase 3: ABL on PhysicalStrip buffers
    return showAll();  // Phase 4: PhysicalStrip → Hardware
}

/**
 * Update effects only (Phase 1)
 * Calculates new pixel values without touching hardware
 * @param deltaTime Time since last update in milliseconds
 */
void NeoPixelManager::updateEffects(uint32_t deltaTime)
{
    if (!_initialized) return;

    // Update all segment effects
    for (auto segment : _segments)
    {
        if (segment)
        {
            segment->update(deltaTime);
        }
    }
}

/**
 * Update all strips (non-blocking)
 * Full pipeline: Phase 1-4
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
    updateEffects(deltaTime);

    // ========== PHASE 2-4: POWER + SYNC + HARDWARE ==========
    updateAll(); // Calls: applyPowerLimit() → syncAll() → showAll()

    _lastUpdateTime = 0; // Override timing for non-blocking mode
}

/**
 * Wait for all strips (blocking)
 * @param timeoutMs Timeout in milliseconds
 * @return true if all strips are ready, false on timeout
 */
bool NeoPixelManager::waitForAll(uint32_t timeoutMs)
{
    // 0 ("unlimited") → absolute ceiling, so a wedged strip can't hang the loop forever.
    if (timeoutMs == 0) timeoutMs = 1000;
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

    // 0 ("unlimited") → absolute ceiling, so a wedged strip can't hang the loop forever.
    if (timeoutMs == 0) timeoutMs = 1000;
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
 * Turn off all strips (clear + updateAll)
 */
void NeoPixelManager::allOff()
{
    clearAll();
    updateAll();
}

ManagerStats NeoPixelManager::getStats() const
{
    ManagerStats stats = {0};
    stats.totalStrips = _strips.size();
    stats.lastUpdateTime = _lastUpdateTime;
    stats.updateCount = _updateCount;
    stats.errorCount = _errorCount;

    for (const auto strip : _strips)
    {
        if (strip)
        {
            if (strip->isInitialized()) stats.activeStrips++;
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
            logDebugP("  Frames:  sent=%lu skipped=%lu error=%s",
                      (unsigned long)strip->getSentFrameCount(),
                      (unsigned long)strip->getSkippedFrameCount(),
                      strip->getLastErrorName());

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
    return 11; // RP2350: every supported serial/SPI strip consumes a PIO SM
    #else
    return 7; // RP2040: every supported serial/SPI strip consumes a PIO SM
    #endif
#elif defined(ARDUINO_ARCH_ESP32)
    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C5)
    return 6; // S2/S3/C5: 4 RMT TX + 2 SPI
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    return 3; // C3: 2 RMT TX + 1 SPI (only one SPI bus)
    #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    return 4; // C6: 2 RMT TX + 2 SPI
    #else
    return 10; // ESP32 original: 8 RMT TX + 2 SPI
    #endif
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
    if (!is1Wire && !isSpi) return false;

#if defined(ARDUINO_ARCH_RP2040)
    if (is1Wire || isSpi)
    {
    #ifdef PICO_RP2350
        return pioUsed < 11; // RP2350: max 11
    #else
        return pioUsed < 7; // RP2040: max 7
    #endif
    }
#elif defined(ARDUINO_ARCH_ESP32)
    if (is1Wire)
    {
    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C5)
        return rmtUsed < 4; // S2/S3/C5: 4 RMT TX channels
    #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
        return rmtUsed < 2; // C3/C6: 2 RMT TX channels
    #else
        return rmtUsed < 8; // ESP32 original: 8 RMT TX channels
    #endif
    }
    else if (isSpi)
    {
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
        return spiUsed < 1; // C3: only 1 SPI bus available
    #else
        return spiUsed < 2;
    #endif
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
        // A strip claims its hardware slot when it is accepted by the manager,
        // not only after the later batch init.  This is essential for OAM,
        // which creates the whole topology before it initializes it.
        if (strip && strip->isValid())
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
#if defined(ARDUINO_ARCH_RP2040)
                // RP SPI is implemented by PIO as well, so reserve one state
                // machine before initialization just like a 1-Wire strip.
                pioUsed++;
#else
                spiUsed++;
#endif
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
    if (totalLeds == 0)
    {
        logDebugP("NeoPixelManager: Refusing zero-length VirtualStrip");
        _errorCount++;
        return nullptr;
    }
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

    VirtualStrip* vstrip = new (std::nothrow) VirtualStrip(totalLeds, colorOrder);
    if (!vstrip || !vstrip->isValid())
    {
        delete vstrip;
        _errorCount++;
        return nullptr;
    }

    // Attach power manager for automatic current limiting
    vstrip->setPowerManager(&_powerManager);

    _virtualStrips.push_back(vstrip);
    _mappingDirty = true; // Mapping needs rebuild
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
    if (!managerOwns(_virtualStrips, vstrip) || !managerOwns(_strips, pstrip))
    {
        _errorCount++;
        return false;
    }

    // A physical frame has a single logical producer.  Without explicit
    // compositing, allowing a second virtual owner makes sync order decide
    // which pixels survive.
    for (const auto* existing : _virtualStrips)
    {
        if (existing == vstrip) continue;
        for (uint16_t i = 0; existing && i < existing->getPhysicalStripCount(); ++i)
        {
            if (existing->getPhysicalStrip(i) == pstrip)
            {
                logDebugP("NeoPixelManager: Physical strip already belongs to another VirtualStrip");
                _errorCount++;
                return false;
            }
        }
    }

    if (!vstrip->attachPhysicalStrip(pstrip, offset)) return false;

    _mappingDirty = true;
    _physToVirtualMap.clear();
    _stripPowerCache.erase(pstrip);
    return true;
}

bool NeoPixelManager::detachPhysicalFromVirtual(VirtualStrip* vstrip, PhysicalStrip* pstrip)
{
    if (!managerOwns(_virtualStrips, vstrip) || !managerOwns(_strips, pstrip))
    {
        _errorCount++;
        return false;
    }

    if (!vstrip->detachPhysicalStrip(pstrip)) return false;

    _mappingDirty = true;
    _physToVirtualMap.clear();
    _stripPowerCache.erase(pstrip);
    return true;
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

    if (!managerOwns(_virtualStrips, vstrip))
    {
        _errorCount++;
        return nullptr;
    }

    if (startLed > endLed || endLed >= vstrip->getLedCount())
    {
        logDebugP("NeoPixelManager: Invalid segment range (%u-%u, strip length %u)",
                  startLed, endLed, vstrip->getLedCount());
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
    if (!managerOwns(_segments, segment) || !effect)
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
            _mappingDirty = true;
            _physToVirtualMap.clear();
            _stripPowerCache.clear();
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
