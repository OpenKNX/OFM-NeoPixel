#include "EffectPool.h"

// ============================================================================
// Effect Selection - Individual Effect Control
// ============================================================================
//
// CRITICAL: Effect Registration Order = ETS XML IDs
//
// The order of effects in getEffectByIndex() directly defines the XML enumeration IDs!
// Build-EffectParameters.ps1 parses this file to extract the registration order.
//
// Registration Order Example:
//   getEffectByIndex():
//     - index 0 → getSolid()        → ETS XML: <Enumeration Value="0" Text="Solid"/>
//     - index 1 → getWipe()         → ETS XML: <Enumeration Value="1" Text="Wipe"/>
//     - index 2 → getRainbow()      → ETS XML: <Enumeration Value="2" Text="Rainbow"/>
//
// When KNX sends "Effect ID = 23", OAM calls getEffectByIndex(23).
// The XML ID must match the C++ registration index exactly!
//
// !!!  DO NOT REORDER getEffectByIndex() without regenerating XML!
// !!! Always run Build-EffectParameters.ps1 after changing effect order!
//
// ============================================================================
// Define these to DISABLE specific effects (saves flash memory)
// Solid effect cannot be disabled (always included)
//
// #define NEOPIXEL_DISABLE_WIPE
// #define NEOPIXEL_DISABLE_RAINBOW
// #define NEOPIXEL_DISABLE_PRIDE
// #define NEOPIXEL_DISABLE_CONFETTI
// #define NEOPIXEL_DISABLE_JUGGLE
// #define NEOPIXEL_DISABLE_BPM
// #define NEOPIXEL_DISABLE_CYLON
// #define NEOPIXEL_DISABLE_RGBWTEST
// #define NEOPIXEL_DISABLE_RGBCCTTEST
// GarageDoorEffect is deprecated — replaced by Effektmanager Sequencer.
// Define NEOPIXEL_ENABLE_GARAGEDOOR to re-enable if needed for migration.
#ifndef NEOPIXEL_ENABLE_GARAGEDOOR
    #define NEOPIXEL_DISABLE_GARAGEDOOR
#endif
// #define NEOPIXEL_DISABLE_GARAGEDOOR
// #define NEOPIXEL_DISABLE_FIRE
// #define NEOPIXEL_DISABLE_THEATERCHASE
// #define NEOPIXEL_DISABLE_THEATERCHASERAINBOW
// #define NEOPIXEL_DISABLE_SINELON
// #define NEOPIXEL_DISABLE_TWINKLE
// #define NEOPIXEL_DISABLE_SPARKLE
// #define NEOPIXEL_DISABLE_BREATHING
// #define NEOPIXEL_DISABLE_STROBE
// #define NEOPIXEL_DISABLE_PULSE
// #define NEOPIXEL_DISABLE_COMET
// #define NEOPIXEL_DISABLE_METEOR
// #define NEOPIXEL_DISABLE_NOISE
// #define NEOPIXEL_DISABLE_PALETTE
// #define NEOPIXEL_DISABLE_LIGHTNING
// #define NEOPIXEL_DISABLE_GRADIENT
// ============================================================================

#include "EffectSolid.h"

#ifndef NEOPIXEL_DISABLE_WIPE
    #include "EffectWipe.h"
#endif

#ifndef NEOPIXEL_DISABLE_RAINBOW
    #include "RainbowCycleEffect.h"
    #include "RainbowEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_PRIDE
    #include "PrideEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_CONFETTI
    #include "ConfettiEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_JUGGLE
    #include "JuggleEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_BPM
    #include "BPMEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_CYLON
    #include "CylonEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_RGBWTEST
    #include "RGBWTestEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_RGBCCTTEST
    #include "RGBCCTTestEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_GARAGEDOOR
    #include "GarageDoorEffect.h"
#endif

