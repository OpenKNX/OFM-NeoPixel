# OFM-NeoPixel

**Version:** 0.0.1  
**Platform:** OpenKNX (RP2040, RP2350, ESP32-S3)  
**License:** GNU GPL v3.0  
**Author:** Erkan Colak

A high-performance, hardware-optimized LED control library for addressable RGB/RGBW strips on OpenKNX devices.

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Installation](#installation)
- [Hardware Support](#hardware-support)
- [Console Commands](#console-commands)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

---

## Overview

OFM-NeoPixel provides a three-layer architecture for managing addressable LED strips:

1. **PhysicalStrip** - Hardware abstraction for individual LED strips
2. **VirtualStrip** - Logical composition of multiple physical strips
3. **Segment** - Effect zones with independent animations

This design enables complex LED configurations with minimal CPU overhead through hardware acceleration (PIO/DMA on RP2040, RMT on ESP32).

### What Makes This Library Different?

- **Hardware Accelerated**: Zero CPU overhead during LED updates (DMA/PIO/RMT)
- **Multi-Strip Composition**: Combine multiple physical strips into one logical strip
- **Segment-Based Effects**: Apply different animations to different LED ranges
- **Stateless Effect System**: Single effect instance shared by multiple segments
- **Platform Optimized**: RP2040 (PIO/DMA), RP2350 (PIO/DMA), ESP32-S3 (RMT)

---

## Key Features

### Hardware Layer

- Multiple strip support (up to 7 on RP2040, 11 on RP2350, 7 on ESP32-S3)
- Protocol support: WS2812, WS2812B, WS2813, WS2815, SK6812, APA102, WS2801
- Automatic driver selection based on platform and protocol
- DMA transfers for zero-CPU overhead (RP2040/RP2350)
- RMT hardware acceleration (ESP32-S3)
- SPI support for APA102/WS2801 strips

### Software Layer

- Virtual strip abstraction with automatic offset calculation
- Segment-based effect system
- Integrated effects: Solid, Rainbow, Pride2015, Confetti, Juggle, BPM, Cylon, Wipe
- Per-segment brightness control
- Color order abstraction (RGB, GRB, BGR, RGBW, GRBW)
- Performance tracking and statistics

### Integration

- OpenKNX module interface
- Console command system for configuration and testing
- GroupObject support (planned)
- Real-time performance monitoring

---

## Quick Start

### Basic Example

```cpp
#include "OpenKNX.h"
#include "NeoPixel.h"

void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Create a physical strip (GPIO 9, 64 LEDs, WS2812B)
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    // Update the strip
    neoPixelModule.updateAll();
}

void loop() {
    openknx.loop();
}
```

### Multi-Strip Example with Effects

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Add two physical strips
    neoPixelModule.addStrip(9, 8, LedProtocol::WS2812B);   // Strip 0: 8 LEDs
    neoPixelModule.addStrip(22, 64, LedProtocol::WS2812B); // Strip 1: 64 LEDs
    
    // Create virtual strip (total 72 LEDs)
    auto virt = neoPixelModule.addVirtualStrip(72);
    virt->attachPhysical(0, 0);  // PhysStrip[0] at offset 0
    virt->attachPhysical(1, 8);  // PhysStrip[1] at offset 8
    
    // Create two segments with different effects
    auto seg1 = neoPixelModule.addSegment(0, 0, 35);   // First half
    auto seg2 = neoPixelModule.addSegment(0, 36, 71);  // Second half
    
    seg1->setEffect(1);  // Rainbow effect
    seg2->setEffect(6);  // Cylon effect
    
    // Virtual strip composition is handled automatically:
    // Strip 0 maps to Virtual[0-7]
    // Strip 1 maps to Virtual[8-71]
    
    // Enable auto-update at 20 FPS
    neoPixelModule.setAutoUpdate(true);
    neoPixelModule.setUpdateSpeed(UpdateSpeed::NORMAL);
}
```

### Console Configuration Example

```bash
# Add physical strips
neo phys add 9 8        # GPIO 9: 8 LEDs
neo phys add 22 64      # GPIO 22: 64 LEDs

# Create virtual strip
neo virt add 72         # 72 LEDs total

# Attach physical strips to virtual
neo virt attach 0 0     # Attach PhysStrip[0] to VirtStrip[0] at offset 0
neo virt attach 0 1     # Attach PhysStrip[1] to VirtStrip[0] at offset 8

# Create segments (effect zones)
neo seg add 0 0 35      # Segment[0]: LEDs 0-35 in VirtStrip[0]
neo seg add 0 36 71     # Segment[1]: LEDs 36-71 in VirtStrip[0]

# Assign effects
neo effect 0 1          # Rainbow on Segment[0]
neo effect 1 6          # Cylon on Segment[1]
neo brightness 1 200    # 78% brightness

# Start rendering
neo auto on             # Auto-update at 20 FPS

# Check performance
neo perf                # Show CPU usage and frame rate
```

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────┐
│                    NeoPixel Module                      │
│              (OpenKNX Integration Layer)                │
│  - Console commands                                     │
│  - GroupObject handling (planned)                       │
│  - Lifecycle management                                 │
└────────────────────────┬────────────────────────────────┘
                         |
┌────────────────────────▼────────────────────────────────┐
│                  NeoPixelManager                        │
│  - Physical strip lifecycle                             │
│  - Virtual strip composition                            │
│  - Segment orchestration                                │
│  - Effect update scheduling                             │
└────┬────────────┬─────────────┬─────────────────────────┘
     |            |             |
┌────▼───────┐  ┌─▼────────┐  ┌─▼────────┐
│ Physical   │  │ Virtual  │  │ Segment  │
│ Strip      │  │ Strip    │  │          │
└────┬───────┘  └──────────┘  └─────┬────┘
     |                              |
┌────▼───────────────┐        ┌─────▼─────┐
│ IHardwareDriver    │        │  Effect   │
│  - PIO (RP2040)    │        │           │
│  - RMT (ESP32)     │        └───────────┘
│  - SPI (All)       │
└────────────────────┘
```

### Data Flow

```
1. Application -> Segment.setPixel()
            |
            ▼
2. VirtualStrip buffer update
            |
            ▼
3. Effect.update() -> Modify segment pixels
            |
            ▼
4. VirtualStrip.mapToPhysical() -> Copy to physical buffers
            |
            ▼
5. PhysicalStrip.show() -> Hardware transfer (DMA/RMT/SPI)
            |
            ▼
6. GPIO -> LED Strip
```

### Memory Layout

```
System Components                RAM Usage
────────────────────────────────────────────
NeoPixelManager                  ~200 bytes
PhysicalStrip (per strip)        ~150 bytes + LED buffer
  ├─ LED buffer (RGB)            N × 3 bytes
  └─ DMA buffer (if enabled)     N × 4 bytes
VirtualStrip                     ~70 bytes + LED buffer
  └─ LED buffer                  N × bytesPerLed
Segment (per segment)            ~180 bytes
Effect instances (shared)        ~200 bytes total

Example: 3 strips (100+64+8 LEDs), 1 virtual (172 LEDs), 3 segments
Total RAM: ~3.2 KB
```

---

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
[env:your_board]
lib_deps =
    https://github.com/OpenKNX/OFM-NeoPixel.git

build_flags =
    -DNEOPIXEL_MODULE
    ; Optional: Configure resource limits
    ; -DNEOPIXEL_MAX_PHYSICAL_STRIPS=12
    ; -DNEOPIXEL_MAX_VIRTUAL_STRIPS=6
    ; -DNEOPIXEL_MAX_SEGMENTS=32
    ; -DNEOPIXEL_ENFORCE_LIMITS=1
```

**Resource Limits:**

The library includes configurable limits to prevent memory exhaustion:

- `NEOPIXEL_MAX_PHYSICAL_STRIPS` (default: 6) - Maximum physical LED strips
- `NEOPIXEL_MAX_VIRTUAL_STRIPS` (default: 12) - Maximum virtual strips
- `NEOPIXEL_MAX_SEGMENTS` (default: 16) - Maximum segments
- `NEOPIXEL_ENFORCE_LIMITS` (default: 1) - Enable/disable limit enforcement

These limits are used for vector pre-allocation and optional runtime enforcement.

### Arduino IDE

1. Download the repository as ZIP
2. Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded ZIP file

### OpenKNX Project

1. Clone into your OpenKNX project's `lib` directory:
```bash
cd lib
git clone https://github.com/OpenKNX/OFM-NeoPixel.git
```

2. Add to your `main.cpp`:
```cpp
#ifdef NEOPIXEL_MODULE
    #include "NeoPixel.h"
#endif

void setup() {
    // ...
    #ifdef NEOPIXEL_MODULE
        openknx.addModule(13, neoPixelModule);
    #endif
    // ...
}
```

3. Define in `platformio.ini`:
```ini
build_flags =
    -DNEOPIXEL_MODULE
    ; Optional: Enable tests and benchmarks
    ; -DOPENKNX_NEOPIXEL_TESTS
    ; -DOPENKNX_NEOPIXEL_BENCHMARK
```

---

## Hardware Support

### Supported Microcontrollers

| Platform | Architecture | PIO/RMT Channels | Max Strips | Status |
|----------|--------------|------------------|------------|--------|
| RP2040   | ARM Cortex-M0+ | 8 PIO (7 usable) | 7 + 2 SPI | Tested |
| RP2350   | ARM Cortex-M33 | 12 PIO (11 usable) | 11 + 2 SPI | Tested |
| ESP32-S3 | Xtensa LX7 | 4 RMT (3 usable) | 7 + 2 SPI | Not Tested |

### Supported LED Protocols

#### 1-Wire Protocols (Single Data Line)

| Protocol | Voltage | Colors | Speed | Color Order | Notes |
|----------|---------|--------|-------|-------------|-------|
| WS2812   | 5V      | RGB    | 800kHz | GRB | Original |
| WS2812B  | 5V      | RGB    | 800kHz | GRB | Most common |
| WS2813   | 5V      | RGB    | 800kHz | GRB | Data backup line |
| WS2815   | 12V     | RGB    | 800kHz | GRB | High voltage |
| WS2811   | 12V     | RGB    | 400kHz | RGB | Slower timing |
| SK6812   | 5V/12V  | RGBW   | 800kHz | GRBW | 4-channel |
| SK6805   | 5V      | RGBW   | 800kHz | GRBW | 4-channel |
| WS2814   | 12V     | RGBW   | 800kHz | GRBW | 4-channel |
| TM1814   | 12V     | RGBW   | 800kHz | GRBW | 4-channel |
| GS8208   | 12V     | RGB    | 800kHz | GRB | High voltage |

#### SPI Protocols (Clock + Data Lines)

| Protocol | Voltage | Colors | Max Speed | Features |
|----------|---------|--------|-----------|----------|
| APA102   | 5V      | RGB + Brightness | 20 MHz | Per-LED brightness |
| SK9822   | 5V      | RGB + Brightness | 15 MHz | APA102 clone |
| WS2801   | 5V      | RGB    | 25 MHz | Simple SPI |
| LPD8806  | 5V      | RGB (7-bit) | 20 MHz | Legacy |

### Wiring

#### 1-Wire (WS2812B, SK6812, etc.)

```
RP2040/ESP32         LED Strip
─────────────────────────────
GPIO Pin    ────────► DIN
GND         ────────► GND
5V/12V      ────────► VCC
```

**Important:** A 3.3V-to-5V **level shifter is highly recommended** for reliable operation. While some setups work without it (especially with short wires <30cm), LED strips expect 5V logic levels. The RP2040/RP2350/ESP32 output 3.3V, which may cause:
- Random flickering or glitches
- First LED showing wrong colors
- Unreliable data transmission with longer strips

#### SPI (APA102, WS2801)

```
RP2040/ESP32         LED Strip
─────────────────────────────
SPI MOSI    ────────► DI/SDI
SPI SCK     ────────► CI/CLK
GND         ────────► GND
5V          ────────► VCC
```

**Important:** A 3.3V-to-5V **level shifter is highly recommended** for both MOSI and SCK lines. APA102/WS2801 strips expect 5V logic levels for reliable high-speed SPI communication.

### Power Considerations

- **Current Draw:**
  - WS2812B: ~60mA per LED at full white
  - SK6812 RGBW: ~80mA per LED at full white
  - APA102: ~60mA per LED at full brightness
  
- **Power Supply:**
  - Use external 5V power supply for >10 LEDs
  - Calculate total current: (LED count × 60mA) + 20% safety margin
  - Example: 100 LEDs × 60mA = 6A minimum power supply
  
- **Wiring Best Practices:**
  - Add 1000µF capacitor between VCC and GND near LED strip
  - Use 470Ω resistor on data line for signal protection
  - Keep data wire short (<30cm) or use level shifter
  - Use proper wire gauge for power (18-22 AWG for <3A, 16 AWG for >3A)
  
- **Level Shifter:**
  - **Highly recommended** for 3.3V microcontrollers (RP2040/RP2350/ESP32)
  - Prevents flickering, glitches, and unreliable operation
  - Required for long cable runs (>30cm)
  - See wiring section above for recommended ICs and tutorials

---

## Console Commands

### Overview

All commands start with `neo` prefix. Use `neo help` to see available commands.

### Core Commands

#### Information

```bash
neo                     # Show module info and strip count
neo list                # List all strips, virtual strips, and segments
neo info                # Detailed system information
neo perf                # Performance statistics (FPS, CPU usage)
```

#### Update Control

```bash
neo update              # Manual update (send buffer to LEDs)
neo clear               # Clear all LEDs (turn off)
neo speed <ms>          # Set update interval (e.g., 'neo speed 50' = 20 FPS)
neo auto <on|off>       # Enable/disable auto-update mode
```

**Update Speed Presets:**

```bash
neo speed slow          # 10 FPS (100ms)
neo speed normal        # 20 FPS (50ms) - default
neo speed fast          # 30 FPS (33ms)
neo speed max           # 50 FPS (20ms)
neo speed extreme       # 80 FPS (12ms)
neo speed ludicrous     # 120 FPS (4ms)
```

### Physical Strip Commands

```bash
neo phys add <pin> <count> [protocol]
    # Add physical strip
    # Examples:
    neo phys add 9 64              # GPIO 9, 64 LEDs, WS2812B (default)
    neo phys add 22 100 SK6812     # GPIO 22, 100 LEDs, SK6812 RGBW
    neo phys add 5 50 APA102       # GPIO 5, 50 LEDs, APA102 (SPI)

neo phys del <index>
    # Delete physical strip by index
    neo phys del 0

neo phys list
    # List all physical strips with details
```

### Virtual Strip Commands

```bash
neo virt add <count> [colorOrder]
    # Create virtual strip
    # Examples:
    neo virt add 72                # 72 LEDs, GRB (default)
    neo virt add 100 RGB           # 100 LEDs, RGB order
    neo virt add 150 RGBW          # 150 LEDs, RGBW (4-channel)

neo virt del <index>
    # Delete virtual strip
    neo virt del 0

neo virt attach <virtIndex> <physIndex> [offset]
    # Attach physical strip to virtual strip
    # Offset is auto-calculated if omitted
    neo virt attach 0 0            # Auto offset (0)
    neo virt attach 0 1            # Auto offset (after strip 0)
    neo virt attach 0 2 50         # Manual offset at LED 50

neo virt detach <virtIndex> <physIndex>
    # Detach physical strip from virtual strip
    neo virt detach 0 1

neo virt list
    # List all virtual strips with attachments
```

### Segment Commands

```bash
neo seg add <virtIndex> <startLed> <endLed>
    # Create segment in virtual strip
    # Examples:
    neo seg add 0 0 35             # Segment 0: LEDs 0-35
    neo seg add 0 36 71            # Segment 1: LEDs 36-71
    neo seg add 0 10 20            # Segment 2: LEDs 10-20 (overlap OK)

neo seg del <index>
    # Delete segment
    neo seg del 0

neo seg list
    # List all segments with details
```

### Effect Commands

```bash
neo effects
    # List all available effects

neo effect <segIndex> <effectId>
    # Assign effect to segment
    # Effect IDs:
    #   0 = Solid Color
    #   1 = Rainbow
    #   2 = Pride2015
    #   3 = Confetti
    #   4 = Juggle
    #   5 = BPM
    #   6 = Cylon
    #   7 = Wipe
    # Examples:
    neo effect 0 0                 # Solid color on Segment 0
    neo effect 1 1                 # Rainbow on Segment 1
    neo effect 2 6                 # Cylon on Segment 2

neo color <segIndex> <r> <g> <b> [w]
    # Set color for segment (Solid effect only)
    neo color 0 255 0 0            # Red
    neo color 1 0 255 0            # Green
    neo color 2 0 0 255 128        # Blue + 50% white (RGBW)

neo brightness <segIndex> <value>
    # Set brightness (0-255)
    neo brightness 0 128           # 50% brightness
    neo brightness 1 255           # 100% brightness
```

### Testing Commands

Available when compiled with `-DOPENKNX_NEOPIXEL_TESTS`:

```bash
neo test anim start             # Start animation test
neo test anim stop              # Stop animation test
neo test simple start           # Start simple test (color cycle)
neo test simple stop            # Stop simple test
```

### Benchmark Commands

Available when compiled with `-DOPENKNX_NEOPIXEL_BENCHMARK`:

```bash
neo benchmark led <count>       # Benchmark LED count scaling
neo benchmark update            # Benchmark update performance
neo benchmark driver            # Test different driver types
neo benchmark effect            # Compare effect performance
```

---

## API Reference

### NeoPixel Module

```cpp
class NeoPixel : public OpenKNX::Module {
public:
    // Strip Management
    PhysicalStrip* addStrip(uint8_t pin, uint16_t ledCount, 
                           LedProtocol protocol = LedProtocol::WS2812B);
    VirtualStrip* addVirtualStrip(uint16_t ledCount, 
                                 ColorOrder order = ColorOrder::GRB);
    Segment* addSegment(uint8_t virtStripIndex, 
                       uint16_t startLed, uint16_t endLed);
    
    // Update Control
    void updateAll();
    void clearAll();
    void setAutoUpdate(bool enable);
    void setUpdateSpeed(UpdateSpeed speed);
    void setUpdateInterval(uint32_t intervalMs);
    
    // Access
    NeoPixelManager* getManager();
};

extern NeoPixel neoPixelModule;  // Global instance
```

### NeoPixelManager

```cpp
class NeoPixelManager {
public:
    // Physical Strip Management
    PhysicalStrip* addPhysicalStrip(uint8_t pin, uint16_t ledCount,
                                   LedProtocol protocol);
    bool removePhysicalStrip(uint8_t index);
    PhysicalStrip* getPhysicalStrip(uint8_t index);
    uint8_t getPhysicalStripCount() const;
    
    // Virtual Strip Management
    VirtualStrip* addVirtualStrip(uint16_t ledCount, ColorOrder order);
    bool removeVirtualStrip(uint8_t index);
    VirtualStrip* getVirtualStrip(uint8_t index);
    uint8_t getVirtualStripCount() const;
    
    // Segment Management
    Segment* addSegment(uint8_t virtStripIndex, 
                       uint16_t startLed, uint16_t endLed);
    bool removeSegment(uint8_t index);
    Segment* getSegment(uint8_t index);
    uint8_t getSegmentCount() const;
    
    // Update Control
    void updateAll();
    void updateSegments(uint32_t deltaTime);
    void clearAll();
    
    // Performance
    ManagerStats getStats() const;
};
```

### PhysicalStrip

```cpp
class PhysicalStrip {
public:
    // Construction
    PhysicalStrip(uint8_t pin, uint16_t ledCount, LedProtocol protocol);
    
    // Pixel Control
    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void setPixel(uint16_t index, uint32_t color);
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void clear();
    
    // Display Control
    void show();
    void setBrightness(uint8_t brightness);
    
    // Properties
    uint16_t getLedCount() const;
    LedProtocol getProtocol() const;
    uint8_t getPin() const;
    uint8_t getBytesPerLed() const;
    
    // Buffer Access
    uint8_t* getBuffer();
    const uint8_t* getBuffer() const;
    
    // Driver
    IHardwareDriver* getDriver() const;
};
```

### VirtualStrip

```cpp
class VirtualStrip {
public:
    // Construction
    VirtualStrip(uint16_t ledCount, ColorOrder order = ColorOrder::GRB);
    
    // Physical Strip Management
    bool attachPhysical(PhysicalStrip* physical, uint16_t offset = UINT16_MAX);
    bool detachPhysical(PhysicalStrip* physical);
    
    // Pixel API
    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void clear();
    
    // Buffer Access
    uint8_t* getBuffer();
    
    // Sync & Transfer
    void mapToPhysical();
};
```

### Segment

```cpp
class Segment {
public:
    // Construction
    Segment(VirtualStrip* strip, uint16_t startLed, uint16_t endLed);
    
    // Properties
    uint16_t getStartLed() const;
    uint16_t getEndLed() const;
    uint16_t getLedCount() const;
    VirtualStrip* getVirtualStrip() const;
    
    // Effect Management
    void setEffect(uint8_t effectId);
    void setEffect(Effect* effect);
    Effect* getEffect() const;
    
    // State Machine
    void update(uint32_t deltaTime);
    void pause();
    void resume();
    void stop();
    bool isRunning() const;
    
    // Configuration
    void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void setBrightness(uint8_t brightness);
    LedConfig& getConfig();
    LedState& getState();             // Access state (for effects)
};
```

### Effect System

```cpp
// Base Effect Interface
class Effect {
public:
    virtual void update(Segment* segment, uint32_t deltaTime) = 0;
    virtual void reset() {}
    virtual const char* getName();
    virtual bool isDone(const Segment* segment) const;
};

// Built-in Effects (in EffectPool namespace)
namespace EffectPool {
    extern EffectSolid solidEffect;      // ID: 0
    extern EffectRainbow rainbowEffect;  // ID: 1
    extern EffectPride pridEffect;       // ID: 2
    extern EffectConfetti confettiEffect;// ID: 3
    extern EffectJuggle juggleEffect;    // ID: 4
    extern EffectBPM bpmEffect;          // ID: 5
    extern EffectCylon cylonEffect;      // ID: 6
    extern EffectWipe wipeEffect;        // ID: 7
    
    Effect* getEffect(uint8_t id);
}
```

---

## Examples

### Example 1: Single Strip with Rainbow Effect

```cpp
#include "OpenKNX.h"
#include "NeoPixel.h"

void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Create physical strip
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    // Create virtual strip and attach physical
    auto virt = neoPixelModule.addVirtualStrip(64);
    virt->attachPhysical(strip);
    
    // Create segment covering all LEDs
    auto segment = neoPixelModule.addSegment(0, 0, 63);
    segment->setEffect(1);  // Rainbow effect
    segment->setBrightness(128);
    
    // Enable auto-update
    neoPixelModule.setAutoUpdate(true);
    neoPixelModule.setUpdateSpeed(UpdateSpeed::NORMAL);
}

void loop() {
    openknx.loop();
}
```

### Example 2: Multi-Strip Composition

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Create three physical strips
    auto strip1 = neoPixelModule.addStrip(9, 100, LedProtocol::WS2812B);
    auto strip2 = neoPixelModule.addStrip(10, 64, LedProtocol::WS2812B);
    auto strip3 = neoPixelModule.addStrip(11, 8, LedProtocol::SK6812);  // RGBW
    
    // Create virtual strip (total 172 LEDs)
    auto virt = neoPixelModule.addVirtualStrip(172);
    
    // Attach physical strips (auto offset calculation)
    virt->attachPhysical(strip1);  // Offset: 0
    virt->attachPhysical(strip2);  // Offset: 100
    virt->attachPhysical(strip3);  // Offset: 164
    
    // Create segments with different effects
    auto seg1 = neoPixelModule.addSegment(0, 0, 99);    // Strip 1
    auto seg2 = neoPixelModule.addSegment(0, 100, 163); // Strip 2
    auto seg3 = neoPixelModule.addSegment(0, 164, 171); // Strip 3
    
    seg1->setEffect(1);  // Rainbow
    seg2->setEffect(6);  // Cylon
    seg3->setEffect(0);  // Solid white
    seg3->setColor(0, 0, 0, 255);  // Pure white on W channel
    
    // Enable auto-update
    neoPixelModule.setAutoUpdate(true);
    neoPixelModule.setUpdateSpeed(UpdateSpeed::FAST);
}
```

### Example 3: Dynamic Control

```cpp
uint32_t lastChange = 0;
uint8_t currentEffect = 0;

void loop() {
    openknx.loop();
    
    // Change effect every 10 seconds
    if (millis() - lastChange > 10000) {
        auto segment = neoPixelModule.getManager()->getSegment(0);
        if (segment) {
            currentEffect = (currentEffect + 1) % 8;  // Cycle through 8 effects
            segment->setEffect(currentEffect);
            
            // Random brightness
            uint8_t brightness = random(64, 255);
            segment->setBrightness(brightness);
            
            logInfoP("Changed to effect %d, brightness %d", currentEffect, brightness);
        }
        lastChange = millis();
    }
}
```

### Example 4: RGBW Strip with White Channel

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // SK6812 RGBW strip
    auto strip = neoPixelModule.addStrip(9, 30, LedProtocol::SK6812);
    auto virt = neoPixelModule.addVirtualStrip(30, ColorOrder::GRBW);
    virt->attachPhysical(strip);
    
    // Create three segments
    auto seg1 = neoPixelModule.addSegment(0, 0, 9);    // First 10 LEDs
    auto seg2 = neoPixelModule.addSegment(0, 10, 19);  // Middle 10 LEDs
    auto seg3 = neoPixelModule.addSegment(0, 20, 29);  // Last 10 LEDs
    
    // Segment 1: Pure RGB color (no white)
    seg1->setEffect(0);  // Solid
    seg1->setColor(255, 0, 0, 0);  // Red
    
    // Segment 2: RGB + White
    seg2->setEffect(0);
    seg2->setColor(0, 255, 0, 128);  // Green + 50% white
    
    // Segment 3: Pure white
    seg3->setEffect(0);
    seg3->setColor(0, 0, 0, 255);  // White channel only
    
    // Update once
    neoPixelModule.updateAll();
}
```

### Example 5: Performance Monitoring

```cpp
uint32_t lastPrint = 0;

