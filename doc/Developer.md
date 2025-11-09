# OFM-NeoPixel Developer Documentation

**Version:** 0.0.1  
**Date:** November 2025  
**Author:** Erkan Colak

Complete API reference and development guide for OFM-NeoPixel library.

---

## Related Documentation

- **[README](../README.md)** - Project overview and features
- **[Quickstart Guide](Quickstart.md)** - Get started quickly
- **[Architecture & Flow Diagrams](Architecture.md)** - Detailed system architecture
- **[Effects Porting Guide](Effects-Porting.md)** - Port FastLED effects

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Module Integration](#module-integration)
- [API Reference](#api-reference)
  - [NeoPixel Module](#neopixel-module)
  - [NeoPixelManager](#neopixelmanager)
  - [PhysicalStrip](#physicalstrip)
  - [VirtualStrip](#virtualstrip)
  - [Segment](#segment)
  - [Effect System](#effect-system)
- [Console Commands](#console-commands)
- [Testing & Benchmarking](#testing--benchmarking)
- [Build Configuration](#build-configuration)
- [Advanced Topics](#advanced-topics)

---

## Architecture Overview

### Layer Structure

```
Application Layer (main.cpp)
    │
    └──> NeoPixel Module (OpenKNX integration)
            │
            └──> NeoPixelManager (Core orchestration)
                    │
                    ├──> PhysicalStrip (Hardware abstraction)
                    │       └──> IHardwareDriver (PIO/RMT/SPI)
                    │
                    ├──> VirtualStrip (Multi-strip aggregation)
                    │
                    └──> Segment (Effect zones)
                            └──> Effect (Animations)
```

### File Organization

```
src/
├── NeoPixel.h/cpp               # OpenKNX Module wrapper
├── NeoPixelConsole.cpp          # Console command handlers
├── NeoPixelManager.h/cpp        # Core manager
├── PhysicalStrip.h/cpp          # Hardware strip wrapper
├── VirtualStrip.h/cpp           # Multi-strip composition
├── Segment.h/cpp                # LED range + effect
├── LedState.h                   # State machine
├── IHardwareDriver.h            # Driver interface
│
├── effects/
│   ├── Effect.h                 # Base class
│   ├── EffectPool.h/cpp         # Effect registry
│   └── (various effects)
│
├── hal/
│   ├── DriverFactory.h          # Auto driver selection
│   └── HW_NeoPixel_SPI.h        # Hardware SPI driver
│
├── pio/                         # RP2040/RP2350 PIO drivers
└── rmt/                         # ESP32 RMT drivers
```

---

## Module Integration

### OpenKNX Integration

OFM-NeoPixel integrates with OpenKNX through the Module interface.

#### Activation

In `platformio.ini`:

```ini
build_flags =
    -DNEOPIXEL_MODULE                 # Enable NeoPixel module
    -DOPENKNX_NEOPIXEL_TESTS          # Optional: Enable test system
    -DOPENKNX_NEOPIXEL_BENCHMARK      # Optional: Enable benchmarks
```

#### Module Registration

In `main.cpp`:

```cpp
#ifdef NEOPIXEL_MODULE
    #include "NeoPixel.h"
#endif

void setup() {
    openknx.init(0);
    
    #ifdef NEOPIXEL_MODULE
        openknx.addModule(13, neoPixelModule);  // Module ID 13
    #endif
    
    openknx.setup();
}

void loop() {
    openknx.loop();  // Calls neoPixelModule.loop() automatically
}
```

The module is automatically instantiated as a global `neoPixelModule` object.

---

## API Reference

### NeoPixel Module

Main module class providing OpenKNX integration and high-level API.

#### Header

```cpp
#include "NeoPixel.h"
```

#### Module Interface

```cpp
class NeoPixel : public OpenKNX::Module
{
  public:
    // OpenKNX Module interface
    const std::string name() override;           // Returns "NeoPixel"
    const std::string version() override;        // Returns "1.0.0"
    
    void init();                                 // Initialize module
    void setup(bool configured) override;        // Setup LED strips
    void loop(bool configured) override;         // Auto-update loop
    
    void processInputKo(GroupObject& ko) override;  // Process GroupObjects
    
    void showHelp() override;                    // Console help
    bool processCommand(const std::string command, bool diagnose) override;
};
```

#### Strip Management

```cpp
// Add physical strip (1-wire protocol)
PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, 
                       LedProtocol protocol = LedProtocol::WS2812B);

// Add virtual strip
VirtualStrip* addVirtualStrip(uint16_t totalLeds, 
                             ColorOrder colorOrder = ColorOrder::GRB);
```

**Example:**

```cpp
// Single strip
auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);

// Multi-strip with virtual
neoPixelModule.addStrip(9, 8, LedProtocol::WS2812B);     // Physical 0
neoPixelModule.addStrip(22, 64, LedProtocol::WS2812B);   // Physical 1
auto vstrip = neoPixelModule.addVirtualStrip(72, ColorOrder::GRB);
```

#### Update Control

```cpp
void updateAll();                     // Manual update all strips
void clearAll();                      // Clear all LEDs
void setUpdateSpeed(UpdateSpeed speed); // Set update interval
void setAutoUpdate(bool enabled);     // Enable/disable auto-update

bool getAutoUpdate() const;           // Get auto-update state
uint32_t getUpdateInterval() const;   // Get update interval (ms)
```

**Update Speed Presets:**

```cpp
enum class UpdateSpeed : uint8_t
{
    SLOW = 100,      // 10 FPS
    NORMAL = 50,     // 20 FPS (default)
    FAST = 33,       // 30 FPS
    MAX = 20,        // 50 FPS
    EXTREME = 12,    // 80 FPS
    LUDICROUS = 4,   // 120 FPS
    FTL = 0          // 240 FPS
};
```

**Example:**

```cpp
void setup() {
    // ...
    neoPixelModule.setUpdateSpeed(UpdateSpeed::FAST);  // 30 FPS
    neoPixelModule.setAutoUpdate(true);                // Auto-update in loop()
}
```

#### Information

```cpp
bool isInitialized() const;           // Initialization status
uint32_t getStripCount() const;       // Number of physical strips
uint32_t getTotalLeds() const;        // Total LED count
NeoPixelManager* getManager();        // Access core manager
```

---

### NeoPixelManager

Core manager orchestrating all strips, virtual strips, and segments.

#### Header

```cpp
#include "NeoPixelManager.h"
```

#### Resource Limits

Configure in `platformio.ini`:

```ini
build_flags =
    -DNEOPIXEL_MAX_PHYSICAL_STRIPS=12   # Max physical strips
    -DNEOPIXEL_MAX_VIRTUAL_STRIPS=6     # Max virtual strips
    -DNEOPIXEL_MAX_SEGMENTS=32          # Max segments
    -DNEOPIXEL_ENFORCE_LIMITS=1         # Enable enforcement
```

#### Physical Strip Management

```cpp
// Add 1-wire strip
PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, 
                       LedProtocol protocol = LedProtocol::WS2812B);

// Add 1-wire strip with specific driver
PhysicalStrip* addStrip(uint32_t pin, uint16_t ledCount, 
                       LedProtocol protocol, DriverType driverType);

// Add SPI strip
PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, 
                          uint16_t ledCount, LedProtocol protocol);

// Add SPI strip with specific driver
PhysicalStrip* addSpiStrip(uint32_t mosiPin, uint32_t sckPin, 
                          uint16_t ledCount, LedProtocol protocol, 
                          DriverType driverType);

// Remove strip
bool removeStrip(PhysicalStrip* strip);

// Access strips
PhysicalStrip* getStrip(uint32_t index);
PhysicalStrip* findStripByPin(uint32_t pin);
uint32_t getStripCount() const;
```

**Example:**

```cpp
auto mgr = neoPixelModule.getManager();

// Auto driver selection
auto strip1 = mgr->addStrip(9, 64, LedProtocol::WS2812B);

// Force PIO driver on RP2040
auto strip2 = mgr->addStrip(22, 100, LedProtocol::WS2812B, 
                           DriverType::SERIAL_1WIRE);

// SPI strip (APA102)
auto strip3 = mgr->addSpiStrip(11, 10, 50, LedProtocol::APA102);
```

#### Virtual Strip Management

```cpp
// Create virtual strip
VirtualStrip* addVirtualStrip(uint16_t totalLeds, 
                             ColorOrder colorOrder = ColorOrder::RGB);

// Remove virtual strip
bool removeVirtualStrip(VirtualStrip* vstrip);

// Access virtual strips
VirtualStrip* getVirtualStrip(uint32_t index);
uint32_t getVirtualStripCount() const;

// Attach/detach physical strips
bool attachPhysicalToVirtual(VirtualStrip* vstrip, PhysicalStrip* pstrip, 
                            uint16_t offset);
bool detachPhysicalFromVirtual(VirtualStrip* vstrip, PhysicalStrip* pstrip);
```

**Example:**

```cpp
// Create two physical strips
auto phys0 = mgr->addStrip(3, 100, LedProtocol::WS2812B);
auto phys1 = mgr->addStrip(7, 100, LedProtocol::WS2812B);

// Create virtual strip combining both
auto vstrip = mgr->addVirtualStrip(200, ColorOrder::GRB);

// Attach physical strips
mgr->attachPhysicalToVirtual(vstrip, phys0, 0);    // Offset 0
mgr->attachPhysicalToVirtual(vstrip, phys1, 100);  // Offset 100

// Now vstrip[0-99] maps to phys0, vstrip[100-199] maps to phys1
```

#### Segment Management

```cpp
// Create segment
Segment* addSegment(VirtualStrip* vstrip, uint16_t startLed, uint16_t endLed);

// Remove segment
bool removeSegment(Segment* segment);

// Access segments
Segment* getSegment(uint32_t index);
uint32_t getSegmentCount() const;

// Effect assignment
bool attachEffect(Segment* segment, Effect* effect);
bool detachEffect(Segment* segment);
```

**Example:**

```cpp
// Create segments on virtual strip
auto seg1 = mgr->addSegment(vstrip, 0, 99);     // First half
auto seg2 = mgr->addSegment(vstrip, 100, 199);  // Second half

// Attach different effects
mgr->attachEffect(seg1, new EffectSolid(255, 0, 0));  // Red
mgr->attachEffect(seg2, new EffectRainbow());         // Rainbow
```

#### Initialization & Control

```cpp
bool init();                          // Initialize all strips
bool isInitialized() const;           // Check initialization
void reset();                         // Reset all strips
uint32_t getErrorCount() const;       // Get error count
```

#### Update Control

```cpp
void update(uint32_t deltaTime);      // Update effects (deltaTime in ms)
bool updateAll();                     // Send buffers to hardware
bool waitForAll(uint32_t timeoutMs = 0);     // Wait for transfers
bool waitForStrip(PhysicalStrip* strip, uint32_t timeoutMs = 0);
bool isAnyBusy() const;               // Check if any strip busy
bool areAllReady() const;             // Check if all strips ready
```

**Example:**

```cpp
void loop() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    uint32_t deltaTime = now - lastUpdate;
    
    // Update effects
    mgr->update(deltaTime);
    
    // Send to hardware
    mgr->updateAll();
    
    lastUpdate = now;
    delay(20);  // 50 FPS
}
```

#### Information

```cpp
uint32_t getTotalLedCount() const;    // Total LEDs across all strips
void printStats();                    // Print statistics to console
```

---

### PhysicalStrip

Hardware abstraction for a single physical LED strip.

#### Header

```cpp
#include "PhysicalStrip.h"
```

#### Constructor

```cpp
// 1-wire protocol
PhysicalStrip(uint32_t pin, uint16_t ledCount, 
             LedProtocol protocol = LedProtocol::WS2812B, 
             DriverType driverType = DriverType::AUTO);

// SPI protocol
PhysicalStrip(uint32_t pin, uint16_t ledCount, 
             LedProtocol protocol, uint32_t sckPin, 
             DriverType driverType = DriverType::AUTO);
```

#### Initialization

```cpp
bool init();                          // Initialize hardware driver
bool isInitialized() const;           // Check initialization status
```

#### Pixel Control

```cpp
// RGB (3-channel)
bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void setAll(uint8_t r, uint8_t g, uint8_t b);

// RGBW (4-channel)
bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

// Clear
void clear();
```

**Example:**

```cpp
auto strip = mgr->addStrip(9, 64, LedProtocol::WS2812B);
strip->init();

strip->setPixel(0, 255, 0, 0);      // First LED red
strip->setPixel(1, 0, 255, 0);      // Second LED green
strip->setAll(0, 0, 255);           // All blue
strip->clear();                     // All off
```

#### Display Control

```cpp
bool show();                          // Send buffer to LEDs (non-blocking)
bool waitForTransfer(uint32_t timeoutMs = 0);  // Wait for completion
bool isBusy() const;                  // Check if transfer in progress
```

**Example:**

```cpp
strip->setPixel(0, 255, 0, 0);
strip->show();                        // Start DMA transfer

// Non-blocking - can continue immediately
doOtherWork();

// Optional: wait for completion
strip->waitForTransfer(100);          // 100ms timeout
```

#### Buffer Access

```cpp
uint8_t* getBuffer();                 // Direct buffer access
size_t getBufferSize() const;         // Buffer size in bytes
```

**Example:**

```cpp
// Direct buffer manipulation
uint8_t* buf = strip->getBuffer();
size_t size = strip->getBufferSize();

// Manual RGB fill (assuming GRB order)
for (size_t i = 0; i < size; i += 3) {
    buf[i] = 128;      // G
    buf[i + 1] = 255;  // R
    buf[i + 2] = 0;    // B
}
strip->show();
```

#### Information

```cpp
uint16_t getLedCount() const;         // Number of LEDs
LedProtocol getProtocol() const;      // Protocol type
uint32_t getDataPin() const;          // Data pin (MOSI for SPI)
uint32_t getClockPin() const;         // Clock pin (SPI only)
DriverCapabilities getCapabilities() const;  // Driver capabilities
const char* getDriverName() const;    // Driver name string
```

#### Advanced

```cpp
bool setUpdateFrequency(uint32_t frequencyHz);  // Set update frequency
IHardwareDriver* getDriver() const;             // Access raw driver
```

---

### VirtualStrip

Logical strip combining multiple physical strips.

#### Header

```cpp
#include "VirtualStrip.h"
```

#### Constructor

```cpp
VirtualStrip(uint16_t totalLeds, ColorOrder colorOrder = ColorOrder::RGB);
```

**Color Order Options:**

```cpp
enum class ColorOrder : uint8_t
{
    RGB,    // Red, Green, Blue
    RBG,    // Red, Blue, Green
    GRB,    // Green, Red, Blue (WS2812B default)
    GBR,    // Green, Blue, Red
    BRG,    // Blue, Red, Green
    BGR,    // Blue, Green, Red
    RGBW,   // Red, Green, Blue, White
    GRBW,   // Green, Red, Blue, White (SK6812 default)
    // ... more variants
};
```

#### Physical Strip Management

```cpp
// Attach physical strip
bool attachPhysicalStrip(PhysicalStrip* physicalStrip, uint16_t offset);

// Detach physical strip
bool detachPhysicalStrip(PhysicalStrip* physicalStrip);

// Access
uint16_t getPhysicalStripCount() const;
PhysicalStrip* getPhysicalStrip(uint16_t index) const;
```

**Example:**

```cpp
auto phys0 = mgr->addStrip(3, 50, LedProtocol::WS2812B);
auto phys1 = mgr->addStrip(7, 50, LedProtocol::WS2812B);

auto vstrip = new VirtualStrip(100, ColorOrder::GRB);
vstrip->attachPhysicalStrip(phys0, 0);    // vstrip[0-49] -> phys0
vstrip->attachPhysicalStrip(phys1, 50);   // vstrip[50-99] -> phys1
```

#### Pixel API

```cpp
// Set brightness (for APA102)
void setBrightness(uint8_t brightness);
uint8_t getBrightness() const;

// Set pixels (RGB)
bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void setRange(uint16_t startIndex, uint16_t length, uint8_t r, uint8_t g, uint8_t b);
void setAll(uint8_t r, uint8_t g, uint8_t b);
void clear();

// Set pixels (RGBW)
bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

// Get pixels
bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b) const;
bool getPixel(uint16_t index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;
```

**Example:**

```cpp
vstrip->setPixel(0, 255, 0, 0);       // First LED on phys0
vstrip->setPixel(75, 0, 255, 0);      // LED 25 on phys1
vstrip->setRange(10, 5, 0, 0, 255);   // 5 blue LEDs starting at 10
vstrip->setAll(128, 128, 128);        // All gray
```

#### Buffer Access

```cpp
uint8_t* getBuffer();                 // Unified virtual buffer
const uint8_t* getBuffer() const;
size_t getBufferSize() const;         // Buffer size
uint8_t getBytesPerLed() const;       // Bytes per LED (3 or 4)
```

#### Sync & Transfer

```cpp
// Sync virtual buffer to physical buffers
bool syncToPhysical();

// Show on all physical strips
bool show();

// Wait for completion
bool waitForCompletion(uint32_t timeoutMs = 0);
```

**Example:**

```cpp
// Modify virtual buffer
vstrip->setPixel(0, 255, 0, 0);
vstrip->setPixel(60, 0, 255, 0);

// Sync to physical buffers
vstrip->syncToPhysical();

// Send to hardware
vstrip->show();
```

#### Information

```cpp
uint16_t getLedCount() const;         // Total virtual LEDs
ColorOrder getColorOrder() const;     // Color order
bool isDirty() const;                 // Buffer modified since sync
void markDirty();                     // Mark buffer as modified
void clearDirty();                    // Clear dirty flag
```

---

### Segment

LED range within a VirtualStrip with its own effect.

#### Header

```cpp
#include "Segment.h"
```

#### Constructor

```cpp
Segment(VirtualStrip* virtualStrip, uint16_t startLed, uint16_t endLed);
```

#### Properties

```cpp
uint16_t getStartLed() const;         // Start LED index
uint16_t getEndLed() const;           // End LED index (inclusive)
uint16_t getLength() const;           // Segment length
VirtualStrip* getVirtualStrip() const;  // Parent virtual strip
```

#### Effect Management

```cpp
void setEffect(Effect* effect);       // Assign effect
Effect* getEffect() const;            // Get current effect
bool hasEffect() const;               // Check if effect assigned
void clearEffect();                   // Remove effect
```

**Example:**

```cpp
auto seg = mgr->addSegment(vstrip, 0, 49);
seg->setEffect(new EffectRainbow());
```

#### State Management

```cpp
LedState getLedState() const;         // Get state (IDLE, RUNNING, etc.)
void setState(LedState state);        // Set state

bool isRunning() const;               // Check if effect running
bool isPaused() const;                // Check if paused

void pause();                         // Pause effect
void resume();                        // Resume effect
void stop();                          // Stop effect
void start();                         // Start effect
```

**LED States:**

```cpp
enum class LedState : uint8_t
{
    IDLE,                 // No effect
    EFFECT_RUNNING,       // Effect active
    TRANSITIONING,        // Switching effects
    ERROR                 // Error state
};
```

#### Effect Configuration

```cpp
EffectConfig& getConfig();            // Get effect config (mutable)
const EffectConfig& getConfig() const;  // Get effect config (const)
```

**EffectConfig Structure:**

```cpp
struct EffectConfig
{
    uint8_t speed;            // Speed 1-255
    uint8_t intensity;        // Intensity 1-255
    uint8_t brightness;       // Segment brightness 0-255
    uint8_t apa102Brightness; // APA102 hardware brightness
    uint32_t primaryRGBW;     // Primary color (packed RGBW)
    uint32_t secondaryRGBW;   // Secondary color (packed RGBW)
    uint8_t reverse;          // Reverse direction
    uint8_t count;            // Count parameter
    uint8_t fade;             // Fade amount
    uint8_t mode;             // Effect mode
    uint32_t option1;         // Additional parameter 1
    uint32_t option2;         // Additional parameter 2
};
```

**Example:**

```cpp
auto seg = mgr->addSegment(vstrip, 0, 49);
seg->setEffect(new EffectRainbow());

EffectConfig& config = seg->getConfig();
config.speed = 200;
config.brightness = 128;
config.primaryRGBW = 0xFF0000FF;  // Red
```

#### Update

```cpp
void update(uint32_t deltaTime);      // Update effect (called by manager)
```

#### Pixel Access

```cpp
bool setPixel(uint16_t localIndex, uint8_t r, uint8_t g, uint8_t b);
bool setPixel(uint16_t localIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
bool getPixel(uint16_t localIndex, uint8_t& r, uint8_t& g, uint8_t& b) const;
void fill(uint8_t r, uint8_t g, uint8_t b);
void clear();
```

**Note:** Pixel indices are relative to segment start (0 = startLed).

---

### Effect System

Base class for creating custom effects.

#### Header

```cpp
#include "effects/Effect.h"
```

#### Base Class

```cpp
class Effect
{
  public:
    virtual ~Effect() = default;
    
    // Main update function (REQUIRED)
    virtual void update(Segment* segment, EffectConfig& config, 
                       EffectState& state, uint32_t deltaTime) = 0;
    
    // Effect metadata
    virtual const char* getName() const = 0;
    virtual uint8_t getEffectId() const = 0;
    
    // Optional: initialization/cleanup
    virtual void init(Segment* segment, EffectConfig& config, EffectState& state) {}
    virtual void cleanup(Segment* segment, EffectState& state) {}
};
```

#### Creating Custom Effects

**Example: Solid Color Effect**

```cpp
class EffectSolid : public Effect
{
  public:
    EffectSolid(uint8_t r, uint8_t g, uint8_t b)
        : _r(r), _g(g), _b(b) {}
    
    void update(Segment* segment, EffectConfig& config, 
               EffectState& state, uint32_t deltaTime) override
    {
        // Fill segment with solid color
        segment->fill(_r, _g, _b);
    }
    
    const char* getName() const override { return "Solid"; }
    uint8_t getEffectId() const override { return 0; }
    
  private:
    uint8_t _r, _g, _b;
};
```

**Example: Animation Effect**

```cpp
class EffectChase : public Effect
{
  public:
    void update(Segment* segment, EffectConfig& config, 
               EffectState& state, uint32_t deltaTime) override
    {
        // Clear segment
        segment->clear();
        
        // Update position
        state.position = (state.position + config.speed / 50) % segment->getLength();
        
        // Set moving pixel
        uint8_t r = (config.primaryRGBW >> 24) & 0xFF;
        uint8_t g = (config.primaryRGBW >> 16) & 0xFF;
        uint8_t b = (config.primaryRGBW >> 8) & 0xFF;
        
        segment->setPixel(state.position, r, g, b);
    }
    
    const char* getName() const override { return "Chase"; }
    uint8_t getEffectId() const override { return 10; }
};
```

#### Effect Pool

Pre-registered effects accessible by ID.

```cpp
#include "effects/EffectPool.h"

// Get effect by ID
Effect* effect = EffectPool::getEffect(1);  // Rainbow

// Available effects:
// 0 = Solid
// 1 = Rainbow
// 2 = Pride2015
// 3 = Confetti
// 4 = Juggle
// 5 = BPM
// 6 = Cylon
// 7 = Wipe
```

---

## Console Commands

Complete reference for all `neo` console commands.

### Command Structure

```
neo <category> <action> [parameters]
```

### Core Commands

| Command | Description |
|---------|-------------|
| `neo` | Show module info |
| `neo ?` | Show help |
| `neo list` | List all strips, virtual strips, segments |
| `neo info` | Detailed system information |
| `neo perf` | Performance statistics |

### Physical Strip Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo phys add <pin> <leds> [protocol]` | Add physical strip | `neo phys add 9 64 WS2812B` |
| `neo phys del <index>` | Delete strip | `neo phys del 0` |
| `neo phys list` | List strips | `neo phys list` |

**Supported Protocols:**
- WS2812, WS2812B, WS2813, WS2815
- SK6812, SK6805
- APA102, WS2801

### Virtual Strip Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo virt add <leds> [order]` | Create virtual strip | `neo virt add 72 GRB` |
| `neo virt del <index>` | Delete virtual strip | `neo virt del 0` |
| `neo virt list` | List virtual strips | `neo virt list` |
| `neo virt attach <virt> <phys>` | Attach physical to virtual | `neo virt attach 0 0` |
| `neo virt detach <virt> <phys>` | Detach physical | `neo virt detach 0 0` |
| `neo virt order <virt> <phys> <order>` | Set mapping order | `neo virt order 0 0 1` |

**Color Orders:**
- RGB, RBG, GRB, GBR, BRG, BGR
- RGBW, GRBW, BRGW, BGRW, etc.

### Segment Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo seg add <virt> <start> <end>` | Create segment | `neo seg add 0 0 35` |
| `neo seg del <index>` | Delete segment | `neo seg del 0` |
| `neo seg list` | List segments | `neo seg list` |
| `neo seg pause <index>` | Pause effect | `neo seg pause 0` |
| `neo seg resume <index>` | Resume effect | `neo seg resume 0` |
| `neo seg stop <index>` | Stop effect | `neo seg stop 0` |

### Effect Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo effect <seg> <id>` | Assign effect | `neo effect 0 1` |
| `neo color <seg> <r> <g> <b>` | Set color | `neo color 0 255 0 0` |
| `neo brightness <seg> <value>` | Set brightness | `neo brightness 0 128` |

### Update Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo update` | Manual update | `neo update` |
| `neo clear` | Clear all LEDs | `neo clear` |
| `neo speed <ms>` | Set interval | `neo speed 50` |
| `neo speed slow/normal/fast/max` | Speed preset | `neo speed fast` |
| `neo auto on/off` | Auto-update | `neo auto on` |

### Test Commands

| Command | Description | Example |
|---------|-------------|---------|
| `neo test` | Animation test | `neo test` |
| `neo simpletest start` | Start simple test | `neo simpletest start` |
| `neo simpletest stop` | Stop simple test | `neo simpletest stop` |
| `neo animtest start` | Start animation test | `neo animtest start` |
| `neo animtest stop` | Stop animation test | `neo animtest stop` |

---

## Testing & Benchmarking

### Test System

Enable tests in `platformio.ini`:

```ini
build_flags =
    -DOPENKNX_NEOPIXEL_TESTS
    -DOPENKNX_NEOPIXEL_AUTO_TEST    # Auto-start on boot
```

#### Test Commands

```bash
neo test                    # Run animation test
neo animtest start          # Start animation test
neo animtest stop           # Stop animation test
neo simpletest start        # Start simple test
neo simpletest stop         # Stop simple test
```

### Benchmark System

Enable benchmarks:

```ini
build_flags =
    -DOPENKNX_NEOPIXEL_BENCHMARK
```

#### Benchmark Commands

```bash
neo benchmark run           # Run all benchmarks
neo benchmark speed [strip] # Update speed test
neo benchmark colors [strip]  # Color pattern test
neo benchmark size          # LED count scaling
neo benchmark compare       # Protocol comparison
neo benchmark stability [strip]  # Stability test
neo benchmark cpu [strip]   # CPU usage analysis
neo benchmark dma           # DMA comparison
```

---

## Build Configuration

### Feature Flags

```ini
build_flags =
    # Core
    -DNEOPIXEL_MODULE                  # Enable module
    
    # Resource limits
    -DNEOPIXEL_MAX_PHYSICAL_STRIPS=12
    -DNEOPIXEL_MAX_VIRTUAL_STRIPS=6
    -DNEOPIXEL_MAX_SEGMENTS=32
    -DNEOPIXEL_ENFORCE_LIMITS=1
    
    # Optional features
    -DOPENKNX_NEOPIXEL_TESTS           # Test system
    -DOPENKNX_NEOPIXEL_AUTO_TEST       # Auto-start tests
    -DOPENKNX_NEOPIXEL_BENCHMARK       # Benchmark system
```

### Platform-Specific

#### RP2040/RP2350

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino

lib_deps =
    OpenKNX/OFM-NeoPixel

build_flags =
    -DNEOPIXEL_MODULE
    -DARDUINO_ARCH_RP2040
```

#### ESP32-S3

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    OpenKNX/OFM-NeoPixel

build_flags =
    -DNEOPIXEL_MODULE
    -DARDUINO_ARCH_ESP32
```

---

## Advanced Topics

### Direct Buffer Manipulation

For maximum performance, you can manipulate LED buffers directly:

```cpp
auto vstrip = neoPixelModule.addVirtualStrip(100, ColorOrder::GRB);
uint8_t* buf = vstrip->getBuffer();
size_t size = vstrip->getBufferSize();

// Assuming GRB order, 3 bytes per LED
for (size_t i = 0; i < size; i += 3) {
    buf[i] = 128;      // G
    buf[i + 1] = 255;  // R
    buf[i + 2] = 0;    // B
}

vstrip->syncToPhysical();
vstrip->show();
```

### Custom Driver Implementation

Implement `IHardwareDriver` interface for custom drivers:

```cpp
class MyCustomDriver : public IHardwareDriver
{
  public:
    bool init(const DriverConfig& config) override;
    bool setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) override;
    bool show() override;
    // ... implement other methods
};
```

### Performance Optimization

**Tips:**

1. Use auto-update mode instead of manual `updateAll()` calls
2. Keep segment count reasonable (< 16)
3. Use hardware SPI for APA102 strips when possible
4. Avoid frequent virtual strip recomposition
5. Batch pixel updates before calling `show()`

**Monitoring:**

```cpp
// Check performance
neo perf

// Output:
// Update time: 22µs
// FPS: 30
// CPU usage: 0.07%
```

---

## Migration from Other Libraries

### From FastLED

See **[Effects Porting Guide](Effects-Porting.md)** for detailed migration instructions.

Quick reference:

```cpp
// FastLED
FastLED.addLeds<WS2812B, 9, GRB>(leds, 64);
leds[0] = CRGB::Red;
FastLED.show();

// OFM-NeoPixel
auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
strip->setPixel(0, 255, 0, 0);
strip->show();
```

### From Adafruit NeoPixel

```cpp
// Adafruit
Adafruit_NeoPixel strip(64, 9, NEO_GRB + NEO_KHZ800);
strip.begin();
strip.setPixelColor(0, strip.Color(255, 0, 0));
strip.show();

// OFM-NeoPixel
auto strip = neoPixelModule.addStrip(9, 64, LedProtocol::WS2812B);
strip->setPixel(0, 255, 0, 0);
strip->show();
```

---

## Troubleshooting

### Common Issues

**LEDs not lighting up:**
- Check power supply (5V/12V depending on strip)
- Verify wiring (DIN, GND, VCC)
- Check GPIO pin number
- Try different protocol (WS2812 vs WS2812B)

**Flickering:**
- Add 1000µF capacitor near strip
- Add 470Ω resistor on data line
- Check power supply capacity
- Reduce update frequency

**Wrong colors:**
- Check color order (RGB vs GRB)
- Try different ColorOrder setting
- Verify protocol matches strip

**Performance issues:**
- Reduce segment count
- Lower update frequency
- Use hardware acceleration (PIO/RMT)
- Check `neo perf` output

### Debug Logging

Enable debug output:

```ini
build_flags =
    -DDEBUG
```

Then check console for debug messages during initialization and updates.

---

## Support

**Documentation:**
- [Quickstart Guide](Quickstart.md)
- [Architecture](Architecture.md)
- [Effects Porting](Effects-Porting.md)

**Repository:** https://github.com/OpenKNX/OFM-NeoPixel  
**Issues:** https://github.com/OpenKNX/OFM-NeoPixel/issues

---

**Last Updated:** November 2025  
**Version:** 0.0.1