#ifndef NEOPIXEL_MINIMAL_EFFECTS

    #ifndef NEOPIXEL_DISABLE_FIRE
        #include "FireEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
        #include "TheaterChaseEffect.h"
        #include "TheaterChaseRainbowEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_SINELON
        #include "SinelonEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_TWINKLE
        #include "TwinkleEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_SPARKLE
        #include "SparkleEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_BREATHING
        #include "BreathingEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_STROBE
        #include "StrobeEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_PULSE
        #include "PulseEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_COMET
        #include "CometEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_METEOR
        #include "MeteorEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_NOISE
        #include "NoiseEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_PALETTE
        #include "PaletteEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_LIGHTNING
        #include "LightningEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_GRADIENT
        #include "GradientEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_CANDLE
        #include "CandleEffect.h"
        #include "CandleMultiEffect.h"
    #endif

#endif // NEOPIXEL_MINIMAL_EFFECTS

    // 2D effects (always included unless explicitly disabled)
    #ifndef NEOPIXEL_DISABLE_FIRE2D
        #include "Fire2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_NOISE2D
        #include "Noise2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_CYLON2D
        #include "Cylon2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_SCROLLTEXT
        #include "ScrollTextEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_CLOCK2D
        #include "Clock2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_SNAKE2D
        #include "Snake2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_MATRIX2D
        #include "Matrix2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_TETRIS2D
        #include "Tetris2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_TRON2D
        #include "Tron2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
        #include "StarfieldWarp2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
        #include "PlasmaNebula2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_UFOSWARM2D
        #include "UfoSwarm2DEffect.h"
    #endif

// -----------------------------------------------------------
// Static singleton instances - (Initialized on first use)
// Add here for each new effect type to be included in the pool
//
static EffectSolid* s_solid = nullptr;

#ifndef NEOPIXEL_DISABLE_WIPE
static EffectWipe* s_wipe = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_RAINBOW
static RainbowEffect* s_rainbow = nullptr;
static RainbowCycleEffect* s_rainbowCycle = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_PRIDE
static PrideEffect* s_pride = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_CONFETTI
static ConfettiEffect* s_confetti = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_JUGGLE
static JuggleEffect* s_juggle = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_BPM
static BPMEffect* s_bpm = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_CYLON
static CylonEffect* s_cylon = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_RGBWTEST
static RGBWTestEffect* s_rgbwTest = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_RGBCCTTEST
static RGBCCTTestEffect* s_rgbcctTest = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_GARAGEDOOR
static GarageDoorEffect* s_garageDoor = nullptr;
#endif

#ifndef NEOPIXEL_MINIMAL_EFFECTS

    #ifndef NEOPIXEL_DISABLE_FIRE
static FireEffect* s_fire = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
static TheaterChaseEffect* s_theaterChase = nullptr;
static TheaterChaseRainbowEffect* s_theaterChaseRainbow = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_SINELON
static SinelonEffect* s_sinelon = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_TWINKLE
static TwinkleEffect* s_twinkle = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_SPARKLE
static SparkleEffect* s_sparkle = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_BREATHING
static BreathingEffect* s_breathing = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_STROBE
static StrobeEffect* s_strobe = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_PULSE
static PulseEffect* s_pulse = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_COMET
static CometEffect* s_comet = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_METEOR
static MeteorEffect* s_meteor = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_NOISE
static NoiseEffect* s_noise = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_PALETTE
static PaletteEffect* s_palette = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_LIGHTNING
static LightningEffect* s_lightning = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_GRADIENT
static GradientEffect* s_gradient = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_CANDLE
static CandleEffect* s_candle = nullptr;
static CandleMultiEffect* s_candleMulti = nullptr;
    #endif

#endif // NEOPIXEL_MINIMAL_EFFECTS
// -----------------------------------------------------------

#ifndef NEOPIXEL_DISABLE_SNAKE2D
static Snake2DEffect* s_snake2d = nullptr;
#endif