void loop() {
    openknx.loop();
    
    // Print performance stats every 5 seconds
    if (millis() - lastPrint > 5000) {
        auto stats = neoPixelModule.getManager()->getStats();
        
        logInfoP("Performance:");
        logInfoP("  FPS: %.1f", stats.fps);
        logInfoP("  Update time: %lu µs", stats.avgUpdateTime);
        logInfoP("  CPU: %.2f%%", stats.cpuPercent);
        logInfoP("  Strips: %d physical, %d virtual", 
                 stats.physicalStripCount, stats.virtualStripCount);
        logInfoP("  Segments: %d", stats.segmentCount);
        
        lastPrint = millis();
    }
}
```

---

## Performance

### Benchmarks (RP2040 @ 133 MHz)

| Operation | Time | Notes |
|-----------|------|-------|
| Update 64 LEDs (WS2812B) | ~22 µs | Effect calculation |
| DMA Transfer (64 LEDs) | ~2 µs | Non-blocking |
| Total CPU @ 30 FPS | 0.07% | Hardware accelerated |

### Memory Usage

```
Component                        RAM Usage
──────────────────────────────────────────
NeoPixelManager                  ~200 bytes
PhysicalStrip (per strip)        ~150 bytes
  + LED buffer (64 LEDs RGB)     192 bytes
