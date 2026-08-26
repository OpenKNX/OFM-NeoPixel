/**
 * @file EffectSolid.h
 * @brief Solid Color Effect - STATELESS
 *
 * Simplest effect: Set all pixels in segment to one color
 *
 * Note: Uses segment's config color components, no additional parameters
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once

#include "Effect.h"
#include <stdint.h>

class EffectSolid : public Effect
{
  public:
    EffectSolid() = default;
    virtual ~EffectSolid() = default;

    void update(Segment* segment, uint32_t deltaTime) override;
    const char* getName(const char* lang = nullptr) override { return "Solid"; }
    const char* getDescription(const char* lang = nullptr) override { return EFFECT_DESC_DE_EN(
                "Statische Farbe ohne Animation",
                "Static solid color"); }

    // ====================================================================
    // Parameter API (Solid has no params - uses config color components)
    // ====================================================================
    uint8_t getParameterCount() const override { return 0; }
    const char* getParameterName(uint8_t index) const override { return nullptr; }
    ParameterType getParameterType(uint8_t index) const override { return ParameterType::PARAM_UINT8; }
    uint32_t getParameterDefault(uint8_t index) const override { return 0; }
    uint32_t getParameter(const Segment* segment, uint8_t index) const override { return 0; }
    void setParameter(Segment* segment, uint8_t index, uint32_t value) override {}
};
