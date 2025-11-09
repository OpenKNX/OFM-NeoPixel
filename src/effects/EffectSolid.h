/**
 * @file EffectSolid.h
 * @brief Solid Color Effect - STATELESS
 *
 * Simplest effect: Set all pixels in segment to one color
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "Effect.h"
#include <stdint.h>

class EffectSolid : public Effect
{
  public:
    EffectSolid() = default;                                    // Constructor
    virtual ~EffectSolid() = default;                           // Destructor
    void update(Segment* segment, uint32_t deltaTime) override; // Update method
    const char* getName() override { return "Solid"; }          // Effect name
};