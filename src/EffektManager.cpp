/**
 * @file EffektManager.cpp
 * @brief Effektmanager controller implementation
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#include "EffektManager.h"
#include <Arduino.h>

// ============================================================================
// Start / Stop
// ============================================================================

void EffektManagerController::start(uint8_t emId, Segment* segment, const EffektManagerData* emData)
{
    restart(emId, segment, emData);
}

void EffektManagerController::restart(uint8_t emId, Segment* segment, const EffektManagerData* emData)
{
    if (!segment) return;

    if (emId == EM_NONE)
    {
        stop(segment);
        return;
    }

    uint8_t emIdx = emId - 1; // 0-based index
    if (emIdx >= EM_COUNT) return;

    // Guard the EM data pointer. _emData is new(nothrow) in OAM and may be null on a
    // low-memory boot; the flash-restore path (restoreState→start) is the one caller
    // that doesn't pre-check it, so guard here to cover every caller (incl. chain start).
    if (!emData) return;

    const EffektManagerData& em = emData[emIdx];
    if (!em.header.isEnabled()) return;

    // Capture the segment's DIRECT state on the idle->running transition only (not on an
    // EM->EM handover, where the current state is already a cue), so a later Stop returns
    // the segment to the manual light it had before the EM took over.
    if (!_rt.isRunning()) segment->saveDirectState();

    _rt.activeEmId   = emId;
    _rt.activeCueIdx = 0;
    _rt.cueStartMs   = millis();
    _rt.fading       = false;
    _rt.fadeIn       = false;
    _rt.fadeStartMs  = 0;
    _rt.fadeMs       = 0;
    _rt.fadeFromBri  = 255;
    _rt.cueBri       = 255;
    _rt.paused       = false;
    _rt.pauseElapsed = 0;
    segment->resume();   // clear any prior pause/stop freeze before rendering cue 0

    // Pause Effektkette (virtual band) — EM takes priority
    pauseEffektkette(segment);

    applyCue(em.cues[0], segment);

#ifdef OPENKNX_DEBUG
    Serial.printf("[EM] Started EM %d (cues=%d, loop=%d)\n", emId, em.header.cueCount, em.header.loop);
#endif
}

void EffektManagerController::stop(Segment* segment, uint8_t mode)
{
    if (!_rt.isRunning()) return;

    // Restore brightness if stopped mid-fade (ramp may have left it near 0)
    if (_rt.fading && segment) segment->setBrightness(_rt.fadeFromBri);

    _rt.activeEmId   = EM_NONE;
    _rt.activeCueIdx = 0;
    _rt.fading       = false;
    _rt.fadeIn       = false;
    _rt.paused       = false;

    // Resume Effektkette if it was paused
    if (segment) resumeEffektkette(segment);
    if (!segment) return;

    // P6 — what the segment shows after the EM stops / a chain finishes:
    //   LAST  → restore the DIRECT snapshot (keeps live dim, conflict#1=b) / blank if none
    //   OFF   → blank
    //   LEAVE → leave as-is (interrupt: the direct KO sets the new visual)
    //   DEFAULT → applied by OAM (reads ETS); if it reaches here, fall back to LAST.
    switch (mode)
    {
        case EM_STOP_LEAVE:
            break;
        case EM_STOP_OFF:
            segment->stop();
            break;
        case EM_STOP_DEFAULT:
        case EM_STOP_LAST:
        default:
            if (!segment->restoreDirectState()) segment->stop();
            break;
    }

#ifdef OPENKNX_DEBUG
    Serial.printf("[EM] Stopped (mode %u)\n", mode);
#endif
}

void EffektManagerController::pause(Segment* segment)
{
    if (!_rt.isRunning() || _rt.paused) return;
    // Freeze: remember how far into the current cue we are, then stop advancing + rendering.
    _rt.pauseElapsed = millis() - _rt.cueStartMs;
    _rt.paused = true;
    if (segment) segment->pause(); // effect stops updating -> last frame stays shown
}

void EffektManagerController::resume(Segment* segment)
{
    if (!_rt.isRunning() || !_rt.paused) return;
    _rt.paused = false;
    // Continue the current cue from where it was frozen (do not restart its duration).
    uint32_t oldCueStartMs = _rt.cueStartMs;
    _rt.cueStartMs = millis() - _rt.pauseElapsed;
    // Shift the fade clock by the same pause duration (= cueStartMs delta) so a crossfade in
    // progress continues from where it froze instead of being skipped on resume.
    if (_rt.fading)
        _rt.fadeStartMs += (_rt.cueStartMs - oldCueStartMs);
    if (segment)
    {
        segment->resume();
        reapplyMasterBrightness(segment);
    }
}

// ============================================================================
// Tick — called every loop() iteration
// ============================================================================

void EffektManagerController::tick(Segment* segment, const EffektManagerData* emData)
{
    if (!_rt.isRunning() || !segment) return;
    if (_rt.paused) return;  // frozen on current cue — no advance, no fade

    uint8_t emIdx             = _rt.activeEmId - 1;
    const EffektManagerData& em = emData[emIdx];
    // Guard against a stale/out-of-range cue index (e.g. EM data reloaded with
    // fewer cues). cueCount <= EM_CUE_COUNT, so this keeps the access in bounds.
    if (_rt.activeCueIdx >= em.header.cueCount) { _rt.activeCueIdx = 0; return; }
    const EffektCue& cue      = em.cues[_rt.activeCueIdx];
    uint32_t now              = millis();

    // ── Fade transition in progress ──────────────────────────────────
    if (_rt.fading)
    {
        tickFade(segment, emData);
        return;
    }

    // ── Duration / finite-effect completion check ─────────────────────────
    if (cue.durationSec == EM_DURATION_UNTIL_EFFECT_DONE)
    {
        Effect* effect = segment->getEffect();
        if (!effect || !effect->isDone(segment)) return;

        if (cue.fadeMs > 0)
            startFade(segment, cue.fadeMs);
        else
            advanceToNextCue(segment, emData);
        return;
    }
    if (cue.durationSec == 0) return; // hold indefinitely

    uint32_t elapsed = now - _rt.cueStartMs;
    uint32_t durationMs = (uint32_t)cue.durationSec * 1000;

    if (elapsed >= durationMs)
    {
        // Start fade-out if configured
        if (cue.fadeMs > 0)
            startFade(segment, cue.fadeMs);
        else
            advanceToNextCue(segment, emData);
    }
}

// ============================================================================
// Apply a cue to a segment
// ============================================================================

void EffektManagerController::applyCue(const EffektCue& cue, Segment* segment)
{
    if (!segment) return;

    // Every cue application is a fresh effect start. Runtime state belongs to the
    // segment, so explicitly discard the previous cue's animation position/timers.
    // pause()/resume() never calls applyCue() and therefore preserves this state.
    segment->resetEffectState();

    // Set colour
    segment->setPrimaryColor(cue.r, cue.g, cue.b, cue.w);

    // Cue brightness is RELATIVE to the segment's master (KO/global/console) brightness:
    // render = master * cue / 255. This way a runtime dim (brightness KO) stays authoritative
    // and is NOT yanked back to the cue value on every cue switch. master defaults to 255,
    // so an unconfigured segment renders cues at their own brightness (backward compatible).
    _rt.cueBri = cue.brightness;
    segment->setBrightness((uint8_t)(((uint16_t)segment->getMasterBrightness() * (uint16_t)cue.brightness + 127) / 255));

    // Set effect parameters
    Effect* effect = EffectPool::getEffectByIndex(cue.effectId);
    if (effect)
    {
        uint8_t paramCount = effect->getParameterCount();
        for (uint8_t i = 0; i < paramCount && i < EM_PARAM_COUNT; i++)
        {
            // String parameters take the cue's text, not the numeric slot
            // (a numeric value would be misinterpreted as a char pointer).
            // Only overwrite when the cue actually carries a text; otherwise keep the
            // current text (e.g. one set/appended at runtime via the Effekt-Text KO) -
            // same "runtime value stays authoritative" idea as the brightness above.
            if (effect->getParameterType(i) == ParameterType::PARAM_STRING)
            {
                if (cue.effectText[0] != '\0')
                    effect->setParameter(segment, i, (uint32_t)(uintptr_t)cue.effectText);
                continue;
            }

            uint32_t value = cue.params[i];
            const uint32_t minValue = effect->getParameterMin(i);
            const uint32_t maxValue = effect->getParameterMax(i);

            if (value < minValue)
            {
                value = effect->getParameterDefault(i);
            }
            if (value < minValue) value = minValue;
            if (value > maxValue) value = maxValue;

            effect->setParameter(segment, i, value);
        }


        segment->setEffect(effect);
    }
    else
    {
        segment->clearEffect();
        segment->clear();
    }

    _rt.cueStartMs = millis();
#ifdef OPENKNX_DEBUG
    Serial.printf("[EM] Applied cue %d (effect=%d, dur=%ds)\n", _rt.activeCueIdx + 1, cue.effectId, cue.durationSec);
#endif
}

// ============================================================================
// Re-apply master brightness to the active cue (after a runtime KO/global change)
// ============================================================================

void EffektManagerController::reapplyMasterBrightness(Segment* segment)
{
    if (!segment || !_rt.isRunning()) return;
    // Don't fight an in-flight crossfade — tickFade owns the brightness register
    // during a transition and will pick up the new master on the next cue anyway.
    if (_rt.fading) return;
    segment->setBrightness((uint8_t)(((uint16_t)segment->getMasterBrightness() * (uint16_t)_rt.cueBri + 127) / 255));
}

// ============================================================================
// Advance to next cue / chain / loop
// ============================================================================

void EffektManagerController::advanceToNextCue(Segment* segment, const EffektManagerData* emData)
{
    uint8_t emIdx             = _rt.activeEmId - 1;
    const EffektManagerData& em = emData[emIdx];

    uint8_t nextCue = _rt.activeCueIdx + 1;

    if (nextCue < em.header.cueCount)
    {
        // More cues in this EM
        _rt.activeCueIdx = nextCue;
        applyCue(em.cues[nextCue], segment);
    }
    else if (em.header.loop)
    {
        // Loop: restart from Cue 0
        _rt.activeCueIdx = 0;
        applyCue(em.cues[0], segment);
#ifdef OPENKNX_DEBUG
        Serial.printf("[EM] EM %d looping\n", _rt.activeEmId);
#endif
    }
    else if (em.header.nextEmId != EM_NONE)
    {
        // Chain to next EM — validate target first: start() silently refuses
        // disabled EMs, which would otherwise retry the advance every tick
        // and freeze the segment (dark, if mid-fade).
        uint8_t nextIdx = em.header.nextEmId - 1;
        if (nextIdx < EM_COUNT && emData[nextIdx].header.isEnabled())
        {
#ifdef OPENKNX_DEBUG
            Serial.printf("[EM] EM %d chaining to EM %d\n", _rt.activeEmId, em.header.nextEmId);
#endif
            start(em.header.nextEmId, segment, emData);
        }
        else
        {
#ifdef OPENKNX_DEBUG
            Serial.printf("[EM] EM %d chain target EM %d invalid/disabled — stopping\n", _rt.activeEmId, em.header.nextEmId);
#endif
            stop(segment);
        }
    }
    else
    {
        // Done — stop and resume Effektkette if applicable
#ifdef OPENKNX_DEBUG
        Serial.printf("[EM] EM %d finished\n", _rt.activeEmId);
#endif
        stop(segment);
    }
}

// ============================================================================
// Fade helpers
// ============================================================================

void EffektManagerController::startFade(Segment* segment, uint16_t fadeMs)
{
    _rt.fading      = true;
    _rt.fadeIn      = false;
    _rt.fadeStartMs = millis();
    _rt.fadeMs      = fadeMs ? fadeMs : 1;
    _rt.fadeFromBri = segment ? segment->getBrightness() : 255;
}

// Real crossfade: ramp current cue's brightness down over fadeMs/2,
// switch to the next cue, then ramp the new cue's brightness up over fadeMs/2.
// Works because effects redraw every frame and Segment::setPixel scales by brightness.
void EffektManagerController::tickFade(Segment* segment, const EffektManagerData* emData)
{
    uint32_t elapsed = millis() - _rt.fadeStartMs;
    uint16_t half    = _rt.fadeMs / 2 ? _rt.fadeMs / 2 : 1;

    if (!_rt.fadeIn)
    {
        // ── Fade-out phase ──
        if (elapsed < half)
        {
            segment->setBrightness((uint8_t)((uint32_t)_rt.fadeFromBri * (half - elapsed) / half));
            return;
        }
        // Fade-out done — switch cue (applyCue sets the new cue's brightness)
        advanceToNextCue(segment, emData);
        if (!_rt.isRunning()) return; // EM stopped — no fade-in (stop() restored brightness)
        // Fade in the new cue — also across a chain boundary (start() cleared fading)
        _rt.fading      = true;
        _rt.fadeIn      = true;
        _rt.fadeStartMs = millis();
        _rt.fadeFromBri = segment->getBrightness(); // target = new cue brightness
        segment->setBrightness(0);                  // start fade-in from black
        return;
    }

    // ── Fade-in phase ──
    if (elapsed < half)
    {
        segment->setBrightness((uint8_t)((uint32_t)_rt.fadeFromBri * elapsed / half));
        return;
    }
    segment->setBrightness(_rt.fadeFromBri);
    _rt.fading = false;
    _rt.fadeIn = false;
}

bool EffektManagerController::triggerCue(uint8_t cueNum, Segment* segment, const EffektManagerData* emData)
{
    if (!_rt.isRunning() || !segment || !emData) return false;

    if (cueNum == 0) return false;

    uint8_t emIdx = _rt.activeEmId - 1;
    if (emIdx >= EM_COUNT) return false;

    const EffektManagerData& em = emData[emIdx];
    if (!em.header.isEnabled() || cueNum > em.header.cueCount) return false;

    _rt.activeCueIdx = cueNum - 1;
    _rt.fading = false;
    _rt.fadeIn = false;
    _rt.fadeStartMs = 0;

    applyCue(em.cues[_rt.activeCueIdx], segment);
    return true;
}

void EffektManagerController::pauseEffektkette(Segment* segment)
{
    if (!segment || !segment->isVirtualBand()) return;
    _savedBandTotal  = segment->getVirtualTotalLength();
    _savedBandOffset = segment->getVirtualOffset();
    segment->clearVirtualBand();
}

void EffektManagerController::resumeEffektkette(Segment* segment)
{
    if (!segment || _savedBandTotal == 0) return;
    segment->setVirtualBand(_savedBandTotal, _savedBandOffset);
    _savedBandTotal  = 0;
    _savedBandOffset = 0;
}
