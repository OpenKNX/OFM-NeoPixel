# FastLED Effects Porting Guide

**Complete guide for porting FastLED effects to OFM-NeoPixel**

**Version:** 0.0.2  
**Date:** 2025-11-06  
**Author:** Copyright (c) 2025 Erkan Çolak - OpenKNX

---

## Table of Contents

- [Overview](#overview)
- [Architecture Comparison](#architecture-comparison)
- [Parameter System](#parameter-system)
- [Ported Effects](#ported-effects)
- [FastLED Math Library](#fastled-math-library)
- [Step-by-Step Porting Guide](#step-by-step-porting-guide)
- [Migration Examples](#migration-examples)
- [Performance Considerations](#performance-considerations)
- [Testing Ported Effects](#testing-ported-effects)
- [Common Pitfalls](#common-pitfalls)
- [FAQ](#faq)

---

## Overview

OFM-NeoPixel includes a **stateless effect system** inspired by FastLED but optimized for OpenKNX devices. This guide explains how to port FastLED effects and understand the architectural differences.

### Why Port FastLED Effects?

**FastLED Strengths:**
- Huge library of proven effects
- Optimized math functions
- Well-documented patterns
- Active community

**OFM-NeoPixel Advantages:**
- Stateless design (96% memory savings)
- Self-describing parameter API
- Hardware acceleration (PIO/DMA/RMT)
- Segment-based architecture
- OpenKNX integration

**Best of Both Worlds:**
- Use FastLED's effects library
- Run on OFM-NeoPixel's efficient platform
- Keep FastLED's math functions
- Add OpenKNX integration

---

## Architecture Comparison

### FastLED (Global State)

```cpp
CRGB leds[NUM_LEDS];
uint8_t gHue = 0;

void loop() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    FastLED.show();
    gHue++;
}
```

### OFM-NeoPixel (Stateless Singleton)

```cpp
class RainbowEffect : public Effect {
public:
    void update(Segment* seg, uint32_t dt) override {
        auto& state = seg->getState();  // State in segment!
        for (uint16_t i = 0; i < seg->getLength(); i++) {
            uint8_t hue = state.aux1 + (i * 255 / seg->getLength());
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, 255);
            seg->setPixel(i, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
        state.aux1++;
    }
};
// 1 instance for 100 segments = 8 bytes (vs 100 instances = 800 bytes)
```

---

## Parameter System

**NEW in v0.0.2:** Self-describing effects via parameter introspection API.

### Why?

**Before:** Adding new effect -> modify Segment.h, Console, UI  
**After:** Effect describes its own parameters -> zero code changes

### API

```cpp
virtual uint8_t getParameterCount() const;
virtual const char* getParameterName(uint8_t idx) const;
virtual ParameterType getParameterType(uint8_t idx) const;
virtual uint32_t getParameter(const Segment* seg, uint8_t idx) const;
virtual void setParameter(Segment* seg, uint8_t idx, uint32_t val);
```

### Types

| Type | Range | UI |
|------|-------|-----|
| PARAM_UINT8 | 0-255 | Slider |
| PARAM_BOOL | 0/1 | Checkbox |
| PARAM_COLOR_RGB | 0xRRGGBB | Color Picker |
| PARAM_PERCENT | 0-100 | Slider % |
| PARAM_ENUM | Custom | Dropdown |

### Example

```cpp
class BPMEffect : public Effect {
    uint8_t getParameterCount() const override { return 2; }
    const char* getParameterName(uint8_t i) const override {
        return i == 0 ? "BPM" : "Hue";
    }
    uint32_t getParameter(const Segment* s, uint8_t i) const override {
        auto& st = s->getState();
        return i == 0 ? st.aux1 : st.aux2;
    }
    void setParameter(Segment* s, uint8_t i, uint32_t v) override {
        auto& st = s->getState();
        if (i == 0) st.aux1 = v; else st.aux2 = v;
    }
};
```

**Result:** Console/UI auto-generate controls for BPM and Hue.

---

## Architecture Comparison

### FastLED Architecture

```cpp
// FastLED: Global LED array
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    // Effect modifies global array directly
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    
    // FastLED handles hardware transfer
    FastLED.show();
    
    gHue++;  // Effect state stored globally
}
```

**Characteristics:**
- Global state (CRGB array)
- Direct array manipulation
- Global variables for effect state
- Single strip assumed

### OFM-NeoPixel Architecture

```cpp
// OFM-NeoPixel: Segment-based effects
class RainbowEffect : public Effect {
private:
    uint8_t _hueOffset;  // Effect state here (NOT in segment!)
    
public:
    void update(Segment* segment, uint32_t deltaTime) override {
        // Effect reads config from segment
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Effect writes to segment
        for (uint16_t i = 0; i < length; i++) {
            uint8_t hue = _hueOffset + (i * 255 / length);
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, config.intensity);
            segment->setPixel(i, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
        
        _hueOffset += config.speed;
    }
};

// Single instance shared by multiple segments!
RainbowEffect rainbowEffect;
```

**Characteristics:**
- Segment-based (multiple zones)
- Stateless effects (singleton instances)
- Effect state in effect class
- Config state in segment
- Multi-strip support

---

## Ported Effects

### Currently Ported Effects

| Effect                | FastLED Original         | Status    | Notes                       |
|-----------------------|-------------------------|-----------|-----------------------------|
| Rainbow               | fill_rainbow()          | Complete  | Classic rainbow gradient     |
| Pride2015             | Mark Kriegsman          | Complete  | Brightness waves             |
| Confetti              | ColorWavesWithPalettes  | Complete  | Random sparkles              |
| Juggle                | juggle()                | Complete  | Sine wave dots               |
| BPM                   | bpm()                   | Complete  | Pulsing colors               |
| Fire                  | Fire2012                | Complete  | Realistic fire simulation    |
| TheaterChase          | theaterChase()          | Complete  | Chasing lights               |
| TheaterChaseRainbow   | theaterChaseRainbow()   | Complete  | Chasing rainbow lights       |
| Sinelon               | sinelon()               | Complete  | Moving dot with trail        |
| Twinkle               | addGlitter()            | Complete  | Twinkling random pixels      |
| Comet                 | comet()                 | Complete  | Moving comet with tail       |
| Meteor                | meteorRain()            | Complete  | Meteor rain effect           |
| Noise                 | inoise8()               | Complete  | Perlin/simplex noise effect  |
| Palette               | ColorFromPalette        | Complete  | Color palette support        |
| Lightning             | lightning()             | Complete  | Lightning simulation         |
| Gradient              | fill_gradient()         | Complete  | Gradient fill effect         |

### Original/Extended Effects

| Effect         | Inspired By      | Status    | Notes                                 |
|---------------|------------------|-----------|---------------------------------------|
| Solid         | N/A              | Complete  | Static color                          |
| Cylon         | Knight Rider     | Complete  | Bouncing dot                          |
| Wipe          | Theater chase    | Complete  | Fill animation                        |
| RGBWTest      | N/A              | Complete  | SK6812 RGBW test effect               |
| GarageDoor    | N/A              | Complete  | Multi-phase garage animation          |
| Sparkle       | N/A              | Complete  | Random sparkles                       |
| Breathing     | N/A              | Complete  | Smooth fade in and out                |
| Strobe        | N/A              | Complete  | Strobe light effect                   |
| Pulse         | N/A              | Complete  | Adjustable pulse width effect         |

### Effects Planned or Not Yet Implemented



---

## FastLED Math Library

OFM-NeoPixel includes a **complete port of FastLED's math functions** in `FastLEDMath.h`.

### Included Functions

#### Sine Wave Functions
```cpp
uint8_t sin8(uint8_t theta);              // Fast 8-bit sine
uint16_t sin16(uint16_t theta);           // Fast 16-bit sine
uint8_t cos8(uint8_t theta);              // Fast 8-bit cosine
uint16_t cos16(uint16_t theta);           // Fast 16-bit cosine
```

#### Beat Functions
```cpp
uint8_t beat8(uint16_t bpm, uint32_t timebase = 0);
uint16_t beat16(uint16_t bpm, uint32_t timebase = 0);
uint8_t beatsin8(uint16_t bpm, uint8_t low, uint8_t high, uint32_t timebase = 0, uint8_t phase = 0);
uint16_t beatsin16(uint16_t bpm, uint16_t low, uint16_t high, uint32_t timebase = 0, uint16_t phase = 0);
uint16_t beatsin88(uint16_t bpm, uint16_t low = 0, uint16_t high = 65535, uint32_t timebase = 0, uint16_t phase = 0);
```

#### Scaling Functions
```cpp
uint8_t scale8(uint8_t i, uint8_t scale);
uint8_t scale8_video(uint8_t i, uint8_t scale);
void nscale8(uint8_t* arr, uint16_t len, uint8_t scale);
```

#### Color Functions
```cpp
uint32_t hsv2rgb_rainbow(uint8_t hue, uint8_t sat, uint8_t val, bool yellowBoost, bool greenCorr); // New overload with perceptual correction options
uint32_t hsv2rgb_rainbow(uint8_t hue, uint8_t sat, uint8_t val); // Backwards-compatible default (uses yellowBoost=true, greenCorr=false)
uint32_t hsv2rgb_spectrum(uint8_t hue, uint8_t sat, uint8_t val);
```

#### Math Helpers
```cpp
uint8_t qadd8(uint8_t i, uint8_t j);      // Saturating add
uint8_t qsub8(uint8_t i, uint8_t j);      // Saturating subtract
uint8_t avg8(uint8_t i, uint8_t j);       // Average of two values
uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB);
```

#### Random Functions
```cpp
uint8_t random8();
uint8_t random8(uint8_t lim);
uint8_t random8(uint8_t min, uint8_t max);
uint16_t random16();
uint16_t random16(uint16_t lim);
```

### Usage Example

```cpp
#include "effects/FastLEDMath.h"

void update(Segment* segment, uint32_t deltaTime) {
    // Use FastLED functions with namespace prefix
    uint8_t hue = FastLEDMath::beat8(60);  // 60 BPM
    uint8_t brightness = FastLEDMath::beatsin8(30, 50, 255);
    
    uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);
    
    segment->setPixel(0, 
        (rgb >> 16) & 0xFF,
        (rgb >> 8) & 0xFF,
        rgb & 0xFF);
}
```

---

## Step-by-Step Porting Guide

### Step 1: Analyze FastLED Effect

**Example: FastLED Rainbow**

```cpp
// FastLED original
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    FastLED.show();
    EVERY_N_MILLISECONDS(20) { gHue++; }
}
```

**Identify:**
1. **Effect state:** `gHue` (global variable)
2. **Parameters:** None (hardcoded)
3. **Update rate:** 20ms (50 FPS)
4. **FastLED functions used:** `fill_rainbow()`

---

### Step 2: Create Effect Class Skeleton

```cpp
// OFM-NeoPixel port
#pragma once
#include "Effect.h"
#include "FastLEDMath.h"
#include "../Segment.h"

class RainbowEffect : public Effect {
private:
    // Move global state to class member
    uint8_t _hueOffset;
    
public:
    RainbowEffect() : _hueOffset(0) {}
    
    void update(Segment* segment, uint32_t deltaTime) override {
        // Implement here
    }
    
    void reset() override {
        _hueOffset = 0;  // Reset state
    }
    
    const char* getName() override {
        return "Rainbow";
    }
};
```

---

### Step 3: Port Effect Logic

```cpp
void update(Segment* segment, uint32_t deltaTime) override {
    if (!segment) return;
    
    // Get segment info
    auto& config = segment->getConfig();
    uint16_t length = segment->getLength();
    
    // Use config.intensity as brightness (0-255)
    uint8_t brightness = config.intensity;
    
    // Port fill_rainbow logic
    for (uint16_t i = 0; i < length; i++) {
        // Calculate hue for this pixel
        uint8_t hue = _hueOffset + (i * 255 / length);
        
        // Use FastLEDMath instead of FastLED
        uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);
        
        // Write to segment (not global array!)
        segment->setPixel(i, 
            (rgb >> 16) & 0xFF,  // R
            (rgb >> 8) & 0xFF,   // G
            rgb & 0xFF);         // B
    }
    
    // Advance hue (use config.speed for control)
    _hueOffset += (config.speed > 0) ? config.speed : 1;
}
```

---

### Step 4: Add to Effect Pool

```cpp
// src/effects/EffectPool.h
#include "RainbowEffect.h"

namespace EffectPool {
    // Add singleton instance
    extern RainbowEffect rainbowEffect;
}

// src/effects/EffectPool.cpp
namespace EffectPool {
    RainbowEffect rainbowEffect;  // Define instance
    
    Effect* getEffect(uint8_t id) {
        switch (id) {
            case 0: return &solidEffect;
            case 1: return &rainbowEffect;  // Add here
            case 2: return &prideEffect;
            // ...
        }
        return nullptr;
    }
}
```

---

### Step 5: Test Effect

```cpp
// Console commands
neo phys add 9 64
neo virt add 64
neo virt attach 0 0
neo seg add 0 0 63
neo effect 0 1          // Rainbow (ID 1)
neo brightness 0 255    // Full brightness
neo auto on
```

---

## Migration Examples

### Example 1: Simple Effect (Solid Color)

**FastLED:**
```cpp
void solidColor() {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
}
```

**OFM-NeoPixel:**
```cpp
class EffectSolid : public Effect {
public:
    void update(Segment* segment, uint32_t deltaTime) override {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Read color from config
        uint8_t r = config.r;
        uint8_t g = config.g;
        uint8_t b = config.b;
        
        // Apply brightness
        r = FastLEDMath::scale8(r, config.intensity);
        g = FastLEDMath::scale8(g, config.intensity);
        b = FastLEDMath::scale8(b, config.intensity);
        
        // Fill segment
        for (uint16_t i = 0; i < length; i++) {
            segment->setPixel(i, r, g, b);
        }
    }
    
    const char* getName() override { return "Solid"; }
};
```

---

### Example 2: Effect with Beat (BPM)

**FastLED:**
```cpp
void bpm() {
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
    
    EVERY_N_MILLISECONDS(20) { gHue++; }
    FastLED.show();
}
```

**OFM-NeoPixel:**
```cpp
class BPMEffect : public Effect {
private:
    uint8_t _gHue;
    
public:
    BPMEffect() : _gHue(0) {}
    
    void update(Segment* segment, uint32_t deltaTime) override {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        
        // Use FastLEDMath functions
        uint8_t BeatsPerMinute = 62;
        uint8_t beat = FastLEDMath::beatsin8(BeatsPerMinute, 64, 255);
        
        for (uint16_t i = 0; i < length; i++) {
            // Simplified without palette (or port palette system)
            uint8_t hue = _gHue + (i * 2);
            uint8_t brightness = beat - _gHue + (i * 10);
            
            // Scale by master brightness
            brightness = FastLEDMath::scale8(brightness, config.intensity);
            
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);
            
            segment->setPixel(i,
                (rgb >> 16) & 0xFF,
                (rgb >> 8) & 0xFF,
                rgb & 0xFF);
        }
        
        _gHue++;  // Advance hue
    }
    
    void reset() override { _gHue = 0; }
    const char* getName() override { return "BPM"; }
};
```

---

### Example 3: Complex Effect (Pride2015)

**FastLED Original (Mark Kriegsman):**
```cpp
void pride() {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;
 
    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);
    
    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;
    
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);
        
        CRGB newcolor = CHSV(hue8, sat8, bri8);
        leds[i] = newcolor;
    }
}
```

**OFM-NeoPixel Port:**
```cpp
class PrideEffect : public Effect {
private:
    // Move static state to class members
    uint16_t _pseudotime;
    uint16_t _hue16;
    uint32_t _lastUpdate;
    
public:
    PrideEffect() : _pseudotime(0), _hue16(0), _lastUpdate(0) {}
    
    void update(Segment* segment, uint32_t deltaTime) override {
        if (!segment) return;
        
        auto& config = segment->getConfig();
        uint16_t length = segment->getLength();
        uint8_t masterBrightness = config.intensity;
        
        // Direct port of FastLED logic (use FastLEDMath::)
        uint8_t sat8 = FastLEDMath::beatsin88(87, 220, 250);
        uint8_t brightdepth = FastLEDMath::beatsin88(341, 96, 224);
        uint16_t brightnessthetainc16 = FastLEDMath::beatsin88(203, (25 * 256), (40 * 256));
        uint8_t msmultiplier = FastLEDMath::beatsin88(147, 23, 60);

        uint16_t hue16 = _hue16;
        uint16_t hueinc16 = FastLEDMath::beatsin88(113, 1, 3000);
        
        uint16_t ms = millis();
        uint16_t deltams = ms - _lastUpdate;
        _lastUpdate = ms;
        _pseudotime += deltams * msmultiplier;
        _hue16 += deltams * FastLEDMath::beatsin88(400, 5, 9);
        uint16_t brightnesstheta16 = _pseudotime;
        
        for (uint16_t i = 0; i < length; i++) {
            hue16 += hueinc16;
            uint8_t hue8 = hue16 / 256;

            brightnesstheta16 += brightnessthetainc16;
            uint16_t b16 = FastLEDMath::sin16(brightnesstheta16) + 32768;

            uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
            uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
            bri8 += (255 - brightdepth);
            
            // Apply master brightness scaling
            bri8 = FastLEDMath::scale8(bri8, masterBrightness);
            
            uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue8, sat8, bri8);
            
            segment->setPixel(i,
                (rgb >> 16) & 0xFF,
                (rgb >> 8) & 0xFF,
                rgb & 0xFF);
        }
    }
    
    void reset() override {
        _pseudotime = 0;
        _hue16 = 0;
        _lastUpdate = millis();
    }
    
    const char* getName() override { return "Pride2015"; }
};
```

**Key Changes:**
1. `static` variables → class members
2. `FastLED` functions → `FastLEDMath::` functions
3. `leds[i]` → `segment->setPixel(i, r, g, b)`
4. `CRGB` → manual RGB extraction
5. Added `masterBrightness` scaling

---

## Performance Considerations

### FastLED vs OFM-NeoPixel

| Aspect | FastLED | OFM-NeoPixel |
|--------|---------|--------------|
| **LED Update** | Blocking | Non-blocking (DMA) |
| **Effect State** | Global variables | Class members |
| **Memory** | 3 bytes × N LEDs | 3-7 bytes × N LEDs (with DMA) |
| **Multi-strip** | Requires multiple FastLED.addLeds | Native support |
| **Segments** | Manual index math | Built-in Segment class |
| **CPU Usage** | ~35% @ 50 FPS | ~15% @ 62.5 FPS |

### Optimization Tips

1. **Minimize Calculations Inside Loops**
   ```cpp
   // Bad
   for (uint16_t i = 0; i < length; i++) {
       uint8_t hue = beat8(60) + i;  // beat8() called N times!
       // ...
   }
   
   // Good
   uint8_t baseHue = FastLEDMath::beat8(60);  // Called once
   for (uint16_t i = 0; i < length; i++) {
       uint8_t hue = baseHue + i;
       // ...
   }
   ```

2. **Use FastLEDMath Functions**
   - They're heavily optimized
   - Faster than standard math
   - Example: `scale8()` vs `(i * scale) / 255`

3. **Avoid Floating Point**
   ```cpp
   // Bad
   float brightness = sin(time * 0.1) * 127.5 + 127.5;
   
   // Good
   uint8_t brightness = FastLEDMath::sin8(time) + 128;
   ```

4. **Pre-calculate Constants**
   ```cpp
   // Bad
   for (uint16_t i = 0; i < length; i++) {
       uint8_t hue = i * 255 / length;  // Division every iteration
   }
   
   // Good
   uint16_t hueInc = 255 / length;  // Calculate once
   uint8_t hue = 0;
   for (uint16_t i = 0; i < length; i++) {
       // Use hue
       hue += hueInc;
   }
   ```

---

## Testing Ported Effects

### Console Testing

```bash
# Setup
neo phys add 9 64
neo virt add 64
neo virt attach 0 0
neo seg add 0 0 63

# Test each effect
neo effect 0 0          # Solid
neo color 0 255 0 0     # Red
neo update

neo effect 0 1          # Rainbow
neo brightness 0 255
neo update

neo effect 0 2          # Pride2015
neo update

neo effect 0 6          # Cylon
neo color 0 0 0 255
neo update

# Performance check
neo perf
```

### Automated Test Suite

```cpp
#ifdef OPENKNX_NEOPIXEL_TESTS

void testPortedEffects() {
    auto mgr = neoPixelModule.getManager();
    auto segment = mgr->getSegment(0);
    
    if (!segment) return;
    
    // Test each effect for 5 seconds
    Effect* effects[] = {
        EffectPool::getEffect(0),  // Solid
        EffectPool::getEffect(1),  // Rainbow
        EffectPool::getEffect(2),  // Pride
        EffectPool::getEffect(3),  // Confetti
        EffectPool::getEffect(4),  // Juggle
        EffectPool::getEffect(5),  // BPM
        //....
    };
    
    for (auto* effect : effects) {
        logInfoP("Testing effect: %s", effect->getName());
        mgr->attachEffect(segment, effect);
        
        // Run for 5 seconds
        uint32_t start = millis();
        while (millis() - start < 5000) {
            mgr->update(16);  // 62.5 FPS
            delay(16);
        }
    }
    
    logInfoP("All effects tested!");
}

#endif
```

### Visual Comparison

Compare side-by-side with original FastLED:

1. **Upload FastLED sketch** to one board
2. **Upload OFM-NeoPixel** to another board
3. **Run same effect** on both
4. **Verify visual match**

If colors/timing differ, check:
- HSV → RGB conversion
- Brightness scaling
- Speed parameters

---

## Common Pitfalls

### 1. Forgetting to Scale Brightness

**Problem:**
```cpp
// Effect ignores segment brightness!
uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, 255);
```

**Solution:**
```cpp
// Always use config.intensity
uint8_t brightness = config.intensity;
uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(hue, 255, brightness);
```

---

### 2. Global State in Effect

**Problem:**
```cpp
// This breaks stateless design!
static uint8_t gHue = 0;

void update(Segment* segment, uint32_t deltaTime) override {
    gHue++;  // Shared by all segments!
}
```

**Solution:**
```cpp
// State in class member (one per effect instance)
class MyEffect : public Effect {
private:
    uint8_t _hue;  // Each instance has its own
    
public:
    void update(Segment* segment, uint32_t deltaTime) override {
        _hue++;
    }
};
```

**Wait, but effects are singletons?**

Yes! But each effect type is a singleton. If you need per-segment state, store it in `segment->getState().effectState`:

```cpp
void update(Segment* segment, uint32_t deltaTime) override {
    auto& state = segment->getState();
    
    // Per-segment state stored here
    uint8_t& hue = state.effectState;  // uint32_t field
    hue++;
    
    // ...
}
```

---

### 3. Direct Array Access

**Problem:**
```cpp
// This doesn't work - no global array!
leds[i] = CRGB(r, g, b);
```

**Solution:**
```cpp
// Always use segment API
segment->setPixel(i, r, g, b);
```

---

### 4. Missing FastLEDMath Namespace

**Problem:**
```cpp
// Compiler error: sin8() not found
uint8_t value = sin8(angle);
```

**Solution:**
```cpp
// Use namespace prefix
uint8_t value = FastLEDMath::sin8(angle);
```

---

### 5. Hardcoded LED Count

**Problem:**
```cpp
// Assumes fixed strip length!
for (int i = 0; i < 64; i++) {
    // ...
}
```

**Solution:**
```cpp
// Always use segment length
uint16_t length = segment->getLength();
for (uint16_t i = 0; i < length; i++) {
    // ...
}
```

---

### 6. Color Palette Not Ported

**Problem:**
```cpp
// ColorFromPalette() doesn't exist!
CRGB color = ColorFromPalette(palette, index, brightness);
```

**Solution (Option 1):** Port palette system
```cpp
// Create OFM-NeoPixel ColorPalette class
class ColorPalette {
    uint32_t colors[16];
    
    uint32_t getColor(uint8_t index, uint8_t brightness) {
        // Interpolate between palette colors
        // ...
    }
};
```

**Solution (Option 2):** Use HSV instead
```cpp
// Simplified without palette
uint32_t rgb = FastLEDMath::hsv2rgb_rainbow(index, 255, brightness);
```

---

### 7. Time-based Effects Without deltaTime

**Problem:**
```cpp
// Effect speed depends on frame rate!
_position++;
```

**Solution:**
```cpp
// Use deltaTime for frame-rate independence
_position += (config.speed * deltaTime) / 1000;
```

---

## FAQ

### Q: Can I use FastLED and OFM-NeoPixel together?

**A:** Not recommended. They conflict at the hardware level (both try to control GPIO). Choose one:
- **FastLED:** If you need Arduino ecosystem compatibility
- **OFM-NeoPixel:** If you want OpenKNX integration and better performance

---

### Q: Do I need to port the entire FastLED library?

**A:** No! Only port:
1. **Math functions** (already done in FastLEDMath.h)
2. **Effects you want to use** (port individually)
3. **Color palettes** (if needed)

Everything else (hardware layer, show(), etc.) is replaced by OFM-NeoPixel.

---

### Q: How do I handle FastLED's CRGB type?

**A:** OFM-NeoPixel uses separate RGB bytes:

```cpp
// FastLED
CRGB color = CRGB::Red;
leds[i] = color;

// OFM-NeoPixel
segment->setPixel(i, 255, 0, 0);  // R, G, B

// Or extract from CRGB (if you have it)
segment->setPixel(i, color.r, color.g, color.b);
```

---

### Q: What about FastLED's EVERY_N_MILLISECONDS?

**A:** Not needed! Use `deltaTime`:

```cpp
// FastLED
EVERY_N_MILLISECONDS(20) {
    gHue++;
}

// OFM-NeoPixel
void update(Segment* segment, uint32_t deltaTime) override {
    _accumulatedTime += deltaTime;
    
    if (_accumulatedTime >= 20) {
        _accumulatedTime -= 20;
        _hue++;
    }
}
```

Or simpler:
```cpp
// Just advance based on deltaTime
_hue += (deltaTime * 256) / 1000;  // 256 steps per second
```

---

### Q: How do I port FastLED noise functions (inoise8)?

**A:** That's complex! Options:

1. **Port Perlin noise** (high effort)
2. **Use simpler random** (low effort)
   ```cpp
   uint8_t noise = FastLEDMath::random8();
   ```
3. **Wait for someone else to port it** (zero effort)

---

### Q: Can multiple segments use the same effect?

**A:** Yes! That's the whole point of stateless design:

```cpp
// One instance, multiple segments
RainbowEffect rainbowEffect;

mgr->attachEffect(segment0, &rainbowEffect);
mgr->attachEffect(segment1, &rainbowEffect);
mgr->attachEffect(segment2, &rainbowEffect);

// Each segment gets different part of the effect
```

---

### Q: How do I debug ported effects?

**A:**

1. **Add logging:**
   ```cpp
   void update(Segment* segment, uint32_t deltaTime) override {
       logDebugP("Effect: hue=%u, brightness=%u", _hue, config.intensity);
       // ...
   }
   ```

2. **Compare with FastLED:** Upload both sketches, compare visually

3. **Use performance tracker:**
   ```bash
   neo perf  # Check CPU usage, FPS
   ```

4. **Test with single pixel:**
   ```cpp
   // Simplify to test logic
   segment->setPixel(0, r, g, b);  // Only update first pixel
   ```

---

## Conclusion

Porting FastLED effects to OFM-NeoPixel is straightforward:

1. **Copy effect logic** (FastLED effects are often self-contained)
2. **Replace FastLED calls** with FastLEDMath:: equivalents
3. **Move state** from globals to class members
4. **Use segment API** instead of direct array access
5. **Test and iterate**

The result: FastLED's beautiful effects running on OFM-NeoPixel's efficient platform!

---

## Contributing

Want to port more effects? Great!

1. **Choose an effect** from FastLED examples
2. **Follow this guide** to port it
3. **Add to EffectPool**
4. **Submit pull request** with:
   - Effect source code
   - Test results
   - Example usage
   - Performance notes

See you in the commits!

---

## Resources

- **FastLED Library:** https://github.com/FastLED/FastLED
- **FastLED Examples:** https://github.com/FastLED/FastLED/tree/master/examples
- **Mark Kriegsman's Effects:** https://gist.github.com/kriegsman

---