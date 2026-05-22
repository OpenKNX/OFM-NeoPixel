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
      _physicalLength(endLed - startLed + 1),
      _virtualLength(endLed - startLed + 1),
      _grouping(1),
      _spacing(0),
      _offset(0),
      _reverse(false),
      _mirror(false),
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
        _physicalLength = _endLed - _startLed + 1;
        _virtualLength = _physicalLength;
    }

    logDebugP("Segment erstellt: %u-%u (Länge=%u)", _startLed, _endLed, _physicalLength);
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
 * @param index Virtual pixel index within the segment (accounts for grouping/spacing)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return true on success, false on failure
 */
bool Segment::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // NOTE: HCL is now applied via VirtualStrip::PixelTransformCallback during syncToPhysical()
    // This ensures 1× Kelvin calculation per frame instead of per-pixel

    // Apply segment brightness (gamma correction happens later in PhysicalStrip)
    if (_config.brightness < 255)
    {
        r = (r * _config.brightness + 127) / 255;
        g = (g * _config.brightness + 127) / 255;
        b = (b * _config.brightness + 127) / 255;
    }

    _dirty = true;

    // Map virtual index to physical index, handling reverse and mirror
    uint16_t mappedIndex = index;

    // Handle reverse direction
    if (_reverse)
    {
        mappedIndex = _virtualLength - 1 - index;
    }

    // Calculate physical start position for this virtual pixel
    uint16_t physicalStart = mapVirtualToPhysical(mappedIndex);

    // Set all LEDs in the group to the same color
    for (uint16_t g_idx = 0; g_idx < _grouping && (physicalStart + g_idx) < _physicalLength; g_idx++)
    {
        uint16_t virtualStripIndex = _startLed + physicalStart + g_idx;
        _virtualStrip->setPixel(virtualStripIndex, r, g, b);
    }

    // Handle mirror: also set the mirrored position
    if (_mirror && _virtualLength > 1)
    {
        uint16_t mirrorIndex = _virtualLength - 1 - mappedIndex;
        if (mirrorIndex != mappedIndex) // Don't double-set center pixel
        {
            uint16_t mirrorPhysicalStart = mapVirtualToPhysical(mirrorIndex);
            for (uint16_t g_idx = 0; g_idx < _grouping && (mirrorPhysicalStart + g_idx) < _physicalLength; g_idx++)
            {
                uint16_t virtualStripIndex = _startLed + mirrorPhysicalStart + g_idx;
                _virtualStrip->setPixel(virtualStripIndex, r, g, b);
            }
        }
    }

    return true;
}

/**
 * @brief Set pixel (RGBW)
 * @param index Virtual pixel index within the segment (accounts for grouping/spacing)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 * @return true on success, false on failure
 */
bool Segment::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // NOTE: HCL is now applied via VirtualStrip::PixelTransformCallback during syncToPhysical()
    // This ensures 1× Kelvin calculation per frame instead of per-pixel

    // Apply segment brightness (gamma correction happens later in PhysicalStrip)
    if (_config.brightness < 255)
    {
        r = (r * _config.brightness + 127) / 255;
        g = (g * _config.brightness + 127) / 255;
        b = (b * _config.brightness + 127) / 255;
        w = (w * _config.brightness + 127) / 255;
    }

    _dirty = true;

    // Map virtual index to physical index, handling reverse
    uint16_t mappedIndex = index;
    if (_reverse)
    {
        mappedIndex = _virtualLength - 1 - index;
    }

    // Calculate physical start position for this virtual pixel
    uint16_t physicalStart = mapVirtualToPhysical(mappedIndex);

    // Set all LEDs in the group to the same color
    for (uint16_t g_idx = 0; g_idx < _grouping && (physicalStart + g_idx) < _physicalLength; g_idx++)
    {
        uint16_t virtualStripIndex = _startLed + physicalStart + g_idx;
        _virtualStrip->setPixel(virtualStripIndex, r, g, b, w);
    }

    // Handle mirror: also set the mirrored position
    if (_mirror && _virtualLength > 1)
    {
        uint16_t mirrorIndex = _virtualLength - 1 - mappedIndex;
        if (mirrorIndex != mappedIndex)
        {
            uint16_t mirrorPhysicalStart = mapVirtualToPhysical(mirrorIndex);
            for (uint16_t g_idx = 0; g_idx < _grouping && (mirrorPhysicalStart + g_idx) < _physicalLength; g_idx++)
            {
                uint16_t virtualStripIndex = _startLed + mirrorPhysicalStart + g_idx;
                _virtualStrip->setPixel(virtualStripIndex, r, g, b, w);
            }
        }
    }

    return true;
}

