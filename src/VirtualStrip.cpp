/**
 * @file VirtualStrip.cpp
 * @brief VirtualStrip Implementation
 */

#include "VirtualStrip.h"
#include "OpenKNX.h"
#include <Arduino.h>
#include <algorithm>
#include <string.h>

/**
 * @brief Constructor
 * @param totalLeds Total number of virtual LEDs
 * @param colorOrder Color order for the virtual strip
 */
VirtualStrip::VirtualStrip(uint16_t totalLeds, ColorOrder colorOrder)
    : _totalLeds(totalLeds),
      _colorOrder(colorOrder),
      _dirty(false),
      _brightness(255)
{

    // Determine bytes per LED based on ColorOrder
    // BGR also gets 4 bytes for APA102 Brightness (WS2801 ignores 4th byte)
    _bytesPerLed = (colorOrder == ColorOrder::RGBW || colorOrder == ColorOrder::GRBW || colorOrder == ColorOrder::BGR) ? 4 : 3;

    // Allocate unified buffer
    _bufferSize = (size_t)totalLeds * _bytesPerLed;
    _buffer = new uint8_t[_bufferSize];
    memset(_buffer, 0, _bufferSize);

    logDebugP("VirtualStrip initialized: %u LEDs, %u Bytes/LED, Buffer=%u Bytes",
              _totalLeds, _bytesPerLed, (uint32_t)_bufferSize);
}

/**
 * @brief Destructor
 */
VirtualStrip::~VirtualStrip()
{
    if (_buffer)
    {
        delete[] _buffer;
        _buffer = nullptr;
    }
    _physicalStrips.clear();
}

/**
 * @brief Attach a PhysicalStrip at given offset
 * @param physicalStrip Pointer to the PhysicalStrip
 * @param offset Offset within the virtual strip to attach the physical strip
 * @return true if attachment was successful
 * @return false if attachment failed
 */
bool VirtualStrip::attachPhysicalStrip(PhysicalStrip* physicalStrip, uint16_t offset)
{
    if (!physicalStrip)
    {
        logErrorP("VirtualStrip::attachPhysicalStrip - nullptr!");
        return false;
    }

    // Check Offset + LED Count vs Total Leds
    if (offset + physicalStrip->getLedCount() > _totalLeds)
    {
        logErrorP("VirtualStrip attach - range overflow! Offset=%u, Count=%u, Total=%u",
                  offset, physicalStrip->getLedCount(), _totalLeds);
        return false;
    }

    // Check if already attached
    for (const auto& mapping : _physicalStrips)
    {
        if (mapping.physicalStrip == physicalStrip)
        {
            logErrorP("PhysicalStrip already attached!");
            return false;
        }
    }

    // Attach
    VirtualToPhysicalMapping mapping;
    mapping.physicalStrip = physicalStrip;
    mapping.virtualOffset = offset;
    mapping.physicalLedCount = physicalStrip->getLedCount();

    _physicalStrips.push_back(mapping);

    logDebugP("VirtualStrip: PhysicalStrip attached (Offset=%u, Count=%u)",
              offset, physicalStrip->getLedCount());

    return true;
}

/**
 * @brief Detach a PhysicalStrip
 * @param physicalStrip Pointer to the PhysicalStrip to detach
 * @return true if detachment was successful
 * @return false if detachment failed
 */
bool VirtualStrip::detachPhysicalStrip(PhysicalStrip* physicalStrip)
{
    if (!physicalStrip) return false;

    auto it = std::find_if(_physicalStrips.begin(), _physicalStrips.end(),
                           [physicalStrip](const VirtualToPhysicalMapping& m) {
                               return m.physicalStrip == physicalStrip;
                           });

    if (it != _physicalStrips.end())
    {
        _physicalStrips.erase(it);
        return true;
    }

    return false;
}

/**
 * @brief Get PhysicalStrip at given index
 * @param index Index of the PhysicalStrip
 * @return Pointer to the PhysicalStrip or nullptr if out of range
 */
PhysicalStrip* VirtualStrip::getPhysicalStrip(uint16_t index) const
{
    if (index >= _physicalStrips.size()) return nullptr;
    return _physicalStrips[index].physicalStrip;
}

/**
 * @brief Find PhysicalStrip and physical index for given virtual index
 * @param virtualIndex Index within the virtual strip
 * @param outPhysicalIndex Output parameter for the index within the physical strip
 * @return Pointer to the PhysicalStrip or nullptr if not found
 */
