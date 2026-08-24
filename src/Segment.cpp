#include "Segment.h"
#include "OpenKNX.h"
#include "effects/Effect.h"
#include <Arduino.h>

// ============================================================================
// Effektkette helpers
// ============================================================================

bool Segment::setVirtualBand(uint16_t totalLength, uint16_t offset)
{
    // Virtual-band coordinates are logical pixels, so grouping/spacing must
    // be reflected in the window validation.
    if (totalLength == 0 || offset >= totalLength ||
        static_cast<uint32_t>(offset) + _virtualLength > totalLength)
    {
        logErrorP("Segment: invalid virtual band total=%u offset=%u length=%u",
                  totalLength, offset, _virtualLength);
        return false;
    }
    _virtualTotalLength = totalLength;
    _virtualOffset      = offset;
    return true;
}

void Segment::clearVirtualBand()
{
    _virtualTotalLength = 0;
    _virtualOffset      = 0;
}

/**
 * @brief Translate virtual band index to local segment index.
 *
 * When the segment is part of an Effektkette (virtual band), effects compute
 * positions in the full virtual space [0 .. virtualTotalLength-1]. This helper
 * maps a virtual band index to a local [0 .. _virtualLength-1] index.
 *
 * Pixels that fall outside this segment’s window are silently dropped.
 */
