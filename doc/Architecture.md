# OFM-NeoPixel - Architecture & Flow Diagrams

**Version:** 0.4.0  
**Date:** June 2026

Detailed architecture diagrams and data flow visualizations for the OFM-NeoPixel library.

---

## Related Documentation

- **[README](../README.md)** - Project overview and features
- **[Quickstart Guide](Quickstart.md)** - Get started quickly
- **[Developer API Reference](Developer.md)** - Complete API documentation
- **[Effects Porting Guide](Effects-Porting.md)** - Port FastLED effects

---

## Table of Contents

- [System Architecture](#system-architecture)
- [PhysicalStripConfig Architecture](#physicalstripconfig-architecture)
- [Data Flow](#data-flow)
- [Memory Layout](#memory-layout)
- [Effect System](#effect-system)
- [Effektmanager (Cue Sequencer)](#effektmanager-cue-sequencer)
- [2D / 3D Matrix Geometry](#2d--3d-matrix-geometry)
- [Effektkette (Distributed Rendering)](#effektkette-distributed-rendering)
- [Hardware Abstraction](#hardware-abstraction)
- [GPIO Optimizations](#gpio-optimizations)
- [Update Loop](#update-loop)

---

## System Architecture

### Layer Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                        Application Layer                           │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              OpenKNX Integration (main.cpp)                  │  │
│  │  - Module registration                                       │  │
│  │  - Lifecycle management                                      │  │
│  │  - GroupObject handling (planned)                            │  │
│  └──────────────────────┬───────────────────────────────────────┘  │
└─────────────────────────┼──────────────────────────────────────────┘
                          │
┌─────────────────────────▼──────────────────────────────────────────┐
│                     NeoPixel Module Layer                          │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     NeoPixel Module                          │  │
│  │  - Console command interface                                 │  │
│  │  - User-facing API                                           │  │
│  │  - Configuration management                                  │  │
│  │  - OpenKNX::Module interface implementation                  │  │
│  └──────────────────────┬───────────────────────────────────────┘  │
└─────────────────────────┼──────────────────────────────────────────┘
                          │
┌─────────────────────────▼──────────────────────────────────────────┐
│                    Management Layer                                │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                  NeoPixelManager                             │  │
│  │  - Physical strip lifecycle management                       │  │
│  │  - Virtual strip composition orchestration                   │  │
│  │  - Segment creation and management                           │  │
│  │  - Effect assignment and scheduling                          │  │
│  │  - Update loop coordination                                  │  │
│  └──────────┬────────────────┬─────────────────┬────────────────┘  │
└─────────────┼────────────────┼─────────────────┼───────────────────┘
              │                │                 │
      ┌───────▼──────┐  ┌──────▼────────┐  ┌─────▼────────┐
      │              │  │               │  │              │
┌─────▼─────────┐ ┌──▼──▼───────┐ ┌─────▼──▼──────┐ ┌─────▼──────┐
│ PhysicalStrip │ │VirtualStrip │ │   Segment     │ │   Effect   │
│               │ │             │ │               │ │   System   │
│ - HW control  │ │ - Multi-    │ │ - LED ranges  │ │            │
│ - Buffer mgmt │ │   strip     │ │ - Effect      │ │ - Stateless│
│ - Driver      │ │   compose   │ │   zones       │ │ - Pooled   │
│   wrapper     │ │ - Mapping   │ │ - State mgmt  │ │ - Shared   │
└───────┬───────┘ └─────────────┘ └───────────────┘ └────────────┘
        │
┌───────▼─────────────────────────────────────────────────────────┐
│                Hardware Abstraction Layer                        │
│                                                                  │
│  ┌──────────────────┐  ┌───────────────┐  ┌─────────────────┐  │
│  │IHardwareDriver   │  │ DriverFactory │  │  Protocol       │  │
│  │   Interface      │  │               │  │  Helper         │  │
│  └────────┬─────────┘  └───────┬───────┘  └─────────────────┘  │
│           │                    │                                │
│  ┌────────▼────────┬───────────▼──────┬─────────────────────┐  │
│  │                 │                  │                     │  │
│  │ PIO Driver      │  RMT Driver      │   SPI Driver        │  │
│  │ (RP2040/2350)   │  (ESP32-S3)      │   (All platforms)   │  │
│  │                 │                  │                     │  │
│  │ - DMA support   │  - Hardware      │ - Hardware SPI      │  │
│  │ - 1-Wire        │    timing        │ - APA102/WS2801     │  │
│  │ - Zero CPU      │  - 1-Wire        │ - PIO-SPI option    │  │
│  └─────────────────┴──────────────────┴─────────────────────┘  │
└──────────────────────────────┬───────────────────────────────────┘
                               │
┌──────────────────────────────▼───────────────────────────────────┐
│                         Hardware Layer                           │
│                                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │   PIO/DMA   │  │     RMT     │  │     SPI     │             │
│  │  (RP2040)   │  │   (ESP32)   │  │ (Universal) │             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
│         │                │                │                     │
│         └────────────────┴────────────────┘                     │
│                          │                                      │
│                    ┌─────▼──────┐                               │
│                    │    GPIO    │                               │
│                    └─────┬──────┘                               │
└──────────────────────────┼───────────────────────────────────────┘
                           │
                    ┌──────▼───────┐
                    │  LED Strip   │
                    └──────────────┘
```

---

## PhysicalStripConfig Architecture

### Three-Tier Configuration Hierarchy

```
┌──────────────────────────────────────────────────────────────────┐
│                        DriverConfig                              │
│                    (Hardware Limits)                             │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  - Hardware-specific constraints                          │  │
│  │  - Example: APA102 brightness 0-31                        │  │
│  │  - Example: WS2812B software brightness only             │  │
│  │  - Driver-defined default values                          │  │
│  │  - Frequency limits (SPI)                                  │  │
│  │  - Timing mode support (Serial)                           │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────┬───────────────────────────────────────┘
                           │ provides limits to
┌──────────────────────────▼───────────────────────────────────────┐
│                  PhysicalStripConfig                             │
│                  (Per-Strip Settings)                            │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  - Software brightness (0-255)                            │  │
│  │  - Hardware brightness (driver-specific range)            │  │
│  │  - SPI frequency (for APA102/SK9822)                      │  │
│  │  - Timing mode (for WS2812B/SK6812)                       │  │
│  │  - Validated against driver limits                         │  │
│  │  - Console configurable                                    │  │
│  │  - Persistent storage ready                                │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────┬───────────────────────────────────────┘
                           │ inherited by
┌──────────────────────────▼───────────────────────────────────────┐
│                  VirtualStripConfig                              │
│                (Composition Settings)                            │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  - Virtual strip brightness                                │  │
│  │  - Can override physical strip settings                    │  │
│  │  - Multi-strip composition rules                           │  │
│  │  - Segment-level overrides                                 │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Configuration Flow

```
User Command: neo phys config 0 hwbrightness 20
     │
     ▼
NeoPixelConsole::processPhysConfigCommand()
     │
     ├─► Parse command arguments
     └─► Validate index
            │
            ▼
PhysicalStrip::getConfig() / setConfig()
     │
     ├─► Get current PhysicalStripConfig
     ├─► Update brightness value
     └─► Validate against DriverConfig limits
            │
            ▼
HardwareDriver::applyConfig()
     │
     ├─► Update hardware registers
     ├─► Recalculate timing if needed
     └─► Update clkdiv for SPI
            │
            ▼
Show confirmation to user
```

### Config Commands

```
neo phys config <id> info          -> Show all config values
neo phys config <id> brightness    -> Software brightness (0-255)
neo phys config <id> hwbrightness  -> Hardware brightness (driver-specific)
neo phys config <id> frequency     -> SPI frequency (SPI only)
neo phys config <id> timing        -> Timing mode (Serial only)
```

---

## Data Flow

### Initialization Flow

```
Application Start
     │
     ▼
openknx.init(firmwareRevision)
     │
     ▼
openknx.addModule(13, neoPixelModule)
     │
     ▼
NeoPixel::init()
     │
     ├─► Create NeoPixelManager instance
     └─► Initialize performance tracker
            │
            ▼
openknx.setup()
     │
     ▼
NeoPixel::setup(configured)
     │
     ├─► Initialize manager
     └─► Ready for strip addition
            │
            ▼
User adds strips (via console or code)
     │
     ├─► addStrip(pin, count, protocol)
     │      │
     │      ├─► Check resources (PIO/RMT/SPI available?)
     │      ├─► Create PhysicalStrip
     │      ├─► DriverFactory::createDriver()
     │      │      │
     │      │      ├─► [RP2040] PIO_NeoPixel_Serial
     │      │      │      │
     │      │      │      ├─► Allocate PIO state machine
     │      │      │      ├─► Allocate DMA channel
     │      │      │      ├─► Load PIO program
     │      │      │      └─► Allocate buffers (LED + DMA)
     │      │      │
     │      │      ├─► [ESP32] RMT_NeoPixel_Serial
     │      │      │      │
     │      │      │      ├─► Allocate RMT channel
     │      │      │      ├─► Configure encoder
     │      │      │      └─► Allocate LED buffer
     │      │      │
     │      │      └─► [SPI] HW_NeoPixel_SPI
     │      │             │
     │      │             ├─► Configure SPI bus
     │      │             └─► Allocate LED buffer
     │      │
     │      ├─► PhysicalStrip::init()
     │      └─► Add to _strips vector
     │
     ├─► addVirtualStrip(count, colorOrder)
     │      │
     │      ├─► Create VirtualStrip
     │      ├─► Allocate virtual LED buffer
     │      └─► Add to _virtualStrips vector
     │
     ├─► attachPhysicalToVirtual(vstrip, pstrip, offset)
     │      │
     │      └─► VirtualStrip::attachStrip()
     │             │
     │             └─► Store mapping (strip + offset)
     │
     └─► addSegment(vstrip, startLed, endLed)
            │
            ├─► Create Segment
            ├─► Initialize LedState
            └─► Add to _segments vector
```

### Runtime Update Flow (Auto-Update Mode)

```
Main Loop (openknx.loop())
     │
     ▼
NeoPixel::loop(configured)
     │
     ├─► Check if auto-update enabled
     ├─► Check if update interval elapsed
     └─► if (time_elapsed >= _updateInterval)
            │
            ▼
     NeoPixel::updateAll()
            │
            ▼
     NeoPixelManager::update(deltaTime)
            │
            ├─────────────────────────┐
            │                         │
            ▼                         ▼
     [1] Effect Updates          [2] Virtual Strip Mapping
            │                         │
     for (segment in _segments)   for (vstrip in _virtualStrips)
            │                         │
            ▼                         ▼
     Effect::update(segment, dt)  vstrip->mapToPhysical()
            │                         │
     segment->setPixel(...)           ├─► for (attached physical strips)
            │                         │      │
     vstrip->buffer[index] = color    │      ▼
            │                         │   Calculate mapped index:
            │                         │   mappedIdx = offset + localIdx
            │                         │      │
            │                         │      ▼
            │                         │   Copy color from virtual to physical:
            │                         │   pstrip->buffer[mappedIdx] = vstrip->buffer[idx]
            │                         │
            └─────────────────────────┴───────────┐
                                                  │
                                                  ▼
                                      [3] Hardware Transfer
                                                  │
                                  for (strip in _strips)
                                                  │
                                                  ├─► if (!strip->isBusy())
                                                  │      │
                                                  │      ▼
                                                  │   strip->show()
                                                  │      │
                    ┌─────────────────────────────┼──────┴───────────────────────────┐
                    │                             │                                  │
                    ▼                             ▼                                  ▼
         [RP2040] PIO Driver            [ESP32] RMT Driver               [Any] SPI Driver
                    │                             │                                  │
                    ▼                             ▼                                  ▼
    packDataToDMABuffer()            rmt_transmit()                   SPI.transfer()
                    │                             │                                  │
         Pack RGB → 32-bit words          Encode to RMT format        Send via SPI bus
                    │                             │                                  │
                    ▼                             ▼                                  ▼
    dma_channel_start()                 RMT Hardware                 SPI Hardware
                    │                             │                                  │
         Non-blocking!                   Non-blocking!                   Blocking
                    │                             │                                  │
                    ▼                             ▼                                  ▼
         DMA → PIO FIFO                 RMT → GPIO                    SPI → GPIO
                    │                             │                                  │
                    ▼                             ▼                                  ▼
              PIO → GPIO                    LED Strip                   LED Strip
                    │
                    ▼
               LED Strip
                    │
                    ▼
         DMA interrupt fires
                    │
                    ▼
         Set busy = false
                    │
                    ▼
         Ready for next frame
```

### Effect Update Detail

```
Effect::update(segment, deltaTime)
     │
     ├─► Read segment state:
     │      ├─► LedState& state = segment->getState()
     │      ├─► r, g, b, w, brightness, speed, etc.
     │      └─► Effect-specific state variables
     │
     ├─► Calculate effect:
     │      │
     │      ├─► [Solid] memset all pixels to (r,g,b)
     │      │
     │      ├─► [Rainbow] HSV → RGB for each pixel
     │      │      │
     │      │      ├─► hue = (hue + speed * deltaTime) % 256
     │      │      └─► for (i = 0; i < length; i++)
     │      │             hue_i = (hue + i * 255 / length) % 256
     │      │             rgb = HSV2RGB(hue_i, 255, brightness)
     │      │             segment->setPixel(i, rgb.r, rgb.g, rgb.b)
     │      │
     │      ├─► [Cylon] Bouncing dot
     │      │      │
     │      │      ├─► Update position: pos += speed * direction
     │      │      ├─► if (pos >= length || pos <= 0) reverse direction
     │      │      ├─► Clear all pixels
     │      │      ├─► Draw dot at position with fade trail
     │      │      └─► segment->setPixel(pos, r, g, b)
     │      │
     │      └─► [FastLED effects] Use FastLED math functions
     │             │
     │             ├─► beat8(), beatsin8(), random8()
     │             ├─► qadd8(), qsub8(), scale8()
     │             └─► Complex patterns with optimized math
     │
     └─► Update segment state:
            │
            └─► Save any state changes back to segment
                   (e.g., current hue, position, direction)
```

### Virtual to Physical Mapping

```
VirtualStrip::mapToPhysical()
     │
     ├─► Get virtual buffer: uint8_t* vbuf = _buffer
     ├─► Get bytes per LED: uint8_t bpl = _bytesPerLed
     │
     └─► for (mapping in _attachedStrips)
            │
            ├─► Get physical strip: PhysicalStrip* pstrip = mapping.strip
            ├─► Get offset in virtual: uint16_t offset = mapping.offset
            ├─► Get physical buffer: uint8_t* pbuf = pstrip->getBuffer()
            ├─► Get physical LED count: uint16_t pcount = pstrip->getLedCount()
            │
            └─► for (i = 0; i < pcount; i++)
                   │
                   ├─► Virtual index: vidx = (offset + i) * bpl
                   ├─► Physical index: pidx = i * bpl
                   │
                   └─► Copy color:
                          │
                          ├─► pbuf[pidx]     = vbuf[vidx]      (G or R)
                          ├─► pbuf[pidx + 1] = vbuf[vidx + 1]  (R or G)
                          ├─► pbuf[pidx + 2] = vbuf[vidx + 2]  (B)
                          └─► if (bpl == 4)
                                 pbuf[pidx + 3] = vbuf[vidx + 3]  (W)

Example:
  VirtualStrip: 72 LEDs (GRB)
  PhysicalStrip 0: 8 LEDs at offset 0
  PhysicalStrip 1: 64 LEDs at offset 8

  Virtual buffer: [G0,R0,B0, G1,R1,B1, ..., G71,R71,B71]
                   └─────┬─────┘  └──────────────┬──────────────┘
                         │                       │
                   Maps to Strip 0         Maps to Strip 1
                   (LEDs 0-7)              (LEDs 8-71)
                         │                       │
                         ▼                       ▼
  Physical buffers:                      
  Strip 0: [G0,R0,B0, G1,R1,B1, ..., G7,R7,B7]
  Strip 1: [G8,R8,B8, G9,R9,B9, ..., G71,R71,B71]
```

---

## Memory Layout

### Physical Strip Memory (RP2040 with DMA)

```
PhysicalStrip Object (Stack/Heap)
├─ _driver              (pointer, 4 bytes)
├─ _dataPin             (uint32_t, 4 bytes)
├─ _clockPin            (uint32_t, 4 bytes)
├─ _ledCount            (uint16_t, 2 bytes)
├─ _protocol            (enum, 1 byte)
├─ _initialized         (bool, 1 byte)
├─ padding              (2 bytes for alignment)
└─ Total: ~18 bytes

IHardwareDriver (PIO_NeoPixel_Serial)
├─ vtable pointer       (4 bytes)
├─ _inst pointer        (4 bytes)
└─ Total: ~8 bytes

pio_neopixel_serial_inst
├─ pio                  (pointer, 4 bytes)
├─ sm                   (uint, 4 bytes)
├─ offset               (uint, 4 bytes)
├─ pin                  (uint, 4 bytes)
├─ ledCount             (uint16_t, 2 bytes)
├─ bytesPerLed          (uint8_t, 1 byte)
├─ protocol             (enum, 1 byte)
├─ colorOrder           (enum, 1 byte)
├─ frequency            (uint32_t, 4 bytes)
├─ buffer               (pointer, 4 bytes) ──► [LED Buffer]
├─ bufferSize           (size_t, 4 bytes)
├─ dmaBuffer            (pointer, 4 bytes) ──► [DMA Buffer]
├─ dmaBufferSize        (size_t, 4 bytes)
├─ dmaChannel           (int, 4 bytes)
├─ useDMA               (bool, 1 byte)
├─ initialized          (bool, 1 byte)
├─ busy                 (bool, 1 byte)
├─ padding              (1 byte)
└─ Total: ~52 bytes

LED Buffer (Heap)
├─ For 100 RGB LEDs:    100 × 3 = 300 bytes
└─ For 100 RGBW LEDs:   100 × 4 = 400 bytes

DMA Buffer (Heap) - if DMA enabled
├─ Always 32-bit words: N × 4 bytes
├─ For 100 LEDs:        100 × 4 = 400 bytes
└─ Total overhead:      133% of LED buffer size!

Total for 100 RGB LEDs with DMA:
  = 18 + 8 + 52 + 300 + 400
  = 778 bytes
```

### Virtual Strip Memory

```
VirtualStrip Object
├─ _buffer              (pointer, 4 bytes) ──► [Virtual Buffer]
├─ _totalLeds           (uint16_t, 2 bytes)
├─ _bytesPerLed         (uint8_t, 1 byte)
├─ _colorOrder          (enum, 1 byte)
├─ _attachedStrips      (vector) ──► [Mappings]
│  ├─ pointer           (4 bytes)
│  ├─ size              (4 bytes)
│  └─ capacity          (4 bytes)
├─ padding              (2 bytes)
└─ Total: ~26 bytes

Virtual Buffer (Heap)
├─ For 72 RGB LEDs:     72 × 3 = 216 bytes
└─ For 72 RGBW LEDs:    72 × 4 = 288 bytes

Attached Strips Mapping (per strip)
├─ strip pointer        (4 bytes)
├─ offset               (uint16_t, 2 bytes)
├─ padding              (2 bytes)
└─ Total per mapping: 8 bytes

Example: 72 RGB LEDs, 2 physical strips attached
  = 26 + 216 + (2 × 8)
  = 258 bytes
```

### Segment Memory

```
Segment Object
├─ _virtualStrip        (pointer, 4 bytes)
├─ _startLed            (uint16_t, 2 bytes)
├─ _endLed              (uint16_t, 2 bytes)
├─ _effect              (pointer, 4 bytes)
├─ _state               (LedState, 48 bytes)
│  ├─ r, g, b, w        (4 × 1 byte)
│  ├─ brightness        (uint8_t, 1 byte)
│  ├─ speed             (uint8_t, 1 byte)
│  ├─ direction         (int8_t, 1 byte)
│  ├─ position          (uint16_t, 2 bytes)
│  ├─ hue               (uint8_t, 1 byte)
│  ├─ saturation        (uint8_t, 1 byte)
│  ├─ effectState       (uint32_t, 4 bytes)
│  ├─ lastUpdate        (uint32_t, 4 bytes)
│  └─ padding           (~28 bytes)
├─ padding              (~8 bytes)
└─ Total: ~72 bytes
```

### Effect System Memory

```
Effect Objects (Stateless Singletons)
├─ EffectSolid          (~8 bytes)
├─ EffectWipe           (~8 bytes)
├─ RainbowEffect        (~8 bytes)
├─ PrideEffect          (~8 bytes)
├─ JuggleEffect         (~8 bytes)
├─ BPMEffect            (~8 bytes)
├─ CylonEffect          (~8 bytes)
├─ ... (33 effects total)
└─ Total: ~264 bytes

EffectPool
├─ Static effect instances (64 bytes)
├─ Lookup table            (~32 bytes)
└─ Total: ~96 bytes

Note: Effects are SHARED by all segments!
      Single instance per effect type.
      Segment state stored in segment, not effect.
```

### Total System Memory Example

```
Configuration:
- 3 Physical strips (RP2040 with DMA)
  - Strip 0: 100 LEDs RGB
  - Strip 1: 64 LEDs RGB
  - Strip 2: 8 LEDs RGB
- 1 Virtual strip: 172 LEDs RGB
- 3 Segments

Breakdown:
├─ NeoPixelManager              ~200 bytes
├─ Physical Strips (3×)
│  ├─ Strip 0 (100 LEDs)        778 bytes
│  ├─ Strip 1 (64 LEDs)         565 bytes
│  └─ Strip 2 (8 LEDs)          162 bytes
│  └─ Subtotal:                1505 bytes
├─ Virtual Strip (172 LEDs)     258 bytes
├─ Segments (3×)                216 bytes
├─ Effect System                ~100 bytes
└─ Vectors overhead             ~50 bytes

TOTAL RAM USAGE: ~2329 bytes (2.3 KB)

Without DMA (LED buffer only):
- Strip 0: 378 bytes (-400 bytes)
- Strip 1: 301 bytes (-264 bytes)
- Strip 2: 98 bytes (-64 bytes)
- Total: ~1601 bytes (1.6 KB)
- Savings: ~728 bytes (31% reduction)
```

---

## Effect System

### Stateless Design Pattern

```
┌─────────────────────────────────────────────────────────────┐
│                     EffectPool (Singleton)                  │
│                                                             │
│  static EffectSolid      solidEffect;     // ONE instance   │
│  static RainbowEffect    rainbowEffect;   // ONE instance   │
│  static CylonEffect      cylonEffect;     // ONE instance   │
│  // ... all effects ...                                     │
│                                                             │
│  Effect* getEffect(uint8_t id) {                            │
│      switch (id) {                                          │
│          case 0: return &solidEffect;                       │
│          case 2: return &rainbowEffect;                     │
│          case 6: return &cylonEffect;                       │
│      }                                                       │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │Segment 0 │    │Segment 1 │    │Segment 2 │
    │          │    │          │    │          │
    │effect:───┼───►│effect:───┼───►│effect:   │
    │          │ │  │          │ │  │(nullptr) │
    │state:    │ │  │state:    │ │  │state:    │
    │ r=255    │ │  │ hue=0    │ │  │ r=0      │
    │ g=0      │ │  │ speed=5  │ │  │ g=0      │
    │ b=0      │ │  │ dir=1    │ │  │ b=255    │
    └──────────┘ │  └──────────┘ │  └──────────┘
                 │                │
                 └────────────────┘
                   SAME INSTANCE!

Benefits:
1. Memory efficient: ONE effect object shared by ALL segments
2. Zero allocation during runtime
3. Cache-friendly (hot effect code stays in cache)
4. Easy to add new effects (just add to pool)

Traditional (stateful) design would require:
  Effect object PER segment = N × sizeof(Effect)
  
Stateless design:
  Effect object SHARED = 1 × sizeof(Effect)
  
For 10 segments with Rainbow effect:
  Traditional: 10 × ~100 bytes = 1000 bytes
  Stateless:    1 × ~8 bytes   = 8 bytes
  Savings: 992 bytes (99% reduction!)
```

### Effect Update Flow

```
Manager calls: effect->update(segment, deltaTime)
     │
     ▼
┌──────────────────────────────────────────────────────────┐
│  Effect::update(Segment* segment, uint32_t deltaTime)   │
│                                                          │
│  1. Read state from segment                              │
│     ├─► LedState& state = segment->getState()           │
│     ├─► uint8_t r = state.r                             │
│     ├─► uint8_t hue = state.hue                         │
│     └─► uint32_t lastUpdate = state.lastUpdate          │
│                                                          │
│  2. Calculate effect-specific animation                  │
│     ├─► [Rainbow] Advance hue based on deltaTime        │
│     │   hue = (hue + speed * deltaTime / 10) % 256      │
│     │                                                     │
│     ├─► [Cylon] Update position and direction           │
│     │   position += speed * direction                    │
│     │   if (position >= length) direction = -1          │
│     │                                                     │
│     └─► [FastLED] Use optimized math                    │
│         value = beat8(bpm) + phase                       │
│                                                          │
│  3. Render to segment pixels                             │
│     └─► for (i = 0; i < segment->getLength(); i++)     │
│            rgb = calculateColor(i)                       │
│            segment->setPixel(i, rgb.r, rgb.g, rgb.b)    │
│                                                          │
│  4. Save state back to segment                           │
│     ├─► state.hue = hue                                 │
│     ├─► state.position = position                       │
│     └─► state.lastUpdate = currentTime                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
     │
     ▼
Pixels written to VirtualStrip buffer
     │
     ▼
VirtualStrip::mapToPhysical()
     │
     ▼
PhysicalStrip::show()
```

---

## Effektmanager (Cue Sequencer)

The Effektmanager (EM) is a per-segment sequencer that applies a chain of effect presets (cues) over time. EM configuration lives in KNX flash (ETS); runtime state lives in RAM.

### Data Hierarchy

```
┌──────────────────────────────────────────────────────────────┐
│              EffektManagerData[16]  (KNX flash, ~68 KB)        │
│                                                               │
│  EffektManagerHeader (20 B)                                   │
│    ├─ name[16]      ETS description                           │
│    ├─ cueCount      active cues (1..10)                       │
│    ├─ loop:1        restart at cue 1 when done                │
│    ├─ nextEmId      chain target (0=stop, 1..16)              │
│    └─ enabled       0=off, 1=on                               │
│                                                               │
│  EffektCue cues[10]  (48 B each)                              │
│    ├─ effectId            which effect                        │
│    ├─ params[10]          effect parameters                  │
│    ├─ r,g,b,w             primary colour                     │
│    ├─ brightness          0..255                             │
│    ├─ durationSec         0 = hold until trigger             │
│    ├─ fadeMs              fade-out before next cue            │
│    ├─ cueName[14]         label                              │
│    └─ effectText[14]      e.g. ScrollText content            │
└──────────────────────────────────────────────────────────────┘
           │  applyCue(cue, segment)
           ▼
   Segment: setEffect() + params + colour + brightness + text
```

### Sequencer State Machine (per segment, RAM only)

```
        start(emId)
            │
            ▼
     ┌─────────────┐  duration elapsed   ┌──────────────┐
     │  PLAY CUE   │────────────────────►│   FADE-OUT   │
     │ activeCue=n │                     │  (cue.fadeMs)│
     └─────┬───────┘                     └──────┬───────┘
           │ triggerCue(x)                      │ fade done
           │ (jump)                             ▼
           │                          ┌────────────────────┐
           │                          │  advanceToNextCue  │
           │                          └─────────┬──────────┘
           │                                    │
           │              ┌──────────────┬──────┴───────┐
           │              │ more cues    │ last cue     │
           │              ▼              ▼              ▼
           │        next cue n+1    loop? → cue 1   nextEmId? → chain EM
           │                                          else → STOP
           └──────────────────────────────────────────────►

  Notes:
   • start() interrupts any running EM immediately (Variante A)
   • if segment is in an Effektkette → chain is paused during EM, resumed on stop
   • last EM/cue is saved for power-off restore (restart from cue 1)
```

---

## 2D / 3D Matrix Geometry

The LED chain is always 1D on the wire. `Segment` adds a pure-software geometry layer that maps `(x,y)` / `(x,y,z)` coordinates to the linear pixel index according to the configured wiring topology.

```
 setGeometry(width, height[, depth], topology)
                       │
                       ▼
   ┌──────────────────────────────────────────────────────────┐
   │ Segment::update()  — automatic dispatch                  │
   │                                                          │
   │   is3D() && effect supports 3D ──► update3D(seg, dt)     │
   │   is2D() && effect supports 2D ──► update2D(seg, dt)     │
   │   otherwise                     ──► update(seg, dt)  (1D) │
   └──────────────────────────────────────────────────────────┘
                       │ setPixelXY(x,y) / setPixelXYZ(x,y,z)
                       ▼
        xyToIndex() / xyzToIndex()  — topology mapping
                       │
                       ▼
            linear pixel index in VirtualStrip buffer

 Topologies:
   LINEAR_1D            no matrix (default)
   ROWS_SERPENTINE      →→ ←← →→   (most WS2812B panels)
   ROWS_LINEAR          →→ →→ →→
   COLS_SERPENTINE      ↓↑↓↑
   COLS_LINEAR          ↓↓↓↓
   ROWS_SERPENTINE_3D   3D volume, serpentine rows
   COLS_LINEAR_TILED    tiled panel chain, columns linear per block
   COLS_SERP_TILED      tiled panel chain, columns serpentine per block
```

Existing 1D effects work unchanged on a 2D segment — they are rendered line by line. 2D-aware effects (Fire, Noise, Cylon are geometry-aware; ScrollText, Clock2D and the other `*2D` effects are 2D-only) advertise `DIM_2D` capability and receive `update2D()`.

---

## Effektkette (Distributed Rendering)

Several KNX devices render one logical effect across a single **virtual band**. No pixels are streamed over the bus — each device computes the full effect but only draws its local window.

```
  Virtual band: total length = 300

  ┌─────────────── Device A (Master) ───────────────┐
  │ setVirtualBand(total=300, offset=0)             │
  │ effect computes f(t, 300), draws [0..99]        │
  └───────────────┬─────────────────────────────────┘
                  │ effect KO changes
                  ▼
         6-byte sync telegram (K40 group)
          ┌───────┴────────┐
          ▼                ▼
  ┌── Device B (Slave) ──┐  ┌── Device C (Slave) ──┐
  │ offset=100           │  │ offset=200           │
  │ draws [100..199]     │  │ draws [200..299]      │
  │ watchdog: off after  │  │ watchdog: off after   │
  │ sync timeout         │  │ sync timeout          │
  └──────────────────────┘  └───────────────────────┘

  getLength() returns 300 on every device, so the effect math is
  identical everywhere. setPixel(i,...) silently drops pixels that
  fall outside the device's local window → zero effect code changes.
```

---

## Hardware Abstraction

### Driver Selection Flow

```
addStrip(pin, count, protocol, driverType)
     │
     ▼
checkResourcesAvailable(protocol)
     │
     ├─► Count used resources:
     │   ├─► PIO state machines (RP2040/2350)
     │   ├─► RMT channels (ESP32)
     │   └─► SPI buses (all platforms)
     │
     └─► if (resources exhausted) return nullptr
            │
            ▼
DriverFactory::createDriver(pin, count, protocol, driverType)
     │
     ├─► if (driverType == AUTO)
     │      │
     │      ├─► if (ProtocolHelper::is1Wire(protocol))
     │      │      │
     │      │      ├─► [RP2040/2350] Use PIO
     │      │      └─► [ESP32] Use RMT
     │      │
     │      └─► if (ProtocolHelper::isSPI(protocol))
     │             │
     │             └─► Use Hardware SPI (default)
     │
     ├─► if (driverType == SERIAL_1WIRE)
     │      │
     │      ├─► [RP2040/2350] Force PIO
     │      └─► [ESP32] Force RMT
     │
     ├─► if (driverType == SPI_HARDWARE)
     │      │
     │      └─► Use Hardware SPI
     │
     └─► if (driverType == SPI_PIO)
            │
            └─► [RP2040/2350 only] Use PIO-based SPI
```

### PIO Driver Architecture (RP2040/2350)

```
┌──────────────────────────────────────────────────────────┐
│              PIO_NeoPixel_Serial Driver                  │
│                                                          │
│  Constructor:                                            │
│  ├─► Allocate pio_neopixel_serial_inst                  │
│  ├─► Allocate LED buffer (N × bytesPerLed)              │
│  └─► Allocate DMA buffer (N × 4 bytes)                  │
│                                                          │
│  init():                                                 │
│  ├─► Find available PIO (pio0, pio1, pio2)              │
│  ├─► Find available state machine (0-3)                 │
│  ├─► Load PIO program into instruction memory           │
│  ├─► Configure state machine                            │
│  │   ├─► Autopull: 24 bits (RGB) or 32 bits (RGBW)     │
│  │   ├─► Shift direction: LEFT (MSB first!)            │
│  │   ├─► Clock divider: 125MHz / 15.625 = 8MHz         │
│  │   └─► Side-set pin: GPIO output                     │
│  ├─► Find available DMA channel (if DMA enabled)        │
│  └─► Configure DMA                                      │
│      ├─► Source: dmaBuffer address                      │
│      ├─► Dest: PIO FIFO address                         │
│      ├─► Transfer count: ledCount × 1 word              │
│      └─► Enable IRQ on completion                       │
│                                                          │
│  show():                                                 │
│  ├─► if (DMA mode)                                      │
│  │   ├─► packDataToDMABuffer()                          │
│  │   │   └─► Convert RGB bytes → 32-bit words          │
│  │   │       (reverse byte order for LSB-first)        │
│  │   └─► dma_channel_start()                           │
│  │       └─► Non-blocking! Returns immediately         │
│  │                                                       │
│  └─► if (PIO mode, no DMA)                             │
│      └─► sendDataPIO()                                  │
│          └─► pio_sm_put_blocking() for each LED        │
│              └─► Blocking! Waits for PIO FIFO          │
│                                                          │
│  DMA IRQ Handler:                                        │
│  └─► On completion: set busy = false                    │
│                                                          │
└──────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────────────────────────────────┐
│                  PIO Hardware                            │
│                                                          │
│  State Machine (SM 0-3):                                 │
│  ├─► Instruction memory: 4 instructions                 │
│  │   ├─► 0: out x, 1   side 1 [2]  (pull bit, HIGH)    │
│  │   ├─► 1: jmp !x, 3  side 1 [2]  (test bit)          │
│  │   ├─► 2: jmp 0      side 0 [3]  (1-bit: LOW 4 cyc)  │
│  │   └─► 3: nop        side 0 [6]  (0-bit: LOW 7 cyc)  │
│  │                                                       │
│  ├─► TX FIFO (8 words):                                 │
│  │   └─► Fed by DMA or CPU                              │
│  │                                                       │
│  ├─► Output Shift Register:                             │
│  │   ├─► Autopull from FIFO                             │
│  │   ├─► Shift left (MSB first)                         │
│  │   └─► Output 1 bit per instruction cycle             │
│  │                                                       │
│  └─► Side-Set Pin:                                      │
│      └─► GPIO output (controlled by 'side' bits)        │
│                                                          │
└──────────────────────────────────────────────────────────┘
              │
              ▼
         WS2812B LED Strip
```

### RMT Driver Architecture (ESP32-S3)

```
┌──────────────────────────────────────────────────────────┐
│             RMT_NeoPixel_Serial Driver                   │
│                                                          │
│  Constructor:                                            │
│  ├─► Allocate rmt_neopixel_serial_inst                  │
│  └─► Allocate LED buffer (N × bytesPerLed)              │
│                                                          │
│  init():                                                 │
│  ├─► Find available RMT channel (0-3, skip 0)           │
│  ├─► Configure RMT TX channel                           │
│  │   ├─► GPIO pin                                       │
│  │   ├─► Resolution: 25ns (40MHz)                       │
│  │   ├─► Memory block size                              │
│  │   └─► Enable DMA if available                        │
│  └─► Create LED strip encoder                           │
│      ├─► Protocol timing (T0H, T0L, T1H, T1L)          │
│      └─► Reset timing                                    │
│                                                          │
│  show():                                                 │
│  ├─► rmt_transmit()                                     │
│  │   ├─► Source: LED buffer                             │
│  │   ├─► Length: ledCount × bytesPerLed                 │
│  │   └─► Non-blocking! Returns immediately             │
│  └─► Register callback to clear busy flag               │
│                                                          │
└──────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────────────────────────────────┐
│                  RMT Hardware                            │
│                                                          │
│  RMT Channel (0-3):                                      │
│  ├─► Encoder converts RGB bytes → RMT pulses            │
│  │   ├─► 0-bit: T0H=350ns HIGH, T0L=800ns LOW          │
│  │   └─► 1-bit: T1H=700ns HIGH, T1L=600ns LOW          │
│  │                                                       │
│  ├─► Memory buffer (64 words per channel)               │
│  │   └─► Fed by DMA or CPU                              │
│  │                                                       │
│  └─► TX state machine:                                   │
│      ├─► Generate pulses based on timing                │
│      └─► Output to GPIO                                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
              │
              ▼
         WS2812B LED Strip
```

---

## GPIO Optimizations

### Signal Quality Improvements

```
┌──────────────────────────────────────────────────────────────────┐
│                   GPIO Optimization Pipeline                     │
│                                                                  │
│  Before PIO Initialization:                                      │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  1. gpio_put(pin, LOW)                                     │  │
│  │     - Set pin to known state                               │  │
│  │     - Prevents glitches during PIO takeover                │  │
│  │     - Eliminates startup flicker                           │  │
│  └────────────────────────────────────────────────────────────┘  │
│            │                                                     │
│            ▼                                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  2. gpio_set_drive_strength(pin, 12mA)                     │  │
│  │     - Default: 4mA (insufficient for long cables)          │  │
│  │     - 12mA: 3x stronger signal                             │  │
│  │     - Critical for SPI >5 MHz                              │  │
│  │     - Tested stable over 5m cables                         │  │
│  └────────────────────────────────────────────────────────────┘  │
│            │                                                     │
│            ▼                                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  3. gpio_set_slew_rate(pin, FAST)                          │  │
│  │     - Default: SLOW (rounded edges)                        │  │
│  │     - FAST: Sharp clock edges                              │  │
│  │     - Reduced distortion at high frequencies               │  │
│  │     - Precise WS2812B timing (+-150ns tolerance)           │  │
│  └────────────────────────────────────────────────────────────┘  │
│            │                                                     │
│            ▼                                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  4. pio_gpio_init(pio, pin)                                │  │
│  │     - PIO takes over GPIO                                   │  │
│  │     - Optimizations preserved                               │  │
│  │     - Clean signal from first transmission                  │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Applied To

```
SPI Strips (APA102/SK9822):
  ┌─────────────────────────────────────────────┐
  │  CLK  Pin (Clock)     - 12mA, FAST, LOW    │
  │  MOSI Pin (Data)      - 12mA, FAST, LOW    │
  │                                             │
  │  Tested Frequencies:                        │
  │    3 MHz   - Stable over 5m                 │
  │    7.5 MHz - Stable over 3m                 │
  │    20 MHz  - Stable over 1m                 │
  └─────────────────────────────────────────────┘

Serial Strips (WS2812B/SK6812):
  ┌─────────────────────────────────────────────┐
  │  DATA Pin             - 12mA, FAST, LOW    │
  │                                             │
  │  Timing Precision:                          │
  │    T0H: 400ns ± 150ns (meets spec)         │
  │    T0L: 850ns ± 150ns (meets spec)         │
  │    T1H: 800ns ± 150ns (meets spec)         │
  │    T1L: 450ns ± 150ns (meets spec)         │
  └─────────────────────────────────────────────┘
```

### CPU Frequency Adaptation

```
┌──────────────────────────────────────────────────────────────────┐
│                  Dynamic Clock Divider Calculation                │
│                                                                  │
│  clkdiv = CPU_freq / (cycles_per_bit × target_freq)             │
│                                                                  │
│  Examples:                                                        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  SPI @ 2 MHz:                                              │  │
│  │    RP2040  @ 125 MHz → clkdiv = 31.25                     │  │
│  │    RP2350  @ 150 MHz → clkdiv = 37.50                     │  │
│  │    RP2350  @ 300 MHz → clkdiv = 75.00                     │  │
│  │    (Output frequency constant: 2 MHz)                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  WS2812B @ 800 kHz:                                        │  │
│  │    RP2040  @ 125 MHz → clkdiv = 78.125                    │  │
│  │    RP2350  @ 150 MHz → clkdiv = 93.750                    │  │
│  │    RP2350  @ 300 MHz → clkdiv = 187.500                   │  │
│  │    (Output frequency default:  800 kHz)                    │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Implementation: clock_get_hz(clk_sys) at runtime               │
└──────────────────────────────────────────────────────────────────┘
```

---

## Update Loop

### Main Loop Timing Diagram

```
Time: 0ms    16ms   32ms   48ms   64ms   80ms   96ms
      │      │      │      │      │      │      │
      ▼      ▼      ▼      ▼      ▼      ▼      ▼
Loop: ├──────┼──────┼──────┼──────┼──────┼──────┤
      │      │      │      │      │      │      │
      │ Effect updates (1.2ms)                   │
      │ Virtual mapping (0.8ms)                  │
      │ Hardware transfer (0.0ms - non-blocking!)│
      │ OpenKNX tasks (0.4ms)                    │
      │ Idle (13.6ms)                            │
      │      │      │      │      │      │      │
      └──────┴──────┴──────┴──────┴──────┴──────┘

Frame Rate: 62.5 FPS (16ms interval)
CPU Usage:  15% (2.4ms active / 16ms total)
```

### Detailed Timing Breakdown

```
┌────────────────────────────────────────────────────────────┐
│  Frame N (16ms window)                                     │
│                                                            │
│  0.0ms ├─► loop() starts                                  │
│        │                                                   │
│  0.1ms ├─► Check auto-update enabled                      │
│        ├─► Check update interval elapsed                  │
│        └─► if (elapsed) call updateAll()                  │
│           │                                                │
│  0.2ms    ├─► NeoPixelManager::update(deltaTime)         │
│           │   │                                            │
│  0.3ms    │   ├─► Segment 0: Effect update                │
│           │   │   └─► RainbowEffect (HSV calc)            │
│  0.7ms    │   ├─► Segment 1: Effect update                │
│           │   │   └─► SolidEffect (memset)                │
│  0.8ms    │   ├─► Segment 2: Effect update                │
│           │   │   └─► CylonEffect (position calc)         │
│           │   │                                            │
│  1.4ms    │   └─► All segments updated                    │
│           │                                                │
│  1.5ms    ├─► VirtualStrip 0: mapToPhysical()            │
│           │   ├─► Copy 72 LEDs × 3 bytes                  │
│           │   └─► memcpy to physical buffers              │
│           │                                                │
│  2.3ms    ├─► PhysicalStrip 0: show()                    │
│           │   └─► packDataToDMABuffer() (20µs)           │
│           │   └─► dma_channel_start() (non-blocking!)    │
│           │                                                │
│  2.4ms    ├─► PhysicalStrip 1: show()                    │
│           │   └─► DMA transfer (parallel!)                │
│           │                                                │
│  2.4ms    └─► updateAll() complete                        │
│           │                                                │
│  2.5ms ├─► OpenKNX framework tasks                       │
│        │   ├─► KNX bus processing                         │
│        │   ├─► GroupObject updates                        │
│        │   └─► Other module processing                    │
│        │                                                   │
│  2.8ms └─► All tasks complete                             │
│        │                                                   │
│  2.8ms ├─── IDLE ──────────────────────────────────────┐  │
│        │                                               │  │
│ 16.0ms └───────────────────────────────────────────────┘  │
│        │                                                   │
│ 16.0ms ├─► Frame N+1 starts                               │
│                                                            │
└────────────────────────────────────────────────────────────┘

Meanwhile (parallel to CPU):
├─── DMA Channel 0 transferring Strip 0 data (2.4ms - 5.2ms)
└─── DMA Channel 1 transferring Strip 1 data (2.4ms - 10.8ms)

Hardware Efficiency:
- CPU active: 2.8ms (17.5%)
- DMA active: 10.8ms (67.5%)
- Combined utilization: 85%
- CPU idle while DMA transfers!
```

### Effect Update Optimization

```
Effect Update (per segment):

Solid Effect:
┌──────────────────────────────────────┐
│ Time: 0.1ms                          │
│ ├─► memset(buffer, color, length)   │ ← Fast!
│ └─► No calculations                  │
└──────────────────────────────────────┘

Rainbow Effect:
┌──────────────────────────────────────┐
│ Time: 0.3ms                          │
│ ├─► hue = (hue + speed) % 256       │
│ └─► for (36 LEDs)                    │
│     ├─► Calculate local hue          │
│     ├─► HSV2RGB conversion           │ ← Math intensive
│     │   └─► Uses FastLED functions   │
│     └─► setPixel(i, r, g, b)        │
└──────────────────────────────────────┘

Cylon Effect:
┌──────────────────────────────────────┐
│ Time: 0.5ms                          │
│ ├─► Update position                  │
│ ├─► Check bounds, reverse direction  │
│ ├─► Clear all pixels                 │ ← memset
│ └─► Draw dot with fade trail         │
│     └─► 3-5 setPixel calls           │
└──────────────────────────────────────┘

Optimization Tips:
1. Solid is fastest (memset)
2. Rainbow scales with LED count (HSV math per LED)
3. Cylon scales with LED count (clear + draw)
4. Use FastLED math for efficiency (beat8, scale8, etc.)
```

---

## Conclusion

This architecture provides:

1. **Clean Separation**: Hardware, logic, and effects are independent
2. **Efficiency**: Zero-CPU LED updates with DMA/RMT
3. **Flexibility**: Virtual strips enable complex compositions
4. **Scalability**: Stateless effects use minimal memory
5. **Extensibility**: Easy to add new effects and protocols

The design balances performance, memory usage, and code maintainability.