PhysicalStrip* VirtualStrip::findPhysicalAtIndex(uint16_t virtualIndex, uint16_t& outPhysicalIndex) const
{
    if (virtualIndex >= _totalLeds) return nullptr;

    for (const auto& mapping : _physicalStrips)
    {
        uint16_t rangeEnd = mapping.virtualOffset + mapping.physicalLedCount;

        if (virtualIndex >= mapping.virtualOffset && virtualIndex < rangeEnd)
        {
            // Found it! The physical index is virtualIndex - virtualOffset
            outPhysicalIndex = virtualIndex - mapping.virtualOffset;
            return mapping.physicalStrip;
        }
    }
    return nullptr;
}

/**
 * @brief Write pixel to buffer
 * @param index Index of the pixel
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param w White component (if applicable)
 */
void VirtualStrip::writePixelToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (index >= _totalLeds) return;

    size_t offset = (size_t)index * _bytesPerLed;

#ifdef OPENKNX_TRACE1
    if (index == 0)
    {
        openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                              "writePixelToBuffer[0]: R=%d G=%d B=%d W=%d, bytesPerLed=%d, offset=%d, ColorOrder=%d",
                                              r, g, b, w, _bytesPerLed, offset, (int)_colorOrder);
    }
#endif

    // W parameter meaning (AFTER Segment brightness was already applied):
    // - For RGBW/GRBW (4 bytes): W = White channel (separate LED color)
    // - For BGR (4 bytes, APA102): W = Hardware brightness (0-255, controlled via Segment.apa102Brightness)
    // - For RGB/GRB/BRG (3 bytes): W is ignored (no 4th byte allocated)

    switch (_colorOrder)
    {
        case ColorOrder::RGB:
            _buffer[offset] = r;     // Red
            _buffer[offset + 1] = g; // Green
            _buffer[offset + 2] = b; // Blue
            break;

        case ColorOrder::RBG:
            _buffer[offset] = r;     // Red
            _buffer[offset + 1] = b; // Blue
            _buffer[offset + 2] = g; // Green
            break;

        case ColorOrder::GRB:
            _buffer[offset] = g;     // Green
            _buffer[offset + 1] = r; // Red
            _buffer[offset + 2] = b; // Blue
            break;

        case ColorOrder::GBR:
            _buffer[offset] = g;     // Green
            _buffer[offset + 1] = b; // Blue
            _buffer[offset + 2] = r; // Red
            break;

        case ColorOrder::BGR:
            _buffer[offset] = b;     // Blue
            _buffer[offset + 1] = g; // Green
            _buffer[offset + 2] = r; // Red
            _buffer[offset + 3] = w; // White / Brightness
            break;

        case ColorOrder::BRG:
            _buffer[offset] = b;     // Blue
            _buffer[offset + 1] = r; // Red
            _buffer[offset + 2] = g; // Green
            break;

        case ColorOrder::RGBW:
            _buffer[offset] = r;     // Red
            _buffer[offset + 1] = g; // Green
            _buffer[offset + 2] = b; // Blue
            _buffer[offset + 3] = w; // White
            break;

        case ColorOrder::GRBW:
            _buffer[offset] = g;     // Green
            _buffer[offset + 1] = r; // Red
            _buffer[offset + 2] = b; // Blue
            _buffer[offset + 3] = w; // White
            break;

        default:
            // Fallback RGB (3 bytes)
            _buffer[offset] = r;     // Red
            _buffer[offset + 1] = g; // Green
            _buffer[offset + 2] = b; // Blue
            break;
    }

#ifdef OPENKNX_TRACE1
    if (index == 0) // Only log first pixel for brevity
    {
        if (_bytesPerLed >= 4) // RGBW or BGR with brightness
        {
            openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                                  "  Buffer written: [%d]=%02X [%d]=%02X [%d]=%02X [%d]=%02X",
                                                  offset, _buffer[offset],
                                                  offset + 1, _buffer[offset + 1],
                                                  offset + 2, _buffer[offset + 2],
                                                  offset + 3, _buffer[offset + 3]);
        }
        else // RGB (3 bytes)
        {
            openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                                  "  Buffer written: [%d]=%02X [%d]=%02X [%d]=%02X",
                                                  offset, _buffer[offset],
                                                  offset + 1, _buffer[offset + 1],
                                                  offset + 2, _buffer[offset + 2]);
        }
    }
#endif

    _dirty = true;
}

/**
 * @brief Set single pixel (RGB)
 * Uses stored _brightness for APA102 hardware brightness control.
 * @param index Index of the pixel
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
bool VirtualStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= _totalLeds) return false;
    writePixelToBuffer(index, r, g, b, _brightness);
    _dirty = true;
    return true;
}

/**
 * @brief Set single pixel (RGBW)
 * @param index Index of the pixel
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param w White component
 */
bool VirtualStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (index >= _totalLeds) return false;
    writePixelToBuffer(index, r, g, b, w);
    _dirty = true; // Mark as dirty for sync!
    return true;
}