#ifndef NEOPIXEL_DISABLE_MATRIX2D
static Matrix2DEffect* s_matrix2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_TETRIS2D
static Tetris2DEffect* s_tetris2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_TRON2D
static Tron2DEffect* s_tron2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
static StarfieldWarp2DEffect* s_starfieldWarp2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
static PlasmaNebula2DEffect* s_plasmaNebula2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_UFOSWARM2D
static UfoSwarm2DEffect* s_ufoSwarm2d = nullptr;
#endif

/**
 * @brief Get Solid Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getSolid()
{
    if (!s_solid)
    {
        s_solid = new EffectSolid();
    }
    return s_solid;
}

/**
 * @brief Get Wipe Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getWipe()
{
    if (!s_wipe)
    {
        s_wipe = new EffectWipe();
    }
    return s_wipe;
}

/**
 * @brief Get Rainbow Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getRainbow()
{
    if (!s_rainbow)
    {
        s_rainbow = new RainbowEffect();
    }
    return s_rainbow;
}

/**
 * @brief Get RainbowCycle Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getRainbowCycle()
{
    if (!s_rainbowCycle)
    {
        s_rainbowCycle = new RainbowCycleEffect();
    }
    return s_rainbowCycle;
}

/**
 * @brief Get Pride2015 Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getPride()
{
    if (!s_pride)
    {
        s_pride = new PrideEffect();
    }
    return s_pride;
}

/**
 * @brief Get Confetti Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getConfetti()
{
    if (!s_confetti)
    {
        s_confetti = new ConfettiEffect();
    }
    return s_confetti;
}

/**
 * @brief Get Juggle Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getJuggle()
{
    if (!s_juggle)
    {
        s_juggle = new JuggleEffect();
    }
    return s_juggle;
}

/**
 * @brief Get BPM Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getBPM()
{
    if (!s_bpm)
    {
        s_bpm = new BPMEffect();
    }
    return s_bpm;
}

/**
 * @brief Get Cylon Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getCylon()
{
    if (!s_cylon)
    {
        s_cylon = new CylonEffect();
    }
    return s_cylon;
}

#ifndef NEOPIXEL_MINIMAL_EFFECTS
// Advanced effects - excluded when NEOPIXEL_MINIMAL_EFFECTS is defined

/**
 * @brief Get SK6812 Test Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getRGBWTest()
{
    if (!s_rgbwTest)
    {
        s_rgbwTest = new RGBWTestEffect();
    }
    return s_rgbwTest;
}

/**
 * @brief Get RGBCCT 5-Channel Test Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getRGBCCTTest()
{
    #ifndef NEOPIXEL_DISABLE_RGBCCTTEST
    if (!s_rgbcctTest)
    {
        s_rgbcctTest = new RGBCCTTestEffect();
    }
    return s_rgbcctTest;
    #else
    return nullptr;
    #endif
}

/**
 * @brief Get Garage Door Effect singleton
 * @deprecated Replaced by Effektmanager Sequencer. Enable with NEOPIXEL_ENABLE_GARAGEDOOR.
 * @return Effect*
 */
Effect* EffectPool::getGarageDoor()
{
#ifndef NEOPIXEL_DISABLE_GARAGEDOOR
    if (!s_garageDoor)
    {
        s_garageDoor = new GarageDoorEffect();
    }
    return s_garageDoor;
#else
    return getSolid(); // fallback
#endif
}

/**
 * @brief Get Fire Effect singleton (FastLED Fire2012 port)
 * @return Effect*
 */
Effect* EffectPool::getFire()
{
    if (!s_fire)
    {
        s_fire = new FireEffect();
    }
    return s_fire;
}

/**
 * @brief Get Theater Chase Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getTheaterChase()
{
    if (!s_theaterChase)
    {
        s_theaterChase = new TheaterChaseEffect();
    }
    return s_theaterChase;
}

/**
 * @brief Get Theater Chase Rainbow Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getTheaterChaseRainbow()
{
    if (!s_theaterChaseRainbow)
    {
        s_theaterChaseRainbow = new TheaterChaseRainbowEffect();
    }
    return s_theaterChaseRainbow;
}

/**
 * @brief Get Sinelon Effect singleton (FastLED port)
 * @return Effect*
 */