VirtualStrip (72 LEDs)           ~70 bytes
  + LED buffer                   216 bytes
Segment (per segment)            ~180 bytes
Effect instances (shared)        ~200 bytes total
──────────────────────────────────────────
Example Config (2 strips, 1 virtual, 2 segments)
Total RAM                        ~1.4 KB
```

---

## Troubleshooting

### Q: LEDs show wrong colors
**A:** Check color order setting. Try different ColorOrder values (RGB, GRB, BGR, RGBW, GRBW).

```cpp
auto virt = neoPixelModule.addVirtualStrip(64, ColorOrder::RGB);  // Try different orders
```

### Q: LEDs flicker or show random colors
**A:** 
1. Add 470Ω resistor on data line
2. Add 1000µF capacitor between VCC and GND
3. Keep data wire short (<30cm)
4. Ensure proper power supply

### Q: First LED always wrong color
**A:** Common with WS2812B. Add a dummy LED at the start or use a level shifter.

### Q: Performance issues / low FPS
**A:**
1. Reduce update interval: `neo speed slow`
2. Reduce LED count or segment count
3. Use simpler effects
4. Check `neo perf` for diagnostics

### Q: Strip not updating
**A:**
1. Check if auto-update is enabled: `neo auto on`
2. Call `neoPixelModule.updateAll()` manually
3. Verify virtual strip attachments: `neo virt list`
4. Check segment configuration: `neo seg list`

### Q: Out of memory errors
**A:** Reduce resource limits in `platformio.ini`:
```ini
build_flags =
    -DNEOPIXEL_MAX_PHYSICAL_STRIPS=4
    -DNEOPIXEL_MAX_VIRTUAL_STRIPS=2
    -DNEOPIXEL_MAX_SEGMENTS=8
```

### Q: How do I control LEDs from KNX?
**A:** Not yet implemented. Planned for future release via GroupObjects.

---

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

### Development Guidelines

- Follow existing code style
- Add documentation for new features
- Test on target hardware (RP2040/RP2350/ESP32)
- Update relevant documentation files

---

## License

GNU General Public License v3.0

See [LICENSE](LICENSE) file for details.

---

## Credits

**Author:** Erkan Colak  
**Project:** OpenKNX  
**Repository:** https://github.com/OpenKNX/OFM-NeoPixel

Special thanks to the OpenKNX community and contributors.
