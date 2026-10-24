/**
 * @file EffectPool.h
 * @brief Pool with one instance per effect type
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once

// ============================================================================
//  NEOPIXEL_DISABLE_2D - drop every 2D effect at once
//
//  The 2D effects only render on a matrix topology. On a device that drives plain
//  strips they are dead weight, so one flag stands in for the thirteen individual
//  ones below. Setting a single effect's flag still works on its own.
// ============================================================================
#ifdef NEOPIXEL_DISABLE_2D
    #ifndef NEOPIXEL_DISABLE_AURORA2D
        #define NEOPIXEL_DISABLE_AURORA2D
    #endif
    #ifndef NEOPIXEL_DISABLE_CLOCK2D
        #define NEOPIXEL_DISABLE_CLOCK2D
    #endif
    #ifndef NEOPIXEL_DISABLE_DNA2D
        #define NEOPIXEL_DISABLE_DNA2D
    #endif
    #ifndef NEOPIXEL_DISABLE_GAMEOFLIFE2D
        #define NEOPIXEL_DISABLE_GAMEOFLIFE2D
    #endif
    #ifndef NEOPIXEL_DISABLE_LISSAJOUS2D
        #define NEOPIXEL_DISABLE_LISSAJOUS2D
    #endif
    #ifndef NEOPIXEL_DISABLE_MATRIX2D
        #define NEOPIXEL_DISABLE_MATRIX2D
    #endif
    #ifndef NEOPIXEL_DISABLE_METABALLS2D
        #define NEOPIXEL_DISABLE_METABALLS2D
    #endif
    #ifndef NEOPIXEL_DISABLE_PLASMANEBULA2D
        #define NEOPIXEL_DISABLE_PLASMANEBULA2D
    #endif
    #ifndef NEOPIXEL_DISABLE_SNAKE2D
        #define NEOPIXEL_DISABLE_SNAKE2D
    #endif
    #ifndef NEOPIXEL_DISABLE_STARFIELDWARP2D
        #define NEOPIXEL_DISABLE_STARFIELDWARP2D
    #endif
    #ifndef NEOPIXEL_DISABLE_TETRIS2D
        #define NEOPIXEL_DISABLE_TETRIS2D
    #endif
    #ifndef NEOPIXEL_DISABLE_TRON2D
        #define NEOPIXEL_DISABLE_TRON2D
    #endif
    #ifndef NEOPIXEL_DISABLE_UFOSWARM2D
        #define NEOPIXEL_DISABLE_UFOSWARM2D
    #endif
#endif

/**
 * Memory: one pool instance per effect type (member-free effects: 8 bytes on RP2040);
 * each Segment carries its own EffectState (32 bytes incl. the 16-byte scratch area).
 *
 * Usage:
 *   segment1->setEffect(EffectPool::getSolid());
 *   segment2->setEffect(EffectPool::getSolid());  // Same instance as segment1!
 *   segment3->setEffect(EffectPool::getWipe()); // Different instance
 *
 * Runtime state lives in the segment. Buffer effects (EFFECT_PER_SEGMENT_INSTANCE, see Effect.h)
 * give a segment its own instance when the pool instance already renders another segment;
 * getEffect() still returns the pool instance.
 */

#include "Effect.h"

// Forward declarations for all effect types
// Add new effect classes here to include them in the pool
//
class EffectSolid;
class EffectWipe;
class RainbowEffect;
class PrideEffect;
class JuggleEffect;
class BPMEffect;
class CylonEffect;
class RGBWTestEffect;
class FireEffect;
class TheaterChaseEffect;
class SparkleEffect;
class BreathingEffect;
class StrobeEffect;
class CometEffect;
class NoiseEffect;
class PaletteEffect;
class LightningEffect;
class GradientEffect;
class CandleEffect;
class ScrollTextEffect;
class Clock2DEffect;
class Snake2DEffect;
class Matrix2DEffect;
class Tron2DEffect;
class StarfieldWarp2DEffect;
class PlasmaNebula2DEffect;
class UfoSwarm2DEffect;
class GameOfLife2DEffect;
class DNA2DEffect;
class Aurora2DEffect;
class Lissajous2DEffect;
class Metaballs2DEffect;

/**
 * EffectPool - one pool instance per effect type
 */
class EffectPool
{
  public:
    // -----------------------------------------------------------
    // Getters for each effect's pool instance
    static Effect* getSolid();
    static Effect* getWipe();
    static Effect* getRainbow();
    static Effect* getPride();
    static Effect* getJuggle();
    static Effect* getBPM();
    static Effect* getCylon();
    static Effect* getRGBWTest();
    static Effect* getFire();
    static Effect* getTheaterChase();
    static Effect* getSparkle();
    static Effect* getBreathing();
    static Effect* getStrobe();
    static Effect* getComet();
    static Effect* getNoise();
    static Effect* getPalette();
    static Effect* getLightning();
    static Effect* getGradient();
    static Effect* getCandle();
    // 2D effects
    static Effect* getScrollText();
    static Effect* getClock2D();
    static Effect* getSnake2D();
    static Effect* getMatrix2D();
    static Effect* getTetris2D();
    static Effect* getTron2D();
    static Effect* getStarfieldWarp2D();
    static Effect* getPlasmaNebula2D();
    static Effect* getUfoSwarm2D();
    static Effect* getGameOfLife2D();
    static Effect* getDNA2D();
    static Effect* getAurora2D();
    static Effect* getLissajous2D();
    static Effect* getMetaballs2D();
    //
    // -----------------------------------------------------------

    // Dynamic effect registry access
    static uint8_t getEffectCount();
    static Effect* getEffectByIndex(uint8_t index);

  private:
    EffectPool() = delete;
    EffectPool(const EffectPool&) = delete;
    EffectPool& operator=(const EffectPool&) = delete;
};
