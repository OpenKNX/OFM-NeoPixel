/**
 * @file DriverFactory.h
 * @brief Automatical Driver Selection Factory based on Platform (RP2040/RP2350, ESP32) and Protocol (1-Wire vs SPI)
 *
 * This factory class provides a method to create appropriate hardware driver instances
 * based on the current platform and requested LED protocol. It abstracts away the
 * platform-specific details and provides a unified interface for driver creation
 * for different platforms (RP2040, RP2350, ESP32) and protocols (1-Wire, SPI).
 *
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "../IHardwareDriver.h"
#include "../TimingMode.h"
#include "Arduino.h"

class DriverFactory
{
  public:
    static IHardwareDriver* create(
        uint pin,
        uint16_t ledCount,
        LedProtocol protocol,
        DriverType driverType = DriverType::AUTO,  // Default: AUTO
        uint mosiPin = 0xFFFFFFFF,                 // For SPI: MOSI pin (optional)
        uint sckPin = 0xFFFFFFFF,                  // For SPI: SCK pin (optional)
        TimingMode timingMode = TimingMode::AUTO); // Timing mode for clock divider

    static bool isPlatformRP2040();
    static bool isPlatformRP2350();
    static bool isPlatformESP32();
    static bool isSerialProtocol(LedProtocol protocol);
    static bool isSpiProtocol(LedProtocol protocol);
};