/**
 * @brief Set pixel (RGBCCT - 5 channel)
 * @param index Virtual pixel index within the segment (accounts for grouping/spacing)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param ww Warm White component (0-255)
 * @param cw Cool White component (0-255)
 * @return true on success, false on failure
 */
bool Segment::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // NOTE: HCL is now applied via VirtualStrip::PixelTransformCallback during syncToPhysical()
    // This ensures 1× Kelvin calculation per frame instead of per-pixel

    // Apply segment brightness (gamma correction happens later in PhysicalStrip)
    if (_config.brightness < 255)
    {
        r = (r * _config.brightness + 127) / 255;
        g = (g * _config.brightness + 127) / 255;
        b = (b * _config.brightness + 127) / 255;
        ww = (ww * _config.brightness + 127) / 255;
        cw = (cw * _config.brightness + 127) / 255;
    }

    _dirty = true;

    // Map virtual index to physical index, handling reverse
    uint16_t mappedIndex = index;
    if (_reverse)
    {
        mappedIndex = _virtualLength - 1 - index;
    }

    // Calculate physical start position for this virtual pixel
    uint16_t physicalStart = mapVirtualToPhysical(mappedIndex);

    // Set all LEDs in the group to the same color
    for (uint16_t g_idx = 0; g_idx < _grouping && (physicalStart + g_idx) < _physicalLength; g_idx++)
    {
        uint16_t virtualStripIndex = _startLed + physicalStart + g_idx;
        _virtualStrip->setPixel(virtualStripIndex, r, g, b, ww, cw);
    }

    // Handle mirror: also set the mirrored position
    if (_mirror && _virtualLength > 1)
    {
        uint16_t mirrorIndex = _virtualLength - 1 - mappedIndex;
        if (mirrorIndex != mappedIndex)
        {
            uint16_t mirrorPhysicalStart = mapVirtualToPhysical(mirrorIndex);
            for (uint16_t g_idx = 0; g_idx < _grouping && (mirrorPhysicalStart + g_idx) < _physicalLength; g_idx++)
            {
                uint16_t virtualStripIndex = _startLed + mirrorPhysicalStart + g_idx;
                _virtualStrip->setPixel(virtualStripIndex, r, g, b, ww, cw);
            }
        }
    }

    return true;
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

    for (uint16_t i = 0; i < _virtualLength; i++)
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

    for (uint16_t i = 0; i < _virtualLength; i++)
    {
        setPixel(i, r, g, b, w);
    }
}

/**
 * @brief Set all pixels - RGBCCT (5 channel)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param ww Warm White component (0-255)
 * @param cw Cool White component (0-255)
 */
void Segment::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (!_virtualStrip) return;

    for (uint16_t i = 0; i < _virtualLength; i++)
    {
        setPixel(i, r, g, b, ww, cw);
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
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // Map virtual index to physical, considering grouping/spacing
    uint16_t physicalStart = mapVirtualToPhysical(index);
    uint16_t virtualStripIndex = _startLed + physicalStart;
    return _virtualStrip->getPixel(virtualStripIndex, r, g, b);
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
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // Map virtual index to physical, considering grouping/spacing
    uint16_t physicalStart = mapVirtualToPhysical(index);
    uint16_t virtualStripIndex = _startLed + physicalStart;
    return _virtualStrip->getPixel(virtualStripIndex, r, g, b, w);
}

