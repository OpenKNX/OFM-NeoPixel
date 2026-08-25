/**
 * @file VirtualStrip.cpp
 * @brief VirtualStrip Implementation - Unified RGB/RGBW Buffer Management
 *
 * This class manages a unified pixel buffer that is ALWAYS stored in RGB/RGBW format.
 * ColorOrder conversion happens in PhysicalStrip, not here!
 *
 * Key principles:
 * - Internal buffer is ALWAYS RGB (3 bytes) or RGBW (4 bytes)
 * - setPixel() writes RGB(W) directly to buffer (no conversion)
 * - getPixel() reads RGB(W) directly from buffer (no conversion)
 * - syncToPhysical() sends RGB(W) to PhysicalStrips
 * - PhysicalStrip handles ColorOrder conversion (RGB→GRB, RGB→BGR, etc.)
 */

#include "VirtualStrip.h"
#include "OpenKNX.h"
#include "PhysicalStripConfig.h"
#include "PowerManager.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <string.h>

/**
 * @brief Constructor
 * @param totalLeds Total number of virtual LEDs
 * @param colorOrder Only used to determine RGB (3 bytes) vs RGBW (4 bytes) vs RGBCCT (5 bytes) buffer
 *
 * VirtualStrip ALWAYS stores pixels in RGB/RGBW/RGBCCT format internally.
 * ColorOrder conversion (RGB→GRB, RGB→BGR, etc.) happens in PhysicalStrip!
 *
 * The colorOrder parameter determines:
 * - RGB/RBG/GRB/GBR/BGR/BRG → 3 bytes per LED (RGB buffer)
 * - RGBW/GRBW → 4 bytes per LED (RGBW buffer)
 * - RGBCCT/GRBCCT/RGBCTW/GRBCTW → 5 bytes per LED (RGBCCT buffer)
 *
 * ColorOrder details (GRB vs RGB etc.) are handled by PhysicalStrip, not here!
 */
