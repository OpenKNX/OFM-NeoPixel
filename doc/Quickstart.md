# OFM-NeoPixel Quickstart Guide

**Version:** 0.0.1  
**Date:** November 2025

Get up and running with OFM-NeoPixel in minutes.

---

## Related Documentation

- **[README](../README.md)** - Project overview and features
- **[Architecture & Flow Diagrams](Architecture.md)** - Detailed system architecture
- **[Developer API Reference](Developer.md)** - Complete API documentation
- **[Effects Porting Guide](Effects-Porting.md)** - Port FastLED effects

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Your First LED Strip](#your-first-led-strip)
- [Multi-Strip Setup](#multi-strip-setup)
- [Adding Effects](#adding-effects)
- [Console Configuration](#console-configuration)
- [Common Patterns](#common-patterns)
- [Next Steps](#next-steps)

---

## Prerequisites

### Hardware
- RP2040, RP2350, or ESP32-S3 board
- Addressable LED strip (WS2812B, SK6812, APA102, etc.)
- 5V or 12V power supply (depending on LED strip)
- 1000µF capacitor (recommended)
- 470Ω resistor (recommended for data line)

### Software
- PlatformIO or Arduino IDE
- OpenKNX framework (if using OpenKNX integration)

---

## Installation

### PlatformIO

1. Add to your `platformio.ini`:

```ini
[env:your_board]
platform = raspberrypi  ; or espressif32
board = pico            ; your board
framework = arduino

lib_deps =
    https://github.com/OpenKNX/OFM-NeoPixel.git

build_flags =
    -DNEOPIXEL_MODULE
```

2. Include in your `main.cpp`:

```cpp
#ifdef NEOPIXEL_MODULE
    #include "NeoPixel.h"
#endif
```

### Wiring

#### 1-Wire LED Strip (WS2812B, SK6812)

```
Board               LED Strip
─────               ─────────
GPIO 9    ──────────► DIN
GND       ──────────► GND
5V        ──────────► VCC
```

Add between VCC and GND:
- 1000µF capacitor (near strip)
- 470Ω resistor on DIN line (optional but recommended)

#### SPI LED Strip (APA102)

```
Board               LED Strip
─────               ─────────
SPI MOSI  ──────────► DI
SPI SCK   ──────────► CI
GND       ──────────► GND
5V        ──────────► VCC
```

---

## Your First LED Strip

### Minimal Example

```cpp
#include "OpenKNX.h"
#include "NeoPixel.h"

void setup() {
    // Initialize OpenKNX
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Add LED strip (GPIO 9, 64 LEDs, WS2812B)
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    // Set first 10 LEDs to red
    for (int i = 0; i < 10; i++) {
        strip->setPixel(i, 255, 0, 0);
    }
    
    // Update the strip
    strip->show();
}

void loop() {
    openknx.loop();
}
```

### With Auto-Update

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    // Enable auto-update at 20 FPS
    neoPixelModule.setUpdateSpeed(UpdateSpeed::NORMAL);
    neoPixelModule.setAutoUpdate(true);
    
    // Set all LEDs to blue
    strip->setAll(0, 0, 255);
}

void loop() {
    openknx.loop();  // Automatically updates LEDs
}
```

---

## Multi-Strip Setup

### Two Physical Strips

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Add two physical strips
    auto strip0 = neoPixelModule.addStrip(9, 8, LedProtocol::WS2812B);
    auto strip1 = neoPixelModule.addStrip(22, 64, LedProtocol::WS2812B);
    
    // Different colors
    strip0->setAll(255, 0, 0);    // Red
    strip1->setAll(0, 255, 0);    // Green
    
    // Update both
    neoPixelModule.updateAll();
}

void loop() {
    openknx.loop();
}
```

### Virtual Strip (Combining Multiple Physical Strips)

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    // Get manager for advanced features
    auto mgr = neoPixelModule.getManager();
    
    // Add two physical strips
    auto phys0 = mgr->addStrip(9, 8, LedProtocol::WS2812B);
    auto phys1 = mgr->addStrip(22, 64, LedProtocol::WS2812B);
    
    // Create virtual strip combining both (72 LEDs total)
    auto vstrip = mgr->addVirtualStrip(72, ColorOrder::GRB);
    
    // Attach physical strips to virtual
    mgr->attachPhysicalToVirtual(vstrip, phys0, 0);    // Offset 0-7
    mgr->attachPhysicalToVirtual(vstrip, phys1, 8);    // Offset 8-71
    
    // Now you can control all 72 LEDs as one strip
    vstrip->setPixel(0, 255, 0, 0);    // First LED on phys0
    vstrip->setPixel(10, 0, 255, 0);   // Third LED on phys1
    
    // Sync and show
    vstrip->syncToPhysical();
    vstrip->show();
}

void loop() {
    openknx.loop();
}
```

---

## Adding Effects

### Basic Effect

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto mgr = neoPixelModule.getManager();
    
    // Create strip and virtual
    auto phys = mgr->addStrip(9, 64, LedProtocol::WS2812B);
    auto vstrip = mgr->addVirtualStrip(64, ColorOrder::GRB);
    mgr->attachPhysicalToVirtual(vstrip, phys, 0);
    
    // Create segment covering entire strip
    auto segment = mgr->addSegment(vstrip, 0, 63);
    
    // Attach rainbow effect
    mgr->attachEffect(segment, EffectPool::getEffect(1));  // 1 = Rainbow
    
    // Enable auto-update
    neoPixelModule.setUpdateSpeed(UpdateSpeed::FAST);
    neoPixelModule.setAutoUpdate(true);
}

void loop() {
    openknx.loop();  // Updates effect automatically
}
```

### Multiple Effects on Different Segments

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto mgr = neoPixelModule.getManager();
    
    // Create strip
    auto phys = mgr->addStrip(9, 64, LedProtocol::WS2812B);
    auto vstrip = mgr->addVirtualStrip(64, ColorOrder::GRB);
    mgr->attachPhysicalToVirtual(vstrip, phys, 0);
    
    // Create two segments
    auto seg1 = mgr->addSegment(vstrip, 0, 31);    // First half
    auto seg2 = mgr->addSegment(vstrip, 32, 63);   // Second half
    
    // Different effects
    mgr->attachEffect(seg1, EffectPool::getEffect(1));  // Rainbow
    mgr->attachEffect(seg2, EffectPool::getEffect(6));  // Cylon
    
    // Configure effects
    seg1->getConfig().speed = 150;
    seg2->getConfig().speed = 200;
    seg2->getConfig().primaryRGBW = 0xFF0000FF;  // Red
    
    // Enable auto-update
    neoPixelModule.setUpdateSpeed(UpdateSpeed::FAST);
    neoPixelModule.setAutoUpdate(true);
}

void loop() {
    openknx.loop();
}
```

### Available Effects

| ID | Effect | Description |
|----|--------|-------------|
| 0 | Solid | Solid color |
| 1 | Rainbow | Rainbow cycle |
| 2 | Pride2015 | Pride colors |
| 3 | Confetti | Random colored pixels |
| 4 | Juggle | Moving dots |
| 5 | BPM | Pulsing to BPM |
| 6 | Cylon | Bouncing pixel |
| 7 | Wipe | Color wipe |

---

## Console Configuration

Instead of code, you can configure everything via console commands.

### Step-by-Step Console Setup

#### Example 1: Mixed Protocols (WS2812B + APA102)

This example shows how to combine different LED protocols:

```bash
# Add physical strips
neo phys add 22 64 WS2812B      # GPIO 22, 64 LEDs, WS2812B
neo phys add 7 64 WS2812B       # GPIO 7, 64 LEDs, WS2812B
neo phys add 8 40 APA102 9      # GPIO 8 (CLK), GPIO 9 (MOSI), 40 LEDs, APA102

# Create virtual strips
neo virt add 128 GRB            # 128 LEDs total, GRB color order (WS2812B)
neo virt add 40 BGR             # 40 LEDs, BGR color order (APA102)

# Attach physical to virtual
neo virt attach 0 0             # VirtStrip[0] <- PhysStrip[0] (GPIO22)
neo virt attach 0 1             # VirtStrip[0] <- PhysStrip[1] (GPIO7)
neo virt attach 1 2             # VirtStrip[1] <- PhysStrip[2] (APA102)

# Create segments
neo seg add 0 0 127             # Segment[0]: All WS2812B LEDs
neo seg add 1 0 39              # Segment[1]: All APA102 LEDs

# Set colors
neo color 0 21 0 0              # Dark red on WS2812B segment
neo color 1 21 0 0              # Dark red on APA102 segment

# Assign effects
neo effect 0 0                  # Solid color on Segment[0]
neo effect 1 0                  # Solid color on Segment[1]

# Enable auto-update
neo auto on                     # Start updating at 20 FPS
```

**Expected Output:**

```
═══════════════════════════════════════════════════════════════
  NeoPixel System Information
═══════════════════════════════════════════════════════════════

System Status:
  Initialized:     Yes
  Total Strips:    3
  Active Strips:   3
  Total LEDs:      168
  Memory Usage:    552 bytes
  Auto Update:     Enabled
  Update Interval: 50 ms

Available Resources:
  PIO State Machines: 4 available / 8 total
  DMA Channels:       8 available / 12 total

Physical Strips:
  [0] GPIO22: 64 LEDs - OK
      Hardware: PIO1/SM1, DMA Ch1
      Protocol: 800kHz RGB (GRB)
  [1] GPIO7: 64 LEDs - OK
      Hardware: PIO1/SM2, DMA Ch2
      Protocol: 800kHz RGB (GRB)
  [2] CLK:GPIO8, MOSI:GPIO9: 40 LEDs - OK
      Hardware: PIO1/SM3 (SPI), DMA Ch3
      Protocol: APA102, 10MHz (BGR)

Virtual Strips:
  [0] 128 LEDs - 2 Physical Strips attached:
      - PhysStrip[0] GPIO22 (64 LEDs)
      - PhysStrip[1] GPIO7 (64 LEDs)
  [1] 40 LEDs - 1 Physical Strip attached:
      - PhysStrip[2] GPIO9 (40 LEDs)

Segments:
  [0] 128 LEDs → Effect: Solid
      Virtual Strip[0], Range 0-127
  [1] 40 LEDs → Effect: Solid
      Virtual Strip[1], Range 0-39

Statistics:
  Total LEDs:      168
  Memory Usage:    552 bytes
  Updates:         119518
  Errors:          0
  Auto Update:     Enabled
  Target FPS:      20 Hz (50 ms)
  Actual FPS:      20.0 Hz
  Avg Update:      641 µs
  CPU Usage:       1.28%
  DMA Accel:       Active
═══════════════════════════════════════════════════════════════
```

#### Example 2: Simple Setup (WS2812B Only)

```bash
# Add physical strips
neo phys add 9 8 WS2812B        # GPIO 9, 8 LEDs
neo phys add 22 64 WS2812B      # GPIO 22, 64 LEDs
neo phys list                   # Verify

# Create virtual strip
neo virt add 72 GRB             # 72 LEDs total, GRB order
neo virt list                   # Verify

# Attach physical to virtual
neo virt attach 0 0             # VirtStrip[0] <- PhysStrip[0] @ offset 0
neo virt attach 0 1             # VirtStrip[0] <- PhysStrip[1] @ offset 8
neo virt list                   # Verify mappings

# Create segments
neo seg add 0 0 35              # Segment[0]: LEDs 0-35
neo seg add 0 36 71             # Segment[1]: LEDs 36-71
neo seg list                    # Verify

# Add effects
neo effect 0 1                  # Rainbow on Segment[0]
neo effect 1 6                  # Cylon on Segment[1]
neo color 1 255 0 0             # Red for Cylon
neo brightness 0 200            # Dim rainbow slightly

# Enable auto-update
neo speed fast                  # 30 FPS
neo auto on                     # Enable auto-update
neo perf                        # Check performance
```

#### Example 3: SK6812 RGBW Strip

```bash
# Add SK6812 RGBW strip (4-channel)
neo phys add 5 50 SK6812        # GPIO 5, 50 LEDs, SK6812 RGBW

# Create virtual strip with RGBW support
neo virt add 50 GRBW            # 50 LEDs, GRBW color order (SK6812)

# Attach physical to virtual
neo virt attach 0 0             # VirtStrip[0] <- PhysStrip[0]

# Create segment
neo seg add 0 0 49              # Segment[0]: All LEDs

# Assign effect
neo effect 0 1                  # Rainbow effect

# Enable auto-update
neo auto on
```

### Quick Test

```bash
neo test                        # Run built-in animation test
```

---

## Common Patterns

### Pattern 1: Single Strip with Solid Color

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    strip->setAll(255, 128, 0);  // Orange
    strip->show();
}
```

### Pattern 2: Alternating Colors

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    for (int i = 0; i < 64; i++) {
        if (i % 2 == 0) {
            strip->setPixel(i, 255, 0, 0);    // Red
        } else {
            strip->setPixel(i, 0, 0, 255);    // Blue
        }
    }
    
    strip->show();
}
```

### Pattern 3: Progressive Brightness

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    for (int i = 0; i < 64; i++) {
        uint8_t brightness = (i * 255) / 63;  // 0 to 255
        strip->setPixel(i, brightness, 0, brightness);  // Purple fade
    }
    
    strip->show();
}
```

### Pattern 4: Rainbow Static

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
    
    for (int i = 0; i < 64; i++) {
        uint8_t hue = (i * 256) / 64;  // 0-255
        
        // Simple HSV to RGB conversion (approximation)
        uint8_t r, g, b;
        if (hue < 85) {
            r = 255 - hue * 3;
            g = hue * 3;
            b = 0;
        } else if (hue < 170) {
            hue -= 85;
            r = 0;
            g = 255 - hue * 3;
            b = hue * 3;
        } else {
            hue -= 170;
            r = hue * 3;
            g = 0;
            b = 255 - hue * 3;
        }
        
        strip->setPixel(i, r, g, b);
    }
    
    strip->show();
}
```

### Pattern 5: Different Protocols (WS2812B + APA102 + SK6812)

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto mgr = neoPixelModule.getManager();
    
    // 1-Wire WS2812B (GRB order)
    auto strip1 = mgr->addStrip(9, 64, LedProtocol::WS2812B);
    
    // SPI APA102 (BGR order, with clock pin)
    auto strip2 = mgr->addSpiStrip(11, 10, 40, LedProtocol::APA102);
    
    // 1-Wire SK6812 RGBW (GRBW order, 4-channel)
    auto strip3 = mgr->addStrip(5, 30, LedProtocol::SK6812);
    
    // Create virtual strips with appropriate color orders
    auto vstrip1 = mgr->addVirtualStrip(64, ColorOrder::GRB);   // WS2812B
    auto vstrip2 = mgr->addVirtualStrip(40, ColorOrder::BGR);   // APA102
    auto vstrip3 = mgr->addVirtualStrip(30, ColorOrder::GRBW);  // SK6812 RGBW
    
    // Attach physical to virtual
    mgr->attachPhysicalToVirtual(vstrip1, strip1, 0);
    mgr->attachPhysicalToVirtual(vstrip2, strip2, 0);
    mgr->attachPhysicalToVirtual(vstrip3, strip3, 0);
    
    // Different colors for each protocol
    vstrip1->setAll(255, 0, 0);      // WS2812B: Red
    
    vstrip2->setBrightness(128);     // APA102: Set hardware brightness
    vstrip2->setAll(0, 255, 0);      // APA102: Green
    
    // SK6812 RGBW: Pure white using W channel
    for (int i = 0; i < 30; i++) {
        vstrip3->setPixel(i, 0, 0, 0, 255);  // R=0, G=0, B=0, W=255
    }
    
    // Sync and update
    vstrip1->syncToPhysical();
    vstrip2->syncToPhysical();
    vstrip3->syncToPhysical();
    
    neoPixelModule.updateAll();
}
```

### Pattern 6: APA102 with Hardware Brightness

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto mgr = neoPixelModule.getManager();
    
    // APA102 strip (CLK=GPIO8, MOSI=GPIO9)
    auto strip = mgr->addSpiStrip(8, 9, 50, LedProtocol::APA102);
    auto vstrip = mgr->addVirtualStrip(50, ColorOrder::BGR);
    mgr->attachPhysicalToVirtual(vstrip, strip, 0);
    
    // APA102 supports per-LED hardware brightness (0-31)
    // This is more efficient than software dimming
    
    for (int i = 0; i < 50; i++) {
        // Progressive brightness fade
        uint8_t brightness = (i * 255) / 49;
        vstrip->setBrightness(brightness);  // Hardware brightness
        vstrip->setPixel(i, 255, 0, 0);     // Full red color
    }
    
    vstrip->syncToPhysical();
    vstrip->show();
}
```

### Pattern 7: Multi-Protocol Animation

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    auto mgr = neoPixelModule.getManager();
    
    // WS2812B strips
    auto ws1 = mgr->addStrip(22, 64, LedProtocol::WS2812B);
    auto ws2 = mgr->addStrip(7, 64, LedProtocol::WS2812B);
    
    // APA102 strip
    auto apa = mgr->addSpiStrip(8, 9, 40, LedProtocol::APA102);
    
    // Virtual strips
    auto vWS = mgr->addVirtualStrip(128, ColorOrder::GRB);
    auto vAPA = mgr->addVirtualStrip(40, ColorOrder::BGR);
    
    // Attach
    mgr->attachPhysicalToVirtual(vWS, ws1, 0);
    mgr->attachPhysicalToVirtual(vWS, ws2, 64);
    mgr->attachPhysicalToVirtual(vAPA, apa, 0);
    
    // Create segments
    auto seg1 = mgr->addSegment(vWS, 0, 127);   // WS2812B segment
    auto seg2 = mgr->addSegment(vAPA, 0, 39);   // APA102 segment
    
    // Different effects for different protocols
    mgr->attachEffect(seg1, EffectPool::getEffect(1));  // Rainbow on WS2812B
    mgr->attachEffect(seg2, EffectPool::getEffect(6));  // Cylon on APA102
    
    // Configure
    seg1->getConfig().speed = 150;
    seg2->getConfig().speed = 200;
    seg2->getConfig().primaryRGBW = 0xFF0000FF;  // Red
    seg2->getConfig().apa102Brightness = 128;    // 50% hardware brightness
    
    // Enable auto-update
    neoPixelModule.setUpdateSpeed(UpdateSpeed::FAST);
    neoPixelModule.setAutoUpdate(true);
}

void loop() {
    openknx.loop();  // Automatically updates both protocols
}
```

