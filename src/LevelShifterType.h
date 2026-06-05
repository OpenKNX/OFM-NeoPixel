#pragma once

#include <stdint.h>

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
 *   - Used on: QuinLED Dig-Uno V3, Dig-Quad V3, Dig-Octa 32-8L
 *
 * **SN74AHCT125** – TI SN74AHCT125 quad unidirectional bus buffer (AHCT variant):
 *   - Functionally identical to SN74HCT125 but faster switching (~5ns vs ~7ns).
 *   - Same OE/input behaviour; no firmware GPIO changes needed.
 *   - Used on: QuinLED Dig-Next-2
 *
 * RP2040/RP2350 (PIO driver):
 *   - Drive strength is already 12 mA + FAST slew (sufficient for TXS0108E).
 *   - When TXS0108E is selected: internal pull-up/down is explicitly disabled.
 *   - When SN74HCT125/SN74AHCT125 is selected: no GPIO changes (default 4mA fine).
 *
 * ESP32 (RMT driver):
 *   - Default drive capability is 20 mA (GPIO_DRIVE_CAP_2).
 *   - When TXS0108E is selected: boosted to 40 mA (GPIO_DRIVE_CAP_3) +
 *     internal pull explicitly set to FLOATING.
 *   - When SN74HCT125/SN74AHCT125 is selected: no GPIO changes (20mA fine).
 *
 * **Auto-config via build flag:**
 * Set -DNEOPIXEL_HW_LEVELSHIFTER=<value> in platformio.hardware.ini to
 * automatically configure the default level-shifter type at compile time.
 * Values: 0=NONE, 1=TXS0108E, 2=SN74HCT125, 3=SN74AHCT125
 */
enum class LevelShifterType : uint8_t
{
    NONE        = 0,  ///< No level shifter / direct connection
    TXS0108E    = 1,  ///< TI TXS0108E bidirectional auto-direction level shifter (KNeoPiX)
    SN74HCT125  = 2,  ///< TI SN74HCT125 quad unidirectional buffer, OE=GND (QuinLED Dig-Uno/Quad/Octa)
    SN74AHCT125 = 3,  ///< TI SN74AHCT125 quad unidirectional buffer, faster AHCT variant (QuinLED Dig-Next-2)
};

/**
 * @brief Compile-time default level-shifter type from NEOPIXEL_HW_LEVELSHIFTER build flag.
 *
 * Set -DNEOPIXEL_HW_LEVELSHIFTER=1 (etc.) in the board section of platformio.hardware.ini
 * to automatically initialise SerialStripConfig with the correct type for the hardware.
 * The console command 'neo phys config <i> levelshifter' can override at runtime.
 */
#if defined(NEOPIXEL_HW_LEVELSHIFTER) && NEOPIXEL_HW_LEVELSHIFTER != 0
    static constexpr LevelShifterType kHwDefaultLevelShifter = static_cast<LevelShifterType>(NEOPIXEL_HW_LEVELSHIFTER);
#else
    static constexpr LevelShifterType kHwDefaultLevelShifter = LevelShifterType::NONE;
#endif
