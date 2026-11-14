#pragma once

#include <stdint.h>

// HW_LEVELSHIFTER of the board header (OGM-HardwareConfig); included here so every user of this header sees it
#include "HardwareConfig.h"

/**
 * @brief Level-shifter type for GPIO signal conditioning
 *
 * Tells the serial strip driver which (if any) level shifter is connected
 * to the data line. The driver applies hardware-specific GPIO optimizations.
 *
 * **NONE** (default): direct connection or generic/passive level shifter.
 *
 * **TXS0108E** – TI TXS0108E bidirectional auto-direction level shifter:
 *   - Requires strong drive strength on the A-side (MCU output) so that the
 *     internal direction-detect mechanism triggers reliably and the B-side
 *     output edges are clean.
 *   - No internal pull-up/down on the data pin; resistive loads interfere
 *     with the auto-direction RC.
 *   - OE pin is hardware-fixed (not firmware-controlled); no GPIO action
 *     required for enable/disable.
 *   - Used on: OpenKNXiao KNeoPiX (RP2040/RP2350/ESP32S3/C3/C5/C6)
 *
 * **SN74HCT125** – TI SN74HCT125 quad unidirectional bus buffer:
 *   - One-directional (MCU → LED), OE pin typically wired to GND (always on).
 *   - TTL-compatible inputs: VIH ≥ 2.0V → satisfied by 3.3V MCU output.
 *   - No firmware GPIO changes needed; transparent to the MCU.
 *   - Drive strength: default 4mA (RP2040) or 20mA (ESP32) is sufficient.
 *   - Set in the board header of: QuinLED Dig-Uno V3 (WiFi, Ethernet), Dig-Uno V4 (WiFi, Ethernet)
 *
 * **SN74AHCT125** – TI SN74AHCT125 quad unidirectional bus buffer (AHCT variant):
 *   - Functionally identical to SN74HCT125 but faster switching (~5ns vs ~7ns).
 *   - Same OE/input behaviour; no firmware GPIO changes needed.
 *   - Set in the board header of: QuinLED Dig-Next-2
 *
 * **SN74LV8T245** – TI SN74LV8T245 octal single-supply bus transceiver:
 *   - Wired one-directional (MCU → LED): DIR tied high, OE tied to GND.
 *   - TTL-compatible inputs: VIH 2.0V at VCC 5V → satisfied by a 3.3V MCU output.
 *   - External pull-downs on the MCU side define the power-up state, so the
 *     driver must not enable an internal pull-up against them.
 *   - No firmware GPIO changes needed; default drive strength is sufficient.
 *   - Used on: OpenKNXiao KNeoPiX v1.5
 *
 * RP2040/RP2350 (PIO driver):
 *   - Drive strength is already 12 mA + FAST slew (sufficient for TXS0108E).
 *   - When TXS0108E is selected: internal pull-up/down is explicitly disabled.
 *   - When SN74HCT125/SN74AHCT125/SN74LV8T245 is selected: no GPIO changes (default 4mA fine).
 *
 * ESP32 (RMT driver):
 *   - Default drive capability is 20 mA (GPIO_DRIVE_CAP_2).
 *   - When TXS0108E is selected: boosted to 40 mA (GPIO_DRIVE_CAP_3) +
 *     internal pull explicitly set to FLOATING.
 *   - When SN74HCT125/SN74AHCT125/SN74LV8T245 is selected: no GPIO changes (20mA fine).
 *
 * **Auto-config from the board header:**
 * The board block in OGM-HardwareConfig sets `#define HW_LEVELSHIFTER HW_LEVELSHIFTER_<type>`
 * (names in HardwareConfig/OpenKNX/Xiao.h); a -DNEOPIXEL_HW_LEVELSHIFTER=<value> build flag overrides it.
 * Values: 0=NONE, 1=TXS0108E, 2=SN74HCT125, 3=SN74AHCT125, 4=SN74LV8T245 (the numbers live in Xiao.h only)
 */
enum class LevelShifterType : uint8_t
{
    NONE        = HW_LEVELSHIFTER_NONE,         ///< No level shifter / direct connection
    TXS0108E    = HW_LEVELSHIFTER_TXS0108E,     ///< TI TXS0108E bidirectional auto-direction level shifter (KNeoPiX)
    SN74HCT125  = HW_LEVELSHIFTER_SN74HCT125,   ///< TI SN74HCT125 quad unidirectional buffer, OE=GND (QuinLED Dig-Uno V3/V4)
    SN74AHCT125 = HW_LEVELSHIFTER_SN74AHCT125,  ///< TI SN74AHCT125 quad unidirectional buffer, faster AHCT variant (QuinLED Dig-Next-2)
    SN74LV8T245 = HW_LEVELSHIFTER_SN74LV8T245,  ///< TI SN74LV8T245 octal single-supply buffer, DIR=H, OE=GND (KNeoPiX v1.5)
};

/**
 * @brief Compile-time default level-shifter type: NEOPIXEL_HW_LEVELSHIFTER build flag, else HW_LEVELSHIFTER
 * of the board header.
 *
 * Initialises SerialStripConfig with the type of the hardware.
 * The console command 'neo phys config <i> levelshifter' can override at runtime.
 */
#if !defined(NEOPIXEL_HW_LEVELSHIFTER) && defined(HW_LEVELSHIFTER)
    #define NEOPIXEL_HW_LEVELSHIFTER HW_LEVELSHIFTER
#endif
#if defined(NEOPIXEL_HW_LEVELSHIFTER) && NEOPIXEL_HW_LEVELSHIFTER != 0
    static constexpr LevelShifterType kHwDefaultLevelShifter = static_cast<LevelShifterType>(NEOPIXEL_HW_LEVELSHIFTER);
#else
    static constexpr LevelShifterType kHwDefaultLevelShifter = LevelShifterType::NONE;
#endif