VirtualStrip::VirtualStrip(uint16_t totalLeds, ColorOrder colorOrder)
    : _totalLeds(totalLeds),
      _bytesPerLed(ProtocolHelper::getBytesPerLed(colorOrder)),
      _dirty(false),
      _powerManager(nullptr),
      _pixelTransformCallback(nullptr),
      _pixelTransformUserData(nullptr)
{
    // Allocate unified buffer (always RGB/RGBW/RGBCCT format)
    _bufferSize = (size_t)totalLeds * _bytesPerLed;
    _buffer = new uint8_t[_bufferSize];
    memset(_buffer, 0, _bufferSize);

    logDebugP("VirtualStrip initialized: %u LEDs, %u Bytes/LED (%s), Buffer=%u Bytes",
              _totalLeds, _bytesPerLed, 
              (_bytesPerLed == 5) ? "RGBCCT" : ((_bytesPerLed == 4) ? "RGBW" : "RGB"), 
              (uint32_t)_bufferSize);
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
 * @brief Get hardware brightness for power calculation
 * @return For SPI strips: actual config value (16-30), for others: 255 (full power)
 *
 * Reads brightness from first attached SPI strip's config.
 * For Serial strips or if no SPI strip attached, returns 255.
 */
uint8_t VirtualStrip::getHardwareBrightness() const
{
    // Check all attached physical strips for SPI config
    for (const auto& mapping : _physicalStrips)
    {
        PhysicalStrip* pstrip = mapping.physicalStrip;
        if (!pstrip) continue;

        if (pstrip->isSpiStrip())
        {
            auto* cfg = pstrip->getConfig();
            SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
            if (spiCfg)
            {
                // Convert config brightness to 0-255 scale for power calc
                uint8_t hwBright = spiCfg->getHwBrightness();
                uint8_t minBright = spiCfg->getHwBrightnessMin();
                uint8_t maxBright = spiCfg->getHwBrightnessMax();

                // Scale from [min..max] to [0..255]
                uint8_t range = maxBright - minBright;
                if (range == 0) return 255; // Avoid division by zero

                return ((uint32_t)(hwBright - minBright) * 255) / range;
            }
        }
    }

    // Serial strips or no SPI strips: full power
    return 255;
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
 * @brief Write pixel to internal buffer
 * @param index Pixel index
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param ww Warm white component (0-255)
 * @param cw Cool white component (0-255)
 *
 * VirtualStrip ALWAYS stores in RGB/RGBW/RGBCCT format.
 * ColorOrder conversion happens later in PhysicalStrip::setPixel().
 */
void VirtualStrip::writePixelToBuffer(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (index >= _totalLeds) return;

    size_t offset = (size_t)index * _bytesPerLed;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (offset + _bytesPerLed > _bufferSize)
    {
        logErrorP("VirtualStrip: Buffer overflow prevented! Index %u, Offset %u, BufferSize %u",
                  index, (uint32_t)offset, (uint32_t)_bufferSize);
        return;
    }

#ifdef OPENKNX_NEOPIXEL_TRACE1
    if (index == 0)
    {
        openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                              "writePixelToBuffer[0]: R=%d G=%d B=%d WW=%d CW=%d",
                                              r, g, b, ww, cw);
    }
#endif

    // Store in RGB/RGBW/RGBCCT format (no ColorOrder conversion here!)
    _buffer[offset] = r;     // Red
    _buffer[offset + 1] = g; // Green
    _buffer[offset + 2] = b; // Blue
    if (_bytesPerLed >= 4)
    {
        _buffer[offset + 3] = ww; // Warm white channel
    }
    if (_bytesPerLed >= 5)
    {
        _buffer[offset + 4] = cw; // Cool white channel
    }

#ifdef OPENKNX_NEOPIXEL_TRACE1
    if (index == 0)
    {
        if (_bytesPerLed >= 5)
        {
            openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                                  "  Buffer[0]: R=%02X G=%02X B=%02X WW=%02X CW=%02X",
                                                  _buffer[offset], _buffer[offset + 1],
                                                  _buffer[offset + 2], _buffer[offset + 3],
                                                  _buffer[offset + 4]);
        }
        else if (_bytesPerLed >= 4)
        {
            openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                                  "  Buffer[0]: R=%02X G=%02X B=%02X W=%02X",
                                                  _buffer[offset], _buffer[offset + 1],
                                                  _buffer[offset + 2], _buffer[offset + 3]);
        }
        else
        {
            openknx.logger.logWithPrefixAndValues("VirtualStrip",
                                                  "  Buffer[0]: R=%02X G=%02X B=%02X",
                                                  _buffer[offset], _buffer[offset + 1],
                                                  _buffer[offset + 2]);
        }
    }
#endif

    _dirty = true;
}

/**
 * @brief Set single pixel (RGB)
 * VirtualStrip stores pixels in RGB format. No ColorOrder conversion here.
 * @param index Index of the pixel
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
bool VirtualStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= _totalLeds) return false;
    writePixelToBuffer(index, r, g, b, 0, 0);
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
    writePixelToBuffer(index, r, g, b, w, 0);
    _dirty = true; // Mark as dirty for sync!
    return true;
}

/**
 * @brief Set single pixel (RGBCCT - 5 channel with Warm White and Cool White)
 * @param index Index of the pixel
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param ww Warm white component (2700-3000K)
 * @param cw Cool white component (5000-6500K)
 */
