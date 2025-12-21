# OFM-NeoPixel

**Version:** 0.0.2  
**Platform:** OpenKNX (RP2040, RP2350, ESP32-S3)  
**License:** GNU GPL v3.0  
**Author:** Erkan Çolak

A high-performance, hardware-optimized LED control library for addressable RGB/RGBW strips on OpenKNX devices with **self-describing effects** and **stateless architecture**.

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
  - [System Overview](#system-overview)
  - [PhysicalStripConfig Architecture](#physicalstripconfig-architecture)
  - [Data Flow with Global Power Management](#data-flow-with-global-power-management)
  - [Memory Layout](#memory-layout)
  - [ColorOrder Architecture](#colororder-architecture)
- [Installation](#installation)
- [Hardware Support](#hardware-support)
  - [Supported Microcontrollers](#supported-microcontrollers)
  - [Supported LED Protocols](#supported-led-protocols)
  - [Wiring](#wiring)
  - [Power Considerations](#power-considerations)
  - [Timing Modes](#timing-modes)
  - [GPIO Optimizations](#gpio-optimizations)
- [Console Commands](#console-commands)
  - [Core Commands](#core-commands)
  - [Physical Strip Commands](#physical-strip-commands)
  - [Physical Strip Config Commands](#physical-strip-config-commands)
  - [Virtual Strip Commands](#virtual-strip-commands)
  - [Segment Commands](#segment-commands)
  - [Effect Commands](#effect-commands)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Performance](#performance)
- [Power Management & Current Limiting](#power-management--current-limiting)
  - [How It Works](#how-it-works)
  - [LED Current Profiles](#led-current-profiles)
  - [Configuration](#configuration)
  - [Monitoring Power Consumption](#monitoring-power-consumption)
  - [Practical Examples](#practical-examples)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)
- [Credits](#credits)

---

## Overview

OFM-NeoPixel provides a three-layer architecture for managing addressable LED strips:

1. **PhysicalStrip** - Hardware abstraction for individual LED strips
2. **VirtualStrip** - Logical composition of multiple physical strips
3. **Segment** - Effect zones with independent animations

This design enables complex LED configurations with minimal CPU overhead through hardware acceleration (PIO/DMA on RP2040, RMT on ESP32).

### What Makes This Library Different?

- **Hardware Accelerated**: Zero CPU overhead during LED updates (DMA/PIO/RMT)
- **Stateless Effects**: 96% memory savings - single effect instance for all segments
- **Self-Describing Effects**: Auto-generated UI, console commands, and documentation
- **Multi-Strip Composition**: Combine multiple physical strips into one logical strip
- **Platform Optimized**: RP2040 (PIO/DMA), RP2350 (PIO/DMA), ESP32-S3 (RMT)
- **Fine-Grained Timing Control**: 11 timing modes for compatibility (640-1000 kHz)
- **Overclock Safe**: Automatic clock detection, works at any CPU frequency

---

## Key Features

### PhysicalStripConfig Architecture (NEW)

- **Three-Tier Hierarchy**: DriverConfig -> PhysicalStripConfig -> VirtualStripConfig
- **Driver-Defined Limits**: Hardware-specific constraints (e.g. APA102 brightness 0-31)
- **Type-Safe Configuration**: Validated config structs with range checking
- **Persistent Storage Ready**: EEPROM/Flash support for config persistence
- **Console Configuration**: Runtime changes via `neo phys config` commands
- **Config Versioning**: Future-proof config format with version tracking
- **Gamma Correction**: Optional gamma correction for improved color accuracy
- **Skip First LEDs**: Force first N LEDs to black (useful for dummy/sacrificial LEDs)

### Timing Modes

- **11 Timing Modes**: AUTO, AUTO_LEGACY, SLOW_5-20%, FAST_5-25%
- **RP2350/RP2040 Compatible**: AUTO_LEGACY mode for WS2812C/D onboard LEDs (adapts to any CPU frequency)
- **Runtime Adjustable**: Change timing without restart via console or API
- **Overclock Safe**: Automatic CPU frequency detection (125-300+ MHz)
- **Console Commands**: Inspect and modify timing with `neo phys timing`
- **Bitrate Range**: 640 kHz (SLOW_20PCT) to 1000 kHz (FAST_25PCT)

### GPIO Optimizations (NEW)

- **12mA Drive Strength**: 3x stronger than default (4mA) for long cables
- **FAST Slew Rate**: Sharp clock edges, reduced distortion >5 MHz
- **Glitch Prevention**: Pins set LOW before PIO init to eliminate startup flicker
- **SPI Optimized**: Tested stable at 3-20 MHz over 5m cables
- **Serial Optimized**: WS2812B timing precision (+-150ns tolerance)
- **CPU Frequency Adaptive**: Auto-adjusts clkdiv for 125-300 MHz operation

### Effect System

- **Parameter Introspection API**: Effects describe their own parameters
- **Auto-Generated UI**: Console and web UI generate automatically
- **12 Parameter Types**: UINT8, BOOL, COLOR_RGB, PERCENT, ENUM, etc.
- **Zero Code Changes**: Add new effects without modifying Segment/Console/UI
- **Type-Safe**: ParameterType enum for validation

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
    
    NeoPixelManager* neopixel_manager = neoPixelModule.getManager();
    if(neopixel_manager)
    {
      // Create a physical strip (GPIO 9, 64 LEDs, WS2812B, RGB)
      auto strip = neopixel_manager->addStrip(22, 64, LedProtocol::WS2812B, ColorOrder::RGB);
      // Initialize the physical strip
      strip->init(); 
    
      // Update the strip
      neopixel_manager->updateAll();
    }
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
    

    NeoPixelManager* npxmgr = neoPixelModule.getManager();
    if(npxmgr)
    {

      // Add three physical strips (Default Order is RGB)
      auto strip0 = npxmgr->addStrip(22, 64, LedProtocol::WS2812B, ColorOrder::RGB);
      auto strip1 = npxmgr->addStrip(7, 64, LedProtocol::WS2812B, ColorOrder::RGB);
      auto strip2 = npxmgr->addSpiStrip(9, 8, 40, LedProtocol::APA102, ColorOrder::RGB);

      // Initialize the physical strips
      if(strip0) strip0->init();
      if(strip1) strip1->init();
      if(strip2) strip2->init();
      
      // ONE VirtualStrip for all (168 LEDs total: 64+64+40)
      auto virt0 = npxmgr->addVirtualStrip(168, ColorOrder::RGB);  // Default RGB, PhysicalStrips handle conversion
      
      // Attach all physical strips
      npxmgr->attachPhysicalToVirtual(virt0, strip0, 0);   // Offset 0-63
      npxmgr->attachPhysicalToVirtual(virt0, strip1, 64);  // Offset 64-127
      npxmgr->attachPhysicalToVirtual(virt0, strip2, 128); // Offset 128-167

      // ONE segment for all LEDs
      Segment* seg0 = npxmgr->addSegment(virt0, 0, 167);  // All 168 LEDs

      // Set the getSolid effect to the segment0
      seg0->setEffect(EffectPool::getSolid());
      seg0->setPrimaryColor(50, 0, 0, 255); // Dark red
    
      // Enable auto-update
      npxmgr->updateAll();
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

### Advanced Configuration Features

#### Gamma Correction

Gamma correction improves color accuracy and perceived brightness linearity. Applies inverse gamma curve to compensate for human eye perception.

**Configuration via API:**
```cpp
// Enable gamma correction on a physical strip
auto strip = neopixel_manager->addStrip(22, 64, LedProtocol::WS2812B);
auto* cfg = strip->getConfig();
if (cfg) {
    cfg->setGammaCorrection(2.8f);  // Enable gamma correction (typical: 2.2-3.0)
    strip->applyConfig();            // Apply changes
}
```

**Console commands:**
```bash
# Show current gamma setting
neo phys config 0 gamma

# Set gamma correction to 2.8 (recommended)
neo phys config 0 gamma 2.8

# Disable gamma correction (linear, gamma = 1.0)
neo phys config 0 gamma 1.0
```

**Technical details:**
- **Formula**: `output = input^(1/gamma)` → brightens low values, perceptually linear
- **Lookup Table**: Pre-calculated 256-byte table at init (~0.25ms overhead, once)
- **Performance**: O(1) array access in hot path (~3 CPU cycles per color channel)
- **Recommended values**: 2.2 (sRGB), 2.8 (LED strips), 3.0 (dim environments)
- **Disable**: Set `gamma = 1.0` for linear/no correction

#### Skip First LEDs (Fast Path)

Force the first N LEDs to black - useful for dummy/sacrificial LEDs or signal regeneration.

**Configuration via API:**
```cpp
// Skip first 2 LEDs (force them to black)
auto* cfg = strip->getConfig();
if (cfg) {
    cfg->setSkipFirstLeds(2);       // First 2 LEDs always black
    strip->applyConfig();
}
```

**Console commands:**
```bash
# Skip first LED
neo phys config 0 skipfirst 1

# Skip first 3 LEDs
neo phys config 0 skipfirst 3

# Disable skip (default)
neo phys config 0 skipfirst 0
```

**Use cases:**
- **Dummy LEDs**: Some WS2812B strips use LED#0 as a signal regenerator
- **Data integrity**: Skip problematic first LED if signal quality is poor
- **Clock regeneration**: APA102 strips benefit from a dummy LED for timing
- **Cable runs**: First LED after long cable may have corrupt color

**Performance:** O(1) integer compare (~2 CPU cycles per LED)

#### Skip Mask (Flexible Path)

Skip arbitrary LEDs using an efficient bitset. Useful for complex LED arrangements or broken LEDs.

**Configuration via API:**
```cpp
auto* cfg = strip->getConfig();
if (cfg) {
    // Initialize skip mask for 64 LEDs
    cfg->initSkipMask(64);
    
    // Mark individual LEDs to skip
    cfg->setLedSkip(0, true);   // Skip LED#0
    cfg->setLedSkip(15, true);  // Skip LED#15
    cfg->setLedSkip(30, true);  // Skip LED#30
    
    strip->applyConfig();
}
```

**Console commands:**
```bash
# Initialize skip mask for current strip LED count
neo phys config 0 skipmask init

# Skip individual LEDs
neo phys config 0 skipmask set 0 1    # Skip LED#0
neo phys config 0 skipmask set 15 1   # Skip LED#15
neo phys config 0 skipmask set 30 1   # Skip LED#30

# Re-enable LED
neo phys config 0 skipmask set 15 0   # Enable LED#15

# List all skipped LEDs
neo phys config 0 skipmask list

# Clear skip mask (frees memory)
neo phys config 0 skipmask clear
```

**Use cases:**
- **Broken LEDs**: Skip individual defective LEDs
- **Complex patterns**: Create custom LED masks (e.g., checkerboard)
- **Physical wiring**: Skip LEDs used for other purposes
- **Matrix layouts**: Disable unused LEDs in irregular matrix shapes

**Performance:**
- O(1) bitset access (~3 CPU cycles per LED)
- Memory: ~1 bit per LED (300 LEDs = 38 bytes + 24 bytes std::vector overhead)
- Only allocated when `initSkipMask()` called (empty = no memory overhead)

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────┐
│                    NeoPixel Module                      │
│              (OpenKNX Integration Layer)                │
│  - Console commands (with parameter API)                │
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
│  - GLOBAL Power Management (NEW!)                       │
│    └─> PowerManager: Current limiting across ALL strips │
└────┬────────────┬─────────────┬─────────────────────────┘
     |            |             |
┌────▼───────┐  ┌─▼────────┐  ┌─▼────────┐
│ Physical   │  │ Virtual  │  │ Segment  │
│ Strip      │  │ Strip    │  │ + State  │
│+ColorOrder │  │(RGB only)│  │ +Config  │
│+TimingMode │  │          │  │          │
└────┬───────┘  └──────────┘  └─────┬────┘
     |                              |
┌────▼───────────────┐        ┌─────▼─────────────────┐
│ IHardwareDriver    │        │  Effect (Singleton)   │
│  - PIO (RP2040)    │        │  - Stateless          │
│    + 11 Timings    │        │  - Parameter API      │
│  - RMT (ESP32)     │        │  - Self-describing    │
│  - SPI (All)       │        │                       │
└────────────────────┘        └───────────────────────┘
```

**Effect System (NEW):**
```
┌──────────────────────────────────────────────────────┐
│          Effect Pool (Singletons, ~80 bytes)         │
│  Solid │ Rainbow │ BPM │ Pride │ ... (10 effects)    │
└────┬─────────┬──────┬─────┬──────────────────────────┘
     │         │      │     │
     └─────────┴──────┴─────┴────► Shared by 100 segments
                                   = 8 bytes per effect
                                   vs 800+ bytes with state
```

### Data Flow with Global Power Management

```
┌──────────────────────────────────────────────────────────┐
│ PHASE 1: Effect Updates                                  │
│ Effect.update() -> Segment.setPixel() -> VirtualStrip    │
│ Calculates ideal pixel colors (RGB/RGBW)                 │
└───────────────────────┬──────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────┐
│ PHASE 2: GLOBAL POWER MANAGEMENT                         │
│ NeoPixelManager::_applyPowerLimit() [PRIVATE HELPER]     │
│ ├─ Calculate total current across ALL VirtualStrips      │
│ ├─ PowerManager: Sum(I_strip1 + I_strip2 + ...)          │
│ ├─ If total > limit: globalScale = limit / total         │
│ └─ Scale ALL VirtualStrip buffers: pixel *= globalScale  │
└───────────────────────┬──────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────┐
│ PHASE 3: Sync Virtual→Physical [PUBLIC: syncAll()]       │
│ VirtualStrip.syncToPhysical()                            │
│ Copy SCALED buffer to PhysicalStrips with ColorOrder     │
│ conversion (RGB→GRB/BGR/etc.) + Hardware brightness      │
└───────────────────────┬──────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────┐
│ PHASE 4: Hardware Transfer [PUBLIC: showAll()]           │
│ PhysicalStrip.show() → DMA/PIO/RMT/SPI                   │
│ Non-blocking hardware transfer to GPIO                   │
└───────────────────────┬──────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────┐
│ LED Hardware: Displays scaled, safe output               │
│ Power consumption ≤ configured limit                     │
└──────────────────────────────────────────────────────────┘

METHOD FLOW:
  update(deltaTime)      -> updateEffects(dt) -> updateAll() -> Override timing=0
  updateEffects(dt)      -> Segment effects only (Phase 1)
  updateAll()            -> applyPowerLimit() -> syncAll() -> showAll()
  
GRANULAR CONTROL (all 4 phases individually):
  mgr->updateEffects(dt);   // Phase 1: Calculate effects
  mgr->applyPowerLimit();   // Phase 2: Apply power scaling
  mgr->syncAll();           // Phase 3: Sync buffers
  mgr->showAll();           // Phase 4: Hardware transfer
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

### ColorOrder Architecture

**Design Philosophy:** Unified RGB interface with per-strip hardware adaptation.

#### Overview

The ColorOrder system provides automatic color byte reordering for different LED hardware:

```
┌──────────────┐
│ Application  │  Always uses logical RGB(W) colors
│   (Effects)  │  Example: RED = RGB(255, 0, 0)
└──────┬───────┘
       │ Always RGB/RGBW
       ▼
┌──────────────┐
│ VirtualStrip │  Stores pixels in RGB/RGBW format
│   Buffer     │  [R, G, B] or [R, G, B, W]
└──────┬───────┘
       │ syncToPhysical() sends RGB
       ▼
┌──────────────┐
│PhysicalStrip │  Pass-through layer (NO conversion!)
│              │  Forwards RGB directly to driver
│ ColorOrder:  │  Stored but not used for conversion
│   GRB / BGR  │  Driver reads it via getColorOrder()
└──────┬───────┘
       │ RGB unchanged
       ▼
┌──────────────┐
│ Hardware     │  rgbToBuffer(): RGB → ColorOrder
│ Driver (PIO/ │  Example GRB: RGB(255,0,0) → [0,255,0]
│  RMT / SPI)  │  Example BGR: RGB(255,0,0) → [0,0,255]
│              │  WS2812B sends: [G, R, B]
│              │  APA102 sends: [Brightness, B, G, R]
└──────────────┘
```

#### Supported ColorOrders

**RGB Protocols (3-byte):**

| ColorOrder | LED Chips          | Byte Mapping       | Example (RED)      |
|------------|-------------------|--------------------|--------------------|
| `NONE`     | Auto-detect        | Uses protocol default | Protocol-specific |
| `RGB`      | WS2811, SK9822 clones | [R, G, B]       | [255, 0, 0]        |
| `RBG`      | Rare variants      | [R, B, G]          | [255, 0, 0]        |
| `GRB`      | WS2812B, SK6812   | [G, R, B]          | [0, 255, 0]        |
| `GBR`      | Rare variants      | [G, B, R]          | [0, 0, 255]        |
| `BRG`      | Rare variants      | [B, R, G]          | [0, 255, 0]        |
| `BGR`      | APA102 (some clones) | [B, G, R]        | [0, 0, 255]        |

**RGBW Protocols (4-byte):**

| ColorOrder | LED Chips          | Byte Mapping       | Example (RED)      |
|------------|-------------------|--------------------|--------------------|
| `RGBW`     | SK6812-RGBW variants | [R, G, B, W]     | [255, 0, 0, 0]     |
| `RBGW`     | Rare variants      | [R, B, G, W]       | [255, 0, 0, 0]     |
| `GRBW`     | SK6812-RGBW (common) | [G, R, B, W]     | [0, 255, 0, 0]     |
| `GBRW`     | Rare variants      | [G, B, R, W]       | [0, 0, 255, 0]     |
| `BRGW`     | Rare variants      | [B, R, G, W]       | [0, 255, 0, 0]     |
| `BGRW`     | Rare variants      | [B, G, R, W]       | [0, 0, 255, 0]     |

**Total:** All 13 possible color byte orders (1 auto + 6 RGB + 6 RGBW)

#### Data Flow Example

**Scenario:** WS2812B (GRB hardware) showing RED

```
1. Application:      segment->setPrimaryColor(255, 0, 0, 0);  // Logical RGB

2. Effect:           Reads config.primaryRGBW = 0xFF000000
                     Calls segment->setPixel(i, 255, 0, 0, 0);

3. Segment:          Applies brightness
                     Calls virtualStrip->setPixel(idx, 255, 0, 0);

4. VirtualStrip:     Stores in buffer: [255, 0, 0]  (Always RGB!)
                     Calls syncToPhysical()

5. PhysicalStrip:    Passes RGB directly: setPixel(i, 255, 0, 0)
                     Calls driver->setPixel(i, 255, 0, 0);

6. Driver:           Reads ColorOrder = GRB
                     rgbToBuffer(): RGB(255,0,0) → buffer[0]=0, buffer[1]=255, buffer[2]=0
                     Writes bytes: [0, 255, 0]

7. WS2812B LED:      Interprets as: G=0, R=255, B=0 → RED
```

#### Key Design Principles

1. **VirtualStrip is ColorOrder-agnostic**
   - Always stores RGB/RGBW internally
   - No color conversion in VirtualStrip layer
   - Simplifies effect development

2. **PhysicalStrip is a pass-through layer**
   - Forwards RGB values directly to driver
   - Stores ColorOrder setting but doesn't use it
   - Passes ColorOrder to driver via setColorOrder()

3. **Hardware drivers handle ColorOrder conversion**
   - rgbToBuffer() does the ONLY ColorOrder mapping
   - Each driver (PIO/RMT/SPI) implements the same logic
   - sendData/show functions are ColorOrder-agnostic (1:1 byte transfer)

4. **No double-mapping**
   - ColorOrder conversion happens ONCE (in driver's rgbToBuffer)
   - All other layers pass RGB unchanged
   - Clean separation of concerns

#### Mixed ColorOrders in ONE VirtualStrip

**This is the killer feature:** Combine strips with different ColorOrders seamlessly!

```cpp
// Example: WS2812B (GRB) + SK9822 (RGB) in one logical strip
auto strip0 = mgr->addStrip(22, 64, LedProtocol::WS2812B);  // Auto: GRB
auto strip1 = mgr->addSpiStrip(9, 8, 40, LedProtocol::SK9822);  // Auto: RGB

// Combine into ONE VirtualStrip
auto virt = mgr->addVirtualStrip(104);  // Always RGB internally!
mgr->attachPhysicalToVirtual(virt, strip0, 0);    // WS2812B at 0-63 (GRB)
mgr->attachPhysicalToVirtual(virt, strip1, 64);   // SK9822 at 64-103 (RGB)

// ONE segment, ONE effect across BOTH strips with DIFFERENT hardware!
auto seg = mgr->addSegment(virt, 0, 103);
seg->setEffect(EffectPool::getRainbow());
seg->setPrimaryColor(255, 0, 0, 255);  // RED on both strips
```

**Result:** Both strips show the same logical colors despite different hardware!

#### Setting ColorOrder

**Auto-Detect (Recommended):**

```cpp
// ColorOrder is automatically set based on protocol
auto strip1 = mgr->addStrip(pin, count, LedProtocol::WS2812B);  // Auto: GRB
auto strip2 = mgr->addSpiStrip(mosi, sck, count, LedProtocol::SK9822);  // Auto: RGB
```

**Explicit Override (for clones/variants):**

```cpp
// 1-Wire strips
auto strip = mgr->addStrip(pin, count, protocol, ColorOrder::RGB);  // Override

// SPI strips
auto strip = mgr->addSpiStrip(mosi, sck, count, protocol, ColorOrder::BGR);

// Console
neo phys add 9 64 2 1      # GPIO 9, 64 LEDs, WS2812B, ColorOrder=GRB
neo spi add 8 9 40 5 4     # MOSI=8, SCK=9, 40 LEDs, APA102, ColorOrder=BGR
```

**VirtualStrip ColorOrder (Legacy/Ignored):**

VirtualStrip has a ColorOrder parameter for backward compatibility, but it's **not used** for color conversion. VirtualStrip always stores RGB/RGBW internally.

```cpp
// This parameter is ignored for color conversion
auto virt = mgr->addVirtualStrip(100, ColorOrder::RGB);  // Always RGB internally
```

#### Troubleshooting ColorOrder

**Problem:** Wrong colors (e.g., RED shows as GREEN)

**Solution:**

1. **Check hardware datasheet** - Verify actual ColorOrder
   - WS2812B: Usually GRB (but some clones are RGB!)
   - APA102: Usually BGR
   - SK6812: Usually GRB or GRBW

2. **Test all combinations:**
   ```cpp
   // Try each ColorOrder until colors match
   ColorOrder::RGB   // If this works, your LEDs are RGB-native
   ColorOrder::GRB   // Most WS2812B
   ColorOrder::BGR   // Most APA102
   ```

3. **Console test:**
   ```bash
   neo color 0 50 0 0   # Should show RED
   neo color 0 0 50 0   # Should show GREEN
   neo color 0 0 0 50   # Should show BLUE
   ```

**Example:** Your hardware tested as RGB-native (unusual but valid):

```cpp
auto strip0 = mgr->addStrip(22, 64, LedProtocol::WS2812B, ColorOrder::RGB);
auto strip2 = mgr->addSpiStrip(9, 8, 40, LedProtocol::APA102, ColorOrder::RGB);
```

#### Performance Impact

ColorOrder conversion has **negligible** performance impact:

- **When:** Once per frame during `syncToPhysical()`
- **Where:** Simple switch-case byte reordering
- **Cost:** ~3 CPU cycles per LED (~0.01ms for 100 LEDs)
- **DMA/PIO:** Still zero-CPU overhead during GPIO transmission

```
Benchmark (100 LEDs, RP2040 @ 133MHz):
- No ColorOrder:      0.08ms
- With ColorOrder:    0.09ms  (+0.01ms)
- DMA transfer:       0.00ms  (zero CPU)
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
    ; Optional: Enable tests, benchmarks, and debug output
    ; -DOPENKNX_NEOPIXEL_TESTS
    ; -DOPENKNX_NEOPIXEL_BENCHMARK
    ; -DOPENKNX_NEOPIXEL_TRACE1
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

### Timing Modes

Fine-grained bitrate control for compatibility with different LED chips and RP2350/RP2040 timings.

#### Overview

LED strips like WS2812B expect data at **800 kHz** (±5% tolerance). The library calculates PIO clock dividers based on the CPU frequency:

```
PIO Clock Divider = CPU Frequency / (Target Frequency × 10 cycles/bit)
```

| Platform | CPU Frequency | Default Divider | Bitrate |
|----------|--------------|----------------|---------|
| RP2040   | 125 MHz      | 15.625         | 800 kHz |
| RP2350   | 150 MHz      | 18.750         | 800 kHz |

#### Why Timing Modes?

Some scenarios require non-standard timing:

1. **RP2350/RP2040 Onboard LEDs** (XIAO RP2350/RP2040): Newer WS2812C/D chips prefer 960 kHz (AUTO_LEGACY)
2. **Overclocked Systems**: AUTO_LEGACY automatically adapts from 125 MHz to 300+ MHz
3. **Timing-Sensitive Chips**: Some clones need slower/faster bitrates
4. **Cable Length**: Long wires may need slower speeds for reliability

#### Available Modes

| Mode | Bitrate | Multiplier | Use Case |
|------|---------|------------|----------|
| `AUTO` | 800 kHz | 1.0× | **Default** - WS2812B specification |
| `AUTO_LEGACY` | 960 kHz* | Adaptive | **WS2812C/D onboard LEDs** - adapts to CPU freq |
| `SLOW_20PCT` | 640 kHz | 0.80× | Long cables, timing-sensitive chips |
| `SLOW_15PCT` | 680 kHz | 0.85× | Moderate slowdown |
| `SLOW_10PCT` | 720 kHz | 0.90× | Minor adjustment |
| `SLOW_5PCT` | 760 kHz | 0.95× | Fine-tuning |
| `FAST_5PCT` | 840 kHz | 1.05× | Faster refresh |
| `FAST_10PCT` | 880 kHz | 1.10× | Above spec |
| `FAST_15PCT` | 920 kHz | 1.15× | High-speed strips |
| `FAST_20PCT` | 960 kHz | 1.20× | Maximum speed |
| `FAST_25PCT` | 1000 kHz | 1.25× | Extreme (may cause glitches) |

\* AUTO_LEGACY targets 960 kHz at any CPU frequency (125-300 MHz tested)
  - 125 MHz: clkdiv=13.02 → 960 kHz
  - 150 MHz: clkdiv=15.63 → 960 kHz
  - 200 MHz: clkdiv=20.83 → 960 kHz
  - 300 MHz: clkdiv=31.25 → 960 kHz

#### API Usage

**Creating Strips with Timing Modes:**

```cpp
// Auto mode (default) - 800 kHz for WS2812B
auto strip0 = npxmgr->addStrip(22, 64, LedProtocol::WS2812B, ColorOrder::RGB);

// AUTO_LEGACY mode for WS2812C/D onboard LED - 960 kHz (adapts to CPU freq)
auto strip1 = npxmgr->addStrip(12, 1, LedProtocol::WS2812B, ColorOrder::RGB, 
                               TimingMode::AUTO_LEGACY);

// Slow mode for long cables
auto strip2 = npxmgr->addStrip(9, 100, LedProtocol::WS2812B, ColorOrder::RGB, 
                               TimingMode::SLOW_10PCT);
```

**Runtime Timing Changes:**

```cpp
// Get current timing mode
TimingMode mode = strip->getTimingMode();

// Change timing mode (recreates driver, waits for DMA completion)
strip->setTimingMode(TimingMode::FAST_5PCT);
```

#### Console Commands

**List all timing modes:**
```bash
neo phys timings
# Output:
# Available Timing Modes:
# ┌────┬────────────────┬──────────┬────────────┐
# │ ID │ Mode           │ Bitrate  │ Multiplier │
# ├────┼────────────────┼──────────┼────────────┤
# │ 0  │ AUTO           │ 800 kHz  │ 1.00×      │
# │ 1  │ LEGACY_125MHZ  │ 800 kHz* │ Fixed      │
# │ 2  │ SLOW_20PCT     │ 640 kHz  │ 0.80×      │
# ...
```

**Show current timing for a strip:**
```bash
neo phys timing 0
# Output:
# PhysicalStrip[0] Timing Mode: LEGACY_125MHZ
#   System Clock: 150000000 Hz (150.00 MHz)
#   Target Frequency: 800000 Hz (800 kHz)
```

**Change timing mode:**
```bash
neo phys timing 0 fast10        # Set to FAST_10PCT
neo phys timing 1 auto          # Set to AUTO
neo phys timing 2 slow_20pct    # Set to SLOW_20PCT (flexible parsing)
```

**Show detailed timing information:**
```bash
neo phys timing 0 info
# Output:
# PhysicalStrip[0] - Detailed Timing Information
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# System Information:
#   CPU Frequency: 150000000 Hz (150.00 MHz)
#   Platform: RP2350
# 
# Strip Configuration:
#   GPIO: 12
#   LED Count: 1
#   Protocol: WS2812B
#   ColorOrder: RGB
# 
# Timing Configuration:
#   Mode: LEGACY_125MHZ
#   Target Frequency: 800000 Hz (800 kHz)
# 
# PIO Details:
#   PIO Instance: 0
#   State Machine: 0
#   Clock Divider: 15.62
#   Actual Bitrate: 800000 Hz (800 kHz)
#   Deviation: 0.00%
```

#### Timing Mode Parsing

The console accepts flexible input formats:
- Uppercase: `AUTO`, `LEGACY_125MHZ`, `SLOW_10PCT`
- Lowercase: `auto`, `legacy`, `slow10`, `fast25`
- With/without underscore: `fast_20pct`, `fast20`
- Short forms: `legacy`, `slow5`, `fast10`

#### Implementation Notes

- **Overclock Safe**: Automatically detects CPU frequency via `clock_get_hz(clk_sys)`
- **Works at any frequency**: 125, 150, 200, 300 MHz tested
- **Driver Recreation**: Changing timing mode recreates the PIO driver
  - Waits for DMA transfer completion
  - Clears LED buffer for clean state
  - Reassigns PIO/SM/DMA resources
- **No Effect on VirtualStrip**: Timing is per-PhysicalStrip only

#### Troubleshooting Timing Issues

| Symptom | Possible Cause | Solution |
|---------|---------------|----------|
| First LED flickers | Wrong timing, no level shifter | Try `SLOW_10PCT`, add level shifter |
| Random colors | Timing too fast | Use `SLOW_5PCT` or `SLOW_10PCT` |
| RP2350 onboard LED not working | WS2812C needs faster timing | Use `AUTO_LEGACY` (960 kHz) |
| Works on RP2040, fails on RP2350 | Different LED chip generation | Try `AUTO_LEGACY` or `FAST_15PCT` |
| All LEDs off | Timing too slow | Try `FAST_5PCT` or check wiring |

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

neo phys timings
    # List all available timing modes
    # Shows: ID, Mode name, Bitrate, Multiplier

neo phys timing <index>
    # Show current timing mode for a physical strip
    # Example:
    neo phys timing 0              # Show timing for strip 0

neo phys timing <index> <mode>
    # Set timing mode for a physical strip
    # Recreates driver with new timing (waits for DMA completion)
    # Examples:
    neo phys timing 0 auto         # Default 800 kHz
    neo phys timing 1 legacy       # AUTO_LEGACY mode (960 kHz)
    neo phys timing 0 slow10       # 10% slower (720 kHz)
    neo phys timing 2 fast_25pct   # 25% faster (1000 kHz)

neo phys timing <index> info
    # Show detailed timing information
    # Includes: CPU frequency, PIO details, clock divider, actual bitrate
    # Example:
    neo phys timing 0 info
```

### Physical Strip Config Commands

```bash
neo phys config <index> info
    # Show complete config for physical strip
    # Displays: Protocol, timing, brightness, hardware brightness, GPIO pins,
    #           PIO/SM allocation, DMA channel, frequency + clkdiv
    # Example output:
    #   Type: SPI Strip (APA102/SK9822)
    #   Pins: CLK=GPIO2, MOSI=GPIO4
    #   Hardware: PIO0/SM0 (SPI), DMA Ch7
    #   SPI Frequency: 3 MHz (clkdiv: 50.00)
    #   HW Brightness: 18 (range: 0-31, default: 16)
    neo phys config 0 info

neo phys config <index> brightness [value]
    # Get/Set software brightness (0-255)
    neo phys config 0 brightness        # Show current value
    neo phys config 0 brightness 128    # Set to 50%

neo phys config <index> hwbrightness [value]
    # Get/Set hardware brightness (driver-specific range)
    # APA102/SK9822: 0-31 (safe range: 16-30)
    # WS2812B: 0-255 (not used, software only)
    neo phys config 0 hwbrightness      # Show current + limits
    neo phys config 0 hwbrightness 20   # Set to 20/31

neo phys config <index> frequency [hz]
    # Get/Set SPI frequency (SPI strips only)
    # Tested stable: 3-20 MHz over 5m cables
    neo phys config 0 frequency          # Show current
    neo phys config 0 frequency 5000000  # Set to 5 MHz

neo phys config <index> timing [mode]
    # Get/Set timing mode (Serial strips only)
    # Same as 'neo phys timing' command
    neo phys config 0 timing             # Show current
    neo phys config 0 timing auto_legacy # Set mode

neo phys config <index> gamma [value]
    # Get/Set gamma correction value (1.0-4.0)
    # 1.0 = disabled (linear), 2.2 = sRGB, 2.8 = recommended for LEDs
    neo phys config 0 gamma              # Show current value
    neo phys config 0 gamma 2.8          # Set gamma to 2.8
    neo phys config 0 gamma 1.0          # Disable (linear)

neo phys config <index> skipfirst [count]
    # Get/Set how many first LEDs to skip (force to black)
    # Useful for dummy/sacrificial LEDs (0-255)
    neo phys config 0 skipfirst          # Show current value
    neo phys config 0 skipfirst 1        # Skip first LED
    neo phys config 0 skipfirst 0        # Disable (default)

neo phys config <index> skipmask init
    # Initialize skip mask for flexible LED skipping
    # Allocates bitset (1 bit per LED)
    neo phys config 0 skipmask init

neo phys config <index> skipmask set <led> <0|1>
    # Set skip status for individual LED
    # 1 = skip (force black), 0 = enable
    neo phys config 0 skipmask set 0 1   # Skip LED#0
    neo phys config 0 skipmask set 15 1  # Skip LED#15
    neo phys config 0 skipmask set 15 0  # Re-enable LED#15

neo phys config <index> skipmask list
    # List all skipped LEDs in mask
    neo phys config 0 skipmask list

neo phys config <index> skipmask clear
    # Clear skip mask and free memory
    neo phys config 0 skipmask clear
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
    # List all available effects with IDs and parameter counts

neo effect <segIndex> <effectId>
    # Assign effect to segment (with default parameters)
    # Effect IDs:
    #   0 = Solid Color
    #   1 = Wipe (Direction)
    #   2 = Rainbow (Speed, Delta)
    #   3 = Pride2015 (no parameters)
    #   4 = Confetti (FadeSpeed, Saturation)
    #   5 = Juggle (NumDots, FadeSpeed)
    #   6 = BPM (BPM, Hue)
    #   7 = Cylon (Speed, Hue, EyeSize, FadeAmount)
    # │ 8 = RGBW Test ( Rotating RGBW test pattern)
    #   9 = GarageDoor (special effect)
    # Examples:
    neo effect 0 0                 # Solid color on Segment 0
    neo effect 1 2                 # Rainbow on Segment 1
    neo effect 2 7                 # Cylon on Segment 2

neo effect config <segIndex>
    # Show all effect parameters for segment
    # Displays parameter names, types, and current values
    neo effect config 0            # Show parameters for Segment 0

neo effect config <segIndex> get <paramIndex>
    # Get specific parameter value
    neo effect config 0 get 0      # Get parameter 0 (e.g., Speed)
    neo effect config 1 get 1      # Get parameter 1 (e.g., Hue)

neo effect config <segIndex> set <paramIndex> <value>
    # Set specific parameter value
    # Examples:
    neo effect config 0 set 0 150  # Set Speed to 150
    neo effect config 1 set 1 128  # Set Hue to 128
    neo effect config 2 set 2 8    # Set EyeSize to 8

neo garage <segIndex> <phase>
    # Control GarageDoor effect (ID 8)
    # Phases: 0=OPENING, 1=RUNWAY, 2=COMPLETED, 3=STOPPED
    neo garage 0 0                 # Start opening animation
    neo garage 0 1                 # Runway lights
    neo garage 0 2                 # Completed (green)
    neo garage 0 3                 # Stop/pause

neo color <segIndex> <r> <g> <b> [w]
    # Set primary color for segment
    neo color 0 255 0 0            # Red
    neo color 1 0 255 0            # Green
    neo color 2 0 0 255 128        # Blue + 50% white (RGBW)

neo brightness <segIndex> <value>
    # Set software brightness (0-255, all LED types)
    neo brightness 0 128           # 50% brightness
    neo brightness 1 255           # 100% brightness

neo hwbrightness <segIndex> <value>
    # Set hardware brightness (0-255, APA102/SK9822 only)
    # Does not reduce color depth like software brightness
    neo hwbrightness 0 200         # 78% hardware brightness
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

### Debug Output

Available when compiled with `-DOPENKNX_NEOPIXEL_TRACE1`:

Enables detailed debug logging for:
- Hardware initialization (PIO/RMT allocation, GPIO configuration)
- Configuration validation (color order, timing parameters)
- Performance metrics (update times, frame rates)
- Driver-specific operations (SPI transfers, PIO state machine setup)

Output is sent to the serial console at 115200 baud.

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
    
    // Update Control (4-Phase Pipeline)
    void update(uint32_t deltaTime);
    void updateEffects(uint32_t deltaTime);
    void applyPowerLimit();
    void syncAll();
    bool showAll();
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
    PhysicalStrip* addStrip(uint8_t pin, uint16_t ledCount, 
                           LedProtocol protocol = LedProtocol::WS2812B,
                           ColorOrder order = ColorOrder::RGB,
                           TimingMode timingMode = TimingMode::AUTO);
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
    
    // Update Control (4-Phase Pipeline)
    void update(uint32_t deltaTime);      // Phase 1-4: Full pipeline (non-blocking)
    void updateEffects(uint32_t deltaTime); // Phase 1: Effect calculations only
    void applyPowerLimit();               // Phase 2: Global power scaling
    void syncAll();                       // Phase 3: VirtualStrip → PhysicalStrip sync
    bool showAll();                       // Phase 4: PhysicalStrip → Hardware (blocking)
    void updateAll();                     // Phase 2-4: Convenience method
    
    // Clear & Reset
    void clearAll();
    void allOff();                        // Clear + updateAll()
    
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
    
    // Timing Control (NEW)
    TimingMode getTimingMode() const;
    void setTimingMode(TimingMode mode);  // Recreates driver with new timing
    
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

### TimingMode Enum

```cpp
enum class TimingMode : uint8_t {
    AUTO = 0,           // 800 kHz (default, auto-calculated from CPU freq)
    AUTO_LEGACY = 1,    // 960 kHz - Optimized for WS2812C/D onboard LEDs (adapts to CPU freq)
    SLOW_20PCT = 2,     // 640 kHz (0.80× multiplier)
    SLOW_15PCT = 3,     // 680 kHz (0.85× multiplier)
    SLOW_10PCT = 4,     // 720 kHz (0.90× multiplier)
    SLOW_5PCT = 5,      // 760 kHz (0.95× multiplier)
    FAST_5PCT = 6,      // 840 kHz (1.05× multiplier)
    FAST_10PCT = 7,     // 880 kHz (1.10× multiplier)
    FAST_15PCT = 8,     // 920 kHz (1.15× multiplier)
    FAST_20PCT = 9,     // 960 kHz (1.20× multiplier)
    FAST_25PCT = 10     // 1000 kHz (1.25× multiplier)
};
```

**Usage Examples:**

```cpp
// Default AUTO mode (800 kHz)
auto strip = npxmgr->addStrip(9, 64, LedProtocol::WS2812B);

// Legacy mode for RP2350 onboard LED
auto onboardLed = npxmgr->addStrip(12, 1, LedProtocol::WS2812B, 
                                   ColorOrder::RGB, TimingMode::LEGACY_125MHZ);

// Runtime timing change
strip->setTimingMode(TimingMode::FAST_10PCT);

// Get current timing
TimingMode currentMode = strip->getTimingMode();
```

**When to Use Each Mode:**

- **AUTO**: Default for external LED strips
- **LEGACY_125MHZ**: RP2350/RP2040 onboard LEDs (XIAO RP2350/RP2040)
- **SLOW_***: Long cable runs, timing-sensitive clones, signal integrity issues
- **FAST_***: High-speed refresh, overclocked systems, fast-compatible strips

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

### Example 6: RP2350 with Onboard and External LEDs

```cpp
void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    NeoPixelManager* mgr = neoPixelModule.getManager();
    
    // Onboard LED on XIAO RP2350 (requires legacy timing)
    auto onboardStrip = mgr->addStrip(OKNXHW_OPENKNXIAO_NEOPIXEL, 1, LedProtocol::WS2812B, 
                                      ColorOrder::RGB, 
                                      TimingMode::LEGACY_125MHZ);
    onboardStrip->init();
    
    // External LED strip (auto timing)
    auto externalStrip = mgr->addStrip(OKNXHW_OPENKNXIAO_D4, 64, LedProtocol::WS2812B, 
                                       ColorOrder::RGB); // Default is auto timing
    externalStrip->init();
    
    // Create virtual strips for each
    auto virtOnboard = mgr->addVirtualStrip(1, ColorOrder::RGB);
    auto virtExternal = mgr->addVirtualStrip(64, ColorOrder::RGB);
    
    mgr->attachPhysicalToVirtual(virtOnboard, onboardStrip, 0);
    mgr->attachPhysicalToVirtual(virtExternal, externalStrip, 0);
    
    // Segments with different effects
    auto segOnboard = mgr->addSegment(virtOnboard, 0, 0);
    segOnboard->setEffect(EffectPool::getSolid());
    segOnboard->setPrimaryColor(0, 50, 0, 255);  // Green
    
    auto segExternal = mgr->addSegment(virtExternal, 0, 63);
    segExternal->setEffect(EffectPool::getRainbow());
    
    // Check current timing modes
    openknx.logger.logWithPrefixAndValues("Onboard LED timing", 
        (uint8_t)onboardStrip->getTimingMode());
    openknx.logger.logWithPrefixAndValues("External strip timing", 
        (uint8_t)externalStrip->getTimingMode());
    
    // Enable auto-update
    mgr->setAutoUpdate(true);
}
```

### Example 7: Runtime Timing Adjustment

```cpp
PhysicalStrip* strip = nullptr;

void setup() {
    openknx.init(0);
    openknx.addModule(13, neoPixelModule);
    openknx.setup();
    
    NeoPixelManager* mgr = neoPixelModule.getManager();
    
    // Create strip with default timing
    strip = mgr->addStrip(9, 64, LedProtocol::WS2812B);
    strip->init();
    
    auto virt = mgr->addVirtualStrip(64, ColorOrder::RGB);
    mgr->attachPhysicalToVirtual(virt, strip, 0);
    
    auto seg = mgr->addSegment(virt, 0, 63);
    seg->setEffect(EffectPool::getSolid());
    seg->setPrimaryColor(255, 0, 0, 255);  // Red
    
    mgr->setAutoUpdate(true);
}

void loop() {
    openknx.loop();
    
    // Example: Adjust timing if flickering detected
    static bool adjusted = false;
    if (!adjusted && millis() > 5000) {
        // Try slower timing after 5 seconds
        openknx.logger.log("Adjusting timing to SLOW_10PCT...");
        strip->setTimingMode(TimingMode::SLOW_10PCT);
        adjusted = true;
    }
    
    // Console command can also change timing:
    // neo phys timing 0 slow10
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

### Q: RP2350 onboard LED not working
**A:** Onboard LEDs (XIAO RP2350) require legacy 125MHz-based timing:

```cpp
auto strip = mgr->addStrip(OKNXHW_OPENKNXIAO_NEOPIXEL, 1, LedProtocol::WS2812B, ColorOrder::RGB, 
                          TimingMode::LEGACY_125MHZ);
```

Or via console:
```bash
neo phys timing 0 legacy
```

### Q: LEDs work on RP2040 but not on RP2350
**A:** Different CPU frequencies (125 MHz vs 150 MHz) affect timing. Try:

1. Legacy timing mode: `TimingMode::LEGACY_125MHZ`
2. Slightly slower timing: `TimingMode::SLOW_5PCT` or `SLOW_10PCT`
3. Check actual bitrate with: `neo phys timing 0 info`

```cpp
// If flickering on RP2350, try slower timing
strip->setTimingMode(TimingMode::SLOW_10PCT);
```

### Q: Flickering or glitches only on RP2350
**A:**
1. **Try LEGACY_125MHZ mode first** (especially for onboard LEDs)
2. Use level shifter for 5V data signal
3. Adjust timing via console: `neo phys timing 0 slow5`
4. Check console for timing details: `neo phys timing 0 info`

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

### Q: My power supply is getting hot / voltage drops
**A:** Enable power management to limit current draw:
```cpp
mgr->setMaxCurrent(3000);  // Limit to 3A
mgr->getPowerManager()->setLedProfile(LedProfiles::WS2812B);
```
See the **Power Management** section below for details.

---

## Power Management & Current Limiting

**IMPORTANT:** High-power LED installations can exceed your power supply's capacity, causing:
- Voltage drops (LEDs flicker or change color)
- Overheating power supplies
- Damaged hardware or fire hazards
- Unexpected behavior (brownouts, resets)

OFM-NeoPixel includes **software-based current limiting** to prevent these issues.

### How It Works

#### 1. Current Calculation

Each LED color channel draws current proportional to its brightness:

```
Current(channel) = MaxCurrent(channel) × (Brightness / 255)
Current(LED) = Current(R) + Current(G) + Current(B) + Current(W)
```

**Example:** WS2812B at full white (R=255, G=255, B=255)
```
Current = (20mA × 255/255) + (20mA × 255/255) + (20mA × 255/255)
        = 20mA + 20mA + 20mA
        = 60mA per LED
```

For 100 LEDs at full brightness: **100 × 60mA = 6000mA (6A)**

#### 2. Automatic Brightness Scaling

When total current exceeds the configured limit, **all pixel values are proportionally reduced**:

```
Scale = MaxCurrent / CalculatedCurrent
ScaledBrightness = OriginalBrightness × Scale
```

**Example:** 100 LEDs drawing 6A with 5A limit
```
Scale = 5000mA / 6000mA = 0.833 (83.3%)
All pixel values multiplied by 0.833
Result: Max current = 5A ✓
```

#### 3. Architecture Flow

```
┌─────────────────┐
│  Effect Update  │  -> Calculates ideal pixel colors
└────────┬────────┘
         ↓
┌─────────────────┐
│ Segment Buffer  │  -> Stores RGB/RGBW values (0-255)
└────────┬────────┘
         ↓
┌─────────────────┐
│ PowerManager    │  -> Calculates total current
│ ├─ Calculate    │       Sum all pixel currents
│ ├─ Compare      │       vs. MaxCurrent limit
│ └─ Scale        │       Reduce if exceeded
└────────┬────────┘
         ↓
┌─────────────────┐
│ Physical Strip  │  -> Send scaled values to LEDs
│     show()      │
└─────────────────┘
```

### LED Current Profiles

Different LED types have different current consumption:

| LED Type | R (mA) | G (mA) | B (mA) | W (mA) | Total @ White |
|----------|--------|--------|--------|--------|---------------|
| WS2812B  | 20     | 20     | 20     | -      | 60mA          |
| SK6812 RGBW | 20  | 20     | 20     | 20     | 80mA          |
| APA102   | 15     | 15     | 15     | -      | 45mA          |

**Predefined Profiles:**
```cpp
LedProfiles::WS2812B       // 20mA per channel
LedProfiles::SK6812_RGBW   // 20mA per channel + W
LedProfiles::APA102        // 15mA per channel
LedProfiles::CONSERVATIVE  // 20mA all channels (safe default)
```

### Configuration

#### Global Power Limit (Recommended)

Set at manager level - applies to ALL strips:

```cpp
void setup() {
    NeoPixelManager* mgr = neoPixelModule.getManager();
    
    // Set 5A maximum (5V × 5A = 25W max)
    mgr->setMaxCurrent(5000);
    
    // Choose LED profile
    mgr->getPowerManager()->setLedProfile(LedProfiles::WS2812B);
    
    // Enable/disable at runtime
    mgr->setPowerManagementEnabled(true);
}
```

#### Custom LED Profile

For non-standard LEDs with different current draws:

```cpp
LedCurrentProfile customProfile(18, 22, 18, 0);  // R, G, B, W in mA
mgr->getPowerManager()->setLedProfile(customProfile);
```

#### Per-Strip Manual Limiting (Advanced)

For fine-grained control over individual strips:

```cpp
PowerManager pm(3000);  // 3A limit
pm.setLedProfile(LedProfiles::SK6812_RGBW);

// Before show()
pm.applyCurrentLimit(strip->getBuffer(), strip->getLedCount(), 4); // 4 = RGBW
strip->show();
```

### Monitoring Power Consumption

#### Real-Time Current/Power Display

```cpp
void loop() {
    // Get estimated power consumption
    float watts = mgr->getTotalPowerWatts();
    
    // Calculate current at 5V
    float amps = watts / 5.0f;
    
    Serial.printf("Power: %.2fW (%.2fA @ 5V)\n", watts, amps);
}
```

#### Via Console

```bash
neo power status     # Show current consumption
neo power limit 3000 # Set 3A limit
neo power profile ws2812b  # Set LED profile
neo power on/off     # Enable/disable limiting
```

### Practical Examples

#### Example 1: Small Installation (100 LEDs)

```cpp
// 100 WS2812B LEDs
// Max: 100 × 60mA = 6A
// Available: 5A USB power supply

mgr->setMaxCurrent(4500);  // Leave 10% safety margin (5A × 0.9)
mgr->getPowerManager()->setLedProfile(LedProfiles::WS2812B);

// Auto-scales: Full white → 75% brightness
```

#### Example 2: Large Installation (500 LEDs)

```cpp
// 500 SK6812 RGBW LEDs
// Max: 500 × 80mA = 40A
// Available: 15A @ 5V power supply

mgr->setMaxCurrent(14000);  // 15A - 1A safety margin
mgr->getPowerManager()->setLedProfile(LedProfiles::SK6812_RGBW);

// Auto-scales: Full white → 35% brightness
```

#### Example 3: Multiple Power Supplies

```cpp
// Split into two independent managers
NeoPixelManager mgr1, mgr2;

// Power supply 1: 5A
mgr1.setMaxCurrent(5000);
auto strip1 = mgr1.addStrip(22, 100, WS2812B);

// Power supply 2: 3A  
mgr2.setMaxCurrent(3000);
auto strip2 = mgr2.addStrip(23, 50, WS2812B);
```

### Brightness Interaction (Important!)

**Segment Brightness vs. Power Limiting:**

The library applies brightness in this order:
1. **Segment Brightness** (`segment->setBrightness(128)`) - Applied when Effect writes pixels
2. **Power Limiting** - Applied globally in `NeoPixelManager::_applyPowerLimit()` (private helper)
3. **VirtualStrip → PhysicalStrip Sync** - `syncAll()` copies scaled buffers with ColorOrder conversion
4. **Hardware Transfer** - `showAll()` sends to LEDs via DMA/PIO/RMT

**Update Pipeline:**
```cpp
// Method 1: Full pipeline with effects (non-blocking)
mgr->update(deltaTime);
// Phase 1: Effects update
// Phase 2-4: Calls updateAll() internally
// Override timing=0 for non-blocking

// Method 2: Manual control (blocking)
mgr->updateAll();
// _applyPowerLimit()  (private: calculates global power scaling)
// syncAll()           (public: VirtualStrip → PhysicalStrip)
// showAll()           (public: PhysicalStrip → Hardware)
```

**Example:**
```cpp
segment->setBrightness(128);  // 50% brightness
segment->setColor(255, 255, 255);  // White

// Actual values in buffer: (127, 127, 127)
// Power calculation sees: 127 × 3 × 20mA = 7.6mA per LED (not 60mA!)
```

**Result:** Power consumption shown in `neo power` reflects **actual** current after segment brightness is applied.

**Why this is correct:**
- Segment brightness is **user intent** ("I want 50% brightness")
- User **wants** reduced power consumption in this case
- Power limiting protects against **unintentional** overload
- Shown power values = real hardware consumption ✓

**Note:** Hardware brightness (APA102/SK9822) is separately tracked and included in power calculation.

### Important Notes

**Software limiting does NOT replace proper hardware design!**

**You MUST still:**
- [x] Use adequate power supply (calculate: LEDs × 60mA minimum)
- [x] Use proper gauge wiring (5V: 18AWG for <2m, 16AWG for >2m)
- [x] Inject power every 50-100 LEDs for long strips
- [x] Add 1000µF capacitor near LED strip power input
- [x] Add 470Ω resistor on data line
- [x] Use fuse/circuit breaker for safety

**Software limiting:**
- [x] Prevents overload from code/effects
- [x] Protects against unexpected full-brightness scenarios
- [x] Allows headroom for animations/transitions
- [ ] Does NOT protect against hardware shorts
- [ ] Does NOT compensate for inadequate wiring
- [ ] Does NOT eliminate need for proper PSU sizing

### Performance Impact

Current calculation overhead:
- **Per pixel:** ~5-10 CPU cycles
- **100 LEDs:** ~0.05ms on RP2040 @ 133MHz
- **500 LEDs:** ~0.25ms on RP2040 @ 133MHz

**Recommendation:** Enable power management always - the safety benefit far outweighs the minimal performance cost.

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

**Author:** Erkan Çolak  
**Project:** OpenKNX  
**Repository:** https://github.com/OpenKNX/OFM-NeoPixel

Special thanks to the OpenKNX community and contributors.
