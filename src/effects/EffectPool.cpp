#include "EffectPool.h"
#include "BPMEffect.h"
#include "ConfettiEffect.h"
#include "CylonEffect.h"
#include "EffectSolid.h"
#include "EffectWipe.h"
#include "GarageDoorEffect.h"
#include "JuggleEffect.h"
#include "PrideEffect.h"
#include "RainbowEffect.h"
#include "SK6812TestEffect.h"

// -----------------------------------------------------------
// Static singleton instances - (Initialized on first use)
// Add here for each new effect type to be included in the pool
//
static EffectSolid* s_solid = nullptr;
static EffectWipe* s_wipe = nullptr;
static RainbowEffect* s_rainbow = nullptr;
static PrideEffect* s_pride = nullptr;
static ConfettiEffect* s_confetti = nullptr;
static JuggleEffect* s_juggle = nullptr;
static BPMEffect* s_bpm = nullptr;
static CylonEffect* s_cylon = nullptr;
static SK6812TestEffect* s_sk6812Test = nullptr;
static GarageDoorEffect* s_garageDoor = nullptr;
//
// -----------------------------------------------------------

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

/**
 * @brief Get SK6812 Test Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getSK6812Test()
{
    if (!s_sk6812Test)
    {
        s_sk6812Test = new SK6812TestEffect();
    }
    return s_sk6812Test;
}

/**
 * @brief Get Garage Door Effect singleton
 * @return Effect*
 */
Effect* EffectPool::getGarageDoor()
{
    if (!s_garageDoor)
    {
        s_garageDoor = new GarageDoorEffect();
    }
    return s_garageDoor;
}