bool VirtualStrip::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
{
    if (index >= _totalLeds) return false;
    writePixelToBuffer(index, r, g, b, ww, cw);
    _dirty = true;
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
 *
 * VirtualStrip ALWAYS stores in RGB format.
 * No ColorOrder conversion needed here!
 */
bool VirtualStrip::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const
{
    if (index >= _totalLeds) return false;

    size_t offset = (size_t)index * _bytesPerLed;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (offset + 2 >= _bufferSize) return false;

    // Read RGB directly from buffer (always in RGB format)
    r = _buffer[offset];     // Red
    g = _buffer[offset + 1]; // Green
    b = _buffer[offset + 2]; // Blue

    return true;
}

/**
 * @brief Get pixel color (RGBW)
 * @param index Index of the pixel
 * @param r Output parameter for Red component
 * @param g Output parameter for Green component
 * @param b Output parameter for Blue component
 * @param w Output parameter for White component (warm white on 5-channel)
 *
 * VirtualStrip ALWAYS stores in RGB/RGBW/RGBCCT format.
 * No ColorOrder conversion needed here!
 */
bool VirtualStrip::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const
{
    if (index >= _totalLeds) return false;

    size_t offset = (size_t)index * _bytesPerLed;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (offset + _bytesPerLed > _bufferSize) return false;

    // Read RGBW directly from buffer (always in RGB/RGBW format)
    r = _buffer[offset];     // Red
    g = _buffer[offset + 1]; // Green
    b = _buffer[offset + 2]; // Blue

    if (_bytesPerLed >= 4)
    {
        w = _buffer[offset + 3]; // Warm White or Brightness
    }
    else
    {
        w = 0; // No white channel in RGB-only strips
    }

    return true;
}

/**
 * @brief Get pixel color (RGBCCT - 5 channel)
 * @param index Index of the pixel
 * @param r Output parameter for Red component
 * @param g Output parameter for Green component
 * @param b Output parameter for Blue component
 * @param ww Output parameter for Warm White component
 * @param cw Output parameter for Cool White component
 *
 * VirtualStrip ALWAYS stores in RGB/RGBW/RGBCCT format.
 * No ColorOrder conversion needed here!
 */
bool VirtualStrip::getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& ww, uint8_t& cw) const
{
    if (index >= _totalLeds) return false;

    size_t offset = (size_t)index * _bytesPerLed;

    // CRITICAL: Bounds check to prevent buffer overflow
    if (offset + _bytesPerLed > _bufferSize) return false;

    // Read RGBCCT directly from buffer
    r = _buffer[offset];     // Red
    g = _buffer[offset + 1]; // Green
    b = _buffer[offset + 2]; // Blue

    if (_bytesPerLed >= 4)
    {
        ww = _buffer[offset + 3]; // Warm White
    }
    else
    {
        ww = 0;
    }

    if (_bytesPerLed >= 5)
    {
        cw = _buffer[offset + 4]; // Cool White
    }
    else
    {
        cw = 0;
    }

    return true;
}

/**
 * @brief Synchronize VirtualStrip buffer to PhysicalStrips
 * @param hardwareBrightness Hardware brightness for APA102/SK9822 (0-255, default 255 = max)
 * @return true if synchronization was successful
 *
 * This method:
 * 1. Reads RGB/RGBW data from VirtualStrip buffer (always in RGB format)
 * 2. Sets hardware brightness on all attached PhysicalStrips (for APA102/SK9822)
 * 3. Sends RGB data to each attached PhysicalStrip
 * 4. PhysicalStrip converts RGB to hardware ColorOrder (GRB, BGR, etc.)
 * 5. Hardware driver writes converted bytes to LED strip
 */