Effect* EffectPool::getSinelon()
{
    if (!s_sinelon)
    {
        s_sinelon = new SinelonEffect();
    }
    return s_sinelon;
}

/**
 * @brief Get Twinkle Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getTwinkle()
{
    if (!s_twinkle)
    {
        s_twinkle = new TwinkleEffect();
    }
    return s_twinkle;
}

/**
 * @brief Get Sparkle Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getSparkle()
{
    if (!s_sparkle)
    {
        s_sparkle = new SparkleEffect();
    }
    return s_sparkle;
}

/**
 * @brief Get Breathing Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getBreathing()
{
    if (!s_breathing)
    {
        s_breathing = new BreathingEffect();
    }
    return s_breathing;
}

/**
 * @brief Get Strobe Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getStrobe()
{
    if (!s_strobe)
    {
        s_strobe = new StrobeEffect();
    }
    return s_strobe;
}

/**
 * @brief Get Pulse Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getPulse()
{
    if (!s_pulse)
    {
        s_pulse = new PulseEffect();
    }
    return s_pulse;
}

/**
 * @brief Get Comet Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getComet()
{
    if (!s_comet)
    {
        s_comet = new CometEffect();
    }
    return s_comet;
}

/**
 * @brief Get Meteor Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getMeteor()
{
    if (!s_meteor)
    {
        s_meteor = new MeteorEffect();
    }
    return s_meteor;
}

    #ifndef NEOPIXEL_DISABLE_NOISE
Effect* EffectPool::getNoise()
{
    if (!s_noise)
    {
        s_noise = new NoiseEffect();
    }
    return s_noise;
}
    #endif

    #ifndef NEOPIXEL_DISABLE_PALETTE
Effect* EffectPool::getPalette()
{
    if (!s_palette)
    {
        s_palette = new PaletteEffect();
    }
    return s_palette;
}
    #endif

    #ifndef NEOPIXEL_DISABLE_LIGHTNING
Effect* EffectPool::getLightning()
{
    if (!s_lightning)
    {
        s_lightning = new LightningEffect();
    }
    return s_lightning;
}
    #endif

    #ifndef NEOPIXEL_DISABLE_GRADIENT
Effect* EffectPool::getGradient()
{
    if (!s_gradient)
    {
        s_gradient = new GradientEffect();
    }
    return s_gradient;
}
    #endif

    #ifndef NEOPIXEL_DISABLE_CANDLE
/**
 * @brief Get Candle Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getCandle()
{
    if (!s_candle)
    {
        s_candle = new CandleEffect();
    }
    return s_candle;
}

/**
 * @brief Get Candle Multi Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getCandleMulti()
{
    if (!s_candleMulti)
    {
        s_candleMulti = new CandleMultiEffect();
    }
    return s_candleMulti;
}
    #endif

#endif // NEOPIXEL_MINIMAL_EFFECTS

// ── 2D effects ───────────────────────────────────────────────────────────────

#ifndef NEOPIXEL_DISABLE_FIRE2D
static Fire2DEffect*  s_fire2d  = nullptr;
Effect* EffectPool::getFire2D()
{
    if (!s_fire2d) s_fire2d = new Fire2DEffect();
    return s_fire2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_NOISE2D
static Noise2DEffect* s_noise2d = nullptr;
Effect* EffectPool::getNoise2D()
{
    if (!s_noise2d) s_noise2d = new Noise2DEffect();
    return s_noise2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_CYLON2D
static Cylon2DEffect* s_cylon2d = nullptr;
Effect* EffectPool::getCylon2D()
{
    if (!s_cylon2d) s_cylon2d = new Cylon2DEffect();
    return s_cylon2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_SCROLLTEXT
static ScrollTextEffect* s_scrollText = nullptr;
Effect* EffectPool::getScrollText()
{
    if (!s_scrollText) s_scrollText = new ScrollTextEffect();
    return s_scrollText;
}
#endif

#ifndef NEOPIXEL_DISABLE_CLOCK2D
static Clock2DEffect* s_clock2d = nullptr;
Effect* EffectPool::getClock2D()
{
    if (!s_clock2d) s_clock2d = new Clock2DEffect();
    return s_clock2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_SNAKE2D
Effect* EffectPool::getSnake2D()
{
    if (!s_snake2d) s_snake2d = new Snake2DEffect();
    return s_snake2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_MATRIX2D
Effect* EffectPool::getMatrix2D()
{
    if (!s_matrix2d) s_matrix2d = new Matrix2DEffect();
    return s_matrix2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_TETRIS2D
Effect* EffectPool::getTetris2D()
{
    if (!s_tetris2d) s_tetris2d = new Tetris2DEffect();
    return s_tetris2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_TRON2D
Effect* EffectPool::getTron2D()
{
    if (!s_tron2d) s_tron2d = new Tron2DEffect();
    return s_tron2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
Effect* EffectPool::getStarfieldWarp2D()
{
    if (!s_starfieldWarp2d) s_starfieldWarp2d = new StarfieldWarp2DEffect();
    return s_starfieldWarp2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
Effect* EffectPool::getPlasmaNebula2D()
{
    if (!s_plasmaNebula2d) s_plasmaNebula2d = new PlasmaNebula2DEffect();
    return s_plasmaNebula2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_UFOSWARM2D
Effect* EffectPool::getUfoSwarm2D()
{
    if (!s_ufoSwarm2d) s_ufoSwarm2d = new UfoSwarm2DEffect();
    return s_ufoSwarm2d;
}
#endif
// ============================================================================

/**
 * @brief Get total number of available effects
 * @return Number of effects in pool
 */