---

## Next Steps

### Learn More

- **[Developer API Reference](Developer.md)** - Complete API documentation
- **[Architecture Guide](Architecture.md)** - Understand the internal structure
- **[Effects Porting Guide](Effects-Porting.md)** - Create custom effects

### Advanced Topics

- **Custom Effects:** Implement your own animations using the Effect base class
- **Performance Tuning:** Optimize for your specific hardware and LED count
- **Multi-Platform:** Deploy the same code on RP2040, RP2350, and ESP32-S3

### Console Mastery

Explore all console commands:
```bash
neo ?                           # Show all commands
neo help                        # Detailed help
```

### Troubleshooting

**No LEDs lighting up?**
- Check power supply voltage (5V or 12V)
- Verify wiring (DIN, GND, VCC)
- Try console command: `neo phys list` to see detected strips
- Test with: `neo test`

**Wrong colors?**
- Check color order: `neo virt add 64 RGB` vs `neo virt add 64 GRB`
- Different strips use different orders (WS2812B = GRB, APA102 = RGB)

**Flickering?**
- Add capacitor (1000µF) between VCC and GND
- Add resistor (470Ω) on data line
- Reduce update speed: `neo speed slow`

---

## Resources

- **GitHub:** https://github.com/OpenKNX/OFM-NeoPixel
- **Issues:** https://github.com/OpenKNX/OFM-NeoPixel/issues
- **OpenKNX:** https://github.com/OpenKNX

---

**Happy LED Controlling!**

Last Updated: November 2025