bool VirtualStrip::syncToPhysical()
{
    if (_physicalStrips.empty())
    {
        return true; // Nothing to synchronize
    }

    // Send RGB data to each attached PhysicalStrip
    for (const auto& mapping : _physicalStrips)
    {
        PhysicalStrip* pstrip = mapping.physicalStrip;
        if (!pstrip) continue;

        // Hardware brightness is now managed via PhysicalStripConfig
        // VirtualStrip's hardwareBrightness is ignored for SPI strips
        // Use 'neo phys config <id> brightness <16-30>' to change it

        uint16_t offset = mapping.virtualOffset;
        uint16_t count = mapping.physicalLedCount;

        // Read RGB from VirtualStrip buffer, send to PhysicalStrip
        // PhysicalStrip handles ColorOrder conversion based on its hardware
        // Check if this PHYSICAL strip supports RGBW or RGBCCT
        bool physicalIsRGBW = ProtocolHelper::hasWhiteChannel(pstrip->getColorOrder());
        bool physicalIsRGBCCT = ProtocolHelper::hasDualWhiteChannel(pstrip->getColorOrder());

        for (uint16_t i = 0; i < count; i++)
        {
            uint8_t r, g, b, ww = 0, cw = 0;

            // Normal operation: read from virtual buffer
            uint16_t virtualIdx = offset + i;
            if (virtualIdx >= _totalLeds) break;

            size_t bufferOffset = (size_t)virtualIdx * _bytesPerLed;
            r = _buffer[bufferOffset];     // Red
            g = _buffer[bufferOffset + 1]; // Green
            b = _buffer[bufferOffset + 2]; // Blue
            if (_bytesPerLed >= 4)
            {
                ww = _buffer[bufferOffset + 3]; // Warm White (or single White for RGBW)
            }
            if (_bytesPerLed >= 5)
            {
                cw = _buffer[bufferOffset + 4]; // Cool White
            }

            // Apply pixel transform callback (HCL, gamma, etc.) if set
            // This transforms the pixel BEFORE sending to hardware, without modifying the buffer
            if (_pixelTransformCallback)
            {
                // Pass WW and CW pointers based on buffer type
                // For RGBCCT: both ww and cw are valid
                // For RGBW: only ww is valid (cw is nullptr)
                // For RGB: both are nullptr
                uint8_t* wwPtr = (_bytesPerLed >= 4) ? &ww : nullptr;
                uint8_t* cwPtr = (_bytesPerLed >= 5) ? &cw : nullptr;
                
                // Calculate virtual pixel index (offset + physical index)
                uint16_t virtualPixelIndex = mapping.virtualOffset + i;
                _pixelTransformCallback(virtualPixelIndex, r, g, b, wwPtr, cwPtr, _pixelTransformUserData);
            }

            // Send to physical strip based on its capabilities
            if (physicalIsRGBCCT && _bytesPerLed >= 5)
            {
                // Physical strip supports RGBCCT and virtual buffer has WW+CW channels
                pstrip->setPixel(i, r, g, b, ww, cw);
            }
            else if (physicalIsRGBW && _bytesPerLed >= 4)
            {
                // Physical strip supports RGBW and virtual buffer has W channel.
                // Without this an RGB effect leaves the white LED dark and mixes white
                // from the three colours instead.
                auto* wcfg = pstrip->getConfig();
                if (wcfg && wcfg->getWhiteMode() != 0)
                    ProtocolHelper::applyWhiteMode(wcfg->getWhiteMode(), r, g, b, ww);
                pstrip->setPixel(i, r, g, b, ww);
            }
            else
            {
                // Physical strip is RGB only, send RGB (ignore W channels if present)
                pstrip->setPixel(i, r, g, b);
            }
        }
    }

    _dirty = false;
    return true;
}

/**
 * @brief Send to all PhysicalStrips
 * @return true if all sends were successful
 *
 * NOTE: Power management (current limiting) is now handled centrally
 *       in NeoPixelManager::updateAll() BEFORE this is called.
 *       This ensures global current limiting across all VirtualStrips.
 */
bool VirtualStrip::show()
{
    // 1. Synchronisiere Buffer (if not already done)
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
    // Never wait unbounded: 0 ("unlimited") becomes a sane absolute ceiling so a wedged
    // DMA/PIO can't hang the main loop forever → 16 s watchdog reboot.
    if (timeoutMs == 0) timeoutMs = 1000;
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

/**
 * @brief Set pixel transform callback for post-processing during sync
 * @param callback Function to call for each pixel during syncToPhysical()
 * @param userData User context passed to callback
 *
 * The callback is called for each pixel DURING syncToPhysical(), allowing
 * color transformations (HCL, gamma, etc.) without modifying the effect buffer.
 * This ensures effects that read back pixels (like Cylon fade) work correctly.
 */
void VirtualStrip::setPixelTransformCallback(PixelTransformCallback callback, void* userData)
{
    _pixelTransformCallback = callback;
    _pixelTransformUserData = userData;
}
