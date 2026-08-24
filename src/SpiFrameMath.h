/**
 * @file SpiFrameMath.h
 * @brief Platform-independent SPI LED frame layout and encoding helpers.
 */

#pragma once

#include "IHardwareDriver.h"

#include <stddef.h>
#include <stdint.h>

struct SpiFrameLayout
{
    size_t startFrameSize;
    size_t dummyLedSize;
    size_t pixelDataSize;
    size_t endFrameSize;
    size_t bufferSize;
    uint8_t bytesPerLed;
    bool hasGlobalBrightness;
};

inline bool spiMakeFrameLayout(LedProtocol protocol, size_t ledCount,
                               uint8_t startFrameCount, uint8_t dummyLedMode,
                               uint8_t endFrameCount, SpiFrameLayout& layout)
{
    layout.hasGlobalBrightness = protocol == LedProtocol::APA102 ||
                                protocol == LedProtocol::APA102_CLONE ||
                                protocol == LedProtocol::SK9822;
    layout.bytesPerLed = layout.hasGlobalBrightness ? 4 : ProtocolHelper::getBytesPerLed(protocol);
    layout.startFrameSize = layout.hasGlobalBrightness ? (size_t)startFrameCount * 4 : 0;
    layout.dummyLedSize = layout.hasGlobalBrightness && dummyLedMode == 1 ? 4 : 0;
    layout.endFrameSize = layout.hasGlobalBrightness ? (size_t)endFrameCount * 4 : 0;

    if (ledCount > (SIZE_MAX - layout.startFrameSize - layout.dummyLedSize - layout.endFrameSize) /
                       layout.bytesPerLed)
        return false;

    layout.pixelDataSize = ledCount * layout.bytesPerLed;
    layout.bufferSize = layout.startFrameSize + layout.dummyLedSize +
                        layout.pixelDataSize + layout.endFrameSize;
    return true;
}

inline uint8_t spiEncodeLpd8806Channel(uint8_t value)
{
    return 0x80U | (value >> 1);
}

/** PIO uses the high byte of a TX FIFO word for each on-wire SPI byte. */
inline uint32_t spiPioWordForByte(uint8_t value)
{
    return (uint32_t)value << 24;
}