uint8_t EffectPool::getEffectCount()
{
    uint8_t count = 1; // Solid always included

#ifndef NEOPIXEL_DISABLE_WIPE
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_RAINBOW
    count++; // Rainbow
    count++; // RainbowCycle
#endif
#ifndef NEOPIXEL_DISABLE_PRIDE
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_CONFETTI
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_JUGGLE
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_BPM
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_CYLON
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_RGBWTEST
    count++;
#endif
#ifndef NEOPIXEL_DISABLE_GARAGEDOOR
    count++;
#endif

#ifndef NEOPIXEL_MINIMAL_EFFECTS
    #ifndef NEOPIXEL_DISABLE_FIRE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
    count += 2; // TheaterChase + TheaterChaseRainbow
    #endif
    #ifndef NEOPIXEL_DISABLE_SINELON
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_TWINKLE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_SPARKLE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_BREATHING
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_STROBE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_PULSE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_COMET
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_METEOR
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_NOISE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_PALETTE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_LIGHTNING
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_GRADIENT
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_RGBCCTTEST
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_CANDLE
    count += 2; // Candle + CandleMulti
    #endif
#endif // NEOPIXEL_MINIMAL_EFFECTS
    // 2D effects
    #ifndef NEOPIXEL_DISABLE_FIRE2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_NOISE2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_CYLON2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_SCROLLTEXT
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_CLOCK2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_SNAKE2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_MATRIX2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_TETRIS2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_TRON2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_UFOSWARM2D
    count++;
    #endif

    return count;
}

/**
 * @brief Get effect instance by index
 * @param index Logical effect index (0-based)
 * @return Effect instance or nullptr if invalid index
 */
