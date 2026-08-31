/**
 * @file EffektManager.h
 * @brief Effektmanager — global effect preset sequencer for NeoPixel segments
 *
 * Architecture:
 *   EffektManager  (16 global instances, stored in KNX-Flash via ETS)
 *     └── EffektCue  (up to EM_CUE_COUNT=10 cues per EM, sequential playback)
 *           └── applyTo(Segment*)  → sets effect + parameters + colour + text
 *
 * Each segment can activate any EM via KO. The EM runs its cues in order,
 * applying each to the segment. When finished it can chain to another EM
 * or loop. Cue transitions use a configurable fade-out.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once

#include "Segment.h"
#include "effects/EffectPool.h"
#include "effects/ScrollTextEffect.h"
#include <stdint.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================
static constexpr uint8_t  EM_COUNT      = 16;   ///< Number of Effektmanager instances
static constexpr uint8_t  EM_CUE_COUNT  = 10;   ///< Max cues per EM (matches ETS: Cue 1..10 per EM)
static constexpr uint8_t  EM_TEXT_LEN   = 14;   ///< Cue text length (DPT 16 compatible)
static constexpr uint8_t  EM_PARAM_COUNT = 10;  ///< Max effect parameters per cue

// EM ID 0 = "no EM / stop"
static constexpr uint8_t  EM_NONE       = 0;
static constexpr uint16_t EM_DURATION_UNTIL_EFFECT_DONE = UINT16_MAX;

// P6 — what the segment does after an EM stops / a chain finishes.
// Values 0..2 map 1:1 to the ETS "EM-Stop-Rückkehr" enum; LEAVE is internal (interrupt).
enum EmStopMode : uint8_t
{
    EM_STOP_LAST    = 0,  ///< restore the pre-EM DIRECT snapshot (keeps live dim, conflict#1=b) — ETS "Letzter Zustand"
    EM_STOP_DEFAULT = 1,  ///< apply the segment's ETS default config — handled in OAM — ETS "Default"
    EM_STOP_OFF     = 2,  ///< blank the segment — ETS "Aus"
    EM_STOP_LEAVE   = 3,  ///< leave the segment as-is (a direct KO sets the new visual) — interrupt only
};

// ============================================================================
// EffektCue — one effect preset (48 bytes)
// ============================================================================
struct EffektCue
{
    uint8_t  effectId;                  ///< Effect ID (0-33, 0=Solid/Aus)
    uint8_t  params[EM_PARAM_COUNT];    ///< Effect parameters 0-9
    uint8_t  r, g, b, w;               ///< Primary colour
    uint8_t  brightness;               ///< Brightness 0-255
    uint16_t durationSec;              ///< Seconds; 0=hold, UINT16_MAX=until effect reports done
    uint16_t fadeMs;                   ///< Fade-out duration before next cue (ms, 0 = hard cut)
    char     cueName[EM_TEXT_LEN];     ///< Cue name (null-terminated)
    char     effectText[EM_TEXT_LEN];  ///< Effect-specific text (null-terminated)
};
// Struct is packed by layout: 1+10+4+1+2+2+14+14 = 48 bytes (no padding needed)
static_assert(sizeof(EffektCue) == 48, "EffektCue size mismatch — check layout");

// ============================================================================
// EffektManagerHeader — one EM configuration (~8 bytes)
// ============================================================================
struct EffektManagerHeader
{
    // Layout muss exakt dem ETS-XML entsprechen (NeoPixel.EM.templ.xml):
    uint8_t  reserved[16];   ///< Byte  0–15: frei/reserviert (EM-Beschreibung ist ETS-only, nicht gespeichert)
    uint8_t  cueCount;       ///< Byte 16:    Anzahl aktiver Cues (1–10)
    uint8_t  loop : 1;       ///< Byte 17, Bit 0: Loop
    uint8_t  _pad : 7;
    uint8_t  nextEmId;       ///< Byte 18:    Folgeziel (0=Stop, 1–16)
    uint8_t  enabled;        ///< Byte 19:    Zustand (0=Deaktiviert, 1=Aktiv, 2=Suspendiert)

    EffektManagerHeader()
        : cueCount(1), loop(0), _pad(0), nextEmId(EM_NONE), enabled(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }

    // Runnable only when ACTIVE (1). Paused (2) keeps full config + KOs but does
    // NOT render; Inactive (0) is fully off (ETS also removes its KOs).
    bool isEnabled() const { return enabled == 1 && cueCount > 0; }
    bool isPaused() const { return enabled == 2; }
    bool isConfigured() const { return enabled != 0; }
};

// ============================================================================
// EffektManagerData — full data for one EM (stored in KNX-Flash via ETS)
// ============================================================================
struct EffektManagerData
{
    EffektManagerHeader header;
    EffektCue           cues[EM_CUE_COUNT];

    EffektManagerData() = default;
};
// 16 × (20 + 10 × 48) = 16 × 500 = 8000 bytes ≈ 8 KB (EM_CUE_COUNT matches ETS Cue 1..10)

// ============================================================================
// EffektManagerRuntime — per-segment runtime state (RAM only, not persisted)
// ============================================================================
struct EffektManagerRuntime
{
    uint8_t  activeEmId    = EM_NONE;  ///< Currently running EM (0 = idle)
    uint8_t  activeCueIdx  = 0;        ///< Current cue index (0-based)
    uint32_t cueStartMs    = 0;        ///< millis() when current cue started
    uint32_t fadeStartMs   = 0;        ///< millis() when fade phase started (0 = not fading)
    bool     fading        = false;    ///< true during fade transition
    bool     fadeIn        = false;    ///< false = fade-out phase, true = fade-in phase
    uint16_t fadeMs        = 300;      ///< total fade duration (from cue.fadeMs)
    uint8_t  fadeFromBri   = 255;      ///< brightness reference for current ramp
    uint8_t  cueBri        = 255;      ///< current cue's RELATIVE brightness (scaled against segment master) — kept so a runtime master change can be re-applied without a cue switch
    uint8_t  lastEmId      = EM_NONE;  ///< EM active before power-off (for restore)
    uint8_t  lastCueIdx    = 0;        ///< Cue active before power-off
    bool     paused        = false;    ///< true = EM frozen on current cue (Pause), still showing it
    uint32_t pauseElapsed  = 0;        ///< cue elapsed time captured at pause, to continue on resume

    bool isRunning() const { return activeEmId != EM_NONE; }
    bool isPaused()  const { return paused && activeEmId != EM_NONE; }
};

// ============================================================================
// EffektManagerController — manages EM execution for one segment
// ============================================================================
class EffektManagerController
{
  public:
    /**
     * @brief Start an Effektmanager on a segment.
     *
     * Interrupts any currently running EM immediately (Variante A).
     * If the segment is part of an Effektkette (virtual band), the chain
     * is paused until the EM finishes or is stopped.
     *
     * @param emId   1-based EM index (1-16), or EM_NONE (0) to stop
     * @param segment Target segment
     * @param emData  Pointer to the EM data array (indexed by emId-1)
     */
    void start(uint8_t emId, Segment* segment, const EffektManagerData* emData);

    /**
     * @brief Stop the running EM.
     * @param mode  EmStopMode — what the segment does afterwards:
     *              EM_STOP_LAST   → restore the DIRECT snapshot (keeps live dim) / blank if none
     *              EM_STOP_OFF    → blank the segment
     *              EM_STOP_LEAVE  → leave as-is (interrupt: the direct KO sets the new visual)
     *              EM_STOP_DEFAULT→ OAM intercepts (applies ETS config); OFM falls back to LAST.
     * Effektkette (if active) resumes after stop.
     */
    void stop(Segment* segment, uint8_t mode = EM_STOP_LAST);

    /**
     * @brief Pause the running EM: freeze the current cue (keeps it shown), stop advancing.
     * No-op if not running or already paused.
     */
    void pause(Segment* segment);

    /**
     * @brief Resume a paused EM: continue the current cue from where it was frozen.
     * No-op if not running or not paused.
     */
    void resume(Segment* segment);

    /**
     * @brief Advance the sequencer — call every loop() tick.
     *
     * Handles:
     *   - Cue duration timer
     *   - Fade-out transition
     *   - Auto-advance to next cue / chain to next EM
     *   - Loop
     */
    void tick(Segment* segment, const EffektManagerData* emData);

    /**
     * @brief Apply a single cue to a segment immediately.
     * Sets effect, parameters, colour, brightness, and (if applicable) text.
     */
    void applyCue(const EffektCue& cue, Segment* segment);

    /** @brief Returns true if an EM is currently running on this segment (incl. paused). */
    bool isRunning() const { return _rt.isRunning(); }

    /** @brief Returns true if the running EM is paused (frozen on a cue). */
    bool isPaused() const { return _rt.isPaused(); }

    /** @brief Run-state for status KO: 0 = stopped, 1 = running, 2 = paused. */
    uint8_t runState() const { return _rt.activeEmId == EM_NONE ? 0 : (_rt.paused ? 2 : 1); }

    /**
     * @brief Re-apply the segment's master brightness to the active cue.
     * Call after a KO/global/console brightness change while an EM is running, so the
     * new master takes effect immediately instead of only at the next cue switch.
     * No-op if no EM is running. Cue brightness is RELATIVE: render = master*cueBri/255.
     */
    void reapplyMasterBrightness(Segment* segment);

    /**
     * @brief Trigger a specific cue of the currently active EM immediately.
     *
     * @param cueNum 1-based cue number
     * @return true if cue was applied, false on invalid state/range
     */
    bool triggerCue(uint8_t cueNum, Segment* segment, const EffektManagerData* emData);

    /** @brief Returns the active EM ID (0 if idle). */
    uint8_t activeEmId()  const { return _rt.activeEmId; }

    /** @brief Returns the active Cue index (1-based, 0 if idle). */
    uint8_t activeCueNum() const { return _rt.isRunning() ? _rt.activeCueIdx + 1 : 0; }

    /** @brief Save state for power-off persistence. */
    void saveState()
    {
        _rt.lastEmId   = _rt.activeEmId;
        _rt.lastCueIdx = _rt.activeCueIdx;
    }

    /** @brief Restore after power-on (restarts the saved EM from Cue 1). */
    void restoreState(Segment* segment, const EffektManagerData* emData)
    {
        if (_rt.lastEmId != EM_NONE)
            start(_rt.lastEmId, segment, emData);
    }

    /** @brief Get the last known EM ID (for flash persistence). */
    uint8_t lastEmId() const { return _rt.lastEmId; }

    /** @brief Set the last known EM ID (loaded from flash). */
    void setLastEmId(uint8_t emId) { _rt.lastEmId = emId; }
  private:
    EffektManagerRuntime _rt;
    uint16_t _savedBandTotal  = 0; ///< Effektkette total length saved before EM start
    uint16_t _savedBandOffset = 0; ///< Effektkette offset saved before EM start

    void advanceToNextCue(Segment* segment, const EffektManagerData* emData);
    void startFade(Segment* segment, uint16_t fadeMs);
    void tickFade(Segment* segment, const EffektManagerData* emData);
    void pauseEffektkette(Segment* segment);
    void resumeEffektkette(Segment* segment);
};
