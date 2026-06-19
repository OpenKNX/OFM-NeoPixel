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
// #define NEOPIXEL_DISABLE_RAINBOW   (Rainbow incl. Rainbow Cycle mode)
// #define NEOPIXEL_DISABLE_PRIDE
// #define NEOPIXEL_DISABLE_JUGGLE
// #define NEOPIXEL_DISABLE_BPM
// #define NEOPIXEL_DISABLE_CYLON     (Cylon incl. 2D + center mode)
// #define NEOPIXEL_DISABLE_RGBWTEST  (Test incl. RGB+CCT mode)
// NOTE: GarageDoor, Rainbow Cycle, Confetti, Twinkle, Sinelon, Meteor, Pulse,
//       RGBCCT-Test, Candle-Multi and the dedicated Fire/Noise/Cylon 2D effects
//       were consolidated into their parent effects (mode/waveform parameters).
// #define NEOPIXEL_DISABLE_FIRE      (Fire incl. 2D)
// #define NEOPIXEL_DISABLE_THEATERCHASE  (incl. Rainbow + Bounce modes)
// #define NEOPIXEL_DISABLE_SPARKLE   (Sparkle incl. Twinkle/Confetti modes)
// #define NEOPIXEL_DISABLE_BREATHING (Breathing incl. Pulse waveforms)
// #define NEOPIXEL_DISABLE_STROBE
// #define NEOPIXEL_DISABLE_COMET     (Comet incl. Meteor/Sinelon modes)
// #define NEOPIXEL_DISABLE_NOISE     (Noise incl. 2D)
// #define NEOPIXEL_DISABLE_PALETTE
// #define NEOPIXEL_DISABLE_LIGHTNING
// #define NEOPIXEL_DISABLE_GRADIENT
// ============================================================================

#include "EffectSolid.h"

#ifndef NEOPIXEL_DISABLE_WIPE
    #include "EffectWipe.h"
#endif

#ifndef NEOPIXEL_DISABLE_RAINBOW
    #include "RainbowEffect.h"
#endif

#ifndef NEOPIXEL_DISABLE_PRIDE
    #include "PrideEffect.h"
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

#ifndef NEOPIXEL_MINIMAL_EFFECTS

    #ifndef NEOPIXEL_DISABLE_FIRE
        #include "FireEffect.h"
    #endif

    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
        #include "TheaterChaseEffect.h"
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

    #ifndef NEOPIXEL_DISABLE_COMET
        #include "CometEffect.h"
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
    #endif

#endif // NEOPIXEL_MINIMAL_EFFECTS

    // 2D effects (always included unless explicitly disabled)
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
    #ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
        #include "GameOfLife2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_DNA2D
        #include "DNA2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_AURORA2D
        #include "Aurora2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
        #include "Lissajous2DEffect.h"
    #endif
    #ifndef NEOPIXEL_DISABLE_METABALLS2D
        #include "Metaballs2DEffect.h"
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
#endif

#ifndef NEOPIXEL_DISABLE_PRIDE
static PrideEffect* s_pride = nullptr;
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

#ifndef NEOPIXEL_MINIMAL_EFFECTS

    #ifndef NEOPIXEL_DISABLE_FIRE
static FireEffect* s_fire = nullptr;
    #endif

    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
static TheaterChaseEffect* s_theaterChase = nullptr;
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

    #ifndef NEOPIXEL_DISABLE_COMET
static CometEffect* s_comet = nullptr;
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
#ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
static GameOfLife2DEffect* s_gameOfLife2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_DNA2D
static DNA2DEffect* s_dna2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_AURORA2D
static Aurora2DEffect* s_aurora2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
static Lissajous2DEffect* s_lissajous2d = nullptr;
#endif
#ifndef NEOPIXEL_DISABLE_METABALLS2D
static Metaballs2DEffect* s_metaballs2d = nullptr;
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
    #endif

#endif // NEOPIXEL_MINIMAL_EFFECTS

// ── 2D effects ───────────────────────────────────────────────────────────────

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

#ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
Effect* EffectPool::getGameOfLife2D()
{
    if (!s_gameOfLife2d) s_gameOfLife2d = new GameOfLife2DEffect();
    return s_gameOfLife2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_DNA2D
Effect* EffectPool::getDNA2D()
{
    if (!s_dna2d) s_dna2d = new DNA2DEffect();
    return s_dna2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_AURORA2D
Effect* EffectPool::getAurora2D()
{
    if (!s_aurora2d) s_aurora2d = new Aurora2DEffect();
    return s_aurora2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
Effect* EffectPool::getLissajous2D()
{
    if (!s_lissajous2d) s_lissajous2d = new Lissajous2DEffect();
    return s_lissajous2d;
}
#endif

#ifndef NEOPIXEL_DISABLE_METABALLS2D
Effect* EffectPool::getMetaballs2D()
{
    if (!s_metaballs2d) s_metaballs2d = new Metaballs2DEffect();
    return s_metaballs2d;
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
    count++; // Rainbow (incl. Rainbow Cycle mode)
#endif
#ifndef NEOPIXEL_DISABLE_PRIDE
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

#ifndef NEOPIXEL_MINIMAL_EFFECTS
    #ifndef NEOPIXEL_DISABLE_FIRE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
    count++; // TheaterChase (incl. Rainbow mode)
    #endif
    #ifndef NEOPIXEL_DISABLE_SPARKLE
    count++; // Sparkle (incl. Twinkle/Confetti modes)
    #endif
    #ifndef NEOPIXEL_DISABLE_BREATHING
    count++; // Breathing (incl. Pulse waveforms)
    #endif
    #ifndef NEOPIXEL_DISABLE_STROBE
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_COMET
    count++; // Comet (incl. Meteor/Sinelon modes)
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
    #ifndef NEOPIXEL_DISABLE_CANDLE
    count++; // Candle (incl. multi-zone)
    #endif
#endif // NEOPIXEL_MINIMAL_EFFECTS
    // 2D effects
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
    #ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_DNA2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_AURORA2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
    count++;
    #endif
    #ifndef NEOPIXEL_DISABLE_METABALLS2D
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
#endif
#ifndef NEOPIXEL_DISABLE_PRIDE
    if (index == currentIndex++) return getPride();
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

#ifndef NEOPIXEL_MINIMAL_EFFECTS
    #ifndef NEOPIXEL_DISABLE_FIRE
    if (index == currentIndex++) return getFire();
    #endif
    #ifndef NEOPIXEL_DISABLE_THEATERCHASE
    if (index == currentIndex++) return getTheaterChase();
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
    #ifndef NEOPIXEL_DISABLE_COMET
    if (index == currentIndex++) return getComet();
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
    #ifndef NEOPIXEL_DISABLE_CANDLE
    if (index == currentIndex++) return getCandle();
    #endif
#endif // NEOPIXEL_MINIMAL_EFFECTS
    // 2D effects
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
    #ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
    if (index == currentIndex++) return getGameOfLife2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_DNA2D
    if (index == currentIndex++) return getDNA2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_AURORA2D
    if (index == currentIndex++) return getAurora2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
    if (index == currentIndex++) return getLissajous2D();
    #endif
    #ifndef NEOPIXEL_DISABLE_METABALLS2D
    if (index == currentIndex++) return getMetaballs2D();
    #endif

    return nullptr; // Index out of range
}
