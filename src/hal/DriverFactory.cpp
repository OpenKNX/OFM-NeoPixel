#include "DriverFactory.h"
#include "HW_NeoPixel_SPI.h"

#if defined(ARDUINO_ARCH_RP2040)
    #include "../pio/pio_neopixel_serial.h"
    #include "../pio/pio_neopixel_spi.h"
#endif

#if defined(ARDUINO_ARCH_ESP32)
    #include "../rmt/rmt_neopixel_serial.h"
#endif

/**
 * Create a hardware driver with automatic selection
 * @param pin GPIO pin for LED data
 * @param ledCount Number of LEDs
 * @param protocol LED protocol (WS2812, APA102, etc.)
 * @param driverType Driver type (AUTO, SERIAL_1WIRE, SPI_HARDWARE, SPI_PIO)
 * @return Pointer to IHardwareDriver instance, nullptr on error
 * @note Caller is responsible for deleting the pointer!
 *
 * Selection logic:
 * 1-Wire Protocols (WS2812, SK6812, etc.):
 *  - RP2040/50: PIO_NeoPixel_Serial (Hardware timer via PIO)
 * - ESP32:     RMT_NeoPixel_Serial (Hardware timer via RMT, all variants)
 *
 * SPI Protocols (APA102, WS2801, etc.):
 * - RP2040/50: Hardware SPI (faster, less flexible)
 * - ESP32:     Hardware SPI
 */
IHardwareDriver* DriverFactory::create(uint pin,              // Data pin (for 1-Wire) or MOSI pin (for SPI)
                                       uint16_t ledCount,     // Number of LEDs
                                       LedProtocol protocol,  // LED protocol (WS2812, APA102, etc.)
                                       DriverType driverType, // Driver type (AUTO, SERIAL_1WIRE, SPI_HARDWARE, SPI_PIO)
                                       uint mosiPin,          // For SPI: MOSI pin (optional)
                                       uint sckPin,           // For SPI: SCK pin (optional)
                                       TimingMode timingMode) // Timing mode for clock divider
{
    if (ledCount == 0) // Check for valid LED count
    {
        return nullptr;
    }

    bool isSerial = isSerialProtocol(protocol);
    bool isSpi = isSpiProtocol(protocol);

    if (!isSerial && !isSpi)
    {
        return nullptr; // Unknown protocol. We support only 1-Wire and SPI protocols.
    }

    if (driverType == DriverType::AUTO) // Check for automatic selection
    {
        if (isSerial)
        {
            driverType = DriverType::SERIAL_1WIRE;
        }
        else
        {
            driverType = DriverType::SPI_HARDWARE;
        }
    }
#if defined(ARDUINO_ARCH_RP2040)
    if (driverType == DriverType::SERIAL_1WIRE && isSerial)
    {
        // 1-Wire: Use PIO with DMA, pass timingMode parameter
        return new PIO_NeoPixel_Serial(pin, ledCount, protocol, true, timingMode);
    }
    else if (driverType == DriverType::SPI_HARDWARE && isSpi)
    {
        // The RP PIO backend uses protocol-native byte streams for every
        // supported SPI LED protocol and retains flexible pin assignment.
        return new PIO_NeoPixel_SPI(sckPin, mosiPin, ledCount, protocol);
    }
    else if (driverType == DriverType::SPI_PIO && isSpi)
    {
        // SPI via PIO with protocol-native framing.
        return new PIO_NeoPixel_SPI(sckPin, mosiPin, ledCount, protocol);
    }

#elif defined(ARDUINO_ARCH_ESP32)
    if (driverType == DriverType::SERIAL_1WIRE && isSerial)
    {
        return new RMT_NeoPixel_Serial(pin, ledCount, protocol);
    }
    else if (driverType == DriverType::SPI_HARDWARE && isSpi)
    {
        return new HW_NeoPixel_SPI(ledCount, protocol);
    }
#else
    return nullptr; // Not supported platform
#endif
    return nullptr;
}

/*
 * Check if the current platform is RP2040 (excluding RP2350)
 */
bool DriverFactory::isPlatformRP2040()
{
#if defined(ARDUINO_ARCH_RP2040) && !defined(PICO_RP2350)
    return true;
#else
    return false;
#endif
}

/*
 * Check if the current platform is RP2350
 */
bool DriverFactory::isPlatformRP2350()
{
#if defined(PICO_RP2350)
    return true;
#else
    return false;
#endif
}

/*
 * Check if the current platform is ESP32
 */
bool DriverFactory::isPlatformESP32()
{
#if defined(ARDUINO_ARCH_ESP32)
    return true;
#else
    return false;
#endif
}

/*
 * Check if the current protocol is 1-Wire (Serial)
 */
bool DriverFactory::isSerialProtocol(LedProtocol protocol)
{
    return ProtocolHelper::is1Wire(protocol);
}

/*
 * Check if the current protocol is SPI
 */
bool DriverFactory::isSpiProtocol(LedProtocol protocol)
{
    return ProtocolHelper::isSPI(protocol);
}