/**
 * @brief Set range of pixels
 * @param startIndex Starting index of the range
 * @param length Number of pixels in the range
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
void VirtualStrip::setRange(uint16_t startIndex, uint16_t length, uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0; i < length && (startIndex + i) < _totalLeds; i++)
    {
        setPixel(startIndex + i, r, g, b);
    }
}

/**
 * @brief Set all pixels
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
void VirtualStrip::setAll(uint8_t r, uint8_t g, uint8_t b)
{
    setRange(0, _totalLeds, r, g, b);
}

/**
 * @brief Clear all pixels
 */
void VirtualStrip::clear()
{
    memset(_buffer, 0, _bufferSize);
    _dirty = true;
}

/**
 * @brief Get pixel color (RGB)
 * @param index Index of the pixel
 * @param r Output parameter for Red component
 * @param g Output parameter for Green component
 * @param b Output parameter for Blue component
 */
bool VirtualStrip::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const
{
    if (index >= _totalLeds) return false;

    size_t offset = (size_t)index * _bytesPerLed;

    switch (_colorOrder)
    {
        case ColorOrder::RGB:
            r = _buffer[offset];     // Red
            g = _buffer[offset + 1]; // Green
            b = _buffer[offset + 2]; // Blue
            break;

        case ColorOrder::RBG:
            r = _buffer[offset];     // Red
            b = _buffer[offset + 1]; // Blue
            g = _buffer[offset + 2]; // Green
            break;

        case ColorOrder::GRB:
            g = _buffer[offset];     // Green
            r = _buffer[offset + 1]; // Red
            b = _buffer[offset + 2]; // Blue
            break;

        case ColorOrder::GBR:
            g = _buffer[offset];     // Green
            b = _buffer[offset + 1]; // Blue
            r = _buffer[offset + 2]; // Red
            break;

        case ColorOrder::BGR:
            b = _buffer[offset];     // Blue
            g = _buffer[offset + 1]; // Green
            r = _buffer[offset + 2]; // Red
            break;

        case ColorOrder::BRG:
            b = _buffer[offset];     // Blue
            r = _buffer[offset + 1]; // Red
            g = _buffer[offset + 2]; // Green
            break;

        default:
            r = g = b = 0; // Default to black if unknown color order
            return false;
    }
    return true;
}

/**
 * @brief Get pixel color (RGBW)
 * @param index Index of the pixel
 * @param r Output parameter for Red component
 * @param g Output parameter for Green component
 * @param b Output parameter for Blue component
 * @param w Output parameter for White component
 */
bool VirtualStrip::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const
{
    if (index >= _totalLeds) return false;

    size_t offset = (size_t)index * _bytesPerLed;

    switch (_colorOrder)
    {
        case ColorOrder::RGBW:
            r = _buffer[offset];     // Red
            g = _buffer[offset + 1]; // Green
            b = _buffer[offset + 2]; // Blue
            w = _buffer[offset + 3]; // White
            break;

        case ColorOrder::GRBW:
            g = _buffer[offset];     // Green
            r = _buffer[offset + 1]; // Red
            b = _buffer[offset + 2]; // Blue
            w = _buffer[offset + 3]; // White
            break;

        default:
            r = g = b = w = 0; // Default to black if unknown color order
            return false;
    }

    return true;
}

/**
 * @brief Synchronize to Physical Strips
 * @return true if synchronization was successful
 */