Effect* EffectPool::getEffectByIndex(uint8_t index)
{
    uint8_t currentIndex = 0;

    // Index 0: Solid (always included)
    if (index == currentIndex++) return getSolid();

#ifndef NEOPIXEL_DISABLE_WIPE
    if (index == currentIndex++) return getWipe();
#endif
#ifndef NEOPIXEL_DISABLE_RAINBOW
    if (index == currentIndex++) return getRainbow();
    if (index == currentIndex++) return getRainbowCycle();
#endif
#ifndef NEOPIXEL_DISABLE_PRIDE
    if (index == currentIndex++) return getPride();
#endif
#ifndef NEOPIXEL_DISABLE_CONFETTI
    if (index == currentIndex++) return getConfetti();
#endif
#ifndef NEOPIXEL_DISABLE_JUGGLE
    if (index == currentIndex++) return getJuggle();
#endif
#ifndef NEOPIXEL_DISABLE_BPM
    if (index == currentIndex++) return getBPM();
#endif
#ifndef NEOPIXEL_DISABLE_CYLON
    if (index == currentIndex++) return getCylon();
#endif
#ifndef NEOPIXEL_DISABLE_RGBWTEST
    if (index == currentIndex++) return getRGBWTest();
#endif
#ifndef NEOPIXEL_DISABLE_GARAGEDOOR
    if (index == currentIndex++) return getGarageDoor();
#endif

#ifndef NEOPIXEL_MINIMAL_EFFECTS
    #ifndef NEOPIXEL_DISABLE_FIRE
    if (index == currentIndex++) return getFire();
    #endif
    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
    if (index == currentIndex++) return getTheaterChase();
    if (index == currentIndex++) return getTheaterChaseRainbow();
    #endif
    #ifndef NEOPIXEL_DISABLE_SINELON
    if (index == currentIndex++) return getSinelon();
    #endif
    #ifndef NEOPIXEL_DISABLE_TWINKLE
    if (index == currentIndex++) return getTwinkle();
    #endif
    #ifndef NEOPIXEL_DISABLE_SPARKLE
    if (index == currentIndex++) return getSparkle();
    #endif
    #ifndef NEOPIXEL_DISABLE_BREATHING
    if (index == currentIndex++) return getBreathing();
    #endif
    #ifndef NEOPIXEL_DISABLE_STROBE
    if (index == currentIndex++) return getStrobe();
    #endif
    #ifndef NEOPIXEL_DISABLE_PULSE
    if (index == currentIndex++) return getPulse();
    #endif
    #ifndef NEOPIXEL_DISABLE_COMET
    if (index == currentIndex++) return getComet();
    #endif
    #ifndef NEOPIXEL_DISABLE_METEOR
    if (index == currentIndex++) return getMeteor();
    #endif
    #ifndef NEOPIXEL_DISABLE_NOISE
    if (index == currentIndex++) return getNoise();
    #endif
    #ifndef NEOPIXEL_DISABLE_PALETTE
    if (index == currentIndex++) return getPalette();
    #endif
    #ifndef NEOPIXEL_DISABLE_LIGHTNING
    if (index == currentIndex++) return getLightning();
    #endif
    #ifndef NEOPIXEL_DISABLE_GRADIENT
    if (index == currentIndex++) return getGradient();
    #endif
    #ifndef NEOPIXEL_DISABLE_RGBCCTTEST
    if (index == currentIndex++) return getRGBCCTTest();
    #endif
    #ifndef NEOPIXEL_DISABLE_CANDLE
    if (index == currentIndex++) return getCandle();
    if (index == currentIndex++) return getCandleMulti();
    #endif
#endif // NEOPIXEL_MINIMAL_EFFECTS
    // 2D effects
    #ifndef NEOPIXEL_DISABLE_FIRE2D
    if (index == currentIndex++) return getFire2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_NOISE2D
    if (index == currentIndex++) return getNoise2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_CYLON2D
    if (index == currentIndex++) return getCylon2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_SCROLLTEXT
    if (index == currentIndex++) return getScrollText();
    #endif
    #ifndef NEOPIXEL_DISABLE_CLOCK2D
    if (index == currentIndex++) return getClock2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_SNAKE2D
    if (index == currentIndex++) return getSnake2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_MATRIX2D
    if (index == currentIndex++) return getMatrix2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_TETRIS2D
    if (index == currentIndex++) return getTetris2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_TRON2D
    if (index == currentIndex++) return getTron2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
    if (index == currentIndex++) return getStarfieldWarp2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
    if (index == currentIndex++) return getPlasmaNebula2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_UFOSWARM2D
    if (index == currentIndex++) return getUfoSwarm2D();
    #endif

    return nullptr; // Index out of range
}
