#include "Segment.h"
#include "OpenKNX.h"
#include "effects/Effect.h"
#include <Arduino.h>

/**
 * @brief Constructor
 * @param virtualStrip The parent VirtualStrip
 * @param startLed Start LED index (inclusive)
 * @param endLed End LED index (inclusive)
 */
Segment::Segment(VirtualStrip* virtualStrip, uint16_t startLed, uint16_t endLed)
    : _virtualStrip(virtualStrip),
      _startLed(startLed),
      _endLed(endLed),
      _length(endLed - startLed + 1),
      _effect(nullptr),
      _dirty(false),
      _paused(false),
      _ledState(LedState::IDLE)
{

    if (!virtualStrip)
    {
        logErrorP("Segment - virtualStrip ist nullptr!");
        return;
    }

    // Validierung
    if (startLed >= virtualStrip->getLedCount())
    {
        logErrorP("Segment - startLed (%u) >= Total (%u)!",
                  startLed, virtualStrip->getLedCount());
        return;
    }

    if (endLed >= virtualStrip->getLedCount())
    {
        logErrorP("Segment - endLed (%u) clamped to (%u)",
                  endLed, virtualStrip->getLedCount() - 1);
        _endLed = virtualStrip->getLedCount() - 1;
        _length = _endLed - _startLed + 1;
    }

    logDebugP("Segment erstellt: %u-%u (Länge=%u)", _startLed, _endLed, _length);
}

/**
 * @brief Destructor
 */
Segment::~Segment()
{
    // Effect is NOT deleted - user is responsible for cleanup!
    _effect = nullptr;
}

/**
 * @brief Set effect (Stateless - no init()!)
 * @param effect Pointer to the effect
 */
void Segment::setEffect(Effect* effect, bool initializeDefaults)
{
    _effect = effect;

    // Initialize parameters with defaults and reset state
    if (_effect && initializeDefaults)
    {
        _effect->initializeDefaults(this);
        _effect->reset();
    }
}

/**
 * @brief Set pixel (RGB)
 * @param index Pixel index within the segment
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return true on success, false on failure
 */
bool Segment::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_virtualStrip || index >= _length)
    {
        return false;
    }

    uint16_t virtualIndex = _startLed + index;

    // Apply segment brightness (gamma correction happens later in PhysicalStrip)
    if (_config.brightness < 255)
    {
        r = (r * _config.brightness + 127) / 255;
        g = (g * _config.brightness + 127) / 255;
        b = (b * _config.brightness + 127) / 255;
    }

    _dirty = true;

    // Write pixel to VirtualStrip (gamma correction applied in PhysicalStrip::setPixel)
    return _virtualStrip->setPixel(virtualIndex, r, g, b);
}

/**
 * @brief Set pixel (RGBW)
 * @param index Pixel index within the segment
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 * @return true on success, false on failure
 */
bool Segment::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_virtualStrip || index >= _length)
    {
        return false;
    }

    uint16_t virtualIndex = _startLed + index;

    // Apply segment brightness (gamma correction happens later in PhysicalStrip)
    if (_config.brightness < 255)
    {
        r = (r * _config.brightness + 127) / 255;
        g = (g * _config.brightness + 127) / 255;
        b = (b * _config.brightness + 127) / 255;
        w = (w * _config.brightness + 127) / 255;
    }

    _dirty = true;
    return _virtualStrip->setPixel(virtualIndex, r, g, b, w);
}

/**
 * @brief Set all pixels - RGB
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void Segment::setAll(uint8_t r, uint8_t g, uint8_t b)
{
    if (!_virtualStrip) return;

    for (uint16_t i = 0; i < _length; i++)
    {
        setPixel(i, r, g, b);
    }
}

/**
 * @brief Set all pixels - RGBW
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 */
void Segment::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_virtualStrip) return;

    for (uint16_t i = 0; i < _length; i++)
    {
        setPixel(i, r, g, b, w);
    }
}

/**
 * @brief Clear all pixels - RGB
 */
void Segment::clear()
{
    setAll(0, 0, 0);
}

/**
 * @brief Clear all pixels - RGBW
 */
void Segment::clearAll()
{
    setAll(0, 0, 0, 0);
}

/**
 * @brief Get pixel - RGB
 * @param index Pixel index within the segment
 * @param r Reference to store Red component (0-255)
 * @param g Reference to store Green component (0-255)
 * @param b Reference to store Blue component (0-255)
 * @return true on success, false on failure
 */
bool Segment::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const
{
    if (!_virtualStrip || index >= _length)
    {
        return false;
    }

    uint16_t virtualIndex = _startLed + index;
    return _virtualStrip->getPixel(virtualIndex, r, g, b);
}

/**
 * @brief Get pixel - RGBW
 * @param index Pixel index within the segment
 * @param r Reference to store Red component (0-255)
 * @param g Reference to store Green component (0-255)
 * @param b Reference to store Blue component (0-255)
 * @param w Reference to store White component (0-255)
 * @return true on success, false on failure
 */
bool Segment::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const
{
    if (!_virtualStrip || index >= _length)
    {
        return false;
    }

    uint16_t virtualIndex = _startLed + index;
    return _virtualStrip->getPixel(virtualIndex, r, g, b, w);
}

/**
 * @brief Update Segment (calls effect ) - STATELESS
 * @param deltaTime Time difference since last update in ms
 */
void Segment::update(uint32_t deltaTime)
{
    // Skip update when paused
    if (_paused)
    {
        return;
    }

    if (!_effect || !_virtualStrip)
    {
        return; // No effect or no virtual strip
    }

    // Effect gets segment pointer for config/state access
    _effect->update(this, deltaTime);

    // Virtual strip is marked as dirty by setPixel() calls
}

/**
 * Stop effect (pause and clear pixels)
 */
void Segment::stop()
{
    _paused = true;
    clear();
}