bool Segment::translateVirtualIndex(uint16_t& index) const
{
    if (_virtualTotalLength == 0)
    {
        // Standalone: normal bounds check
        return (index < _virtualLength);
    }
    // Virtual band: translate band index → local index
    if (index < _virtualOffset) return false;
    uint16_t local = index - _virtualOffset;
    if (local >= _virtualLength) return false;
    index = local;
    return true;
}

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
    if (!_virtualStrip || !translateVirtualIndex(index))
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
    if (!_virtualStrip || !translateVirtualIndex(index))
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
    if (!_virtualStrip || !translateVirtualIndex(index))
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

    // In virtual band mode setPixel expects band indices — iterate our window
    uint16_t start = (_virtualTotalLength > 0) ? _virtualOffset : 0;
    uint16_t count = (_virtualTotalLength > 0) ? _physicalLength : _virtualLength;
    for (uint16_t i = 0; i < count; i++)
    {
        setPixel(start + i, r, g, b);
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

    uint16_t start = (_virtualTotalLength > 0) ? _virtualOffset : 0;
    uint16_t count = (_virtualTotalLength > 0) ? _physicalLength : _virtualLength;
    for (uint16_t i = 0; i < count; i++)
    {
        setPixel(start + i, r, g, b, w);
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

    uint16_t start = (_virtualTotalLength > 0) ? _virtualOffset : 0;
    uint16_t count = (_virtualTotalLength > 0) ? _physicalLength : _virtualLength;
    for (uint16_t i = 0; i < count; i++)
    {
        setPixel(start + i, r, g, b, ww, cw);
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
    // In band mode, index is a band index (may exceed local length) — translated below
    if (!_virtualStrip || (_virtualTotalLength == 0 && index >= _virtualLength))
    {
        return false;
    }

    // In virtual band mode, translate back to local index for reading
    uint16_t localIdx = index;
    if (_virtualTotalLength > 0)
    {
        if (index < _virtualOffset || index >= _virtualOffset + _physicalLength) { r = g = b = 0; return false; }
        localIdx = index - _virtualOffset;
    }
    uint16_t physicalStart = mapVirtualToPhysical(localIdx);
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
    // In band mode, index is a band index (may exceed local length) — translated below
    if (!_virtualStrip || (_virtualTotalLength == 0 && index >= _virtualLength))
    {
        return false;
    }

    uint16_t localIdx = index;
    if (_virtualTotalLength > 0)
    {
        if (index < _virtualOffset || index >= _virtualOffset + _physicalLength) { r = g = b = w = 0; return false; }
        localIdx = index - _virtualOffset;
    }
    uint16_t physicalStart = mapVirtualToPhysical(localIdx);
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
    // In band mode, index is a band index (may exceed local length) — translated below
    if (!_virtualStrip || (_virtualTotalLength == 0 && index >= _virtualLength))
    {
        return false;
    }

    uint16_t localIdx = index;
    if (_virtualTotalLength > 0)
    {
        if (index < _virtualOffset || index >= _virtualOffset + _physicalLength) { r = g = b = ww = cw = 0; return false; }
        localIdx = index - _virtualOffset;
    }

    // Map virtual index to physical, considering grouping/spacing
    uint16_t physicalStart = mapVirtualToPhysical(localIdx);
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

    // Route to dimensionally appropriate update function:
    // - 3D segment + effect supports 3D → update3D()
    // - 2D segment + effect supports 2D → update2D()
    // - All others (1D, or effect doesn't support higher dim) → update()
    //   update2D/3D default to update() so 1D effects still work on 2D segments.
    if (_geo.is3D() && _effect->supports3D())
        _effect->update3D(this, deltaTime);
    else if (_geo.is2D() && _effect->supports2D())
        _effect->update2D(this, deltaTime);
    else
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
        const uint32_t stepSize = static_cast<uint32_t>(_grouping) + _spacing;
        if (stepSize == 0)
        {
            _virtualLength = 0;
            logErrorP("Segment: grouping/spacing overflow");
            return;
        }
        const uint32_t logicalLength = (static_cast<uint32_t>(_physicalLength) + _spacing) / stepSize;
        _virtualLength = logicalLength > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(logicalLength);

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
        const uint32_t stepSize = static_cast<uint32_t>(_grouping) + _spacing;
        const uint32_t mapped = static_cast<uint32_t>(virtualIndex) * stepSize;
        if (stepSize == 0 || mapped >= _physicalLength) return _physicalLength;
        physicalIndex = static_cast<uint16_t>(mapped);
    }

    // Apply offset with wrap-around
    if (_offset > 0 && _physicalLength > 0)
    {
        physicalIndex = (physicalIndex + _offset) % _physicalLength;
    }

    return physicalIndex;
}

// ============================================================================
// 2D / 3D Matrix Geometry
// ============================================================================

bool Segment::setGeometry(uint8_t width, uint8_t height, LedTopology t)
{
    if (width == 0 || height == 0 || static_cast<uint32_t>(width) * height > _physicalLength)
    {
        logErrorP("Segment: invalid 2D geometry %ux%u for %u LEDs", width, height, _physicalLength);
        return false;
    }
    _geo.width    = width;
    _geo.height   = height;
    _geo.depth    = 0;
    _geo.topology = t;
    return true;
}

bool Segment::setGeometry(uint8_t width, uint8_t height, uint8_t depth, LedTopology t)
{
    if (width == 0 || height == 0 || depth == 0 ||
        static_cast<uint32_t>(width) * height * depth > _physicalLength)
    {
        logErrorP("Segment: invalid 3D geometry %ux%ux%u for %u LEDs", width, height, depth, _physicalLength);
        return false;
    }
    _geo.width    = width;
    _geo.height   = height;
    _geo.depth    = depth;
    _geo.topology = t;
    return true;
}

/**
 * @brief Convert 2D coordinates to a linear segment index.
 *
 * Supports ROWS_SERPENTINE (default), ROWS_LINEAR,
 * COLS_SERPENTINE, COLS_LINEAR.
 *
 * Returns 0xFFFF for out-of-bounds or missing geometry.
 */
uint16_t Segment::xyToIndex(uint8_t x, uint8_t y) const
{
    if (_geo.is1D()) return 0xFFFF;
    if (x >= _geo.width || y >= _geo.height) return 0xFFFF;

    uint16_t index;
    switch (_geo.topology)
    {
        case LedTopology::ROWS_SERPENTINE:
            // Even rows left→right, odd rows right→left
            if ((y & 1) == 0)
                index = (uint16_t)y * _geo.width + x;
            else
                index = (uint16_t)y * _geo.width + (_geo.width - 1 - x);
            break;

        case LedTopology::ROWS_LINEAR:
            index = (uint16_t)y * _geo.width + x;
            break;

        case LedTopology::COLS_SERPENTINE:
            // Even columns top→bottom, odd columns bottom→top
            if ((x & 1) == 0)
                index = x * _geo.height + y;
            else
                index = x * _geo.height + (_geo.height - 1 - y);
            break;

        case LedTopology::COLS_LINEAR:
            index = x * _geo.height + y;
            break;

        case LedTopology::COLS_LINEAR_TILED:
        case LedTopology::COLS_SERP_TILED:
        {
            // Generic tiled panel-chain mapping:
            // - Data is chained panel-by-panel (tile-major)
            // - Inside each panel, columns are wired top->bottom (linear or serpentine)
            // Tile block height is configured via matrix depth (for this topology only).
            uint8_t tileH = _geo.depth;
            if (tileH == 0) tileH = _geo.height;
            if (tileH > _geo.height) tileH = _geo.height;

            uint16_t panel = y / tileH;
            uint16_t yInTile = y % tileH;

            if (_geo.topology == LedTopology::COLS_SERP_TILED && (x & 1) != 0)
            {
                yInTile = tileH - 1 - yInTile;
            }

            uint16_t panelSize = static_cast<uint16_t>(_geo.width) * tileH;
            index = panel * panelSize + static_cast<uint16_t>(x) * tileH + yInTile;
            break;
        }

        default:
            index = (uint16_t)y * _geo.width + x;
            break;
    }

    if (index >= _physicalLength) return 0xFFFF;
    return index;
}

/**
 * @brief Convert 3D coordinates to a linear segment index.
 *
 * Layout: z-layers stacked after each other, each layer uses xyToIndex().
 * Returns 0xFFFF for out-of-bounds or missing geometry.
 */
uint16_t Segment::xyzToIndex(uint8_t x, uint8_t y, uint8_t z) const
{
    if (!_geo.is3D()) return 0xFFFF;
    if (x >= _geo.width || y >= _geo.height || z >= _geo.depth) return 0xFFFF;

    const uint32_t layerSize = static_cast<uint32_t>(_geo.width) * _geo.height;
    // xyToIndex applies the configured topology and validates the local layer
    // against the same physical-length bound.  Calculate it locally so a 3D
    // segment is not rejected merely because one layer is shorter than total.
    uint32_t xyIdx;
    switch (_geo.topology)
    {
        case LedTopology::ROWS_SERPENTINE: xyIdx = static_cast<uint32_t>(y) * _geo.width + ((y & 1) ? (_geo.width - 1 - x) : x); break;
        case LedTopology::COLS_SERPENTINE: xyIdx = static_cast<uint32_t>(x) * _geo.height + ((x & 1) ? (_geo.height - 1 - y) : y); break;
        case LedTopology::COLS_LINEAR: xyIdx = static_cast<uint32_t>(x) * _geo.height + y; break;
        default: xyIdx = static_cast<uint32_t>(y) * _geo.width + x; break;
    }
    const uint32_t index = static_cast<uint32_t>(z) * layerSize + xyIdx;
    if (index >= _physicalLength) return 0xFFFF;
    return index;
}

bool Segment::setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t idx = xyToIndex(x, y);
    if (idx == 0xFFFF) return false;
    return setPixel(idx, r, g, b);
}

