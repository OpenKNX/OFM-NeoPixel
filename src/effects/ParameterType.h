/**
 * @file ParameterType.h
 * @brief Effect Parameter Type Descriptors
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#pragma once
#include <stdint.h>

/**
 * @brief Parameter Type Enum
 * 
 * Describes parameter types for effect parameters.
 * Used for UI generation and console commands.
 */
enum class ParameterType : uint8_t
{
    PARAM_UINT8,      // 0-255 (Speed, BPM, etc.)
    PARAM_UINT16,     // 0-65535 (Position, Counter)
    PARAM_UINT32,     // 0-4294967295 (Timestamp)
    PARAM_INT8,       // -128 to 127 (Offset)
    PARAM_INT16,      // -32768 to 32767 (Direction)
    PARAM_BOOL,       // 0 or 1 (Reverse, Mirror)
    PARAM_COLOR_RGB,  // 0xRRGGBB00 (24-bit RGB)
    PARAM_COLOR_RGBW, // 0xRRGGBBWW (32-bit RGBW)
    PARAM_PERCENT,    // 0-100 (Intensity, Fade)
    PARAM_ANGLE,      // 0-359 (Rotation, Phase)
    PARAM_HUE,        // 0-255 (HSV Hue)
    PARAM_ENUM        // Effect-specific enum
};
