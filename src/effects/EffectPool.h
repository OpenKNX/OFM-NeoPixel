/**
 * @file EffectPool.h
 * @brief Singleton Pool für alle Effect-Instanzen
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once
/**
 * Memory Optimization:
 * - Without Pool: 10 Segments × 500 bytes = 5000 bytes
 * - With Pool: 5 Singletons × 32 bytes + 10 × 52 bytes = 680 bytes
 * - Savings: 86% (4320 bytes)
 *
 * Usage:
 *   segment1->setEffect(EffectPool::getSolid());
 *   segment2->setEffect(EffectPool::getSolid());  // Same instance as segment1!
 *   segment3->setEffect(EffectPool::getWipe()); // Different instance
 */

#include "Effect.h"

// Forward declarations for all effect types
// Add new effect classes here to include them in the pool
//
class EffectSolid;
class EffectWipe;
class RainbowEffect;
class PrideEffect;
class ConfettiEffect;
class JuggleEffect;
class BPMEffect;
class CylonEffect;
class SK6812TestEffect;
class GarageDoorEffect;

/**
 * EffectPool - Singleton Pool for all Effect Instances
 */
class EffectPool
{
  public:
    // -----------------------------------------------------------
    // Getters for each Effect singleton
    static Effect* getSolid();
    static Effect* getWipe();
    static Effect* getRainbow();
    static Effect* getPride();
    static Effect* getConfetti();
    static Effect* getJuggle();
    static Effect* getBPM();
    static Effect* getCylon();
    static Effect* getSK6812Test();
    static Effect* getGarageDoor();
    //
    // -----------------------------------------------------------

  private:
    EffectPool() = delete;
    EffectPool(const EffectPool&) = delete;
    EffectPool& operator=(const EffectPool&) = delete;
};