/**
 * @brief Get pixel - RGBCCT (5 channel)
 * @param index Pixel index within the segment
 * @param r Reference to store Red component (0-255)
 * @param g Reference to store Green component (0-255)
 * @param b Reference to store Blue component (0-255)
 * @param ww Reference to store Warm White component (0-255)
 * @param cw Reference to store Cool White component (0-255)
 * @return true on success, false on failure
 */
bool Segment::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& ww, uint8_t& cw) const
{
    if (!_virtualStrip || index >= _virtualLength)
    {
        return false;
    }

    // Map virtual index to physical, considering grouping/spacing
    uint16_t physicalStart = mapVirtualToPhysical(index);
    uint16_t virtualStripIndex = _startLed + physicalStart;
    return _virtualStrip->getPixel(virtualStripIndex, r, g, b, ww, cw);
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

// ============================================================================
// Grouping & Spacing Implementation
// ============================================================================

/**
 * @brief Set grouping - how many physical LEDs show the same color
 * @param grouping Number of LEDs per group (1 = no grouping, default)
 */
void Segment::setGrouping(uint16_t grouping)
{
    _grouping = (grouping > 0) ? grouping : 1;
    recalculateVirtualLength();
}

/**
 * @brief Set spacing - how many LEDs to skip (turn off) between groups
 * @param spacing Number of LEDs to skip (0 = no spacing, default)
 */
void Segment::setSpacing(uint16_t spacing)
{
    _spacing = spacing;
    recalculateVirtualLength();
}

/**
 * @brief Recalculate virtual length based on grouping and spacing
 *
 * Virtual length = how many "logical pixels" the effect sees
 * Physical length = actual number of LEDs
 *
 * Example: 12 physical LEDs with grouping=2, spacing=1
 *   Pattern: [GG_GG_GG_GG_] where G=group, _=spacing
 *   Groups fit = 12 / (2+1) = 4 groups
 *   Virtual length = 4 (effect sees 4 logical pixels)
 */
void Segment::recalculateVirtualLength()
{
    if (_grouping == 1 && _spacing == 0)
    {
        // No grouping/spacing - virtual = physical
        _virtualLength = _physicalLength;
    }
    else
    {
        // Calculate how many complete groups fit in physical length
        uint16_t stepSize = _grouping + _spacing;
        _virtualLength = (_physicalLength + _spacing) / stepSize; // Round up for partial groups

        if (_virtualLength == 0)
        {
            _virtualLength = 1; // At least 1 virtual pixel
        }
    }

    logDebugP("Segment recalculated: physical=%u, virtual=%u (group=%u, space=%u)",
              _physicalLength, _virtualLength, _grouping, _spacing);
}

/**
 * @brief Map virtual pixel index to physical LED start position
 * @param virtualIndex The virtual pixel index (0 to virtualLength-1)
 * @return The physical LED index where this virtual pixel starts
 *
 * Example: grouping=2, spacing=1, virtualIndex=2, offset=0
 *   Physical pattern: [GG_GG_GG_GG_]
 *                      01 23 45 67   <- physical indices
 *                      0  1  2  3    <- virtual indices
 *   virtualIndex=2 maps to physical=6
 *
 * With offset=3: the effect starts 3 LEDs into the segment
 */
uint16_t Segment::mapVirtualToPhysical(uint16_t virtualIndex) const
{
    uint16_t physicalIndex;

    if (_grouping == 1 && _spacing == 0)
    {
        // No grouping/spacing - direct mapping
        physicalIndex = virtualIndex;
    }
    else
    {
        // Calculate physical start position for this virtual pixel
        uint16_t stepSize = _grouping + _spacing;
        physicalIndex = virtualIndex * stepSize;
    }

    // Apply offset with wrap-around
    if (_offset > 0 && _physicalLength > 0)
    {
        physicalIndex = (physicalIndex + _offset) % _physicalLength;
    }

    return physicalIndex;
}