bool VirtualStrip::syncToPhysical()
{
    if (_physicalStrips.empty())
    {
        return true; // Nothing to synchronize
    }

    // For each PhysicalStrip: Copy relevant buffer section
    for (const auto& mapping : _physicalStrips)
    {
        PhysicalStrip* pstrip = mapping.physicalStrip;
        if (!pstrip) continue;

        uint16_t offset = mapping.virtualOffset;
        uint16_t count = mapping.physicalLedCount;

        // !!!!
        // IMPORTANT: VirtualStrip buffer stores RGB in _colorOrder format (e.g. BGR).
        // But PhysicalStrip expects LOGICAL RGB values (not pre-converted),
        // because the driver will apply the correct protocol format (APA102=BGR, etc.)
        //
        // So we must READ from VirtualStrip buffer WITHOUT reverse conversion,
        // then pass RAW bytes to PhysicalStrip driver.
        LedProtocol protocol = pstrip->getProtocol();
        bool isSpi = (protocol == LedProtocol::APA102 || protocol == LedProtocol::SK9822 || protocol == LedProtocol::WS2801);

        if (isSpi)
        {
            // For SPI: Copy RAW buffer bytes WITHOUT color order conversion
            // The physical driver will handle the correct protocol format
            for (uint16_t i = 0; i < count; i++)
            {
                uint16_t virtualIdx = offset + i;
                if (virtualIdx >= _totalLeds) break;

                // Get RAW pixel data from buffer (no ColorOrder reverse conversion!)
                size_t bufferOffset = (size_t)virtualIdx * _bytesPerLed;
                uint8_t byte0 = _buffer[bufferOffset];
                uint8_t byte1 = _buffer[bufferOffset + 1];
                uint8_t byte2 = _buffer[bufferOffset + 2];

                // Interpret bytes as RGB (VirtualStrip stores in ColorOrder format,
                // but we pass it as-is because physical driver expects logical RGB)
                // For BGR ColorOrder: byte0=B, byte1=G, byte2=R
                // We need to reverse this back to R, G, B for the driver
                uint8_t r, g, b;
                switch (_colorOrder)
                {
                    case ColorOrder::RGB:
                        r = byte0; // Red
                        g = byte1; // Green
                        b = byte2; // Blue
                        break;
                    case ColorOrder::RBG:
                        r = byte0; // Red
                        b = byte1; // Blue
                        g = byte2; // Green
                        break;
                    case ColorOrder::GRB:
                        g = byte0; // Green
                        r = byte1; // Red
                        b = byte2; // Blue
                        break;
                    case ColorOrder::GBR:
                        g = byte0; // Green
                        b = byte1; // Blue
                        r = byte2; // Red
                        break;
                    case ColorOrder::BGR:
                        b = byte0; // Blue
                        g = byte1; // Green
                        r = byte2; // Red
                        break;
                    case ColorOrder::BRG:
                        b = byte0; // Blue
                        r = byte1; // Red
                        g = byte2; // Green
                        break;
                    case ColorOrder::RGBW:
                    case ColorOrder::GRBW:
// ToDo: 4 BIT - Support?!
#if OPENKNX_DEBUG
                        logErrorP("VirtualStrip::syncToPhysical - encountered RGBW ColorOrder for SPI strip!");
#endif
                        // Should not happen for 3-byte SPI strips
                        r = g = b = 0; // Not supported here
                        break;
                    default:
                        r = g = b = 0;
                        break;
                }

                // Write logical RGB(W) to physical strip (driver converts to protocol format)
                // For SPI strips (APA102), the 4th byte is brightness (0-31)
                // For RGBW strips (SK6812), the 4th byte is white channel
                if (_bytesPerLed >= 4)
                {
                    uint8_t w = _buffer[bufferOffset + 3];
                    pstrip->setPixel(i, r, g, b, w);
                }
                else
                {
                    pstrip->setPixel(i, r, g, b);
                }
            }
        }
        else
        {
            // For 1-Wire (WS2812, SK6812): Direct memcpy is OK
            size_t srcOffset = (size_t)offset * _bytesPerLed;
            size_t copySize = (size_t)count * _bytesPerLed;

            uint8_t* pstripBuffer = pstrip->getBuffer();
            if (!pstripBuffer) continue;

            memcpy(pstripBuffer, &_buffer[srcOffset], copySize);
        }
    }

    _dirty = false;
    return true;
}

/**
 * @brief Send to all PhysicalStrips
 * @return true if all sends were successful
 */
bool VirtualStrip::show()
{
    // 1. Synchronisiere Buffer
    if (!syncToPhysical())
    {
        logErrorP("show - syncToPhysical failed!");
        return false;
    }

    // 2. Sende zu jedem Physical Strip
    bool allSuccess = true;
    for (const auto& mapping : _physicalStrips)
    {
        PhysicalStrip* pstrip = mapping.physicalStrip;
        if (pstrip && !pstrip->show())
        {
            logErrorP("PhysicalStrip show() failed!");
            allSuccess = false;
        }
    }

    return allSuccess;
}

/**
 * @brief Wait for completion
 * @param timeoutMs Timeout in milliseconds
 * @return true if all operations completed within the timeout
 */
bool VirtualStrip::waitForCompletion(uint32_t timeoutMs)
{
    uint32_t startTime = millis();

    while (true)
    {
        if (!isAnyBusy())
        {
            return true; // All done
        }

        if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs)
        {
            logErrorP("waitForCompletion timeout!");
            return false;
        }

        // Small pause to save CPU
        delayMicroseconds(10);
        // ToDo: Prevent busy-waiting, adjust delay as needed and optimize
    }
}

/**
 * @brief Check if any PhysicalStrip is busy
 * @return true if any PhysicalStrip is busy
 */
bool VirtualStrip::isAnyBusy() const
{
    for (const auto& mapping : _physicalStrips)
    {
        if (mapping.physicalStrip && mapping.physicalStrip->isBusy())
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get total number of physical LEDs
 * @return Total number of physical LEDs
 */
uint16_t VirtualStrip::getTotalPhysicalLeds() const
{
    uint16_t total = 0;
    for (const auto& mapping : _physicalStrips)
    {
        total += mapping.physicalLedCount;
    }
    return total;
}
