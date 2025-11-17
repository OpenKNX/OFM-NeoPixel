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
#include "RGBWTestEffect.h"
#include "FireEffect.h"
#include "TheaterChaseEffect.h"
#include "SinelonEffect.h"
#include "TwinkleEffect.h"
#include "BreathingEffect.h"
#include "CometEffect.h"

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
static RGBWTestEffect* s_rgbwTest = nullptr;
static GarageDoorEffect* s_garageDoor = nullptr;
static FireEffect* s_fire = nullptr;
static TheaterChaseEffect* s_theaterChase = nullptr;
static TheaterChaseRainbowEffect* s_theaterChaseRainbow = nullptr;
static SinelonEffect* s_sinelon = nullptr;
static TwinkleEffect* s_twinkle = nullptr;
static SparkleEffect* s_sparkle = nullptr;
static BreathingEffect* s_breathing = nullptr;
static StrobeEffect* s_strobe = nullptr;
static PulseEffect* s_pulse = nullptr;
static CometEffect* s_comet = nullptr;
static MeteorEffect* s_meteor = nullptr;
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
Effect* EffectPool::getRGBWTest()
{
    if (!s_rgbwTest)
    {
        s_rgbwTest = new RGBWTestEffect();
    }
    return s_rgbwTest;
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