bool Segment::setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    uint16_t idx = xyToIndex(x, y);
    if (idx == 0xFFFF) return false;
    return setPixel(idx, r, g, b, w);
}

uint16_t Segment::getRenderWidth() const
{
    if (_geo.is1D()) return _virtualLength;

    // Distributed 2D across a virtual band: derive global width from band length and local panel height.
    if (_virtualTotalLength > _physicalLength && _geo.height > 0 && (_virtualTotalLength % _geo.height) == 0)
    {
        return _virtualTotalLength / _geo.height;
    }

    return _geo.width;
}

uint16_t Segment::getRenderHeight() const
{
    if (_geo.is1D()) return 1;
    return _geo.height;
}

bool Segment::mapGlobalXYToLocal(uint16_t x, uint16_t y, uint8_t& localX, uint8_t& localY) const
{
    if (_geo.is1D() || _geo.width == 0 || _geo.height == 0)
    {
        return false;
    }

    // Standalone/local matrix path.
    if (_virtualTotalLength == 0)
    {
        if (x >= _geo.width || y >= _geo.height) return false;
        localX = static_cast<uint8_t>(x);
        localY = static_cast<uint8_t>(y);
        return true;
    }

    // Distributed 2D path over virtual band.
    // Convention: panels of equal height are arranged SIDE BY SIDE along the band.
    // Global matrix = (totalLength / height) columns × height rows.
    // virtualOffset = LEDs before this panel → panelStartX = virtualOffset / height.
    if (_geo.height == 0 || (_virtualTotalLength % _geo.height) != 0 ||
        (_virtualOffset % _geo.height) != 0)
    {
        return false;
    }

    const uint16_t globalWidth = _virtualTotalLength / _geo.height;
    if (x >= globalWidth || y >= _geo.height)
    {
        return false;
    }

    const uint16_t panelStartX = _virtualOffset / _geo.height;
    if (x < panelStartX)
    {
        return false;
    }

    const uint16_t lx = x - panelStartX;
    if (lx >= _geo.width || lx > 255 || y > 255)
    {
        return false;
    }

    localX = static_cast<uint8_t>(lx);
    localY = static_cast<uint8_t>(y);
    return true;
}

bool Segment::setPixelGlobalXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t localX = 0;
    uint8_t localY = 0;
    if (!mapGlobalXYToLocal(x, y, localX, localY))
    {
        return false;
    }
    return setPixelXY(localX, localY, r, g, b);
}

bool Segment::setPixelGlobalXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    uint8_t localX = 0;
    uint8_t localY = 0;
    if (!mapGlobalXYToLocal(x, y, localX, localY))
    {
        return false;
    }
    return setPixelXY(localX, localY, r, g, b, w);
}

bool Segment::setPixelXYZ(uint8_t x, uint8_t y, uint8_t z, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t idx = xyzToIndex(x, y, z);
    if (idx == 0xFFFF) return false;
    return setPixel(idx, r, g, b);
}
