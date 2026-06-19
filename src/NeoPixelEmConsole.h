/**
 * @file        NeoPixelEmConsole.h
 * @brief       Console bridge between OFM-NeoPixel (rendering/parsing) and the
 *              OAM backend (Effektmanager/Effektkette data and actions).
 *
 * All console RENDERING for neo em/cue/chain lives in the OFM
 * (NeoPixelConsole.cpp). The OAM only provides raw data via the weak
 * provider hooks below and executes mutating actions via the action hook.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */
#pragma once

#include "EffektManager.h"
#include <stdint.h>

// ============================================================================
// Mutating console actions (executed by the OAM backend)
// ============================================================================
enum : uint8_t
{
    NEO_EM_START = 1, ///< arg1=segment, arg2=EM id (0=stop)
    NEO_EM_STOP,      ///< arg1=segment
    NEO_EM_CUE,       ///< arg1=segment, arg2=cue number (1-based)
    NEO_CHAIN_SET,    ///< arg1=segment, arg2=mode (0=off, 1=master, 2=slave)
    NEO_CHAIN_OVERRIDE, ///< arg1=segment, arg2=flag (0|1)
    NEO_CHAIN_TRIGGER,  ///< arg1=segment
    // ── Local-test EM/cue authoring (build a cycling EM from the console) ──────
    NEO_EM_CONFIG,    ///< arg1=EM id (1-based), arg2=(loop & 0xFF) | (nextEm << 8); sets enabled=1
    NEO_EM_BIND,      ///< arg1=manager segment index → wrap into OAM _segments (test mode)
    NEO_CUE_CLEAR,    ///< arg1=EM id (1-based) → cueCount=0
};

// ============================================================================
// Data snapshots provided by the OAM backend for OFM-side rendering
// ============================================================================
struct NeoEmSegStatus
{
    bool hasSegment;      ///< Segment instance exists
    uint8_t activeEmId;   ///< Active EM id (1-based, 0 = none)
    uint8_t activeCueNum; ///< Active cue number (1-based, 0 = idle)
    bool running;         ///< EM currently running
    uint8_t startupEm;    ///< Startup EM from ETS (0 = none)
    uint8_t lastEmId;     ///< Last EM (power-off persistence)
};

struct NeoChainSegStatus
{
    uint8_t syncMode;           ///< 0=off, 1=master, 2=slave
    uint8_t overridePolicy;     ///< Sync override policy
    uint8_t timeoutSteps;       ///< Sync timeout steps
    bool localOverride;         ///< Local override active
    uint32_t lastSyncMs;        ///< millis() of last sync telegram
    uint16_t virtualTotalLength; ///< Virtual band total length
    uint16_t virtualOffset;      ///< Virtual band offset of this segment
};

// ============================================================================
// Weak backend hooks (default no-op in OFM, implemented by the OAM)
// ============================================================================
/** @brief Number of EM-managed segments, -1 if no backend present. */
int openknxNeoPixelEmSegmentCount();

/** @brief Fill EM status snapshot for a segment. Returns false if seg invalid. */
bool openknxNeoPixelGetEmStatus(uint8_t seg, NeoEmSegStatus& out);

/** @brief Get EM definition data (header + cues). emId is 1-based; nullptr if invalid. */
const EffektManagerData* openknxNeoPixelGetEmData(uint8_t emId);

/** @brief Fill Effektkette status snapshot for a segment. Returns false if seg invalid. */
bool openknxNeoPixelGetChainStatus(uint8_t seg, NeoChainSegStatus& out);

/** @brief Execute a mutating action (NEO_EM_xxx / NEO_CHAIN_xxx). Returns true on success;
 *         on failure the backend prints the reason via openknxNeoPixelConsolePrintf. */
bool openknxNeoPixelHandleEmChainAction(uint8_t action, int arg1, int arg2);

/** @brief Define/overwrite one cue (local-test authoring). emId & cueNum are 1-based;
 *         params keep effect defaults. Auto-bumps the EM's cueCount. Returns true on success. */
bool openknxNeoPixelHandleCueSet(uint8_t emId, uint8_t cueNum, uint8_t effectId,
                                 uint16_t durSec, uint16_t fadeMs,
                                 uint8_t bri, uint8_t r, uint8_t g, uint8_t b);

/** @brief Override one effect parameter of an existing cue (local-test authoring).
 *         emId & cueNum are 1-based; paramIdx is 0-based (0..EM_PARAM_COUNT-1).
 *         The cue must already exist (via cue set). Returns true on success. */
bool openknxNeoPixelHandleCueParam(uint8_t emId, uint8_t cueNum, uint8_t paramIdx, uint8_t value);

/** @brief Plain console output bridge for OAM backend messages (errors etc.). */
void openknxNeoPixelConsolePrintf(const char* fmt, ...);
