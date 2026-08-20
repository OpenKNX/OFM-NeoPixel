/**
 * @file        NeoPixelConsole.cpp
 * @brief       Console command handlers for NeoPixel LED control system
 *
 * This file contains all console command processing for the NeoPixel module.
 * Separated from main NeoPixel.cpp for better code organization.
 *
 * @copyright Copyright (c) 2025 Erkan Çolak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include "NeoPixel.h"

// BStandard library includes
#include <cstdarg>
#include <cstdio>
#include <sstream>

// Effect system includes
#include "effects/Effect.h"
#include "effects/EffectPool.h"

// Performance tracking
#include "test/PerformanceTracker.h"

// Platform-specific driver includes for hardware info
#ifdef ARDUINO_ARCH_RP2040
    #include "hardware/clocks.h" // For clock_get_hz()
    #include "pio/pio_neopixel_serial.h"
    #include "pio/pio_neopixel_spi.h"
#endif
#ifdef ARDUINO_ARCH_ESP32
    #include "rmt/rmt_neopixel_serial.h"
#endif

// Test systems (optional)
#ifdef OPENKNX_NEOPIXEL_TESTS
    #include "test/AnimationTest.h"
    #include "test/SimpleTest.h"
#endif

// Benchmark system (optional)
#ifdef OPENKNX_NEOPIXEL_BENCHMARK
    #include "test/Benchmark.h"
#endif

// External performance tracker (defined in NeoPixel.cpp)!!
extern PerformanceTracker g_perfTracker;

#include "NeoPixelTimingScan.h"

// Effektmanager/Effektkette console bridge (action enum, data provider hooks)
#include "NeoPixelEmConsole.h"

// Weak backend hooks — implemented by the OAM. Defaults: no backend present.
int __attribute__((weak)) openknxNeoPixelEmSegmentCount()
{
    return -1;
}

bool __attribute__((weak)) openknxNeoPixelGetEmStatus(uint8_t seg, NeoEmSegStatus& out)
{
    (void)seg;
    (void)out;
    return false;
}

const EffektManagerData* __attribute__((weak)) openknxNeoPixelGetEmData(uint8_t emId)
{
    (void)emId;
    return nullptr;
}

bool __attribute__((weak)) openknxNeoPixelGetChainStatus(uint8_t seg, NeoChainSegStatus& out)
{
    (void)seg;
    (void)out;
    return false;
}

bool __attribute__((weak)) openknxNeoPixelHandleEmChainAction(uint8_t action, int arg1, int arg2)
{
    (void)action;
    (void)arg1;
    (void)arg2;
    return false;
}

// Plain console output bridge for OAM backend messages (errors etc.).
void openknxNeoPixelConsolePrintf(const char* fmt, ...)
{
    char buffer[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    openknx.logger.log(buffer);
}

// ============================================================================
// Clone Timing Profiles (definition — declared extern in NeoPixelTimingScan.h)
// ============================================================================
const CloneTimingProfile kCloneProfiles[] = {

    // name            T0H  T0L  T1H  T1L  reset  description
    { "STANDARD",    375, 875, 750, 500, 300, "800kHz canonical waveform, 300us reset" },
    { "SK6812_STD",  375, 875, 750, 500,  80, "800kHz canonical waveform, 80us reset" },
    { "SLOW_RESET",  375, 875, 750, 500, 280, "800kHz canonical waveform, 280us reset" },
    { "SLOW10",      416, 972, 833, 555,  80, "720kHz waveform, 80us reset" },
    { "SLOW10_LONG", 416, 972, 833, 555, 280, "720kHz waveform, 280us reset" },
    { "SLOW20_LONG", 469,1094, 938, 625, 300, "640kHz waveform, 300us reset" },
};
const uint8_t  kCloneProfileCount   = sizeof(kCloneProfiles) / sizeof(kCloneProfiles[0]);
const uint32_t kScanColorDurationMs = 2000;  ///< Show each profile for 2 s before prompting
const uint32_t kScanPauseDurationMs = 300;   ///< Brief blank gap between profiles (ms)
const uint32_t kScanWaitTimeoutMs   = 10000; ///< Auto-advance if no input after 10 s

bool applyCloneTimingProfile(PhysicalStrip* strip, const CloneTimingProfile& profile)
{
    return strip && strip->setCustomTiming(profile.t0hNs, profile.t0lNs,
                                            profile.t1hNs, profile.t1lNs,
                                            profile.resetUs);
}

bool writeCloneTimingStressPayload(PhysicalStrip* strip)
{
    if (!strip) return false;
    uint8_t* buffer = strip->getBuffer();
    const size_t size = strip->getBufferSize();
    if (!buffer || size == 0) return false;

    static constexpr uint8_t pattern[] = {0x00, 0xFF, 0xAA, 0x55, 0x80, 0x01};
    for (size_t i = 0; i < size; i++) buffer[i] = pattern[i % sizeof(pattern)];
    return true;
}

// ============================================================================
// Helper Functions
// ============================================================================
/**
 * @brief Get protocol name string from LedProtocol enum
 * @param protocol LED protocol enum value
 * @return Protocol name as C-string
 */
static const char* getProtocolName(LedProtocol protocol)
{
    switch (protocol)
    {
        case LedProtocol::WS2812: return "WS2812";
        case LedProtocol::WS2812B: return "WS2812B";
        case LedProtocol::WS2813: return "WS2813";
        case LedProtocol::WS2815: return "WS2815";
        case LedProtocol::WS2811: return "WS2811";
        case LedProtocol::SK6812: return "SK6812";
        case LedProtocol::SK6805: return "SK6805";
        case LedProtocol::WS2814: return "WS2814";
        case LedProtocol::TM1814: return "TM1814";
        case LedProtocol::GS8208: return "GS8208";
        case LedProtocol::SK6812_RGBCCT: return "SK6812_RGBCCT";
        case LedProtocol::WS2814_RGBCCT: return "WS2814_RGBCCT";
        case LedProtocol::WS2805_RGBCCT: return "WS2805_RGBCCT";
        case LedProtocol::APA102: return "APA102";
        case LedProtocol::APA102_CLONE: return "APA102_CLONE";
        case LedProtocol::SK9822: return "SK9822";
        case LedProtocol::WS2801: return "WS2801";
        case LedProtocol::LPD8806: return "LPD8806";
        default: return "Unknown";
    }
}

// ============================================================================
// Console Command Interface
// ============================================================================
/**
 * @brief Show help for console commands
 */
void NeoPixel::showHelp()
{
    openknx.console.printHelpLine("neo", "NeoPixel LED Control Module. Use 'neo ?' for more.");
    openknx.console.printHelpLine("neo em ?", "Effektmanager commands (status/start/stop/dump)");
    openknx.console.printHelpLine("neo cue ?", "Cue commands (trigger/list via em dump)");
    openknx.console.printHelpLine("neo chain ?", "Effektkette sync commands (status/set/override/trigger)");
}

/**
 * @brief Process console commands
 * @param command Command to process
 * @param diagnose Diagnostic mode flag
 * @return true if command was processed
 */
bool NeoPixel::processCommand(const std::string command, bool diagnose)
{
    // Don't process in diagnostic mode
    if (diagnose)
    {
        return false;
    }

    // Scan control: intercept next/apply/stop while a qualify scan is active
    if (_scanPhase != ScanPhase::IDLE)
    {
        if (command == "neo scan next"  || command == "neo qualify next")  return processScanControlCommand("next");
        if (command == "neo scan apply" || command == "neo qualify apply") return processScanControlCommand("apply");
        if (command == "neo scan stop"  || command == "neo qualify stop")  return processScanControlCommand("stop");
    }

    // Check if command starts with "neo"
    if (command.compare(0, 4, "neo ") != 0 && command != "neo" && command != "neo ?")
    {
        return false;
    }

    // Handle help command
    if (command == "neo ?" || command == "neo help" || command.length() == 3)
    {
        openknx.logger.begin();
        printHelpSectionHeader("NeoPixel LED Control Module");
        openknx.logger.log("Command(s)               Description");
        openknx.logger.log("─────────────────────────────────────────────────────────────────────────────────");

        // Basic Commands
        openknx.console.printHelpLine("neo info", "Show complete system information & status");
        openknx.console.printHelpLine("neo list", "List all physical LED strips");
        openknx.console.printHelpLine("neo update", "Force update all strips");
        openknx.console.printHelpLine("neo clear", "Turn off all LEDs");
        openknx.console.printHelpLine("neo test <strip>", "Run test pattern on strip (0-based index)");
        openknx.console.printHelpLine("neo speed <mode>", "Set update speed: slow|normal|fast|max|extrameludicrous|ftl");
        openknx.console.printHelpLine("neo auto on|off", "Enable/disable auto-update mode");
        openknx.console.printHelpLine("neo perf", "Show performance statistics (requires auto-update)");

        printHelpSectionHeader("PhysicalStrip Management");
        openknx.console.printHelpLine("neo phys ?", "Show detailed PhysicalStrip commands");
        openknx.console.printHelpLine("neo phys list", "List all physical strips");
        openknx.console.printHelpLine("neo phys add ?", "Create new physical strip (1-Wire or SPI)");
        openknx.console.printHelpLine("neo phys del <i>", "Delete physical strip by ID");
        openknx.console.printHelpLine("neo phys timing ?", "Configure timing modes");
        openknx.console.printHelpLine("neo phys config ?", "Configure strip settings (SPI/Serial)");

        printHelpSectionHeader("VirtualStrip Management");
        openknx.console.printHelpLine("neo virt ?", "Show detailed VirtualStrip commands");
        openknx.console.printHelpLine("neo virt list", "List all virtual strips");
        openknx.console.printHelpLine("neo virt add ?", "Create virtual strip (RGB, RGBW, or RGBCCT)");
        openknx.console.printHelpLine("neo virt del <i>", "Delete virtual strip by ID");

        printHelpSectionHeader("Segment Management");
        openknx.console.printHelpLine("neo seg ?", "Show detailed Segment commands");
        openknx.console.printHelpLine("neo seg list", "List all segments");
        openknx.console.printHelpLine("neo seg add ?", "Create segment on virtual strip");
        openknx.console.printHelpLine("neo seg del <i>", "Delete segment by ID");

        printHelpSectionHeader("Power Management");
        openknx.console.printHelpLine("neo power ?", "Show detailed Power commands");

        printHelpSectionHeader("Effektmanager (EM)");
        openknx.console.printHelpLine("neo em status [seg]", "Show EM status (all segments or one)");
        openknx.console.printHelpLine("neo em dump <seg>", "Dump active EM header/runtime state for one segment");
        openknx.console.printHelpLine("neo em start <seg> <em>", "Start Effektmanager <em> on segment <seg>");
        openknx.console.printHelpLine("neo em stop <seg>", "Stop Effektmanager on segment <seg>");
        openknx.console.printHelpLine("neo em cue <seg> <cue>", "Trigger cue <cue> of active EM on segment <seg>");

        printHelpSectionHeader("Cue Commands");
        openknx.console.printHelpLine("neo cue <seg> <cue>", "Trigger cue <cue> of active EM on segment <seg>");
        openknx.console.printHelpLine("neo cue", "Show cue table for all segments");
        openknx.console.printHelpLine("neo cue list [all|seg]", "Cue table: active EMs (default), all configured EMs ('all'), or one segment");
        openknx.console.printHelpLine("neo cue ?", "Show detailed cue command help");

        printHelpSectionHeader("Effektkette (Chain)");
        openknx.console.printHelpLine("neo chain status [seg]", "Show Effektkette status (all segments or one)");
        openknx.console.printHelpLine("neo chain set <seg> <off|master|slave>", "Set Effektkette mode for segment");
        openknx.console.printHelpLine("neo chain override <seg> <0|1>", "Set slave local override flag");
        openknx.console.printHelpLine("neo chain trigger <seg>", "Send sync telegram now (master segment)");
        openknx.console.printHelpLine("neo power", "Show detailed status of global + all strips");
        openknx.console.printHelpLine("neo power <n>", "Show power status for physical strip <n>");

        printHelpSectionHeader("Effect Control");
        openknx.console.printHelpLine("neo effect ?", "Show detailed Effect commands");
        openknx.console.printHelpLine("neo effects", "List all available effects");
        openknx.console.printHelpLine("neo color ?", "Set segment colors");
        openknx.console.printHelpLine("neo brightness ?", "Set brightness levels");

#ifdef OPENKNX_NEOPIXEL_TESTS
        printHelpSectionHeader("Animation Test Commands");
        openknx.console.printHelpLine("neo anim start", "Start/Resume animation test (virtual 9x8 mode)");
        openknx.console.printHelpLine("neo anim stop", "Stop animation test (LEDs turn off)");
        openknx.console.printHelpLine("neo anim stats", "Show animation statistics");
        openknx.console.printHelpLine("neo simple", "Run simple hardware test (one-shot)");
        openknx.console.printHelpLine("neo simple start", "Start continuous simple test");
        openknx.console.printHelpLine("neo simple stop", "Stop simple test");
#endif

#ifdef OPENKNX_NEOPIXEL_BENCHMARK
        printHelpSectionHeader("Benchmark Commands");
        openknx.console.printHelpLine("neo bench", "Run all benchmarks");
        openknx.console.printHelpLine("neo bench speed [strip]", "Update speed test");
        openknx.console.printHelpLine("neo bench colors [strip]", "Color pattern test");
        openknx.console.printHelpLine("neo bench size", "LED count scaling test");
        openknx.console.printHelpLine("neo bench stability [strip]", "Stability test (1000 updates)");
#endif

        printSectionSeparator();
        openknx.logger.end();
        return true;
    }

    // Detail help: PhysicalStrip
    if (command == "neo phys ?" || command == "neo phys help")
    {
        printDetailHelpHeader("PhysicalStrip Commands");
        openknx.console.printHelpLine("list", "List all physical strips");
        openknx.console.printHelpLine("add <gpio> <n> [type]", "1-Wire: ws2812b|sk6812 (default: ws2812b)");
        openknx.console.printHelpLine("add <clk> <n> apa102 <data>", "SPI: APA102 (CLK + DATA pins)");
        openknx.console.printHelpLine("del <i>", "Delete physical strip by ID");
        openknx.console.printHelpLine("timings", "List all available timing modes + clone profiles");
        openknx.console.printHelpLine("timing <i>", "Show current timing mode for strip");
        openknx.console.printHelpLine("timing <i> <mode>", "Set timing (auto|legacy|slow5-20|fast5-25)");
        openknx.console.printHelpLine("timing <i> info", "Show detailed timing information");
        openknx.console.printHelpLine("timing <i> freq <kHz>", "Set bitrate directly (e.g. 775); like the ETS Timing field");
        openknx.console.printHelpLine("timing <i> custom <t0h> <t0l> <t1h> <t1l>", "Set custom timing in ns");
        openknx.console.printHelpLine("timing <i> reset",    "Revert to AUTO timing");
        openknx.console.printHelpLine("timing <i> qualify",   "Clone qualify: full strip, interactive (next/apply/stop)");
        openknx.console.printHelpLine("timing <i> scan",      "Alias for 'qualify'");
        openknx.console.printHelpLine("neo scan next",        "  During qualify: advance to next profile");
        openknx.console.printHelpLine("neo scan apply",       "  During qualify: keep current profile for this boot & finish");
        openknx.console.printHelpLine("neo scan stop",        "  During qualify: abort, restore original timing");
        openknx.console.printHelpLine("timing <i> profile <N>", "Apply clone profile until reboot (set ETS Timing permanently)");
        openknx.console.printHelpLine("timing <i> tune",              "[EXPERT] Enter live-tuner for strip i");
        openknx.console.printHelpLine("timing <i> tune t1h +50",      "  Adjust param while tuner is open");
        openknx.console.printHelpLine("timing <i> tune show/done/abort", "  Status / keep for this boot / restore");
        openknx.console.printHelpLine("config <i> info", "Show config (SPI: APA102/SK9822, Serial: WS2812B/SK6812)");
        openknx.console.printHelpLine("config <i> dummy <0-2>", "Set dummy LED mode (SPI only, 0=none, 1=physical, 2=virtual)");
        openknx.console.printHelpLine("config <i> frames <start> <end>", "Set frame counts (SPI only, start: 1-8, end: 1-80)");
        openknx.console.printHelpLine("config <i> pattern <0x00|0xFF>", "Set end frame pattern (SPI only, 0x00=APA102, 0xFF=SK9822)");
        openknx.console.printHelpLine("config <i> brightness <0-31>", "Set hardware brightness (SPI only)");
        openknx.console.printHelpLine("config <i> freq <MHz>", "Set SPI frequency in MHz (SPI only, e.g. 7.5, 10, 15)");
        openknx.console.printHelpLine("config <i> delay <us>", "Set start frame delay in microseconds (SPI only, 0-1000)");
        openknx.console.printHelpLine("config <i> autodetect <on|off>", "Enable/disable chip auto-detection on init (SPI only)");
        openknx.console.printHelpLine("config <i> detect", "Auto-detect chip type now (SPI: APA102 vs SK9822)");
        openknx.console.printHelpLine("config <i> skipfirst <n>", "Skip first N LEDs (force to black, dummy LED)");
        openknx.console.printHelpLine("config <i> skipmask init", "Initialize flexible skip mask (arbitrary LED skipping)");
        openknx.console.printHelpLine("config <i> skipmask clear", "Clear skip mask and free memory");
        openknx.console.printHelpLine("config <i> skipmask set <idx> <0|1>", "Enable (0) or skip (1) individual LED");
        openknx.console.printHelpLine("config <i> skipmask list", "List all skipped LEDs");
        openknx.console.printHelpLine("config <i> levelshifter <none|txs0108|hct125|ahct125>", "Set level-shifter type (applies GPIO optimizations)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<i>=ID/Index, <n>=LED Count, <gpio>=Pin, <clk>=Clock Pin, <data>=Data Pin");
        printDetailHelpExample("neo phys add 22 64 ws2812b     Create 1-Wire strip on GPIO22 with 64 LEDs");
        printDetailHelpExample("neo phys add 18 32 apa102 19   Create SPI strip (CLK=18, DATA=19, 32 LEDs)");
        printDetailHelpExample("neo phys timing 0 auto         Set strip 0 to auto timing");
        printDetailHelpExample("neo phys config 0 detect       Auto-detect APA102 vs SK9822");
        printDetailHelpExample("neo phys config 0 brightness 25 Set hardware brightness to 25");
        printDetailHelpExample("neo phys config 0 skipfirst 1  Skip LED#0 (dummy LED)");
        printDetailHelpExample("neo phys config 0 skipmask init Init mask, then set individual LEDs");
        printDetailHelpExample("neo phys config 0 skipmask set 5 1 Mark LED#5 as defective (skip)");
        printDetailHelpExample("neo phys config 0 levelshifter txs0108  Enable TXS0108E GPIO optimizations (KNeoPiX)");
        printDetailHelpExample("neo phys config 0 levelshifter hct125   Identify 74HCT125 buffer (no GPIO change)");
        printDetailHelpExample("neo phys config 0 levelshifter ahct125  Identify 74AHCT125 buffer (no GPIO change)");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: VirtualStrip
    if (command == "neo virt ?" || command == "neo virt help")
    {
        printDetailHelpHeader("VirtualStrip Commands");
        openknx.console.printHelpLine("list", "List all virtual strips");
        openknx.console.printHelpLine("add <n> [type]", "Create virtual strip (RGB, RGBW, or RGBCCT, default: RGB)");
        openknx.console.printHelpLine("del <i>", "Delete virtual strip by ID");
        openknx.console.printHelpLine("attach <v> <p>", "Attach physical strip to virtual strip");
        openknx.console.printHelpLine("detach <v>", "Detach physical strip from virtual strip");
        printDetailHelpSeparator();
        printDetailHelpParameter("<i>=ID, <n>=LED Count, <v>=Virtual Strip ID, <p>=Physical Strip ID");
        printDetailHelpExample("neo virt add 72 rgb      Create RGB virtual strip with 72 LEDs");
        printDetailHelpExample("neo virt add 72 rgbcct   Create RGBCCT (5-channel) virtual strip");
        printDetailHelpExample("neo virt attach 0 1      Attach physical strip 1 to virtual strip 0");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Segment
    if (command == "neo seg ?" || command == "neo seg help")
    {
        printDetailHelpHeader("Segment Commands");
        openknx.console.printHelpLine("list", "List all segments");
        openknx.console.printHelpLine("add <v> <start> <end>", "Create segment on virtual strip");
        openknx.console.printHelpLine("del <i>", "Delete segment by ID");
        openknx.console.printHelpLine("pause <i>", "Pause segment effect (freeze animation)");
        openknx.console.printHelpLine("resume <i>", "Resume segment effect");
        openknx.console.printHelpLine("stop <i>", "Stop segment (pause + clear pixels)");
        openknx.console.printHelpLine("clear effect <i>", "Remove effect from segment");
        openknx.console.printHelpLine("geo <i> <w> <h> [topo]", "Set 2D geometry (topo 1=rows-serp..7=cols-serp-tiled, 0=1D)");
        openknx.console.printHelpLine("geo <i> <w> <h> <tile> <topo>", "Tiled panel (tile = tile height)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<i>=Segment ID, <v>=Virtual Strip ID, <start>/<end>=LED Position");
        printDetailHelpExample("neo seg add 0 0 35       Create segment on virtual strip 0, LEDs 0-35");
        printDetailHelpExample("neo seg geo 0 32 16 8 7  32x16 tiled panel (tile=8, cols-serpentine)");
        printDetailHelpExample("neo seg pause 0          Pause segment 0 (freeze animation)");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Power
    if (command == "neo power ?" || command == "neo power help")
    {
        printDetailHelpHeader("Power Management Commands");
        openknx.console.printHelpLine("", "Show global + all physical strips detailed status");
        openknx.console.printHelpLine("<n>", "Show power status for physical strip <n>");
        openknx.console.printHelpLine("g|global limit <mA>", "Set global maximum current limit");
        openknx.console.printHelpLine("g|global profile <type>", "Set global LED profile: ws2812b|sk6812|apa102|conservative");
        openknx.console.printHelpLine("g|global on|off", "Enable/disable global current limiting");
        openknx.console.printHelpLine("<n> limit <mA>", "Set current limit for physical strip <n>");
        openknx.console.printHelpLine("<n> mode <mode>", "Set power mode: 0=Disabled, 1=UseGlobal, 2=FixedValue, 3=PerLED");
        printDetailHelpSeparator();
        printDetailHelpParameter("<n>=Strip Index (0-based), <mA>=Milliampere, <type>=Profile Type");
        printDetailHelpExample("neo power                Show all detailed power status");
        printDetailHelpExample("neo power 0              Show power status for strip 0");
        printDetailHelpExample("neo power g limit 5000   Set global limit to 5A");
        printDetailHelpExample("neo power 0 limit 2000   Set strip 0 limit to 2A");
        printDetailHelpExample("neo power 0 mode 1       Strip 0 uses global mode");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Effektmanager
    if (command == "neo em ?" || command == "neo em help")
    {
        printDetailHelpHeader("Effektmanager Commands");
        openknx.console.printHelpLine("status [seg]", "Show EM status (all segments or one)");
        openknx.console.printHelpLine("dump <seg>", "Dump active EM header/runtime state for one segment");
        openknx.console.printHelpLine("start <seg> <em>", "Start Effektmanager on segment");
        openknx.console.printHelpLine("stop <seg>", "Stop Effektmanager on segment");
        openknx.console.printHelpLine("cue <seg> <cue>", "Trigger cue of currently active EM (or use neo cue)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<seg>=0-based Segment Index, <em>=EM ID (0..16), <cue>=1..99");
        printDetailHelpExample("neo em status         Show status for all segments");
        printDetailHelpExample("neo em start 0 3      Start EM 3 on segment 0");
        printDetailHelpExample("neo em cue 0 2        Trigger cue 2 on segment 0");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Cue
    if (command == "neo cue ?" || command == "neo cue help")
    {
        printDetailHelpHeader("Cue Commands");
        openknx.console.printHelpLine("<seg> <cue>", "Trigger cue of currently active EM on segment");
        openknx.console.printHelpLine("", "Show cue table for all segments");
        openknx.console.printHelpLine("list [all|seg]", "Cue table: active EMs (default), all configured EMs ('all'), or one segment");
        printDetailHelpSeparator();
        printDetailHelpParameter("<seg>=0-based Segment Index, <cue>=1..99");
        printDetailHelpExample("neo cue 0 2           Trigger cue 2 on segment 0");
        printDetailHelpExample("neo cue               Show cue table for all segments");
        printDetailHelpExample("neo cue list 0        Show configured cues for active EM on segment 0");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Effektkette
    if (command == "neo chain ?" || command == "neo chain help")
    {
        printDetailHelpHeader("Effektkette Commands");
        openknx.console.printHelpLine("status [seg]", "Show Effektkette status (all segments or one)");
        openknx.console.printHelpLine("set <seg> <off|master|slave>", "Set segment sync mode");
        openknx.console.printHelpLine("override <seg> <0|1>", "Enable/disable local slave override");
        openknx.console.printHelpLine("trigger <seg>", "Send sync telegram now (master only)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<seg>=0-based Segment Index");
        printDetailHelpExample("neo chain status      Show chain status for all segments");
        printDetailHelpExample("neo chain set 0 slave Set segment 0 to slave");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Effect
    if (command == "neo effect ?" || command == "neo effect help")
    {
        printDetailHelpHeader("Effect Commands");
        openknx.console.printHelpLine("effects", "List all available effects");
        openknx.console.printHelpLine("effect set <s> <eff>", "Assign effect (ID or name) to segment");
        openknx.console.printHelpLine("effect stop <s>", "Stop effect on segment");
        openknx.console.printHelpLine("effect clear <s>", "Remove effect from segment");
        openknx.console.printHelpLine("effect pause <s>", "Pause effect (freeze current state)");
        openknx.console.printHelpLine("effect resume <s>", "Resume paused effect");
        openknx.console.printHelpLine("effect config <s>", "Show effect parameters");
        openknx.console.printHelpLine("effect config <s> get <i>", "Get parameter value");
        openknx.console.printHelpLine("effect config <s> set <i> <v>", "Set parameter value");
        openknx.console.printHelpLine("garage <s> <phase>", "GarageDoor: 0=OPENING 1=RUNWAY 2=DONE 3=STOP");
        printDetailHelpSeparator();
        printDetailHelpParameter("<s>=Segment ID, <eff>=Effect ID or Name (case-insensitive), <i>=Parameter Index, <v>=Value");
        printDetailHelpExample("neo effect set 0 2       Assign effect by ID (Rainbow=2) to segment 0");
        printDetailHelpExample("neo effect set 0 rainbow Assign effect by name to segment 0");
        printDetailHelpExample("neo effect stop 0        Stop effect on segment 0");
        printDetailHelpExample("neo effect clear 0       Remove effect from segment 0");
        printDetailHelpExample("neo effect pause 0       Pause effect on segment 0");
        printDetailHelpExample("neo effect resume 0      Resume effect on segment 0");
        printDetailHelpExample("neo effect config 0      Show all parameters of segment 0 effect");
        printDetailHelpExample("neo effect config 0 set 0 100   Set parameter 0 to value 100");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Color
    if (command == "neo color ?" || command == "neo color help")
    {
        printDetailHelpHeader("Color Commands");
        openknx.console.printHelpLine("color <s> <r> <g> <b> [w] [cw]", "Set segment color (0-255)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<s>=Segment ID, <r>=Red, <g>=Green, <b>=Blue");
        printDetailHelpParameter("<w>=WarmWhite (optional), <cw>=CoolWhite (optional, 5-channel)");
        printDetailHelpExample("neo color 0 255 0 0         Set segment 0 to red");
        printDetailHelpExample("neo color 1 0 255 128 200   Set RGBW segment 1 to green+blue+white");
        printDetailHelpExample("neo color 2 0 0 0 255 128   Set RGBCCT to warm white + cool white");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Brightness
    if (command == "neo brightness ?" || command == "neo brightness help")
    {
        printDetailHelpHeader("Brightness Commands");
        openknx.console.printHelpLine("brightness <s> <v>", "Set software brightness (0-255, all LED types)");
        openknx.console.printHelpLine("hwbrightness <s> <v>", "Set hardware brightness (0-255, APA102/SK9822)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<s>=Segment ID, <v>=Brightness Value (0-255)");
        printDetailHelpExample("neo brightness 0 128     Set segment 0 software brightness to 50%");
        printDetailHelpExample("neo hwbrightness 0 31    Set APA102 hardware brightness to max");
        printDetailHelpEnd();
        return true;
    }

    // Process basic commands
    if (command == "neo info")
    {
        return processInfoCommand();
    }
    else if (command == "neo perf")
    {
        return processPerformanceCommand();
    }
    else if (command == "neo list")
    {
        return processListCommand();
    }
    else if (command == "neo update")
    {
        return processUpdateCommand("");
    }
    else if (command == "neo clear")
    {
        return processClearCommand();
    }
    else if (command.compare(0, 9, "neo test ") == 0)
    {
        return processTestCommand(command.substr(9));
    }
    else if (command.compare(0, 10, "neo speed ") == 0)
    {
        return processSpeedCommand(command.substr(10));
    }
    else if (command.compare(0, 9, "neo auto ") == 0)
    {
        return processAutoCommand(command.substr(9));
    }

    // PhysicalStrip commands
    else if (command.compare(0, 9, "neo phys ") == 0 || command == "neo phys")
    {
        return processPhysCommand(command.length() > 9 ? command.substr(9) : "");
    }

    // VirtualStrip commands
    else if (command.compare(0, 9, "neo virt ") == 0 || command == "neo virt")
    {
        return processVirtCommand(command.length() > 9 ? command.substr(9) : "");
    }

    // Segment commands
    else if (command.compare(0, 8, "neo seg ") == 0 || command == "neo seg")
    {
        return processSegCommand(command.length() > 8 ? command.substr(8) : "");
    }

    // Effect commands
    else if (command == "neo effect list" || command == "neo effects")
    {
        return processEffectsCommand();
    }
    else if (command.compare(0, 18, "neo effect config ") == 0)
    {
        return processEffectConfigCommand(command.substr(18));
    }
    else if (command.compare(0, 11, "neo effect ") == 0)
    {
        return processEffectCommand(command.substr(11));
    }
    else if (command.compare(0, 11, "neo garage ") == 0)
    {
        return processGarageCommand(command.substr(11));
    }
    else if (command.compare(0, 10, "neo color ") == 0)
    {
        return processColorCommand(command.substr(10));
    }
    else if (command.compare(0, 15, "neo brightness ") == 0)
    {
        return processBrightnessCommand(command.substr(15));
    }
    else if (command.compare(0, 17, "neo hwbrightness ") == 0)
    {
        return processHardwareBrightnessCommand(command.substr(17));
    }

    // Power Management commands
    else if (command.compare(0, 10, "neo power ") == 0 || command == "neo power")
    {
        return processPowerCommand(command.length() > 10 ? command.substr(10) : "");
    }

#ifdef OPENKNX_NEOPIXEL_TESTS
    // Animation test commands
    else if (command == "neo anim start")
    {
        return processAnimTestStartCommand();
    }
    else if (command == "neo anim stop")
    {
        return processAnimTestStopCommand();
    }
    else if (command == "neo anim stats")
    {
        AnimationTest::instance().printStats();
        return true;
    }
    // Simple test commands
    else if (command == "neo simple")
    {
        SimpleTest::instance().init(22, 64);
        SimpleTest::instance().runOnce();
        return true;
    }
    else if (command == "neo simple start")
    {
        return processSimpleTestStartCommand();
    }
    else if (command == "neo simple stop")
    {
        return processSimpleTestStopCommand();
    }
#endif

#ifdef OPENKNX_NEOPIXEL_BENCHMARK
    // Benchmark commands
    else if (command == "neo bench")
    {
        return processBenchmarkCommand("");
    }
    else if (command.compare(0, 10, "neo bench ") == 0)
    {
        return processBenchmarkCommand(command.substr(10));
    }
#endif

    // neo em / neo chain commands: parsing + rendering in OFM, data/actions via backend hooks.
    if (command == "neo em" || command.compare(0, 7, "neo em ") == 0)
    {
        std::string sub = (command.length() > 7) ? command.substr(7) : "";

        if (sub.empty() || sub == "status")
        {
            printEmStatusTable(-1);
            return true;
        }
        if (sub == "?")
        {
            openknx.logger.log("Usage: neo em status [seg] | dump <seg> | start <seg> <em> | stop <seg> | cue <seg> <cue>  (seg=0-based)");
            return true;
        }
        if (sub.compare(0, 7, "status ") == 0)
        {
            int seg = atoi(sub.c_str() + 7);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo em status [seg]");
                return true;
            }
            printEmStatusTable(seg);
            return true;
        }
        if (sub.compare(0, 5, "dump ") == 0)
        {
            int seg = atoi(sub.c_str() + 5);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo em dump <seg>");
                return true;
            }
            printEmCueTable(seg);
            return true;
        }
        if (sub.compare(0, 6, "start ") == 0)
        {
            int seg = 0, em = 0;
            if (sscanf(sub.c_str() + 6, "%d %d", &seg, &em) != 2 || seg < 0)
            {
                openknx.logger.log("Usage: neo em start <seg> <em>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_EM_START, seg, em))
                printEmStatusTable(seg);
            else
                openknx.logger.log("ERROR: Invalid segment or EM id");
            return true;
        }
        if (sub.compare(0, 5, "stop ") == 0)
        {
            int seg = atoi(sub.c_str() + 5);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo em stop <seg>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_EM_STOP, seg, 0))
                printEmStatusTable(seg);
            else
                openknx.logger.log("ERROR: Invalid segment");
            return true;
        }
        if (sub.compare(0, 4, "cue ") == 0)
        {
            int seg = 0, cue = 0;
            if (sscanf(sub.c_str() + 4, "%d %d", &seg, &cue) != 2 || seg < 0)
            {
                openknx.logger.log("Usage: neo em cue <seg> <cue>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_EM_CUE, seg, cue))
                printEmStatusTable(seg);
            return true;
        }
        if (sub.compare(0, 7, "config ") == 0)
        {
            int em = 0, loop = 0, nextEm = 0;
            int n = sscanf(sub.c_str() + 7, "%d %d %d", &em, &loop, &nextEm);
            if (n < 2 || em < 1)
            {
                openknx.logger.log("Usage: neo em config <em> <loop 0|1> [nextEm]");
                return true;
            }
            int packed = (loop & 0xFF) | ((nextEm & 0xFF) << 8);
            if (openknxNeoPixelHandleEmChainAction(NEO_EM_CONFIG, em, packed))
                openknx.logger.logWithValues("EM %d configured (loop=%d, nextEm=%d, enabled)", em, loop, nextEm);
            else
                openknx.logger.log("ERROR: invalid EM id (1..16) or no cues defined yet");
            return true;
        }
        if (sub.compare(0, 5, "bind ") == 0)
        {
            int mgrSeg = atoi(sub.c_str() + 5);
            if (mgrSeg < 0)
            {
                openknx.logger.log("Usage: neo em bind <managerSeg>   (wraps a console segment into the EM system)");
                return true;
            }
            openknxNeoPixelHandleEmChainAction(NEO_EM_BIND, mgrSeg, 0);
            return true;
        }

        openknx.logger.log("Usage: neo em status [seg] | dump <seg> | start <seg> <em> | stop <seg> | cue <seg> <cue>");
        openknx.logger.log("       | config <em> <loop> [nextEm] | bind <managerSeg>   (seg/0-based)");
        return true;
    }

    // neo cue commands (alias for neo em cue / em dump)
    if (command == "neo cue" || command.compare(0, 8, "neo cue ") == 0)
    {
        std::string sub = (command.length() > 8) ? command.substr(8) : "";

        if (sub.empty() || sub == "list")
        {
            printEmCueTable(-1);
            return true;
        }
        if (sub == "?")
        {
            openknx.logger.log("Usage: neo cue | neo cue <seg> <cue> | neo cue list [all|<seg>]  (seg=0-based)");
            return true;
        }

        if (sub.compare(0, 5, "list ") == 0)
        {
            const char* arg = sub.c_str() + 5;
            if (strcmp(arg, "all") == 0)
            {
                printEmCueTable(-2); // every configured EM, running or not
                return true;
            }
            int seg = atoi(arg);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo cue list [all|<seg>]");
                return true;
            }
            printEmCueTable(seg);
            return true;
        }
        if (sub.compare(0, 4, "set ") == 0)
        {
            int em = 0, cue = 0, eff = 0, dur = 0, fade = 0, bri = 255, r = 255, g = 255, b = 255;
            int n = sscanf(sub.c_str() + 4, "%d %d %d %d %d %d %d %d %d",
                           &em, &cue, &eff, &dur, &fade, &bri, &r, &g, &b);
            if (n < 5 || em < 1 || cue < 1)
            {
                openknx.logger.log("Usage: neo cue set <em> <cue> <effectId> <durSec> <fadeMs> [bri] [r g b]");
                return true;
            }
            if (openknxNeoPixelHandleCueSet((uint8_t)em, (uint8_t)cue, (uint8_t)eff,
                                            (uint16_t)dur, (uint16_t)fade,
                                            (uint8_t)bri, (uint8_t)r, (uint8_t)g, (uint8_t)b))
                openknx.logger.logWithValues("EM %d cue %d set: effect=%d dur=%ds fade=%dms", em, cue, eff, dur, fade);
            else
                openknx.logger.log("ERROR: invalid EM/cue (em 1..16, cue 1..10)");
            return true;
        }
        if (sub.compare(0, 6, "param ") == 0)
        {
            int em = 0, cue = 0, idx = -1, val = 0;
            int n = sscanf(sub.c_str() + 6, "%d %d %d %d", &em, &cue, &idx, &val);
            if (n != 4 || em < 1 || cue < 1 || idx < 0)
            {
                openknx.logger.log("Usage: neo cue param <em> <cue> <paramIdx> <value>   (override one effect param; cue must exist)");
                return true;
            }
            if (openknxNeoPixelHandleCueParam((uint8_t)em, (uint8_t)cue, (uint8_t)idx, (uint8_t)val))
                openknx.logger.logWithValues("EM %d cue %d param[%d] = %d (clamped to effect range)", em, cue, idx, val);
            else
                openknx.logger.log("ERROR: invalid EM/cue/param (em 1..16, cue 1..10, idx in range, cue must be set first)");
            return true;
        }
        if (sub.compare(0, 5, "text ") == 0)
        {
            int em = 0, cue = 0, pos = 0;
            int n = sscanf(sub.c_str() + 5, "%d %d %n", &em, &cue, &pos);
            if (n < 2 || em < 1 || cue < 1)
            {
                openknx.logger.log("Usage: neo cue text <em> <cue> <text>   (Scroll Text etc.; quotes optional, \\\" = literal quote; long text auto-stored & applied on cue activation)");
                return true;
            }
            // Unescape the rest of the line: an optional surrounding pair of quotes is
            // stripped, "\\\"" becomes a literal quote and "\\\\" a literal backslash; every
            // other byte passes through untouched — full ASCII plus the ISO-8859-1 umlauts
            // (Ä Ö Ü ß …) that Scroll Text renders.
            const std::string raw = sub.substr(5 + pos);
            std::string txt;
            const bool quoted = (!raw.empty() && raw.front() == '"');
            for (size_t i = quoted ? 1 : 0; i < raw.size(); ++i)
            {
                const char ch = raw[i];
                if (ch == '\\' && i + 1 < raw.size()) { txt.push_back(raw[++i]); continue; }
                if (quoted && ch == '"') break; // closing quote ends the text
                txt.push_back(ch);
            }
            const bool longText = txt.size() > (size_t)(EM_TEXT_LEN - 1);
            if (openknxNeoPixelHandleCueText((uint8_t)em, (uint8_t)cue, txt.c_str()))
                openknx.logger.logWithValues("EM %d cue %d text set: \"%s\"%s", em, cue, txt.c_str(),
                                             longText ? " (long text — stored & applied on cue activation)" : "");
            else
                openknx.logger.log("ERROR: invalid EM/cue (em 1..16, cue 1..10, cue must be set first)");
            return true;
        }
        if (sub.compare(0, 6, "clear ") == 0)
        {
            int em = atoi(sub.c_str() + 6);
            if (em < 1)
            {
                openknx.logger.log("Usage: neo cue clear <em>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_CUE_CLEAR, em, 0))
                openknx.logger.logWithValues("EM %d cues cleared", em);
            return true;
        }

        int seg = 0, cue = 0;
        if (sscanf(sub.c_str(), "%d %d", &seg, &cue) != 2 || seg < 0)
        {
            openknx.logger.log("Usage: neo cue [<seg> <cue>] | list [all|seg] | set <em> <cue> <eff> <dur> <fade> [bri r g b] | param <em> <cue> <idx> <val> | text <em> <cue> <text> | clear <em>");
            return true;
        }
        if (openknxNeoPixelHandleEmChainAction(NEO_EM_CUE, seg, cue))
            printEmStatusTable(seg);
        return true;
    }

    if (command == "neo chain" || command.compare(0, 10, "neo chain ") == 0)
    {
        std::string sub = (command.length() > 10) ? command.substr(10) : "";

        if (sub.empty() || sub == "status")
        {
            printChainStatusTable(-1);
            return true;
        }
        if (sub == "?")
        {
            openknx.logger.log("Usage: neo chain status [seg] | set <seg> <off|master|slave> | override <seg> <0|1> | trigger <seg>  (seg=0-based)");
            return true;
        }
        if (sub.compare(0, 7, "status ") == 0)
        {
            int seg = atoi(sub.c_str() + 7);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo chain status [seg]");
                return true;
            }
            printChainStatusTable(seg);
            return true;
        }
        if (sub.compare(0, 4, "set ") == 0)
        {
            int seg = 0;
            char mode[16] = {0};
            if (sscanf(sub.c_str() + 4, "%d %15s", &seg, mode) != 2 || seg < 0)
            {
                openknx.logger.log("Usage: neo chain set <seg> <off|master|slave>");
                return true;
            }
            int modeValue = -1;
            if (strcmp(mode, "off") == 0) modeValue = 0;
            else if (strcmp(mode, "master") == 0) modeValue = 1;
            else if (strcmp(mode, "slave") == 0) modeValue = 2;
            if (modeValue < 0)
            {
                openknx.logger.log("Usage: neo chain set <seg> <off|master|slave>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_CHAIN_SET, seg, modeValue))
                printChainStatusTable(seg);
            else
                openknx.logger.log("ERROR: Invalid segment");
            return true;
        }
        if (sub.compare(0, 9, "override ") == 0)
        {
            int seg = 0, flag = 0;
            if (sscanf(sub.c_str() + 9, "%d %d", &seg, &flag) != 2 || seg < 0 || (flag != 0 && flag != 1))
            {
                openknx.logger.log("Usage: neo chain override <seg> <0|1>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_CHAIN_OVERRIDE, seg, flag))
                printChainStatusTable(seg);
            else
                openknx.logger.log("ERROR: Invalid segment");
            return true;
        }
        if (sub.compare(0, 8, "trigger ") == 0)
        {
            int seg = atoi(sub.c_str() + 8);
            if (seg < 0)
            {
                openknx.logger.log("Usage: neo chain trigger <seg>");
                return true;
            }
            if (openknxNeoPixelHandleEmChainAction(NEO_CHAIN_TRIGGER, seg, 0))
                printChainStatusTable(seg);
            return true;
        }

        openknx.logger.log("Usage: neo chain status [seg] | set <seg> <off|master|slave> | override <seg> <0|1> | trigger <seg>  (seg=0-based)");
        return true;
    }

    return false;
}

// ============================================================================
// Command Handlers
// ============================================================================

// ----------------------------------------------------------------------------
// Effektmanager / Effektkette console rendering
// Data is provided by the OAM backend via NeoPixelEmConsole.h hooks.
// ----------------------------------------------------------------------------

/** @brief Print Effektmanager status table (onlySeg = -1 for all segments) */
void NeoPixel::printEmStatusTable(int onlySeg)
{
    const int segCount = openknxNeoPixelEmSegmentCount();
    if (segCount < 0)
    {
        openknx.logger.log("ERROR: Effektmanager backend not available!");
        return;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Effektmanager Status");
    printSectionSeparator();

    if (segCount == 0)
    {
        openknx.logger.log("No segments created.");
    }
    else
    {
        openknx.logger.log("Seg │ EM │ Cue   │ Running │ Startup │ Last │ Cues │ Loop │ Next");
        openknx.logger.log("────┼────┼───────┼─────────┼─────────┼──────┼──────┼──────┼──────");

        for (int i = 0; i < segCount; i++)
        {
            if (onlySeg >= 0 && i != onlySeg) continue;

            NeoEmSegStatus st{};
            if (!openknxNeoPixelGetEmStatus((uint8_t)i, st)) continue;

            if (!st.hasSegment)
            {
                openknx.logger.logWithValues("%3d │ -- │ %-5s │ %-7s │ %7s │ %4s │ %4s │ %4s │ %4s",
                                             i, "-", "-", "-", "-", "-", "-", "-");
                continue;
            }

            const EffektManagerData* em = openknxNeoPixelGetEmData(st.activeEmId);
            const int cueCount = em ? (int)em->header.cueCount : 0;
            const int loop = (em && em->header.loop) ? 1 : 0;
            const int next = em ? (int)em->header.nextEmId : 0;

            char cueBuf[8] = "-";
            if (em && cueCount > 0)
                snprintf(cueBuf, sizeof(cueBuf), "%d/%d", (int)st.activeCueNum, cueCount);

            openknx.logger.logWithValues("%3d │ %2d │ %-5s │ %-7s │ %7d │ %4d │ %4d │ %4d │ %4d",
                                         i,
                                         (int)st.activeEmId,
                                         cueBuf,
                                         st.running ? "yes" : "no",
                                         (int)st.startupEm,
                                         (int)st.lastEmId,
                                         cueCount,
                                         loop,
                                         next);
        }
    }

    printSectionSeparator();
    openknx.logger.log("");
}

/** @brief Print cue overview table for active EMs (onlySeg = -1 for all segments) */
void NeoPixel::printEmCueTable(int onlySeg)
{
    const int segCount = openknxNeoPixelEmSegmentCount();
    if (segCount < 0)
    {
        openknx.logger.log("ERROR: Effektmanager backend not available!");
        return;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Cue Overview");
    printSectionSeparator();

    if (segCount == 0)
    {
        openknx.logger.log("No segments created.");
    }
    else
    {
        // onlySeg == -2  → 'list all'   : every configured EM (col 1 = EM id), running or not
        // onlySeg == -1  → 'list'       : the active EM of every segment (col 1 = segment)
        // onlySeg >= 0   → 'list <seg>' : the active EM of that one segment
        // The header is printed lazily (only once there's a row), so an empty result returns
        // a short "empty" line instead of a bare header.
        static const char* const kRowSep =
            "────┼─────┼───┼────┼───────────────────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼──────┼──────┼──────────────";
        bool headerShown = false;
        bool firstRow    = true;
        int  lastCol1    = 0;
        auto showHeader = [&headerShown]() {
            if (headerShown) return;
            headerShown = true;
            openknx.logger.log("ID  │ Cue │ A │ FX │ Effect Name       │ P0  │ P1  │ P2  │ P3  │ P4  │ R   │ G   │ B   │ W   │ Bri │ Dur  │ Fade │ Name");
            openknx.logger.log(kRowSep);
        };
        auto printCueRow = [&](int col1, const char* aMark, uint8_t cueNum, const EffektCue& cue) {
            Effect* effect = EffectPool::getEffectByIndex(cue.effectId);
            const char* effectName = effect ? effect->getName() : "unknown";
            showHeader();
            // Divider line whenever the ID column changes (segment→segment / EM→EM).
            if (!firstRow && col1 != lastCol1) openknx.logger.log(kRowSep);
            firstRow = false;
            lastCol1 = col1;
            openknx.logger.logWithValues("%3d │ %3d │ %1s │ %2d │ %-17.17s │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │ %4d │ %4d │ %-14.14s",
                                         col1, (int)cueNum, aMark, (int)cue.effectId, effectName,
                                         (int)cue.params[0], (int)cue.params[1], (int)cue.params[2], (int)cue.params[3], (int)cue.params[4],
                                         (int)cue.r, (int)cue.g, (int)cue.b, (int)cue.w,
                                         (int)cue.brightness, (int)cue.durationSec, (int)cue.fadeMs, cue.cueName);
        };

        if (onlySeg == -2)
        {
            for (uint8_t emId = 1; emId <= EM_COUNT; ++emId)
            {
                const EffektManagerData* em = openknxNeoPixelGetEmData(emId);
                if (!em || em->header.cueCount == 0) continue;
                const uint8_t cueCount = (em->header.cueCount <= EM_CUE_COUNT) ? em->header.cueCount : EM_CUE_COUNT;
                for (uint8_t cueNum = 1; cueNum <= cueCount; ++cueNum)
                    printCueRow((int)emId, " ", cueNum, em->cues[cueNum - 1]);
            }
            if (!headerShown) openknx.logger.log("(empty — no cues configured in any EM)");
        }
        else
        {
            for (int i = 0; i < segCount; i++)
            {
                if (onlySeg >= 0 && i != onlySeg) continue;

                NeoEmSegStatus st{};
                if (!openknxNeoPixelGetEmStatus((uint8_t)i, st)) continue;

                const EffektManagerData* em = st.hasSegment ? openknxNeoPixelGetEmData(st.activeEmId) : nullptr;
                if (!em) continue; // no active EM on this segment → contributes no rows

                const uint8_t cueCount = (em->header.cueCount <= EM_CUE_COUNT) ? em->header.cueCount : EM_CUE_COUNT;
                for (uint8_t cueNum = 1; cueNum <= cueCount; ++cueNum)
                    printCueRow(i, (cueNum == st.activeCueNum) ? "*" : " ", cueNum, em->cues[cueNum - 1]);
            }
            if (!headerShown)
                openknx.logger.log(onlySeg >= 0 ? "(empty — segment has no active Effektmanager)"
                                                : "(empty — no active Effektmanager on any segment)");
        }
    }

    printSectionSeparator();
    openknx.logger.log("");
}

/** @brief Print Effektkette status table (onlySeg = -1 for all segments) */
void NeoPixel::printChainStatusTable(int onlySeg)
{
    const int segCount = openknxNeoPixelEmSegmentCount();
    if (segCount < 0)
    {
        openknx.logger.log("ERROR: Effektkette backend not available!");
        return;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Effektkette Status");
    printSectionSeparator();

    if (segCount == 0)
    {
        openknx.logger.log("No segments created.");
    }
    else
    {
        openknx.logger.log("Seg │ Mode   │ Pol │ TOut │ Ovr │ LastSync │ VTotal │ VOff");
        openknx.logger.log("────┼────────┼─────┼──────┼─────┼──────────┼────────┼──────");

        for (int i = 0; i < segCount; i++)
        {
            if (onlySeg >= 0 && i != onlySeg) continue;

            NeoChainSegStatus st{};
            if (!openknxNeoPixelGetChainStatus((uint8_t)i, st)) continue;

            const char* modeName = "off";
            if (st.syncMode == 1) modeName = "master";
            else if (st.syncMode == 2) modeName = "slave";

            openknx.logger.logWithValues("%3d │ %-6s │ %3d │ %4d │ %3d │ %8lu │ %6d │ %4d",
                                         i,
                                         modeName,
                                         (int)st.overridePolicy,
                                         (int)st.timeoutSteps,
                                         st.localOverride ? 1 : 0,
                                         (unsigned long)st.lastSyncMs,
                                         (int)st.virtualTotalLength,
                                         (int)st.virtualOffset);
        }
    }

    printSectionSeparator();
    openknx.logger.log("");
}

/** @brief Print compact Effektmanager/Effektkette sections for 'neo info' */
void NeoPixel::printEmChainInfoCompact()
{
    const int segCount = openknxNeoPixelEmSegmentCount();
    if (segCount <= 0) return; // No backend or no segments — stay silent in neo info

    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Effektmanager:");
    openknx.logger.color(0);
    for (int i = 0; i < segCount; i++)
    {
        NeoEmSegStatus st{};
        if (!openknxNeoPixelGetEmStatus((uint8_t)i, st)) continue;

        if (!st.hasSegment)
        {
            openknx.logger.logWithValues("  [%d] <no segment>", i);
            continue;
        }

        const EffektManagerData* em = openknxNeoPixelGetEmData(st.activeEmId);
        if (em)
        {
            openknx.logger.logWithValues("  [%d] EM %d -> %s, Cue %d/%d%s",
                                         i,
                                         (int)st.activeEmId,
                                         st.running ? "Running" : "Stopped",
                                         (int)st.activeCueNum,
                                         (int)em->header.cueCount,
                                         em->header.loop ? ", Loop" : "");
        }
        else
        {
            openknx.logger.logWithValues("  [%d] Inactive (Startup EM: %d, Last EM: %d)",
                                         i, (int)st.startupEm, (int)st.lastEmId);
        }
    }
    openknx.logger.log("");

    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Effektkette:");
    openknx.logger.color(0);
    for (int i = 0; i < segCount; i++)
    {
        NeoChainSegStatus st{};
        if (!openknxNeoPixelGetChainStatus((uint8_t)i, st)) continue;

        if (st.syncMode == 1 || st.syncMode == 2)
        {
            openknx.logger.logWithValues("  [%d] %s (Total %d, Offset %d%s)",
                                         i,
                                         (st.syncMode == 1) ? "Master" : "Slave",
                                         (int)st.virtualTotalLength,
                                         (int)st.virtualOffset,
                                         st.localOverride ? ", Override" : "");
        }
        else
        {
            openknx.logger.logWithValues("  [%d] Off", i);
        }
    }
    openknx.logger.log("");
}

/**
 * @brief Process 'neo info' command - Complete system overview
 */
bool NeoPixel::processInfoCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    auto stats = _manager->getStats();

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  NeoPixel System Information");
    printSectionSeparator();
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // SYSTEM STATUS
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("System Status:");
    openknx.logger.color(0);
    openknx.logger.logWithValues("  Initialized:     %s", _initialized ? "Yes" : "No");
    openknx.logger.logWithValues("  Total Strips:    %d", stats.totalStrips);
    openknx.logger.logWithValues("  Active Strips:   %d", stats.activeStrips);
    openknx.logger.logWithValues("  Total LEDs:      %d", stats.totalLeds);
    openknx.logger.logWithValues("  Memory Usage:    %d bytes", _manager->getTotalMemoryUsage());
    openknx.logger.logWithValues("  Auto Update:     %s", _autoUpdate ? "Enabled" : "Disabled");
    if (_autoUpdate)
    {
        openknx.logger.logWithValues("  Update Interval: %d ms", _updateInterval);
    }
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // AVAILABLE RESOURCES
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Available Resources:");
    openknx.logger.color(0);

#ifdef ARDUINO_ARCH_RP2040
    // RP2040/RP2350: PIO State Machines, DMA Channels
    uint availSM = PIO_NeoPixel_Serial::getAvailableStateMachines();
    uint totalSM = 8; // Default RP2040
    #ifdef PICO_RP2350
    totalSM = 12; // RP2350 has 3 PIOs
    #endif

    uint availDMA = PIO_NeoPixel_Serial::getAvailableDmaChannels();
    uint totalDMA = NUM_DMA_CHANNELS;

    openknx.logger.logWithValues("  PIO State Machines: %d available / %d total", availSM, totalSM);
    openknx.logger.logWithValues("  DMA Channels:       %d available / %d total", availDMA, totalDMA);

#elif defined(ARDUINO_ARCH_ESP32)
    // ESP32 (all variants): RMT Channels
    uint availRMT = RMT_NeoPixel_Serial::getAvailableRmtChannels();
    uint totalRMT = RMT_NeoPixel_Serial::getTotalRmtChannels();

    openknx.logger.logWithValues("  RMT Channels: %d available / %d total", availRMT, totalRMT);

#else
    openknx.logger.log("  Platform resource detection not supported");
#endif
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // PHYSICAL STRIPS
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Physical Strips:");
    openknx.logger.color(0);
    uint32_t physCount = _manager->getStripCount();
    if (physCount == 0)
    {
        openknx.logger.log("  No physical strips configured");
        openknx.logger.log("  Use 'neo phys add <gpio> <led_count>'");
    }
    else
    {
        for (uint32_t i = 0; i < physCount; i++)
        {
            auto strip = _manager->getStrip(i);
            if (strip)
            {
                // Get protocol name
                LedProtocol protocol = strip->getProtocol();
                const char* protocolName = "Unknown";
                switch (protocol)
                {
                    case LedProtocol::WS2812: protocolName = "WS2812"; break;
                    case LedProtocol::WS2812B: protocolName = "WS2812B"; break;
                    case LedProtocol::SK6812: protocolName = "SK6812"; break;
                    case LedProtocol::APA102: protocolName = "APA102"; break;
                    case LedProtocol::APA102_CLONE: protocolName = "APA102_CLONE"; break;
                    case LedProtocol::SK9822: protocolName = "SK9822"; break;
                    case LedProtocol::WS2801: protocolName = "WS2801"; break;
                    case LedProtocol::LPD8806: protocolName = "LPD8806"; break;
                    // 5-Channel protocols (RGBCCT)
                    case LedProtocol::SK6812_RGBCCT: protocolName = "SK6812_RGBCCT"; break;
                    case LedProtocol::WS2814_RGBCCT: protocolName = "WS2814_RGBCCT"; break;
                    case LedProtocol::WS2805_RGBCCT: protocolName = "WS2805_RGBCCT"; break;
                    default: protocolName = "Unknown"; break;
                }

                // Get ColorOrder from PhysicalStrip
                ColorOrder order = strip->getColorOrder();
                const char* colorOrder = "???";
                switch (order)
                {
                    case ColorOrder::NONE: colorOrder = "DEFAULT"; break;
                    case ColorOrder::RGB: colorOrder = "RGB"; break;
                    case ColorOrder::RBG: colorOrder = "RBG"; break;
                    case ColorOrder::GRB: colorOrder = "GRB"; break;
                    case ColorOrder::BGR: colorOrder = "BGR"; break;
                    case ColorOrder::GBR: colorOrder = "GBR"; break;
                    case ColorOrder::BRG: colorOrder = "BRG"; break;
                    case ColorOrder::RGBW: colorOrder = "RGBW"; break;
                    case ColorOrder::GRBW: colorOrder = "GRBW"; break;
                    // 5-Channel color orders (RGBCCT)
                    case ColorOrder::RGBCCT: colorOrder = "RGBCCT"; break;
                    case ColorOrder::GRBCCT: colorOrder = "GRBCCT"; break;
                    case ColorOrder::RGBCTW: colorOrder = "RGBCTW"; break;
                    case ColorOrder::GRBCTW: colorOrder = "GRBCTW"; break;
                }

                // Display strip info (GPIO, LEDs, Status)
                openknx.logger.logWithValues("  [%d] GPIO%d: %d LEDs - %s",
                                             i, strip->getDataPin(), strip->getLedCount(),
                                             strip->isInitialized() ? "OK" : "ERROR");

                // Display protocol and ColorOrder
                openknx.logger.logWithValues("      Protocol: %s, ColorOrder: %s",
                                             protocolName, colorOrder);

#ifdef ARDUINO_ARCH_RP2040
                // RP2040: Show PIO/SM/DMA info
                auto driver = strip->getDriver();
                if (driver)
                {
                    // Check for 1-Wire Serial driver (WS2812B, SK6812, etc.)
                    auto pioDriver = dynamic_cast<PIO_NeoPixel_Serial*>(driver);
                    if (pioDriver)
                    {
                        PIO pio = pioDriver->getPio();
                        const char* pioName = (pio == pio0) ? "PIO0" : (pio == pio1) ? "PIO1"
                                                                                     : "PIO2";
                        const int dmaChannel = pioDriver->getDmaChannel();
                        const uint32_t freq = pioDriver->getFrequency();
                        const float actual_bitrate = pioDriver->getActualBitrate();
                        const float actual_clkdiv = pioDriver->getActualClkdiv();

                        if (dmaChannel >= 0)
                        {
                            openknx.logger.logWithValues("      Hardware: %s/SM%d, DMA Ch%d, %dkHz (actual: %.2f kHz, clkdiv: %.2f)",
                                                         pioName, pioDriver->getStateMachine(),
                                                         dmaChannel, freq / 1000, actual_bitrate / 1000.0f, actual_clkdiv);
                        }
                        else
                        {
                            openknx.logger.logWithValues("      Hardware: %s/SM%d (no DMA), %dkHz (actual: %.2f kHz, clkdiv: %.2f)",
                                                         pioName, pioDriver->getStateMachine(),
                                                         freq / 1000, actual_bitrate / 1000.0f, actual_clkdiv);
                        }
                        // Show TimingMode
                        TimingMode mode = strip->getTimingMode();
                        const char* modeName = getTimingModeName(mode);
                        openknx.logger.logWithValues("      Timing: %s", modeName);
                    }
                    else
                    {
                        // Check for SPI driver (APA102, WS2801, etc.)
                        auto spiDriver = dynamic_cast<PIO_NeoPixel_SPI*>(driver);
                        if (spiDriver)
                        {
                            PIO pio = spiDriver->getPio();
                            const char* pioName = (pio == pio0) ? "PIO0" : (pio == pio1) ? "PIO1"
                                                                                         : "PIO2";
                            int dmaChannel = spiDriver->getDmaChannel();
                            uint32_t spiFreq = spiDriver->getSpiFrequency();
                            float clkdiv = spiDriver->getClkdiv();

                            openknx.logger.logWithValues("      Pins: CLK=GPIO%d, MOSI=GPIO%d",
                                                         spiDriver->getClkPin(), spiDriver->getMosiPin());

                            if (dmaChannel >= 0)
                            {
                                openknx.logger.logWithValues("      Hardware: %s/SM%d (SPI), DMA Ch%d, %dMHz (clkdiv: %.2f)",
                                                             pioName, spiDriver->getStateMachine(),
                                                             dmaChannel, spiFreq / 1000000, clkdiv);
                            }
                            else
                            {
                                openknx.logger.logWithValues("      Hardware: %s/SM%d (SPI, no DMA), %dMHz (clkdiv: %.2f)",
                                                             pioName, spiDriver->getStateMachine(),
                                                             spiFreq / 1000000, clkdiv);
                            }
                        }
                    }
                }
#elif defined(ARDUINO_ARCH_ESP32)
                // ESP32: Show RMT info
                auto driver = strip->getDriver();
                if (driver && driver->getDriverType() == DriverImplementation::RMT_SERIAL)
                {
                    auto rmtDriver = static_cast<RMT_NeoPixel_Serial*>(driver);
                    if (rmtDriver->getRmtChannel())
                    {
                        openknx.logger.log("      Hardware: RMT Channel");
                    }
                }
#endif
            }
        }
    }
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // VIRTUAL STRIPS
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Virtual Strips:");
    openknx.logger.color(0);
    uint32_t virtCount = _manager->getVirtualStripCount();
    if (virtCount == 0)
    {
        openknx.logger.log("  No virtual strips created");
        openknx.logger.log("  Use 'neo virt add <led_count>'");
    }
    else
    {
        for (uint32_t i = 0; i < virtCount; i++)
        {
            auto vstrip = _manager->getVirtualStrip(i);
            if (vstrip)
            {
                uint16_t physCount = vstrip->getPhysicalStripCount();
                openknx.logger.logWithValues("  [%d] %d LEDs - %d Physical Strip%s attached:",
                                             i, vstrip->getLedCount(), physCount, physCount == 1 ? "" : "s");

                // List attached physical strips
                for (uint16_t p = 0; p < physCount; p++)
                {
                    auto phys = vstrip->getPhysicalStrip(p);
                    if (phys)
                    {
                        // Find physical strip index in manager
                        int physIdx = -1;
                        for (uint32_t pi = 0; pi < _manager->getStripCount(); pi++)
                        {
                            if (_manager->getStrip(pi) == phys)
                            {
                                physIdx = pi;
                                break;
                            }
                        }
                        if (physIdx >= 0)
                        {
                            openknx.logger.logWithValues("      - PhysStrip[%d] GPIO%d (%d LEDs)",
                                                         physIdx, phys->getDataPin(), phys->getLedCount());
                        }
                    }
                }
            }
        }
    }
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // SEGMENTS
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Segments:");
    openknx.logger.color(0);
    uint32_t segCount = _manager->getSegmentCount();
    if (segCount == 0)
    {
        openknx.logger.log("  No segments created");
        openknx.logger.log("  Use 'neo seg add <virt> <start> <end>'");
    }
    else
    {
        for (uint32_t i = 0; i < segCount; i++)
        {
            auto seg = _manager->getSegment(i);
            if (seg)
            {
                auto effect = seg->getEffect();
                auto vstrip = seg->getVirtualStrip();

                // Find VirtualStrip index
                int vstripIdx = -1;
                for (uint32_t v = 0; v < _manager->getVirtualStripCount(); v++)
                {
                    if (_manager->getVirtualStrip(v) == vstrip)
                    {
                        vstripIdx = v;
                        break;
                    }
                }

                // Get effect name
                const char* effectName = effect ? effect->getName() : "None";

                // Compact format: [0] 36 LEDs -> Effect: BPM
                openknx.logger.logWithValues("  [%d] %d LEDs -> Effect: %s",
                                             i, seg->getLength(), effectName);
                // Second line: Virtual Strip and Range
                openknx.logger.logWithValues("      Virtual Strip[%d], Range %d-%d",
                                             vstripIdx, seg->getStartLed(), seg->getEndLed());
            }
        }
    }
    openknx.logger.log("");

    // -----------------------------------------------------------------------
    // EFFEKTMANAGER / EFFEKTKETTE (compact, data via OAM backend hooks)
    // -----------------------------------------------------------------------
    if (segCount > 0)
    {
        printEmChainInfoCompact();
    }

    // -----------------------------------------------------------------------
    // STATISTICS
    // -----------------------------------------------------------------------
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Statistics:");
    openknx.logger.color(0);
    openknx.logger.logWithValues("  Total LEDs:      %d", stats.totalLeds);
    openknx.logger.logWithValues("  Memory Usage:    %d bytes", _manager->getTotalMemoryUsage());
    openknx.logger.logWithValues("  Updates:         %d", stats.updateCount);
    openknx.logger.logWithValues("  Errors:          %d", stats.errorCount);
    openknx.logger.logWithValues("  Auto Update:     %s", _autoUpdate ? "Enabled" : "Disabled");
    if (_autoUpdate)
    {
        // FTL mode (_updateInterval == 0) runs flat out — avoid /0, derive from frame time.
        const bool ftl = (_updateInterval == 0);
        if (ftl)
            openknx.logger.log("  Target FPS:      unlimited (FTL)");
        else
            openknx.logger.logWithValues("  Target FPS:      %d Hz (%d ms)", (1000 / _updateInterval), _updateInterval);

        // Show actual FPS if performance data available
        if (g_perfTracker.hasData())
        {
            float actualFPS = g_perfTracker.getCurrentFPS(_updateInterval); // FTL-aware
            uint32_t avgUpdateTime = g_perfTracker.getAverageTime();
            uint32_t targetFPS = ftl ? 0 : (1000 / _updateInterval);
            float cpuLoad = ftl ? 100.0f : (avgUpdateTime * targetFPS / 1000000.0f) * 100.0f;

            openknx.logger.logWithValues("  Actual FPS:      %.1f Hz", actualFPS);
            openknx.logger.logWithValues("  Avg Update:      %d µs", avgUpdateTime);
            openknx.logger.logWithValues("  CPU Usage:       %.2f%%", cpuLoad);

#ifdef ARDUINO_ARCH_RP2040
            // Check if any strip uses DMA
            bool anyDMA = false;
            for (uint32_t i = 0; i < physCount; i++)
            {
                auto strip = _manager->getStrip(i);
                if (strip)
                {
                    auto driver = strip->getDriver();
                    if (driver)
                    {
                        auto pioDriver = dynamic_cast<PIO_NeoPixel_Serial*>(driver);
                        if (pioDriver && pioDriver->getDmaChannel() >= 0)
                        {
                            anyDMA = true;
                            break;
                        }
                    }
                }
            }
            openknx.logger.logWithValues("  DMA Accel:       %s", anyDMA ? "Active" : "Disabled");
#endif
        }
    }

    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/** @brief Process 'neo list' command
 */
bool NeoPixel::processListCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  PhysicalLED Strips");
    printSectionSeparator();

    uint32_t count = _manager->getStripCount();
    if (count == 0)
    {
        openknx.logger.log("No LED strips configured.");
        openknx.logger.log("  Use 'neo phys add <gpio_pin> <led_count>' to create one.");
    }
    else
    {
        openknx.logger.log("ID │ Pin  │ LEDs │ Protocol │ Driver          │ Status");
        openknx.logger.log("───┼──────┼──────┼──────────┼─────────────────┼────────");

        for (uint32_t i = 0; i < count; i++)
        {
            auto strip = _manager->getStrip(i);
            if (strip)
            {
                openknx.logger.logWithValues("%2d │ %4d │ %4d │ %8s │ %-11s │ %s",
                                             i,
                                             strip->getDataPin(),
                                             strip->getLedCount(),
                                             getProtocolName(strip->getProtocol()),
                                             strip->getDriverName(),
                                             strip->isInitialized() ? "OK" : "ERROR");
            }
        }
    }
    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo update' command
 */
bool NeoPixel::processUpdateCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("Updating all LED strips...");
    _manager->updateAll();
    openknx.logger.log("Update complete");

    return true;
}

/**
 * @brief Process 'neo clear' command
 */
bool NeoPixel::processClearCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("Clearing all LEDs...");
    _manager->clearAll();
    _manager->updateAll();
    openknx.logger.log("All LEDs turned off");

    return true;
}

/**
 * @brief Process 'neo test' command
 */
bool NeoPixel::processTestCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse strip index
    int stripIndex = atoi(args.c_str());
    auto strip = _manager->getStrip(stripIndex);

    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Strip %d not found!", stripIndex);
        return true;
    }

    openknx.logger.logWithValues("Running test pattern on Pyhsical Strip %d...", stripIndex);

    // Simple test pattern: RGB cycle
    openknx.logger.log("Setting Red and waiting 2 seconds...");
    strip->setAll(255, 0, 0);
    strip->show();
    delay(2000);

    openknx.logger.log("Setting Green and waiting 2 seconds...");
    strip->setAll(0, 255, 0);
    strip->show();
    delay(2000);

    openknx.logger.log("Setting Blue and waiting 2 seconds...");
    strip->setAll(0, 0, 255);
    strip->show();
    delay(2000);

    openknx.logger.log("Turning off all LEDs...");
    strip->setAll(0, 0, 0);
    strip->show();

    openknx.logger.log("Test pattern complete");

    return true;
}

/**
 * @brief Process 'neo speed' command
 */
bool NeoPixel::processSpeedCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Show current FPS if no argument
    if (args.empty())
    {
        if (_updateInterval > 0)
        {
            openknx.logger.logWithValues("Current speed: %d ms (%d FPS target)",
                                         _updateInterval, 1000 / _updateInterval);
        }
        else
        {
            float actualFps = getActualFps();
            if (actualFps > 0.0f)
            {
                openknx.logger.logWithValues("Current speed: FTL mode (%.1f FPS measured)",
                                             actualFps);
            }
            else
            {
                openknx.logger.log("Current speed: FTL mode (measuring...)");
            }
        }
        openknx.logger.log("Available modes: slow, normal, fast, max, ludicrous, ftl");
        return true;
    }

    std::string mode = args;
    // Convert to lowercase
    for (auto& c : mode)
        c = tolower(c);

    if (mode == "slow")
    {
        setUpdateSpeed(UpdateSpeed::SLOW);
    }
    else if (mode == "normal")
    {
        setUpdateSpeed(UpdateSpeed::NORMAL);
    }
    else if (mode == "fast")
    {
        setUpdateSpeed(UpdateSpeed::FAST);
    }
    else if (mode == "max")
    {
        setUpdateSpeed(UpdateSpeed::MAX);
    }
    else if (mode == "ludicrous")
    {
        setUpdateSpeed(UpdateSpeed::LUDICROUS);
        openknx.logger.log("LUDICROUS SPEED! They've gone to plaid!");
    }
    else if (mode == "ftl")
    {
        setUpdateSpeed(UpdateSpeed::FTL);
        openknx.logger.log("FTL - Faster Than Light! Maximum overdrive engaged!");
        openknx.logger.log("Measuring actual FPS... (wait 1 second for measurement)");
    }
    else
    {
        openknx.logger.log("ERROR: Invalid speed mode!");
        openknx.logger.log("Available modes: slow, normal, fast, max, ludicrous, ftl");
        return true;
    }

    if (_updateInterval > 0)
    {
        openknx.logger.logWithValues("Update speed set to %s (%d ms, %d FPS)",
                                     args.c_str(), _updateInterval, 1000 / _updateInterval);
    }
    else
    {
        openknx.logger.logWithValues("Update speed set to %s (%d ms, unlimited FPS)",
                                     args.c_str(), _updateInterval);
    }

    return true;
}

/**
 * @brief Process 'neo auto' command
 */
bool NeoPixel::processAutoCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    if (args == "on")
    {
        setAutoUpdate(true);
        openknx.logger.logWithValues("Auto-update enabled @ %d FPS", 1000 / _updateInterval);
    }
    else if (args == "off")
    {
        setAutoUpdate(false);
        openknx.logger.log("Auto-update disabled");
    }
    else
    {
        openknx.logger.log("ERROR: Use 'neo auto on' or 'neo auto off'");
    }

    return true;
}

#ifdef OPENKNX_NEOPIXEL_TESTS
/**
 * @brief Process 'neo anim start' command
 */
bool NeoPixel::processAnimTestStartCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    if (AnimationTest::instance().isRunning())
    {
        openknx.logger.log("AnimationTest already running!");
        return true;
    }

    openknx.logger.log("Starting AnimationTest...");

    // Try to start (if already initialized), otherwise init
    AnimationTest::instance().start();

    // If start didn't work (not initialized yet), do full init
    if (!AnimationTest::instance().isRunning())
    {
        if (AnimationTest::instance().init(_manager))
        {
            openknx.logger.log("AnimationTest initialized and started");
        }
        else
        {
            openknx.logger.log("Failed to initialize AnimationTest");
        }
    }
    else
    {
        openknx.logger.log("AnimationTest resumed");
    }

    return true;
}

/**
 * @brief Process 'neo anim stop' command
 */
bool NeoPixel::processAnimTestStopCommand()
{
    if (!AnimationTest::instance().isRunning())
    {
        openknx.logger.log("AnimationTest not running!");
        return true;
    }

    AnimationTest::instance().stop();
    openknx.logger.log("AnimationTest stopped");

    return true;
}

/**
 * @brief Process 'neo simple start' command
 */
bool NeoPixel::processSimpleTestStartCommand()
{
    if (SimpleTest::instance().isRunning())
    {
        openknx.logger.log("SimpleTest already running!");
        return true;
    }

    openknx.logger.log("Starting SimpleTest...");
    if (SimpleTest::instance().init())
    {
        openknx.logger.log("SimpleTest started");
    }
    else
    {
        openknx.logger.log("Failed to start SimpleTest");
    }

    return true;
}

/**
 * @brief Process 'neo simple stop' command
 */
bool NeoPixel::processSimpleTestStopCommand()
{
    if (!SimpleTest::instance().isRunning())
    {
        openknx.logger.log("SimpleTest not running!");
        return true;
    }

    SimpleTest::instance().stop();
    openknx.logger.log("SimpleTest stopped");

    return true;
}
#endif // OPENKNX_NEOPIXEL_TESTS

// ============================================================================
// Benchmark Commands
// ============================================================================
#ifdef OPENKNX_NEOPIXEL_BENCHMARK
/**
 * @brief Process 'neo bench' commands
 */
bool NeoPixel::processBenchmarkCommand(const std::string& args)
{
    using namespace NeoPixel;

    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse sub-command
    if (args.empty())
    {
        // Run all benchmarks on ALL strips
        uint32_t stripCount = _manager->getStripCount();
        if (stripCount == 0)
        {
            openknx.logger.log("ERROR: No strips configured!");
            return true;
        }

        openknx.logger.log("");
        printSectionSeparator();
        openknx.logger.logWithValues("  Running ALL Benchmarks on %d Strip%s",
                                     stripCount, stripCount == 1 ? "" : "s");
        printSectionSeparator();
        openknx.logger.log("");

        for (uint32_t i = 0; i < stripCount; i++)
        {
            auto strip = _manager->getStrip(i);
            if (strip)
            {
                // Create fresh Benchmark object for each strip
                Benchmark bench;
                char stripName[32];
                snprintf(stripName, sizeof(stripName), "Strip %d (GPIO%d)", i, strip->getDataPin());
                bench.runAllBenchmarks(strip, stripName);

                // Separator between strips (except last)
                if (i < stripCount - 1)
                {
                    openknx.logger.log("");
                    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                    openknx.logger.log("───────────────────────────────────────────────────────────────");
                    openknx.logger.color(0);
                    openknx.logger.log("");
                }
            }
        }

        return true;
    }

    // Parse "speed [strip]", "colors [strip]", etc.
    std::string cmd = args;
    uint32_t stripIdx = 0;

    // Extract strip index if provided
    size_t spacePos = cmd.find(' ');
    if (spacePos != std::string::npos)
    {
        std::string stripStr = cmd.substr(spacePos + 1);
        stripIdx = std::stoul(stripStr);
        cmd = cmd.substr(0, spacePos);
    }

    // Validate strip index
    if (stripIdx >= _manager->getStripCount())
    {
        openknx.logger.logWithValues("ERROR: Strip %d not found (max: %d)",
                                     stripIdx, _manager->getStripCount() - 1);
        return true;
    }

    auto strip = _manager->getStrip(stripIdx);
    char stripName[32];
    snprintf(stripName, sizeof(stripName), "Strip %d", stripIdx);

    // Create fresh Benchmark object for single benchmark
    Benchmark bench;

    // Execute benchmark
    if (cmd == "speed")
    {
        bench.benchmarkUpdateSpeed(strip, stripName);
    }
    else if (cmd == "colors")
    {
        bench.benchmarkColorPatterns(strip, stripName);
    }
    else if (cmd == "size")
    {
        bench.benchmarkSizeComparison(_manager);
    }
    else if (cmd == "stability")
    {
        bench.benchmarkStability(strip, stripName);
    }
    else
    {
        openknx.logger.log("ERROR: Unknown benchmark command!");
        openknx.logger.log("Usage: neo bench [speed|colors|size|stability] [strip]");
        return true;
    }

    return true;
}
#endif // OPENKNX_NEOPIXEL_BENCHMARK

// ============================================================================
// Performance Monitoring Command
// ============================================================================
/**
 * @brief Process 'neo perf' command - Show performance statistics
 */
bool NeoPixel::processPerformanceCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    if (!g_perfTracker.hasData())
    {
        openknx.logger.log("No performance data yet. Enable auto-update and wait a few seconds:");
        openknx.logger.log("  neo auto on");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  NeoPixel Performance Statistics");
    printSectionSeparator();
    openknx.logger.log("");

    // Calculate statistics. In FTL mode (_updateInterval == 0) the loop runs flat out,
    // so derive rate/budget from the measured frame time instead of dividing by zero.
    const bool ftl = (_updateInterval == 0);
    uint32_t avgUpdateTime = g_perfTracker.getAverageTime();
    float currentFPS = g_perfTracker.getCurrentFPS(_updateInterval); // FTL: 1e6 / avg frame time
    uint32_t uptime = g_perfTracker.getUptimeSeconds();
    uint32_t targetFPS = ftl ? 0 : (1000 / _updateInterval);
    // CPU: throttled = avgFrame / period; FTL = continuous rendering -> ~100%
    float cpuLoad = ftl ? 100.0f : (avgUpdateTime * targetFPS / 1000000.0f) * 100.0f;

    // Calculate total LED count and memory
    uint32_t totalLEDs = 0;
    uint32_t totalMemory = 0;
    for (uint32_t i = 0; i < _manager->getStripCount(); i++)
    {
        auto strip = _manager->getStrip(i);
        if (strip)
        {
            totalLEDs += strip->getLedCount();
            totalMemory += strip->getBufferSize();
        }
    }

    // Update timing
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Update Timing:");
    openknx.logger.color(0);
    openknx.logger.logWithValues("  Total Frames:    %lu", g_perfTracker.frameCount);
    openknx.logger.logWithValues("  Current FPS:     %.1f Hz", currentFPS);
    if (ftl)
        openknx.logger.log("  Target FPS:      unlimited (FTL)");
    else
        openknx.logger.logWithValues("  Target FPS:      %lu Hz", targetFPS);
    openknx.logger.logWithValues("  Min Update:      %lu µs",
                                 g_perfTracker.minUpdateTime == UINT32_MAX ? 0 : g_perfTracker.minUpdateTime);
    openknx.logger.logWithValues("  Max Update:      %lu µs", g_perfTracker.maxUpdateTime);
    openknx.logger.logWithValues("  Avg Update:      %lu µs", avgUpdateTime);

    // Calculate time budget (FTL has no fixed budget — the frame period IS the render time)
    if (ftl)
    {
        openknx.logger.logWithValues("  Frame Budget:    none (FTL) - avg frame %lu µs", avgUpdateTime);
    }
    else
    {
        uint32_t frameBudget = _updateInterval * 1000; // µs per frame
        float budgetUsed = (avgUpdateTime * 100.0f) / frameBudget;
        openknx.logger.logWithValues("  Frame Budget:    %lu µs (%.1f%% used)", frameBudget, budgetUsed);
    }
    openknx.logger.log("");

    // CPU Load Analysis
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("CPU Load:");
    openknx.logger.color(0);

    openknx.logger.logWithValues("  CPU Usage:       %.2f%%", cpuLoad);
    // Free CPU can't be negative — when overloaded (cpuLoad > 100% = frame work exceeds
    // the requested budget) there is simply 0% headroom; the overload size shows in CPU Usage.
    openknx.logger.logWithValues("  Free CPU:        %.2f%%", cpuLoad < 100.0f ? (100.0f - cpuLoad) : 0.0f);

    // Throughput calculation (LEDs updated per second) — uses the capped real FPS.
    uint32_t throughput = totalLEDs * currentFPS;
    openknx.logger.logWithValues("  Throughput:      %lu LEDs/sec", throughput);

    // Status judged against the ACTUAL frame budget — not a fixed µs threshold.
    // (28 ms is perfectly fine for a big 2D matrix as long as it fits the interval;
    //  the old `< 500 µs` test wrongly flagged every non-trivial config as "To Slow".)
    if (ftl)
    {
        openknx.logger.logWithValues("  Status:          FTL - running flat out (%.1f FPS)", currentFPS);
    }
    else
    {
        uint32_t frameBudgetUs = _updateInterval * 1000;
        float used = (frameBudgetUs > 0) ? (avgUpdateTime * 100.0f / frameBudgetUs) : 0.0f;
        if (frameBudgetUs > 0 && avgUpdateTime >= frameBudgetUs)
            openknx.logger.log("  Status:          TOO SLOW - frame exceeds budget! Raise interval or reduce load.");
        else if (used < 70.0f)
            openknx.logger.log("  Status:          Healthy - comfortable budget headroom.");
        else if (used < 90.0f)
            openknx.logger.log("  Status:          OK - getting tight (>70% of budget).");
        else
            openknx.logger.log("  Status:          TIGHT - near frame budget (>90%).");
    }
    openknx.logger.log("");

    // Hardware Status
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Hardware Status:");
    openknx.logger.color(0);

    uint32_t physCount = _manager->getStripCount();
    for (uint32_t i = 0; i < physCount; i++)
    {
        auto strip = _manager->getStrip(i);
        if (strip)
        {
            auto caps = strip->getCapabilities();
            uint32_t stripLEDs = strip->getLedCount();
            uint32_t stripBytes = strip->getBufferSize();

            openknx.logger.logWithValues("  Strip %d:         %d LEDs, %d bytes, DMA=%s, Async=%s",
                                         i, stripLEDs, stripBytes,
                                         caps.supportsDMA ? "YES" : "NO",
                                         caps.supportsAsync ? "YES" : "NO");
        }
    }

    openknx.logger.logWithValues("  Total LEDs:      %lu", totalLEDs);
    openknx.logger.logWithValues("  Total Memory:    %lu bytes", totalMemory);
    openknx.logger.log("");

    // Active Effects
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Active Effects:");
    openknx.logger.color(0);

    uint32_t segCount = _manager->getSegmentCount();
    if (segCount == 0)
    {
        openknx.logger.log("  No active segments");
    }
    else
    {
        for (uint32_t i = 0; i < segCount; i++)
        {
            auto seg = _manager->getSegment(i);
            if (seg)
            {
                auto effect = seg->getEffect();
                const char* effectName = effect ? effect->getName() : "None";
                openknx.logger.logWithValues("  Segment[%d]:     %s (%d LEDs)",
                                             i, effectName, seg->getLength());
            }
        }
    }
    openknx.logger.log("");

    // Session info
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Session:");
    openknx.logger.color(0);

    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    uint32_t seconds = uptime % 60;

    if (hours > 0)
    {
        openknx.logger.logWithValues("  Uptime:          %luh %lum %lus", hours, minutes, seconds);
    }
    else if (minutes > 0)
    {
        openknx.logger.logWithValues("  Uptime:          %lum %lus", minutes, seconds);
    }
    else
    {
        openknx.logger.logWithValues("  Uptime:          %lus", seconds);
    }

    openknx.logger.logWithValues("  Auto-Update:     %s", _autoUpdate ? "ENABLED" : "DISABLED");

    // FPS history (if we have enough frames)
    if (g_perfTracker.frameCount > 10)
    {
        float fpsVariance = (g_perfTracker.maxUpdateTime - g_perfTracker.minUpdateTime) * 100.0f / avgUpdateTime;
        openknx.logger.logWithValues("  FPS Stability:   %.1f%% variance", fpsVariance);
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

// ============================================================================
// PhysicalStrip Management Commands
// ============================================================================
/**
 * @brief Process 'neo phys' command router
 */
bool NeoPixel::processPhysCommand(const std::string& args)
{
    if (args.empty() || args == "list")
    {
        return processPhysListCommand();
    }
    else if (args == "timings")
    {
        return processPhysTimingsCommand();
    }
    else if (args.compare(0, 7, "timing ") == 0)
    {
        return processPhysTimingCommand(args.substr(7));
    }
    else if (args.compare(0, 4, "add ") == 0)
    {
        return processPhysAddCommand(args.substr(4));
    }
    else if (args.compare(0, 4, "del ") == 0)
    {
        return processPhysDelCommand(args.substr(4));
    }
    else if (args.compare(0, 7, "config ") == 0)
    {
        return processPhysConfigCommand(args.substr(7));
    }
    else
    {
        openknx.logger.log("ERROR: Unknown phys command. Use 'neo ?' for help.");
        return true;
    }
}

/**
 * @brief Process 'neo phys list' command
 */
bool NeoPixel::processPhysListCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Physical Strips");
    printSectionSeparator();

    uint32_t stripCount = _manager->getStripCount();

    if (stripCount == 0)
    {
        openknx.logger.log("No physical strips configured.");
        openknx.logger.log("Use 'neo phys add <gpio> <leds>' to create one.");
    }
    else
    {
        openknx.logger.log("ID  │ Pins           │ LEDs │ Protocol │ Order │ Driver      │ Timing        │ Status");
        openknx.logger.log("────┼────────────────┼──────┼──────────┼───────┼─────────────┼───────────────┼────────");

        for (uint32_t i = 0; i < stripCount; i++)
        {
            auto strip = _manager->getStrip(i);
            if (strip)
            {
                // Get protocol name
                const char* protocol = "???";
                LedProtocol proto = strip->getProtocol();
                switch (proto)
                {
                    case LedProtocol::WS2812: protocol = "WS2812"; break;
                    case LedProtocol::WS2812B: protocol = "WS2812B"; break;
                    case LedProtocol::SK6812: protocol = "SK6812"; break;
                    case LedProtocol::APA102: protocol = "APA102"; break;
                    case LedProtocol::SK9822: protocol = "SK9822"; break;
                    case LedProtocol::WS2801: protocol = "WS2801"; break;
                    case LedProtocol::LPD8806: protocol = "LPD8806"; break;
                    default: break;
                }

                // Get ColorOrder from PhysicalStrip
                const char* colorOrder = "???";
                switch (strip->getColorOrder())
                {
                    case ColorOrder::NONE: colorOrder = "DEFAULT"; break;
                    case ColorOrder::RGB: colorOrder = "RGB"; break;
                    case ColorOrder::RBG: colorOrder = "RBG"; break;
                    case ColorOrder::GRB: colorOrder = "GRB"; break;
                    case ColorOrder::BGR: colorOrder = "BGR"; break;
                    case ColorOrder::GBR: colorOrder = "GBR"; break;
                    case ColorOrder::BRG: colorOrder = "BRG"; break;
                    case ColorOrder::RGBW: colorOrder = "RGBW"; break;
                    case ColorOrder::GRBW: colorOrder = "GRBW"; break;
                    // 5-Channel color orders (RGBCCT)
                    case ColorOrder::RGBCCT: colorOrder = "RGBCCT"; break;
                    case ColorOrder::GRBCCT: colorOrder = "GRBCCT"; break;
                    case ColorOrder::RGBCTW: colorOrder = "RGBCTW"; break;
                    case ColorOrder::GRBCTW: colorOrder = "GRBCTW"; break;
                }

                const char* driver = strip->getDriverName();
                const char* status = strip->isInitialized() ? "READY" : "ERROR";

                // Get timing mode name
                const char* timingName = "N/A";
#ifdef ARDUINO_ARCH_RP2040
                TimingMode mode = strip->getTimingMode();
                timingName = getTimingModeName(mode);
#endif

                // Check if SPI strip (APA102, WS2801, etc.)
                bool isSpiStrip = (proto == LedProtocol::APA102 ||
                                   proto == LedProtocol::SK9822 ||
                                   proto == LedProtocol::WS2801 ||
                                   proto == LedProtocol::LPD8806);

                if (isSpiStrip)
                {
#ifdef ARDUINO_ARCH_RP2040
                    // Get CLK and MOSI pins from SPI driver
                    auto driverPtr = strip->getDriver();
                    auto spiDriver = dynamic_cast<PIO_NeoPixel_SPI*>(driverPtr);
                    if (spiDriver)
                    {
                        openknx.logger.logWithValues("[%d] │ CLK: %d MOSI: %d │ %4d │ %-8s │ %-5s │ %-11s │ %-13s │ %s",
                                                     i,
                                                     spiDriver->getClkPin(),
                                                     spiDriver->getMosiPin(),
                                                     strip->getLedCount(),
                                                     protocol,
                                                     colorOrder,
                                                     driver,
                                                     timingName,
                                                     status);
                    }
                    else
#endif
                    {
                        // Fallback if driver not available
                        openknx.logger.logWithValues("[%d] │ GPIO%-2d (SPI)   │ %4d │ %-8s │ %-5s │ %-11s │ %-13s │ %s",
                                                     i,
                                                     strip->getDataPin(),
                                                     strip->getLedCount(),
                                                     protocol,
                                                     colorOrder,
                                                     driver,
                                                     timingName,
                                                     status);
                    }
                }
                else
                {
                    // 1-Wire strip (WS2812B, SK6812, etc.)
                    openknx.logger.logWithValues("[%d] │ GPIO%-2d         │ %4d │ %-8s │ %-5s │ %-11s │ %-13s │ %s",
                                                 i,
                                                 strip->getDataPin(),
                                                 strip->getLedCount(),
                                                 protocol,
                                                 colorOrder,
                                                 driver,
                                                 timingName,
                                                 status);
                }
            }
        }
    }

    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo phys add <gpio> <count> [type]' command
 * LED Types: ws2812b (default, RGB), sk6812 (RGBW), apa102 (RGB+Clock)
 */
bool NeoPixel::processPhysAddCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <gpio> <led_count> [protocol] [data_gpio]
    // For WS2812B/SK6812: neo phys add <gpio> <count> [ws2812b|sk6812]
    // For APA102: neo phys add <clk_gpio> <count> apa102 <data_gpio>
    int gpio, ledCount, dataGpio = -1;
    char protocolStr[20] = "";
    int parsed = sscanf(args.c_str(), "%d %d %19s %d", &gpio, &ledCount, protocolStr, &dataGpio);

    if (parsed < 2)
    {
        openknx.logger.log("ERROR: Usage:");
        openknx.logger.log("  1-Wire (WS2812B/SK6812): neo phys add <gpio> <count> [ws2812b|sk6812]");
        openknx.logger.log("  SPI (APA102):            neo phys add <clk_gpio> <count> apa102 <data_gpio>");
        return true;
    }

    // Validate GPIO
    if (gpio < 0 || gpio > 29)
    {
        openknx.logger.log("ERROR: Invalid GPIO pin (must be 0-29)");
        return true;
    }

    // Validate LED count
    if (ledCount <= 0 || ledCount > 1024)
    {
        openknx.logger.log("ERROR: Invalid LED count (must be 1-1024)");
        return true;
    }

    // Parse LED protocol (default: WS2812B)
    LedProtocol protocol = LedProtocol::WS2812B;
    const char* protocolName = "WS2812B";

    if (parsed >= 3)
    {
        std::string proto(protocolStr);
        // Convert to lowercase
        for (auto& c : proto)
            c = tolower(c);

        if (proto == "ws2812b" || proto == "ws2812")
        {
            protocol = LedProtocol::WS2812B;
            protocolName = "WS2812B";
        }
        else if (proto == "sk6812")
        {
            protocol = LedProtocol::SK6812;
            protocolName = "SK6812";
        }
        else if (proto == "apa102")
        {
            protocol = LedProtocol::APA102;
            protocolName = "APA102";

            // APA102 REQUIRES data GPIO!
            if (dataGpio < 0 || dataGpio > 29)
            {
                openknx.logger.log("ERROR: APA102 requires CLK and DATA pins!");
                openknx.logger.log("       Usage: neo phys add <clk_gpio> <count> apa102 <data_gpio>");
                openknx.logger.logWithValues("       Example: neo phys add 8 100 apa102 9 (CLK=GPIO8, DATA=GPIO9)");
                return true;
            }
        }
        else
        {
            openknx.logger.logWithValues("ERROR: Unknown protocol '%s'", proto.c_str());
            openknx.logger.log("       Supported: ws2812b, sk6812, apa102");
            return true;
        }
    }

    // Create physical strip with chosen protocol
    PhysicalStrip* strip = nullptr;

    if (protocol == LedProtocol::APA102)
    {
        // APA102: gpio = CLK, dataGpio = DATA/MOSI
        // addSpiStrip expects: (mosiPin, sckPin, ...)
        strip = _manager->addSpiStrip(dataGpio, gpio, ledCount, protocol);
        if (!strip)
        {
            openknx.logger.logWithValues("ERROR: Failed to create APA102 strip (CLK=%d, DATA=%d)!", gpio, dataGpio);
            return false;
        }
    }
    else
    {
        // WS2812B/SK6812: 1-Wire
        strip = _manager->addStrip(gpio, ledCount, protocol);
        if (!strip)
        {
            openknx.logger.logWithValues("ERROR: Failed to create physical strip on GPIO %d!", gpio);
            return false;
        }
    }

    // Initialize the new strip
    if (!strip->init())
    {
        openknx.logger.logWithValues("ERROR: Failed to initialize strip on GPIO %d!", gpio);
        return false;
    }

    uint32_t id = _manager->getStripCount() - 1;

    if (protocol == LedProtocol::APA102)
    {
        openknx.logger.logWithValues("Physical strip [%d] created: CLK=%d, DATA=%d, %d LEDs (%s/PIO-SPI)",
                                     id, gpio, dataGpio, ledCount, protocolName);
    }
    else
    {
        openknx.logger.logWithValues("Physical strip [%d] created: GPIO %d, %d LEDs (%s/PIO)",
                                     id, gpio, ledCount, protocolName);
    }

    return true;
}

/**
 * @brief Process 'neo phys del <id>' command
 */
bool NeoPixel::processPhysDelCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse physical strip ID
    int physId = atoi(args.c_str());
    auto strip = _manager->getStrip(physId);

    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Physical strip [%d] not found!", physId);
        return true;
    }

    // Check if strip is attached to any virtual strip
    uint32_t vCount = _manager->getVirtualStripCount();
    for (uint32_t i = 0; i < vCount; i++)
    {
        auto vstrip = _manager->getVirtualStrip(i);
        if (vstrip)
        {
            for (uint16_t j = 0; j < vstrip->getPhysicalStripCount(); j++)
            {
                if (vstrip->getPhysicalStrip(j) == strip)
                {
                    openknx.logger.logWithValues("ERROR: Physical strip [%d] is attached to virtual strip [%d]!",
                                                 physId, i);
                    openknx.logger.log("       Use 'neo virt detach' first");
                    return true;
                }
            }
        }
    }

    // Remove physical strip
    if (!_manager->removeStrip(strip))
    {
        openknx.logger.logWithValues("ERROR: Failed to remove physical strip [%d]!", physId);
        return true;
    }

    openknx.logger.logWithValues("Physical strip [%d] removed", physId);

    return true;
}

// ============================================================================
// VirtualStrip Management Commands
// ============================================================================
/**
 * @brief Process 'neo virt' command router
 */
bool NeoPixel::processVirtCommand(const std::string& args)
{
    if (args.empty() || args == "list")
    {
        return processVirtListCommand();
    }
    else if (args.compare(0, 4, "add ") == 0)
    {
        return processVirtAddCommand(args.substr(4));
    }
    else if (args.compare(0, 4, "del ") == 0)
    {
        return processVirtDelCommand(args.substr(4));
    }
    else if (args.compare(0, 7, "attach ") == 0)
    {
        return processVirtAttachCommand(args.substr(7));
    }
    else if (args.compare(0, 7, "detach ") == 0)
    {
        return processVirtDetachCommand(args.substr(7));
    }
    else
    {
        openknx.logger.log("ERROR: Unknown virt command. Use 'neo ?' for help.");
        return true;
    }
}

/**
 * @brief Process 'neo virt list' command
 */
bool NeoPixel::processVirtListCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Virtual Strips");
    printSectionSeparator();

    uint32_t count = _manager->getVirtualStripCount();
    if (count == 0)
    {
        openknx.logger.log("No virtual strips created.");
        openknx.logger.log("Use 'neo virt add <leds>' to create one.");
    }
    else
    {
        openknx.logger.log("ID │ Total LEDs │ Order  │ Attached │ Status");
        openknx.logger.log("───┼────────────┼────────┼──────────┼────────");

        for (uint32_t i = 0; i < count; i++)
        {
            auto vstrip = _manager->getVirtualStrip(i);
            if (vstrip)
            {
                // Display buffer format (RGB vs RGBW vs RGBCCT)
                const char* bufferFormat = "RGB";
                if (vstrip->hasDualWhiteChannel())
                {
                    bufferFormat = "RGBCCT";
                }
                else if (vstrip->hasWhiteChannel())
                {
                    bufferFormat = "RGBW";
                }

                // Determine status: Check if physical strips attached and if busy
                const char* status = "OK";
                if (vstrip->getPhysicalStripCount() == 0)
                {
                    status = "NO STRIPS";
                }
                else if (vstrip->isAnyBusy())
                {
                    status = "BUSY";
                }

                openknx.logger.logWithValues("%2d │ %10d │ %6s │ %8s │ %s",
                                             i,
                                             vstrip->getLedCount(),
                                             bufferFormat,
                                             vstrip->getPhysicalStripCount() > 0 ? "Yes" : "No",
                                             status);
            }
        }
    }

    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo virt add <leds> [type]' command
 * Type: RGB (3 bytes/LED), RGBW (4 bytes/LED), or RGBCCT (5 bytes/LED), default: RGB
 *
 * VirtualStrip stores pixels in RGB/RGBW/RGBCCT format internally.
 * ColorOrder conversion (GRB, BGR, etc.) happens in PhysicalStrip!
 */
bool NeoPixel::processVirtAddCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse LED count and optional RGBW flag
    int ledCount;
    char typeStr[10] = "";
    int parsed = sscanf(args.c_str(), "%d %9s", &ledCount, typeStr);

    if (parsed < 1 || ledCount <= 0 || ledCount > 1000)
    {
        openknx.logger.log("ERROR: Usage: neo virt add <leds> [type]");
        openknx.logger.log("       type: RGB (default), RGBW, or RGBCCT");
        openknx.logger.log("       Note: VirtualStrip stores pixels in RGB/RGBW/RGBCCT format");
        openknx.logger.log("             ColorOrder conversion happens in PhysicalStrip");
        return true;
    }

    // Parse type (default: RGB)
    ColorOrder colorOrder = ColorOrder::RGB;
    const char* typeName = "RGB";

    if (parsed >= 2)
    {
        std::string type(typeStr);
        if (type == "rgb" || type == "RGB")
        {
            colorOrder = ColorOrder::RGB;
            typeName = "RGB";
        }
        else if (type == "rgbw" || type == "RGBW")
        {
            colorOrder = ColorOrder::RGBW;
            typeName = "RGBW";
        }
        else if (type == "rgbcct" || type == "RGBCCT")
        {
            colorOrder = ColorOrder::RGBCCT;
            typeName = "RGBCCT";
        }
        else
        {
            openknx.logger.log("ERROR: Invalid type! Use: RGB, RGBW, or RGBCCT");
            return true;
        }
    }

    // Create virtual strip
    VirtualStrip* vstrip = _manager->addVirtualStrip(ledCount, colorOrder);
    if (!vstrip)
    {
        openknx.logger.log("ERROR: Failed to create virtual strip!");
        return true;
    }

    uint32_t id = _manager->getVirtualStripCount() - 1;

    openknx.logger.logWithValues("Virtual strip [%d] created: %d LEDs, Type=%s (%d bytes/LED)",
                                 id, ledCount, typeName, vstrip->getBytesPerLed());
    openknx.logger.log("NOTE: ColorOrder (GRB, BGR, etc.) is set on PhysicalStrips, not VirtualStrips");
    return true;
}

/**
 * @brief Process 'neo virt del <id>' command
 */
bool NeoPixel::processVirtDelCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse virtual strip ID
    int virtId = atoi(args.c_str());
    auto vstrip = _manager->getVirtualStrip(virtId);

    if (!vstrip)
    {
        openknx.logger.logWithValues("ERROR: Virtual strip [%d] not found!", virtId);
        return true;
    }

    // Remove virtual strip (also removes all associated segments)
    if (!_manager->removeVirtualStrip(virtId))
    {
        openknx.logger.logWithValues("ERROR: Failed to remove virtual strip [%d]!", virtId);
        return true;
    }

    openknx.logger.logWithValues("Virtual strip [%d] removed (including all segments)", virtId);

    return true;
}

/**
 * @brief Process 'neo virt attach <virt> <phys>' command
 */
bool NeoPixel::processVirtAttachCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <virt_id> <phys_id>
    int virtId, physId;
    if (sscanf(args.c_str(), "%d %d", &virtId, &physId) != 2)
    {
        openknx.logger.log("ERROR: Usage: neo virt attach <virt_id> <phys_id>");
        return true;
    }

    // Get strips
    auto vstrip = _manager->getVirtualStrip(virtId);
    auto pstrip = _manager->getStrip(physId);

    if (!vstrip)
    {
        openknx.logger.logWithValues("ERROR: Virtual strip [%d] not found!", virtId);
        return true;
    }

    if (!pstrip)
    {
        openknx.logger.logWithValues("ERROR: Physical strip [%d] not found!", physId);
        return true;
    }

    // Calculate offset: sum of all already attached physical strips
    uint16_t offset = 0;
    for (uint16_t i = 0; i < vstrip->getPhysicalStripCount(); i++)
    {
        auto attached = vstrip->getPhysicalStrip(i);
        if (attached)
        {
            offset += attached->getLedCount();
        }
    }

    // Attach at calculated offset
    if (!vstrip->attachPhysicalStrip(pstrip, offset))
    {
        openknx.logger.log("ERROR: Failed to attach physical strip!");
        openknx.logger.log("       (LED count overflow or already attached?)");
        return true;
    }

    openknx.logger.logWithValues("Physical strip [%d] attached to virtual strip [%d] at offset %d",
                                 physId, virtId, offset);

    return true;
}

/**
 * @brief Process 'neo virt detach <virt>' command
 */
bool NeoPixel::processVirtDetachCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <virt_id> [phys_id]
    int virtId, physId = -1;
    int parsed = sscanf(args.c_str(), "%d %d", &virtId, &physId);

    if (parsed < 1)
    {
        openknx.logger.log("ERROR: Usage: neo virt detach <virt_id> [phys_id]");
        openknx.logger.log("       If phys_id omitted, detaches ALL physical strips");
        return true;
    }

    auto vstrip = _manager->getVirtualStrip(virtId);
    if (!vstrip)
    {
        openknx.logger.logWithValues("ERROR: Virtual strip [%d] not found!", virtId);
        return true;
    }

    // Detach specific physical strip or all
    if (physId >= 0)
    {
        auto pstrip = _manager->getStrip(physId);
        if (!pstrip)
        {
            openknx.logger.logWithValues("ERROR: Physical strip [%d] not found!", physId);
            return true;
        }

        if (!vstrip->detachPhysicalStrip(pstrip))
        {
            openknx.logger.logWithValues("ERROR: Physical strip [%d] not attached to virtual strip [%d]!",
                                         physId, virtId);
            return true;
        }

        openknx.logger.logWithValues("Physical strip [%d] detached from virtual strip [%d]",
                                     physId, virtId);
    }
    else
    {
        // Detach all by re-creating empty virtual strip (clear all attachments)
        uint16_t count = vstrip->getPhysicalStripCount();
        for (int i = count - 1; i >= 0; i--)
        {
            PhysicalStrip* pstrip = vstrip->getPhysicalStrip(i);
            if (pstrip)
            {
                vstrip->detachPhysicalStrip(pstrip);
            }
        }

        openknx.logger.logWithValues("All physical strips detached from virtual strip [%d]", virtId);
    }

    return true;
}

// ============================================================================
// Segment Management Commands
// ============================================================================
/**
 * @brief Process 'neo seg' command router
 */
bool NeoPixel::processSegCommand(const std::string& args)
{
    if (args.empty() || args == "list")
    {
        return processSegListCommand();
    }
    else if (args.compare(0, 4, "add ") == 0)
    {
        return processSegAddCommand(args.substr(4));
    }
    else if (args.compare(0, 4, "del ") == 0)
    {
        return processSegDelCommand(args.substr(4));
    }
    else if (args.compare(0, 6, "pause ") == 0)
    {
        return processSegPauseCommand(args.substr(6));
    }
    else if (args.compare(0, 7, "resume ") == 0)
    {
        return processSegResumeCommand(args.substr(7));
    }
    else if (args.compare(0, 5, "stop ") == 0)
    {
        return processSegStopCommand(args.substr(5));
    }
    else if (args.compare(0, 13, "clear effect ") == 0)
    {
        return processSegClearEffectCommand(args.substr(13));
    }
    else if (args.compare(0, 4, "geo ") == 0)
    {
        // neo seg geo <id> <w> <h> [topology]            -> 2D matrix
        // neo seg geo <id> <w> <h> <tileDepth> <topology> -> tiled panel (depth = tile height)
        // Lets you set 2D geometry from the console (normally only ETS does this).
        if (!_initialized || !_manager)
        {
            openknx.logger.log("ERROR: NeoPixel module not initialized!");
            return true;
        }
        int id = -1, w = 0, h = 0, a4 = -1, a5 = -1;
        int n = sscanf(args.c_str() + 4, "%d %d %d %d %d", &id, &w, &h, &a4, &a5);
        if (n < 3 || id < 0 || (uint32_t)id >= _manager->getSegmentCount() || w < 1 || h < 1)
        {
            openknx.logger.log("Usage: neo seg geo <id> <w> <h> [topology]");
            openknx.logger.log("       neo seg geo <id> <w> <h> <tileDepth> <topology>   (tiled panels)");
            openknx.logger.log("  topology: 1=ROWS_SERP 2=ROWS_LIN 3=COLS_SERP 4=COLS_LIN 6=COLS_LIN_TILED 7=COLS_SERP_TILED (0=back to 1D)");
            return true;
        }
        Segment* seg = _manager->getSegment(id);
        if (!seg)
        {
            openknx.logger.logWithValues("ERROR: segment %d not found", id);
            return true;
        }
        uint8_t topo = (n == 3) ? (uint8_t)LedTopology::ROWS_SERPENTINE : (uint8_t)((n == 4) ? a4 : a5);
        if (topo > (uint8_t)LedTopology::COLS_SERP_TILED)
        {
            openknx.logger.logWithValues("ERROR: topology %d out of range (0-7)", (int)topo);
            return true;
        }
        if (topo == (uint8_t)LedTopology::LINEAR_1D)
        {
            seg->clearGeometry();
            openknx.logger.logWithValues("Segment %d geometry cleared (back to 1D)", id);
            return true;
        }
        if (n >= 5)
            seg->setGeometry((uint8_t)w, (uint8_t)h, (uint8_t)a4, static_cast<LedTopology>(topo));
        else
            seg->setGeometry((uint8_t)w, (uint8_t)h, static_cast<LedTopology>(topo));

        const uint16_t need = (uint16_t)w * (uint16_t)h;
        if (need > seg->getLength())
            openknx.logger.logWithValues("WARN: %dx%d=%d exceeds segment length %d (out-of-range pixels are clipped)",
                                         w, h, (int)need, (int)seg->getLength());
        openknx.logger.logWithValues("Segment %d geometry set: %dx%d topology=%d%s",
                                     id, w, h, (int)topo, (n >= 5) ? " (tiled, depth set)" : "");
        return true;
    }
    else
    {
        openknx.logger.log("ERROR: Unknown seg command. Use 'neo ?' for help.");
        return true;
    }
}

/**
 * @brief Process 'neo seg list' command
 */
bool NeoPixel::processSegListCommand()
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Segments");
    printSectionSeparator();

    uint32_t count = _manager->getSegmentCount();
    if (count == 0)
    {
        openknx.logger.log("No segments created.");
        openknx.logger.log("Use 'neo seg add <virt> <start> <end>' to create one.");
    }
    else
    {
        openknx.logger.log("ID │ Range     │ State   │ Effect │ Effect Name      │ Color R:G:B (W)    │ Geo");
        openknx.logger.log("───┼───────────┼─────────┼────────┼──────────────────┼────────────────────┼───────────");
        for (uint32_t i = 0; i < count; i++)
        {
            auto seg = _manager->getSegment(i);
            if (seg)
            {
                auto effect = seg->getEffect();
                auto effectName = effect ? effect->getName() : "None";
                auto& config = seg->getConfig();
                const char* state = seg->isPaused() ? "Paused" : "Running";

                // Geometry summary: "1D" or "WxH tN" (N = topology id)
                const auto& geo = seg->getGeometry();
                char geoBuf[12];
                if (geo.is1D())
                    snprintf(geoBuf, sizeof(geoBuf), "1D");
                else
                    snprintf(geoBuf, sizeof(geoBuf), "%dx%d t%d", geo.width, geo.height, (int)geo.topology);

                openknx.logger.logWithValues("%2d │ %3d - %3d │ %-7s │ %-6s │ %-16s │ %3d:%3d:%3d (%3d) │ %-9s",
                                             i,
                                             seg->getStartLed(),
                                             seg->getEndLed(),
                                             state,
                                             effect ? "Set" : "N/A",
                                             effect ? effectName : "N/A",
                                             (config.primaryRGBW >> 24) & 0xFF, // Red from RGBW
                                             (config.primaryRGBW >> 16) & 0xFF, // Green from RGBW
                                             (config.primaryRGBW >> 8) & 0xFF,  // Blue from RGBW
                                             config.primaryRGBW & 0xFF,         // White from RGBW
                                             geoBuf
                );
            }
        }
    }
    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo seg add <virt> <start> <end>' command
 */
bool NeoPixel::processSegAddCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <virt_id> <start> <end>
    int virtId, startLed, endLed;
    if (sscanf(args.c_str(), "%d %d %d", &virtId, &startLed, &endLed) != 3)
    {
        openknx.logger.log("ERROR: Usage: neo seg add <virt_id> <start_led> <end_led>");
        return true;
    }

    // Get virtual strip
    auto vstrip = _manager->getVirtualStrip(virtId);
    if (!vstrip)
    {
        openknx.logger.logWithValues("ERROR: Virtual strip [%d] not found!", virtId);
        return true;
    }

    // Validate range
    if (startLed < 0 || endLed >= (int)vstrip->getLedCount() || startLed > endLed)
    {
        openknx.logger.logWithValues("ERROR: Invalid LED range (0-%d)", vstrip->getLedCount() - 1);
        return true;
    }

    // Create segment
    Segment* seg = _manager->addSegment(vstrip, startLed, endLed);
    if (!seg)
    {
        openknx.logger.log("ERROR: Failed to create segment!");
        return true;
    }

    uint32_t id = _manager->getSegmentCount() - 1;
    openknx.logger.logWithValues("Segment [%d] created: LEDs %d-%d on VStrip [%d]",
                                 id, startLed, endLed, virtId);
    return true;
}

/**
 * @brief Process 'neo seg del <id>' command
 */
bool NeoPixel::processSegDelCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse segment ID
    int segId = atoi(args.c_str());
    auto seg = _manager->getSegment(segId);

    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // Remove segment
    if (!_manager->removeSegment(segId))
    {
        openknx.logger.logWithValues("ERROR: Failed to remove segment [%d]!", segId);
        return true;
    }

    openknx.logger.logWithValues("Segment [%d] removed", segId);

    return true;
}

/**
 * @brief Process 'neo seg pause <id>' command
 */
bool NeoPixel::processSegPauseCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse segment ID
    int segId = atoi(args.c_str());
    auto seg = _manager->getSegment(segId);

    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    seg->pause();
    openknx.logger.logWithValues("Segment [%d] paused (effect frozen)", segId);

    return true;
}

/**
 * @brief Process 'neo seg resume <id>' command
 */
bool NeoPixel::processSegResumeCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse segment ID
    int segId = atoi(args.c_str());
    auto seg = _manager->getSegment(segId);

    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    seg->resume();
    openknx.logger.logWithValues("Segment [%d] resumed", segId);

    return true;
}

/**
 * @brief Process 'neo seg stop <id>' command
 */
bool NeoPixel::processSegStopCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse segment ID
    int segId = atoi(args.c_str());
    auto seg = _manager->getSegment(segId);

    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    seg->stop();
    openknx.logger.logWithValues("Segment [%d] stopped (paused + cleared)", segId);

    return true;
}

/**
 * @brief Process 'neo seg clear effect <id>' command
 */
bool NeoPixel::processSegClearEffectCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }
    // Parse segment ID
    int segId = atoi(args.c_str());
    auto seg = _manager->getSegment(segId);

    if (segId < 0)
    {
        openknx.logger.log("ERROR: Segment ID must be set!");
        return true;
    }

    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    if (!seg->hasEffect())
    {
        openknx.logger.logWithValues("Segment [%d] has no effect assigned!", segId);
        return true;
    }

    seg->stop();
    seg->clearEffect();
    openknx.logger.logWithValues("Segment [%d] effect cleared", segId);

    return true;
}

// ============================================================================
// Effect Management Commands
// ============================================================================
/**
 * @brief Process 'neo effects' command - List all available effects
 */
bool NeoPixel::processEffectsCommand()
{
    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Available Effects");
    printSectionSeparator();
    openknx.logger.color(0);
    openknx.logger.log("ID │ Name                      │ Description");
    openknx.logger.log("───┼───────────────────────────┼──────────────────────────────────────────");

    // Dynamically list all effects from EffectPool
    uint8_t effectCount = EffectPool::getEffectCount();
    for (uint8_t i = 0; i < effectCount; i++)
    {
        Effect* effect = EffectPool::getEffectByIndex(i);
        if (effect)
        {
            char line[120];
            snprintf(line, sizeof(line), "%2d │ %-25s │ %s",
                     i,
                     effect->getName(),
                     effect->getDescription());
            openknx.logger.log(line);
        }
    }

    printSectionSeparator();
    openknx.logger.log("");
    openknx.logger.log("Use 'neo effect set <seg> <id_or_name>' to assign (e.g. 'neo effect set 0 rainbow')");
    openknx.logger.log("Use 'neo garage <seg> <phase>' to control GarageDoor phases");
    openknx.logger.log("  Phases: 0=OPENING, 1=RUNWAY, 2=COMPLETED, 3=STOPPED");
    openknx.logger.log("FastLED effects are battle-tested patterns!");
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo effect config <seg> [get/set <idx> [val]]' command
 */
bool NeoPixel::processEffectConfigCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel not initialized!");
        return true;
    }

    int segId;
    char cmd[8] = "";
    int paramIdx;

    // Parse: <seg> or <seg> get <idx> or <seg> set <idx>
    // Note: We don't parse the value here because it might be a string with spaces
    int parsed = sscanf(args.c_str(), "%d %7s %d", &segId, cmd, &paramIdx);

    if (parsed < 1)
    {
        openknx.logger.log("Usage: neo effect config <seg> [get/set <idx> [val]]");
        return true;
    }

    auto seg = _manager->getSegment(segId);
    if (!seg || !seg->hasEffect())
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] has no effect!", segId);
        return true;
    }

    Effect* effect = seg->getEffect();
    uint8_t count = effect->getParameterCount();

    // Show all parameters
    if (parsed == 1)
    {
        openknx.logger.logWithValues("Effect: %s (Segment %d)", effect->getName(), segId);
        openknx.logger.log("Parameters:");
        for (uint8_t i = 0; i < count; i++)
        {
            const char* name = effect->getParameterName(i);
            ParameterType type = effect->getParameterType(i);
            uint32_t val = effect->getParameter(seg, i);
            uint32_t def = effect->getParameterDefault(i);

            if (type == ParameterType::PARAM_STRING)
            {
                // For string parameters, show the actual text
                const char* strVal = reinterpret_cast<const char*>(val);
                openknx.logger.logWithValues("  [%d] %-15s = \"%s\" (default: %u)", i, name, strVal, def);
            }
            else
            {
                openknx.logger.logWithValues("  [%d] %-15s = %u (default: %u)", i, name, val, def);
            }
        }
        return true;
    }

    // Get single parameter
    if (strcmp(cmd, "get") == 0 && parsed >= 3)
    {
        if (paramIdx < 0 || paramIdx >= count)
        {
            openknx.logger.logWithValues("ERROR: Index %d out of range (0-%d)", paramIdx, count - 1);
            return true;
        }
        ParameterType type = effect->getParameterType(paramIdx);
        uint32_t val = effect->getParameter(seg, paramIdx);

        if (type == ParameterType::PARAM_STRING)
        {
            const char* strVal = reinterpret_cast<const char*>(val);
            openknx.logger.logWithValues("%s.%s = \"%s\"",
                                         effect->getName(),
                                         effect->getParameterName(paramIdx),
                                         strVal);
        }
        else
        {
            openknx.logger.logWithValues("%s.%s = %u",
                                         effect->getName(),
                                         effect->getParameterName(paramIdx),
                                         val);
        }
        return true;
    }

    // Set parameter - handle strings specially
    if (strcmp(cmd, "set") == 0)
    {
        if (paramIdx < 0 || paramIdx >= count)
        {
            openknx.logger.logWithValues("ERROR: Index %d out of range (0-%d)", paramIdx, count - 1);
            return true;
        }

        ParameterType type = effect->getParameterType(paramIdx);

        // Find where the value starts after "set <idx> "
        size_t pos = 0;
        // Skip segment ID
        while (pos < args.size() && args[pos] != ' ') pos++;
        while (pos < args.size() && args[pos] == ' ') pos++;
        // Skip "set"
        while (pos < args.size() && args[pos] != ' ') pos++;
        while (pos < args.size() && args[pos] == ' ') pos++;
        // Skip parameter index
        while (pos < args.size() && args[pos] != ' ') pos++;
        while (pos < args.size() && args[pos] == ' ') pos++;

        const char* valueStart = &args[pos];

        if (type == ParameterType::PARAM_STRING)
        {
            if (*valueStart == '\"')
            {
                // Extract quoted string
                const char* endQuote = strchr(valueStart + 1, '\"');
                if (endQuote)
                {
                    size_t len = endQuote - valueStart - 1;
                    if (len > 63) len = 63; // Safety limit for ScrollTextEffect

                    // Stack buffer: len is already capped at 63 above, so 64 bytes
                    // always fit. setParameter() copies the text into the segment's
                    // own buffer (e.g. ScrollTextEffect -> cfg.effectText), so no
                    // heap allocation is needed here (previously leaked one block
                    // per command since the pointer was never freed).
                    char strVal[64];
                    strncpy(strVal, valueStart + 1, len);
                    strVal[len] = '\0';

                    effect->setParameter(seg, paramIdx, reinterpret_cast<uint32_t>(strVal));
                    openknx.logger.logWithValues("Set %s.%s = \"%s\"",
                                                 effect->getName(),
                                                 effect->getParameterName(paramIdx),
                                                 strVal);
                }
                else
                {
                    openknx.logger.log("ERROR: Invalid string format. Use \"text\"");
                }
            }
            else
            {
                openknx.logger.log("ERROR: String parameter requires quotes. Use \"text\"");
            }
        }
        else
        {
            // Parse numeric value - need to parse again since we skipped it earlier
            uint32_t numValue;
            if (sscanf(valueStart, "%u", &numValue) == 1)
            {
                effect->setParameter(seg, paramIdx, numValue);
                openknx.logger.logWithValues("Set %s.%s = %u",
                                             effect->getName(),
                                             effect->getParameterName(paramIdx),
                                             numValue);
            }
            else
            {
                openknx.logger.log("ERROR: Invalid numeric value");
            }
        }

        return true;
    }

    openknx.logger.log("Usage: neo effect config <seg> [get/set <idx> [val]]");
    return true;
}

/**
 * @brief Process 'neo effect <str_action> <seg> <eff>' command
 * Assign or control effects on a specific segment
 * str_action values: set, stop, clear, pause, resume
 *     set: assign effect to segment (by numeric ID or case-insensitive name)
 *     stop: stop effect (pauses and clears segment)
 *     clear: remove effect from segment
 *     pause: pause effect (freezes current state)
 *     resume: resume paused effect
 * seg: segment ID
 * eff: effect ID (numeric) or effect name (case-insensitive)
 */
bool NeoPixel::processEffectCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <str_action> <seg_id> [<effect_id_or_name>]
    // str_action values: set, stop, clear, pause, resume
    std::string action;
    int segId, effId;

    // Check if help requested or no arguments provided
    if (args.empty() || args.compare("?") == 0)
    {
        openknx.logger.log("ERROR: Usage: neo effect set <seg_id> <effect_id_or_name>");
        openknx.logger.log("Use 'neo effects' to see available effects");
        openknx.logger.log("");
        return true;
    }

    // action and segId are mandatory
    char _action[7] = "";
    if (sscanf(args.c_str(), "%6s %d", _action, &segId) != 2)
    {
        openknx.logger.log("ERROR! Action and Segment ID must be provided!");
        return true;
    }
    action = std::string(_action);

    if (action.compare("set") == 0)
    {
        // Try to parse the third argument as a numeric effect ID first
        char _effArg[64] = "";
        if (sscanf(args.c_str(), "%*s %*d %63s", _effArg) != 1)
        {
            openknx.logger.log("ERROR! Action 'set' requires Segment ID and Effect ID or Name!");
            return true;
        }

        // Check if it is a pure integer
        char* endPtr = nullptr;
        long parsedId = strtol(_effArg, &endPtr, 10);
        Effect* effect = nullptr;
        if (endPtr != _effArg && *endPtr == '\0')
        {
            // Numeric ID
            effId = (int)parsedId;
            effect = EffectPool::getEffectByIndex(effId);
        }
        else
        {
            // Name lookup (case-insensitive)
            std::string effNameLower = std::string(_effArg);
            for (size_t i = 0; i < effNameLower.size(); i++)
                effNameLower[i] = (char)tolower((unsigned char)effNameLower[i]);
            const uint8_t count = EffectPool::getEffectCount();
            for (uint8_t i = 0; i < count; i++)
            {
                Effect* candidate = EffectPool::getEffectByIndex(i);
                if (!candidate) continue;
                std::string candidateName = std::string(candidate->getName());
                for (size_t j = 0; j < candidateName.size(); j++)
                    candidateName[j] = (char)tolower((unsigned char)candidateName[j]);
                if (candidateName.compare(effNameLower) == 0)
                {
                    effect = candidate;
                    break;
                }
            }
        }

        if (!effect)
        {
            openknx.logger.log("ERROR: Effect '" + std::string(_effArg) + "' not found!");
            openknx.logger.log("       Use 'neo effects' to see available effects");
            return true;
        }

        auto seg = _manager->getSegment(segId);
        if (!seg)
        {
            openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
            return true;
        }

        // Assign effect to segment, replacing existing one if necessary.
        if (seg->hasEffect())
        {
            openknx.logger.logWithValues("Effect: '%s' is replaced with '%s' to segment [%d]",
                                         seg->getEffect()->getName(),
                                         effect->getName(),
                                         segId);
            seg->stop();        // Stop current effect
            seg->clearEffect(); // Clear current effect
        }
        else
        {
            openknx.logger.logWithValues("Effect: '%s' is assigned to segment [%d]",
                                         effect->getName(),
                                         segId);
        }
        seg->setEffect(effect, true); // Assign new effect and reset parameters
        seg->resume();                // Start effect after assignment
        return true;
    }

    auto seg = _manager->getSegment(segId);
    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // we need to check here, if set is the action
    // if not, we can ignore effId
    const auto hasEffect = seg->hasEffect();
    if (action.compare("clear") == 0)
    {
        if (!hasEffect)
        {
            openknx.logger.logWithValues("Segment [%d] has no effect assigned!", segId);
            return true;
        }
        const auto _effect_name = seg->getEffect()->getName();
        seg->stop();
        seg->clearEffect();
        openknx.logger.logWithValues("Segment [%d] effect '%s' cleared", segId, _effect_name);
    }
    else if (action.compare("stop") == 0)
    {
        if (!hasEffect)
        {
            openknx.logger.logWithValues("Segment [%d] has no effect assigned!", segId);
            return true;
        }
        seg->stop();
        const auto _effect_name = seg->getEffect()->getName();
        openknx.logger.logWithValues("Segment [%d] effect '%s' stopped", segId, _effect_name);
    }
    else if (action.compare("pause") == 0)
    {
        if (!hasEffect)
        {
            openknx.logger.logWithValues("Segment [%d] has no effect assigned!", segId);
            return true;
        }
        seg->pause();
        const auto _effect_name = seg->getEffect()->getName();
        openknx.logger.logWithValues("Segment [%d] effect '%s' paused", segId, _effect_name);
    }
    else if (action.compare("resume") == 0)
    {
        if (!hasEffect)
        {
            openknx.logger.logWithValues("Segment [%d] has no effect assigned!", segId);
            return true;
        }
        seg->resume();
        const auto _effect_name = seg->getEffect()->getName();
        openknx.logger.logWithValues("Segment [%d] effect '%s' resumed", segId, _effect_name);
    }
    else
    {
        openknx.logger.log("ERROR: Unknown action! Provided action: " + action);
        openknx.logger.log("       Valid actions: set, stop, clear, pause, resume");
        return true;
    }

    return true;
}

/**
 * @brief Process 'neo garage <seg> <phase>' command
 * Control GarageDoorEffect phase for a specific segment
 */
bool NeoPixel::processGarageCommand(const std::string& args)
{
    (void)args;
    // GarageDoor effect was removed during effect consolidation. Its behaviour is
    // covered by Cylon (center mode), Theater Chase and Breathing, and by the
    // Effektmanager Sequencer.
    openknx.logger.log("INFO: GarageDoor effect has been removed.");
    openknx.logger.log("      Use Cylon (center mode), Theater Chase or the Effektmanager instead.");
    return true;
}

/**
 * @brief Process 'neo color <seg> <r> <g> <b> [w]' command
 * White channel is optional for RGBW strips (SK6812)
 */
bool NeoPixel::processColorCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <seg_id> <r> <g> <b> [w]
    int segId, r, g, b, w = 0;
    int parsed = sscanf(args.c_str(), "%d %d %d %d %d", &segId, &r, &g, &b, &w);

    if (parsed < 4)
    {
        openknx.logger.log("ERROR: Usage: neo color <seg_id> <r> <g> <b> [w]");
        openknx.logger.log("       Values: 0-255 for each color component");
        openknx.logger.log("       w is optional for RGBW strips (SK6812)");
        return true;
    }
#ifdef OPENKNX_DEBUG
    if (parsed == 5)
    {

        openknx.logger.logWithValues("DEBUG: Parsed %d args: seg=%d r=%d g=%d b=%d w=%d",
                                     parsed, segId, r, g, b, w);
    }
    else
    {
        openknx.logger.logWithValues("DEBUG: Parsed %d args: seg=%d r=%d g=%d b=%d",
                                     parsed, segId, r, g, b);
    }
#endif

    // Validate color values
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || w < 0 || w > 255)
    {
        openknx.logger.log("ERROR: Color values must be 0-255");
        return true;
    }

    // Get segment
    auto seg = _manager->getSegment(segId);
    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // Store color in RGBW format: 0xRRGGBBWW
    // VirtualStrip stores in RGB/RGBW format, PhysicalStrip handles ColorOrder conversion
    // User inputs logical R G B [W], we store as: (R << 24) | (G << 16) | (B << 8) | W
    // For APA102 (SPI), W is brightness (0-255). If not specified, use 255 (max brightness)
    auto& config = seg->getConfig();

    // If no white/brightness specified, default to max brightness (255) for APA102
    if (parsed == 4)
    {
        w = 255; // APA102 max brightness (will be scaled to 5-bit: 0-31)
        // Check if the underlying strip is APA102/SK9822
        auto vstrip = seg->getVirtualStrip();
        if (vstrip)
        {
            auto pstrip = vstrip->getPhysicalStrip(0); // Check first physical strip
            if (pstrip)
            {
                if (pstrip->getProtocol() == LedProtocol::APA102 ||
                    pstrip->getProtocol() == LedProtocol::SK9822)
                {
#ifdef OPENKNX_DEBUG
                    openknx.logger.logWithValues("DEBUG: No W specified, defaulting to W=%d for APA102/SK9822", w);
#endif
                }
            }
        }
    }

    config.primaryRGBW = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)w;

    if (parsed == 5)
    {
        openknx.logger.logWithValues("Segment [%d] color set to R=%d G=%d B=%d W=%d",
                                     segId, r, g, b, w);
    }
    else
    {
        openknx.logger.logWithValues("Segment [%d] color set to R=%d G=%d B=%d",
                                     segId, r, g, b);
    }
    return true;
}

/**
 * @brief Process software brightness command: neo brightness <seg> <val>
 * Sets software brightness (RGB multiplication) - works for ALL LED types
 */
bool NeoPixel::processBrightnessCommand(const std::string& args)
{
    if (!_manager)
    {
        openknx.logger.log("ERROR: NeoPixel manager not initialized");
        return true;
    }

    // Parse arguments
    int segId = -1;
    int brightness = -1;

    if (sscanf(args.c_str(), "%d %d", &segId, &brightness) != 2)
    {
        openknx.logger.log("Usage: neo brightness <segment_id> <brightness>");
        openknx.logger.log("       segment_id: ID of the segment");
        openknx.logger.log("       brightness: 0-255 (0=off, 128=half, 255=max)");
        openknx.logger.log("");
        openknx.logger.log("Software brightness: RGB multiplication (all LED types)");
        openknx.logger.log("  - Works for: WS2812B, APA102, SK6812, etc.");
        openknx.logger.log("  - Applied in effect calculation");
        openknx.logger.log("  - May reduce color depth at low values");
        return true;
    }

    // Validate brightness
    if (brightness < 0 || brightness > 255)
    {
        openknx.logger.log("ERROR: Brightness must be 0-255");
        return true;
    }

    // Get segment
    auto seg = _manager->getSegment(segId);
    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // Set software brightness. This is a USER-intent change, so also update the master:
    // an EM cue then renders at master*cue/255 and won't reset this on the next cue switch.
    seg->setMasterBrightness((uint8_t)brightness);
    seg->setBrightness((uint8_t)brightness);

    openknx.logger.logWithValues("Segment [%d] SOFTWARE brightness set to %d (%.1f%%)",
                                 segId, brightness, (brightness * 100.0f / 255.0f));
    openknx.logger.log("  Applied via RGB multiplication (all LED types)");

    return true;
}

/**
 * @brief Process hardware brightness command: neo hwbrightness <seg> <val>
 * Sets hardware brightness (APA102/SK9822 global brightness) - only for SPI LEDs
 */
bool NeoPixel::processHardwareBrightnessCommand(const std::string& args)
{
    if (!_manager)
    {
        openknx.logger.log("ERROR: NeoPixel manager not initialized");
        return true;
    }

    // Parse arguments
    int segId = -1;
    int brightness = -1;

    if (sscanf(args.c_str(), "%d %d", &segId, &brightness) != 2)
    {
        openknx.logger.log("Usage: neo hwbrightness <segment_id> <brightness>");
        openknx.logger.log("       segment_id: ID of the segment");
        openknx.logger.log("       brightness: 0-255 (0=off, 128=half, 255=max)");
        openknx.logger.log("");
        openknx.logger.log("Hardware brightness: Global brightness (APA102/SK9822 only)");
        openknx.logger.log("  - Works ONLY for: APA102, SK9822 (SPI protocols)");
        openknx.logger.log("  - Silently ignored for: WS2812B, SK6812, etc.");
        openknx.logger.log("  - Preserves full 8-bit RGB color depth");
        openknx.logger.log("  - Uses hardware PWM (5-bit: 0-31)");
        return true;
    }

    // Validate brightness
    if (brightness < 0 || brightness > 255)
    {
        openknx.logger.log("ERROR: Brightness must be 0-255");
        return true;
    }

    // Get segment
    auto seg = _manager->getSegment(segId);
    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // Set hardware brightness
    seg->setHardwareBrightness((uint8_t)brightness);

    // Trigger update to apply brightness change immediately
    _manager->updateAll();

    openknx.logger.logWithValues("Segment [%d] HARDWARE brightness set to %d (%.1f%%, 5-bit: %d/31)",
                                 segId, brightness, (brightness * 100.0f / 255.0f), brightness >> 3);
    openknx.logger.log("  Only effective for APA102/SK9822 (ignored for WS2812B, SK6812)");

    return true;
}

// ============================================================================
// Power Management Commands
// ============================================================================
/**
 * @brief Process power management commands
 * @param args Command arguments (after "neo power ")
 * @return true if command was processed
 */
bool NeoPixel::processPowerCommand(const std::string& args)
{
    openknx.logger.begin();

    // neo power (show global + all phys status)
    if (args.empty() || args == "status")
    {
        PowerManager* pm = _manager->getPowerManager();
        if (!pm)
        {
            openknx.logger.log("[ERROR] PowerManager not initialized!");
            openknx.logger.end();
            return false;
        }

        openknx.logger.log("");
        printHelpSectionHeader("Power Management - Global Status");

        // Current limiting status
        openknx.logger.logWithValues("Status:           %s", pm->isEnabled() ? "ENABLED" : "DISABLED");

        // Show active power mode (GLOBAL, PER_CHANNEL, PER_LED)
        const char* modeNames[] = {"GLOBAL", "PER_CHANNEL", "PER_LED"};
        PowerLimitMode mode = pm->getPowerLimitMode();
        openknx.logger.logWithValues("Power Mode:       %s", modeNames[(int)mode]);

        // Show mode-specific configuration
        switch (mode)
        {
            case PowerLimitMode::GLOBAL:
                openknx.logger.logWithValues("  Configured:     %u mA global limit", pm->getMaxCurrent());
                break;
            case PowerLimitMode::PER_CHANNEL:
                openknx.logger.logWithValues("  Configured:     %u mA per channel", pm->getMaxCurrentPerChannel());
                break;
            case PowerLimitMode::PER_LED:
                openknx.logger.logWithValues("  Configured:     %u mA per LED", pm->getMaxCurrentPerLed());
                break;
        }

        // ABL configuration
        openknx.logger.logWithValues("ABL Threshold:    %u%% (dimming starts here)", pm->getThresholdPercent());
        openknx.logger.logWithValues("ABL Min Bright:   %u%% (minimum at limit)", pm->getMaxBrightnessPercent());

        // Get cached global power statistics (updated by applyPowerLimit())
        uint32_t globalCurrentMa, globalLimitMa;
        uint8_t globalLoadPercent;
        _manager->getGlobalPowerStats(globalCurrentMa, globalLimitMa, globalLoadPercent);

        openknx.logger.logWithValues("Global Limit:     %u mA (%.2f A)", globalLimitMa, globalLimitMa / 1000.0f);
        openknx.logger.logWithValues("Global Current:   %u mA (%.2f A)", globalCurrentMa, globalCurrentMa / 1000.0f);
        openknx.logger.logWithValues("Global Load:      %u%%", globalLoadPercent);

        // LED profile
        const char* profileName = "CUSTOM";
        LedCurrentProfile profile = pm->getLedProfile();
        if (profile == LedProfiles::WS2812B) profileName = "WS2812B";
        else if (profile == LedProfiles::SK6812_RGBW)
            profileName = "SK6812 RGBW";
        else if (profile == LedProfiles::SK6812_RGBCCT)
            profileName = "SK6812 RGBCCT";
        else if (profile == LedProfiles::APA102)
            profileName = "APA102";
        else if (profile == LedProfiles::CONSERVATIVE)
            profileName = "CONSERVATIVE";
        else if (profile == LedProfiles::CONSERVATIVE_5CH)
            profileName = "CONSERVATIVE 5CH";

        openknx.logger.logWithValues("LED Profile:      %s (R:%umA G:%umA B:%umA WW:%umA CW:%umA)",
                                     profileName, profile.redMA, profile.greenMA,
                                     profile.blueMA, profile.warmWhiteMA, profile.coolWhiteMA);

        // Calculate total power from cached current values
        float totalPowerWatts = _manager->getTotalPowerWatts();
        openknx.logger.logWithValues("Total Power:      %.2f W (all strips)", totalPowerWatts);

        // Show if limiting is active
        if (pm->isEnabled() && globalCurrentMa > globalLimitMa * 0.95f)
        {
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.logWithValues("WARNING: APPROACHING/AT LIMIT - Current Load: %u%%", globalLoadPercent);
            openknx.logger.color(0);
        }

        printSectionSeparator();

        // Show all physical strips
        uint32_t stripCount = _manager->getStripCount();
        if (stripCount > 0)
        {
            openknx.logger.log("");
            printHelpSectionHeader("Physical Strips Power Status");

            for (uint32_t i = 0; i < stripCount; i++)
            {
                PhysicalStrip* phys = _manager->getStrip(i);
                if (!phys) continue;

                PhysicalStripConfig* cfg = phys->getConfig();
                if (!cfg) continue;

                // Get cached power stats (updated by applyPowerLimit())
                uint32_t stripCurrentMa, stripLimitMa;
                uint8_t stripLoadPercent;
                bool hasStats = _manager->getStripPowerStats(phys, stripCurrentMa, stripLimitMa, stripLoadPercent);

                // Fallback: Calculate if not cached (e.g., strip with mode=Disabled)
                if (!hasStats)
                {
                    stripCurrentMa = pm->calculateStripCurrent(
                        phys->getBuffer(),
                        phys->getLedCount(),
                        phys->getProtocol(),
                        phys->getColorOrder(),
                        true); // hasDummyLed = true for APA102
                    stripLimitMa = cfg->getMaxCurrentMa();
                    stripLoadPercent = stripLimitMa > 0 ? (uint8_t)((stripCurrentMa * 100) / stripLimitMa) : 0;
                }

                // Get power mode name
                const char* modeName = "Unknown";
                uint8_t mode = cfg->getPowerLimitMode();
                switch (mode)
                {
                    case 0: modeName = "Disabled"; break;
                    case 1: modeName = "UseGlobal"; break;
                    case 2: modeName = "FixedValue"; break;
                    case 3: modeName = "PerLED"; break;
                }

                // Calculate power in watts
                uint8_t voltage = phys->getVoltage();
                float powerWatts = phys->getPowerWatts(stripCurrentMa);

                // Get protocol name
                LedProtocol protocol = phys->getProtocol();
                const char* protocolName = "Unknown";
                switch (protocol)
                {
                    case LedProtocol::WS2812:
                    case LedProtocol::WS2812B: protocolName = "WS2812B"; break;
                    case LedProtocol::WS2813: protocolName = "WS2813"; break;
                    case LedProtocol::SK6812: protocolName = "SK6812"; break;
                    case LedProtocol::APA102: protocolName = "APA102"; break;
                    case LedProtocol::APA102_CLONE: protocolName = "APA102_CLONE"; break;
                    case LedProtocol::SK9822: protocolName = "SK9822"; break;
                    case LedProtocol::WS2801: protocolName = "WS2801"; break;
                    case LedProtocol::LPD8806: protocolName = "LPD8806"; break;
                    default: break;
                }

                openknx.logger.logWithValues("Strip %u: Protocol=%-12s Pin=%2u LEDs=%3u %2uV Mode=%-10s Limit=%4u mA Current=%4u mA Load=%3u%% (%5.2f W)",
                                             i, protocolName, phys->getDataPin(), phys->getLedCount(), voltage, modeName,
                                             stripLimitMa, stripCurrentMa, stripLoadPercent, powerWatts);
            }

            printSectionSeparator();
        }

        openknx.logger.log("");
        openknx.logger.end();
        return true;
    }

    // neo power <n> (show specific strip status or set strip params)
    // Check if args starts with a digit (strip index)
    if (!args.empty() && isdigit(args[0]))
    {
        // Extract strip index
        size_t spacePos = args.find(' ');
        std::string indexStr = (spacePos == std::string::npos)
                                   ? args
                                   : args.substr(0, spacePos);

        uint32_t stripIndex = atoi(indexStr.c_str());
        PhysicalStrip* phys = _manager->getStrip(stripIndex);

        if (!phys)
        {
            openknx.logger.logWithValues("[ERROR] Physical strip %u not found!", stripIndex);
            openknx.logger.end();
            return false;
        }

        // Check for subcommands
        std::string subCmd = (spacePos == std::string::npos)
                                 ? ""
                                 : args.substr(spacePos + 1);

        // neo power phys_<n> limit <mA>
        if (subCmd.compare(0, 6, "limit ") == 0)
        {
            uint32_t limit = atoi(subCmd.substr(6).c_str());
            if (limit < 100 || limit > 100000)
            {
                openknx.logger.log("[ERROR] Invalid current limit (100-100000 mA)");
                openknx.logger.end();
                return false;
            }

            PhysicalStripConfig* cfg = phys->getConfig();
            if (cfg)
            {
                cfg->setMaxCurrentMa(limit);
                openknx.logger.logWithValues("Strip %u: Power limit set to %u mA", stripIndex, limit);
            }
            openknx.logger.end();
            return true;
        }

        // neo power phys_<n> mode <0|1|2|3>
        if (subCmd.compare(0, 5, "mode ") == 0)
        {
            uint8_t mode = atoi(subCmd.substr(5).c_str());
            if (mode > 3)
            {
                openknx.logger.log("[ERROR] Invalid mode (0=Disabled, 1=UseGlobal, 2=FixedValue, 3=PerLED)");
                openknx.logger.end();
                return false;
            }

            PhysicalStripConfig* cfg = phys->getConfig();
            if (cfg)
            {
                cfg->setPowerLimitMode(mode);
                const char* modeName[] = {"Disabled", "UseGlobal", "FixedValue", "PerLED"};
                openknx.logger.logWithValues("Strip %u: Power mode set to %s", stripIndex, modeName[mode]);
            }
            openknx.logger.end();
            return true;
        }

        // neo power phys_<n> (show strip status)
        if (subCmd.empty())
        {
            PowerManager* pm = _manager->getPowerManager();
            if (!pm)
            {
                openknx.logger.log("[ERROR] PowerManager not initialized!");
                openknx.logger.end();
                return false;
            }

            PhysicalStripConfig* cfg = phys->getConfig();
            if (!cfg)
            {
                openknx.logger.log("[ERROR] Strip configuration not found!");
                openknx.logger.end();
                return false;
            }

            openknx.logger.log("");
            openknx.logger.logWithValues("Physical Strip %u Status", stripIndex);
            printSectionSeparator();

            openknx.logger.logWithValues("Pin:              %u", phys->getDataPin());
            openknx.logger.logWithValues("LED Count:        %u", phys->getLedCount());

            const char* modeName = "Unknown";
            uint8_t mode = cfg->getPowerLimitMode();
            switch (mode)
            {
                case 0: modeName = "Disabled"; break;
                case 1: modeName = "UseGlobal"; break;
                case 2: modeName = "FixedValue"; break;
                case 3: modeName = "PerLED"; break;
            }
            openknx.logger.logWithValues("Power Mode:       %s (%u)", modeName, mode);
            openknx.logger.logWithValues("Max Current:      %u mA", cfg->getMaxCurrentMa());

            // Calculate current for this strip (universal function handles all protocols)
            uint32_t stripCurrent = pm->calculateStripCurrent(
                phys->getBuffer(),
                phys->getLedCount(),
                phys->getProtocol(),
                phys->getColorOrder(),
                true); // hasDummyLed for APA102

            openknx.logger.logWithValues("Current Draw:     %u mA (%.2f W @ 5V)",
                                         stripCurrent, stripCurrent * 5.0f / 1000.0f);

            printSectionSeparator();
            openknx.logger.log("");
            openknx.logger.end();
            return true;
        }
    }

    // neo power g|global <subcommand>
    if (args.compare(0, 7, "global ") == 0 || args.compare(0, 2, "g ") == 0)
    {
        std::string globalCmd = (args[0] == 'g' && args[1] == ' ') ? args.substr(2) : args.substr(7);

        // neo power global limit <mA>
        if (globalCmd.compare(0, 6, "limit ") == 0)
        {
            uint32_t limit = atoi(globalCmd.substr(6).c_str());
            if (limit < 100 || limit > 100000)
            {
                openknx.logger.log("[ERROR] Invalid current limit (100-100000 mA)");
                openknx.logger.end();
                return false;
            }

            _manager->setMaxCurrent(limit);
            openknx.logger.logWithValues("Global power limit set to %u mA (%.2f A)", limit, limit / 1000.0f);
            openknx.logger.end();
            return true;
        }

        // neo power global profile <type>
        if (globalCmd.compare(0, 8, "profile ") == 0)
        {
            std::string profileStr = globalCmd.substr(8);
            PowerManager* pm = _manager->getPowerManager();
            if (!pm)
            {
                openknx.logger.log("[ERROR] PowerManager not initialized!");
                openknx.logger.end();
                return false;
            }

            LedCurrentProfile profile;
            bool found = false;

            if (profileStr == "ws2812b")
            {
                profile = LedProfiles::WS2812B;
                found = true;
            }
            else if (profileStr == "sk6812" || profileStr == "sk6812_rgbw")
            {
                profile = LedProfiles::SK6812_RGBW;
                found = true;
            }
            else if (profileStr == "sk6812_rgbcct")
            {
                profile = LedProfiles::SK6812_RGBCCT;
                found = true;
            }
            else if (profileStr == "ws2805_rgbcct" || profileStr == "ws2814_rgbcct")
            {
                profile = LedProfiles::SK6812_RGBCCT;
                found = true;
            }
            else if (profileStr == "apa102")
            {
                profile = LedProfiles::APA102;
                found = true;
            }
            else if (profileStr == "conservative")
            {
                profile = LedProfiles::CONSERVATIVE;
                found = true;
            }
            else if (profileStr == "conservative_5ch")
            {
                profile = LedProfiles::CONSERVATIVE_5CH;
                found = true;
            }

            if (!found)
            {
                openknx.logger.log("[ERROR] Unknown profile. Use: ws2812b|sk6812|sk6812_rgbcct|apa102|conservative|conservative_5ch");
                openknx.logger.end();
                return false;
            }

            pm->setLedProfile(profile);
            openknx.logger.logWithValues("Global LED profile set to %s", profileStr.c_str());
            openknx.logger.end();
            return true;
        }

        // neo power global on/off
        if (globalCmd == "on")
        {
            _manager->setPowerManagementEnabled(true);
            openknx.logger.log("Global power management ENABLED");
            openknx.logger.end();
            return true;
        }
        if (globalCmd == "off")
        {
            _manager->setPowerManagementEnabled(false);
            openknx.logger.log("Global power management DISABLED");
            openknx.logger.end();
            return true;
        }
    }

    // Legacy compatibility: neo power limit/profile/on/off (maps to global)
    // neo power limit <mA>
    else if (args.compare(0, 6, "limit ") == 0)
    {
        uint32_t limit = atoi(args.substr(6).c_str());
        if (limit < 100 || limit > 100000)
        {
            openknx.logger.log("[ERROR] Invalid current limit (100-100000 mA)");
            openknx.logger.end();
            return false;
        }

        _manager->setMaxCurrent(limit);
        openknx.logger.logWithValues("Power limit set to %u mA (%.2f A)", limit, limit / 1000.0f);
        openknx.logger.end();
        return true;
    }

    // neo power profile <type>
    else if (args.compare(0, 8, "profile ") == 0)
    {
        std::string profileStr = args.substr(8);
        PowerManager* pm = _manager->getPowerManager();
        if (!pm)
        {
            openknx.logger.log("[ERROR] PowerManager not initialized!");
            openknx.logger.end();
            return false;
        }

        LedCurrentProfile profile;
        bool found = false;

        if (profileStr == "ws2812b")
        {
            profile = LedProfiles::WS2812B;
            found = true;
        }
        else if (profileStr == "sk6812" || profileStr == "sk6812_rgbw")
        {
            profile = LedProfiles::SK6812_RGBW;
            found = true;
        }
        else if (profileStr == "sk6812_rgbcct")
        {
            profile = LedProfiles::SK6812_RGBCCT;
            found = true;
        }
        else if (profileStr == "ws2805_rgbcct" || profileStr == "ws2814_rgbcct")
        {
            profile = LedProfiles::SK6812_RGBCCT;
            found = true;
        }
        else if (profileStr == "apa102")
        {
            profile = LedProfiles::APA102;
            found = true;
        }
        else if (profileStr == "conservative")
        {
            profile = LedProfiles::CONSERVATIVE;
            found = true;
        }
        else if (profileStr == "conservative_5ch")
        {
            profile = LedProfiles::CONSERVATIVE_5CH;
            found = true;
        }

        if (!found)
        {
            openknx.logger.log("[ERROR] Unknown profile. Use: ws2812b|sk6812|sk6812_rgbcct|apa102|conservative|conservative_5ch");
            openknx.logger.end();
            return false;
        }

        pm->setLedProfile(profile);
        openknx.logger.logWithValues("LED profile set to %s", profileStr.c_str());
        openknx.logger.end();
        return true;
    }

    // neo power on/off
    else if (args == "on")
    {
        _manager->setPowerManagementEnabled(true);
        openknx.logger.log("Power management ENABLED");
        openknx.logger.end();
        return true;
    }
    else if (args == "off")
    {
        _manager->setPowerManagementEnabled(false);
        openknx.logger.log("Power management DISABLED");
        openknx.logger.end();
        return true;
    }

    // Unknown command
    openknx.logger.log("[ERROR] Unknown power command. Use 'neo power ?' for help");
    openknx.logger.end();
    return false;
}

// ============================================================================
// Console Help Helper Functions
// ============================================================================
/**
 * @brief Print detail help header with title
 */
/**
 * @brief Print simple separator line (full width)
 */
void NeoPixel::printSectionSeparator()
{
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("═════════════════════════════════════════════════════════════════════════════");
    openknx.logger.color(0);
}

/**
 * @brief Print simple header line (for help overview sections)
 */
void NeoPixel::printHelpSectionHeader(const char* title)
{
    // Dynamic header width calculation to avoid buffer overflow
    const int totalWidth = 77; // Max console width (safe buffer size)
    int titleLen = strlen(title);
    int sidesWidth = totalWidth - titleLen - 2; // 2 spaces around title
    int leftWidth = sidesWidth / 2;
    int rightWidth = sidesWidth - leftWidth;

    // Build header with dynamic border
    char header[80];
    int pos = 0;
    for (int i = 0; i < leftWidth && pos < 79; i++)
        header[pos++] = '=';
    if (pos < 79) header[pos++] = ' ';
    for (int i = 0; i < titleLen && pos < 79; i++)
        header[pos++] = title[i];
    if (pos < 79) header[pos++] = ' ';
    for (int i = 0; i < rightWidth && pos < 79; i++)
        header[pos++] = '=';
    header[pos] = '\0';

    openknx.logger.log("");
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log(header);
    openknx.logger.color(0);
}

/**
 * @brief Print detail help header with command table
 */
void NeoPixel::printDetailHelpHeader(const char* title)
{
    openknx.logger.begin();
    printHelpSectionHeader(title);
    openknx.logger.log("Command(s)               Description");
    openknx.logger.log("─────────────────────────────────────────────────────────────────────────────");
}

/**
 * @brief Print separator line before parameter/examples section
 */
void NeoPixel::printDetailHelpSeparator()
{
    openknx.logger.log("─────────────────────────────────────────────────────────────────────────────");
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
}

/**
 * @brief Print parameter line (colored)
 */
void NeoPixel::printDetailHelpParameter(const char* paramDesc)
{
    openknx.logger.logWithValues("Parameter: %s", paramDesc);
    openknx.logger.color(0);
}

/**
 * @brief Print examples section
 */
void NeoPixel::printDetailHelpExample(const char* example)
{
    openknx.logger.log(example);
}

/**
 * @brief End detail help output
 */
void NeoPixel::printDetailHelpEnd()
{
    openknx.logger.end();
}

// ============================================================================
// Timing Helper Functions
// ============================================================================
/**
 * @brief Get human-readable name for TimingMode
 */
const char* NeoPixel::getTimingModeName(TimingMode mode)
{
    switch (mode)
    {
        case TimingMode::AUTO: return "AUTO";
        case TimingMode::AUTO_LEGACY: return "AUTO_LEGACY";
        case TimingMode::SLOW_20PCT: return "SLOW_20PCT";
        case TimingMode::SLOW_15PCT: return "SLOW_15PCT";
        case TimingMode::SLOW_10PCT: return "SLOW_10PCT";
        case TimingMode::SLOW_5PCT: return "SLOW_5PCT";
        case TimingMode::FAST_5PCT: return "FAST_5PCT";
        case TimingMode::FAST_10PCT: return "FAST_10PCT";
        case TimingMode::FAST_15PCT: return "FAST_15PCT";
        case TimingMode::FAST_20PCT: return "FAST_20PCT";
        case TimingMode::FAST_25PCT: return "FAST_25PCT";
        case TimingMode::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse TimingMode from string
 */
TimingMode NeoPixel::parseTimingMode(const char* str)
{
    if (!str) return TimingMode::AUTO;

    // Convert to lowercase for comparison
    std::string mode = str;
    for (auto& c : mode)
        c = tolower(c);

    if (mode == "auto") return TimingMode::AUTO;
    if (mode == "legacy" || mode == "auto_legacy") return TimingMode::AUTO_LEGACY;
    if (mode == "slow20" || mode == "slow_20pct") return TimingMode::SLOW_20PCT;
    if (mode == "slow15" || mode == "slow_15pct") return TimingMode::SLOW_15PCT;
    if (mode == "slow10" || mode == "slow_10pct") return TimingMode::SLOW_10PCT;
    if (mode == "slow5" || mode == "slow_5pct") return TimingMode::SLOW_5PCT;
    if (mode == "fast5" || mode == "fast_5pct") return TimingMode::FAST_5PCT;
    if (mode == "fast10" || mode == "fast_10pct") return TimingMode::FAST_10PCT;
    if (mode == "fast15" || mode == "fast_15pct") return TimingMode::FAST_15PCT;
    if (mode == "fast20" || mode == "fast_20pct") return TimingMode::FAST_20PCT;
    if (mode == "fast25" || mode == "fast_25pct") return TimingMode::FAST_25PCT;

    return TimingMode::AUTO; // Default fallback
}

// ============================================================================
// Timing Management Commands
// ============================================================================
/**
 * @brief Process 'neo phys timings' command - List all TimingMode options
 */
bool NeoPixel::processPhysTimingsCommand()
{
    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.log("  Available Timing Modes");
    printSectionSeparator();
    openknx.logger.log("");

    openknx.logger.log("Wert (0-15) = ETS 'Timing'-Dropdown. Setzen: neo phys timing <id> <Wert>");
    openknx.logger.log("");
    openknx.logger.log("Wert │ Bitrate  │ Hinweis");
    openknx.logger.log("─────┼──────────┼──────────────────────────────────────────");
    openknx.logger.log("  0  │  800 kHz │ Standard (WS2812B, SK6812, WS2813/15) - DEFAULT");
    openknx.logger.log("  1  │  960 kHz │ WS2812C/D onboard");
    openknx.logger.log("  2  │  640 kHz │ sehr schwaches Signal / lange Kette");
    openknx.logger.log("  3  │  680 kHz │");
    openknx.logger.log("  4  │  720 kHz │");
    openknx.logger.log("  5  │  760 kHz │ Clone-Tuning");
    openknx.logger.log("  6  │  840 kHz │");
    openknx.logger.log("  7  │  880 kHz │");
    openknx.logger.log("  8  │  920 kHz │");
    openknx.logger.log("  9  │  750 kHz │  Clone-Feinraster (5-kHz-Schritte 750-790):");
    openknx.logger.log(" 10  │  765 kHz │  hier findest du fuer zickige Clones");
    openknx.logger.log(" 11  │  770 kHz │  den sauberen Punkt (oft 770-780).");
    openknx.logger.log(" 12  │  775 kHz │");
    openknx.logger.log(" 13  │  780 kHz │");
    openknx.logger.log(" 14  │  785 kHz │");
    openknx.logger.log(" 15  │  790 kHz │");
    openknx.logger.log("");
    openknx.logger.log("Frei waehlbar: neo phys timing <id> freq <kHz>   (z.B. freq 783)");
    openknx.logger.log("Experten:      neo phys timing <id> custom <t0h> <t0l> <t1h> <t1l>");
    openknx.logger.log("               PIO leitet clkdiv aus T1H ab (festes 3:7:6:4-Verhaeltnis).");
    openknx.logger.log("");

    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Usage Examples:");
    openknx.logger.color(0);
    openknx.logger.log("  neo phys timing 0             -> Show current timing for strip 0");
    openknx.logger.log("  neo phys timing 0 auto        -> Set to AUTO mode");
    openknx.logger.log("  neo phys timing 0 legacy      -> Set to AUTO_LEGACY");
    openknx.logger.log("  neo phys timing 0 fast25      -> Set to FAST_25PCT (1 MHz)");
    openknx.logger.log("  neo phys timing 0 info        -> Detailed timing information");
    openknx.logger.log("  neo phys timing 0 freq 775    -> Set bitrate to 775 kHz (like the ETS Timing field)");
    openknx.logger.log("  neo phys timing 0 custom 300 900 800 600     -> Custom timing (ns): T0H T0L T1H T1L");
    openknx.logger.log("  neo phys timing 0 custom 300 900 800 600 80  -> Custom timing + reset time (µs)");
    openknx.logger.log("  neo phys timing 0 reset       -> Revert to AUTO timing");
    openknx.logger.log("  neo phys timing 0 qualify     -> Interactive clone qualify (full strip, input-driven)");
    openknx.logger.log("  neo phys timing 0 scan        -> Alias for 'qualify'");
    openknx.logger.log("  neo phys timing 0 profile <N> -> Apply a clone profile until reboot");
    openknx.logger.log("  (during qualify): neo scan next | neo scan apply | neo scan stop");
    openknx.logger.log("");
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("[EXPERT] Clone Live-Tuner — manual timing adjustment:");
    openknx.logger.color(0);
    openknx.logger.log("  NeoPixel 1-wire protocol — what T0H/T1H etc. mean:");
    openknx.logger.log("    T0H / T0L = HIGH / LOW duration for a '0' bit (ns)");
    openknx.logger.log("    T1H / T1L = HIGH / LOW duration for a '1' bit (ns)");
    openknx.logger.log("    Reset     = min. LOW pause between frames (µs)");
    openknx.logger.log("");
#ifdef ARDUINO_ARCH_RP2040
    openknx.logger.log("  Platform: RP2040/RP2350 (PIO driver)");
    openknx.logger.log("    Fixed 3:7:6:4 cycle ratio — only T1H sets the bitrate.");
    openknx.logger.log("    T0H/T0L/T1L are derived automatically.");
    openknx.logger.log("    Start here:  neo phys timing 0 tune t1h +50");
    openknx.logger.log("    Latch fix:   neo phys timing 0 tune reset 280");
#endif
#ifdef ARDUINO_ARCH_ESP32
    openknx.logger.log("  Platform: ESP32 (RMT driver)");
    openknx.logger.log("    All four values are applied independently.");
    openknx.logger.log("    Start here:  neo phys timing 0 tune t1h +50");
    openknx.logger.log("    Also try:    neo phys timing 0 tune t0h 350");
    openknx.logger.log("    Latch fix:   neo phys timing 0 tune reset 280");
#endif
    openknx.logger.log("");
    openknx.logger.log("  [EXPERT] Live-Tuner — strip ID always part of every command:");
    openknx.logger.log("    neo phys timing 0 tune           -> enter tuner on strip 0 (lights white)");
    openknx.logger.log("    neo phys timing 0 tune t1h +50   -> reduce bitrate ~8% (RP2040: start here)");
    openknx.logger.log("    neo phys timing 0 tune reset 280 -> fix strips where only 1-5 LEDs react");
    openknx.logger.log("    neo phys timing 0 tune t0h 350   -> (ESP32) adjust 0-bit high time");
    openknx.logger.log("    neo phys timing 1 tune t1h +50   -> tune strip 1 simultaneously");
    openknx.logger.log("    neo phys timing 0 tune show      -> show current values for strip 0");
    openknx.logger.log("    neo phys timing 0 tune save      -> alias for done (no flash write)");
    openknx.logger.log("    neo phys timing 0 tune done      -> keep timing, exit (no flash write)");
    openknx.logger.log("    neo phys timing 0 tune abort     -> restore original timing & exit");

    openknx.logger.log("");
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Clone Profiles (for non-responding / clone LED strips):");
    openknx.logger.color(0);
    openknx.logger.log("N │ Name           │ Payload │ Description");
    openknx.logger.log("──┼────────────────┼─────────┼─────────────────────────────────────────");
    for (uint8_t i = 0; i < kCloneProfileCount; i++)
    {
        const CloneTimingProfile& p = kCloneProfiles[i];
        openknx.logger.logWithValues("%d │ %-14s │ stress  │ %s", (int)i, p.name, p.desc);
    }

    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo phys timing <id> [mode|info]' command
 */
bool NeoPixel::processPhysTimingCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments
    int stripId;
    char modeStr[32] = "";
    int parsed = sscanf(args.c_str(), "%d %31s", &stripId, modeStr);

    if (parsed < 1)
    {
        // No/invalid strip id (e.g. "neo phys timing ?") -> print the value->kHz table + usage
        openknx.logger.log("Usage: neo phys timing <id> <0-15 | freq <kHz> | custom <t0h> <t0l> <t1h> <t1l> | info>");
        return processPhysTimingsCommand();
    }

    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Strip [%d] not found!", stripId);
        return true;
    }

    // Get current timing mode (need to add getter to PhysicalStrip!)
    auto driver = strip->getDriver();
    if (!driver)
    {
        openknx.logger.log("ERROR: Strip has no driver!");
        return true;
    }

    // neo phys timing <id> info -> Detailed information
    if (parsed == 2 && strcmp(modeStr, "info") == 0)
    {
#ifdef ARDUINO_ARCH_RP2040
        auto pioSerialDriver = dynamic_cast<PIO_NeoPixel_Serial*>(driver);
        auto pioSpiDriver = dynamic_cast<PIO_NeoPixel_SPI*>(driver);

        if (!pioSerialDriver && !pioSpiDriver)
        {
            openknx.logger.log("ERROR: Timing info only available for PIO drivers (Serial/SPI)");
            return true;
        }

        openknx.logger.log("");
        printSectionSeparator();
        openknx.logger.logWithValues("  Timing Information - Strip [%d]", stripId);
        printSectionSeparator();
        openknx.logger.log("");

        // System info
        float sys_clk = (float)clock_get_hz(clk_sys);
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("System:");
        openknx.logger.color(0);
        openknx.logger.logWithValues("  CPU Frequency:   %.1f MHz", sys_clk / 1e6f);

    #ifdef PICO_RP2350
        openknx.logger.log("  Platform:        RP2350");
    #else
        openknx.logger.log("  Platform:        RP2040");
    #endif
        openknx.logger.log("");

        // Strip info
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("Strip Configuration:");
        openknx.logger.color(0);

        if (pioSerialDriver)
        {
            openknx.logger.logWithValues("  GPIO Pin:        %d", strip->getDataPin());
        }
        else if (pioSpiDriver)
        {
            openknx.logger.logWithValues("  MOSI Pin:        %d", pioSpiDriver->getMosiPin());
            openknx.logger.logWithValues("  SCK Pin:         %d", pioSpiDriver->getClkPin());
        }

        openknx.logger.logWithValues("  LED Count:       %d", strip->getLedCount());

        LedProtocol protocol = strip->getProtocol();
        const char* protocolName = "Unknown";
        switch (protocol)
        {
            case LedProtocol::WS2812B: protocolName = "WS2812B"; break;
            case LedProtocol::SK6812: protocolName = "SK6812"; break;
            case LedProtocol::WS2812: protocolName = "WS2812"; break;
            case LedProtocol::APA102: protocolName = "APA102"; break;
            case LedProtocol::SK9822: protocolName = "SK9822"; break;
            case LedProtocol::WS2801: protocolName = "WS2801"; break;
            case LedProtocol::LPD8806: protocolName = "LPD8806"; break;
            default: break;
        }
        openknx.logger.logWithValues("  Protocol:        %s", protocolName);

        // Color order (helps spot an RGBW/GRBW mixup that swaps R/G on clones)
        const char* colorOrderName = "???";
        switch (strip->getColorOrder())
        {
            case ColorOrder::NONE: colorOrderName = "DEFAULT"; break;
            case ColorOrder::RGB: colorOrderName = "RGB"; break;
            case ColorOrder::RBG: colorOrderName = "RBG"; break;
            case ColorOrder::GRB: colorOrderName = "GRB"; break;
            case ColorOrder::BGR: colorOrderName = "BGR"; break;
            case ColorOrder::GBR: colorOrderName = "GBR"; break;
            case ColorOrder::BRG: colorOrderName = "BRG"; break;
            case ColorOrder::RGBW: colorOrderName = "RGBW"; break;
            case ColorOrder::GRBW: colorOrderName = "GRBW"; break;
            case ColorOrder::RGBCCT: colorOrderName = "RGBCCT"; break;
            case ColorOrder::GRBCCT: colorOrderName = "GRBCCT"; break;
            case ColorOrder::RGBCTW: colorOrderName = "RGBCTW"; break;
            case ColorOrder::GRBCTW: colorOrderName = "GRBCTW"; break;
        }
        openknx.logger.logWithValues("  Color Order:     %s", colorOrderName);

        if (pioSerialDriver)
        {
            // Current timing mode (only for serial drivers)
            TimingMode currentMode = strip->getTimingMode();
            const char* modeName = getTimingModeName(currentMode);
            openknx.logger.logWithValues("  Timing Mode:     %s", modeName);
        }
        openknx.logger.log("");

        // PIO details - different for Serial vs SPI
        if (pioSerialDriver)
        {
            PIO pio = pioSerialDriver->getPio();
            const char* pioName = (pio == pio0) ? "PIO0" : (pio == pio1) ? "PIO1"
                                                                         : "PIO2";

            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("PIO Timing:");
            openknx.logger.color(0);
            openknx.logger.logWithValues("  PIO Instance:    %s", pioName);
            openknx.logger.logWithValues("  State Machine:   SM%d", pioSerialDriver->getStateMachine());

            uint32_t target_freq = pioSerialDriver->getFrequency();
            float actual_bitrate = pioSerialDriver->getActualBitrate();
            float actual_clkdiv = pioSerialDriver->getActualClkdiv();

            openknx.logger.logWithValues("  Target Freq:     %.0f kHz", actual_bitrate / 1000.0f);
            openknx.logger.logWithValues("  Calc. ClkDiv:    %.3f", actual_clkdiv);
            openknx.logger.logWithValues("  Actual Bitrate:  %.0f kHz", actual_bitrate / 1000.0f);

            // Timing tolerance (compare actual to protocol target)
            float deviation = ((actual_bitrate - target_freq) / target_freq) * 100.0f;
            openknx.logger.logWithValues("  Deviation:       %.1f%%", deviation);

            if (fabs(deviation) < 1.0f)
            {
                openknx.logger.log("  Status:          Optimal");
            }
            else if (fabs(deviation) < 5.0f)
            {
                openknx.logger.log("  Status:          Acceptable");
            }
            else
            {
                openknx.logger.log("  Status:          Out of spec");
            }
        }
        else if (pioSpiDriver)
        {
            PIO pio = pioSpiDriver->getPio();
            const char* pioName = (pio == pio0) ? "PIO0" : (pio == pio1) ? "PIO1"
                                                                         : "PIO2";

            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("PIO SPI Timing:");
            openknx.logger.color(0);
            openknx.logger.logWithValues("  PIO Instance:    %s", pioName);
            openknx.logger.logWithValues("  State Machine:   SM%d", pioSpiDriver->getStateMachine());

            uint32_t spi_freq = pioSpiDriver->getSpiFrequency();
            openknx.logger.logWithValues("  SPI Frequency:   %.1f MHz", spi_freq / 1e6f);
            openknx.logger.logWithValues("  Bytes per LED:   %d", pioSpiDriver->getBytesPerLed());
            openknx.logger.logWithValues("  Buffer Size:     %d bytes", pioSpiDriver->getBufferSize());

            // Calculate approximate refresh rate
            uint32_t totalBits = pioSpiDriver->getBufferSize() * 8;
            float refreshRate = (float)spi_freq / (float)totalBits;
            openknx.logger.logWithValues("  Max Refresh:     %.0f Hz", refreshRate);

            if (pioSpiDriver->isDMAenabled())
            {
                openknx.logger.logWithValues("  DMA Channel:     %d", pioSpiDriver->getDmaChannel());
            }
            else
            {
                openknx.logger.log("  DMA:             Disabled");
            }
        }

        openknx.logger.log("");
        printSectionSeparator();
        openknx.logger.log("");
#else
        openknx.logger.log("ERROR: Timing info only available on RP2040/RP2350");
#endif
        return true;
    }

    // neo phys timing <id> -> Show current mode
    if (parsed == 1)
    {
        TimingMode currentMode = strip->getTimingMode();
        const char* modeName = getTimingModeName(currentMode);

        openknx.logger.logWithValues("Strip [%d] current timing mode:", stripId);
        openknx.logger.logWithValues("  Mode: %s (ID: %d)", modeName, (int)currentMode);

#ifdef ARDUINO_ARCH_RP2040
        auto pioDriver = dynamic_cast<PIO_NeoPixel_Serial*>(driver);
        if (pioDriver)
        {
            uint32_t freq = pioDriver->getFrequency();
            openknx.logger.logWithValues("  Target Frequency: %d kHz", freq / 1000);

            float sys_clk = (float)clock_get_hz(clk_sys);
            openknx.logger.logWithValues("  System Clock: %.0f MHz", sys_clk / 1e6f);
        }
#endif
        openknx.logger.log("");
        openknx.logger.log("Use 'neo phys timing <id> info' for detailed information");
        return true;
    }

    // neo phys timing <id> <mode> -> Set timing mode
    // Special subcommand: custom <t0h> <t0l> <t1h> <t1l> [resetUs]
    if (strcmp(modeStr, "custom") == 0)
    {
        uint16_t t0h = 0, t0l = 0, t1h = 0, t1l = 0;
        uint32_t resetUs = 0;
        // Parse remaining values after "custom": t0h t0l t1h t1l [resetUs]
        int customParsed = sscanf(args.c_str(), "%*d %*s %hu %hu %hu %hu %u",
                                  &t0h, &t0l, &t1h, &t1l, &resetUs);

        if (customParsed < 4 || t0h == 0 || t0l == 0 || t1h == 0 || t1l == 0)
        {
            openknx.logger.log("ERROR: Usage: neo phys timing <id> custom <t0h> <t0l> <t1h> <t1l> [resetUs]");
            openknx.logger.log("  All timing values in ns (must be > 0).");
            openknx.logger.log("  Example: neo phys timing 0 custom 300 900 800 600");
            openknx.logger.log("  ResetUs optional (µs), default = 0 = keep protocol default");
            return true;
        }

        openknx.logger.logWithValues("Setting custom timing for strip [%d]:", stripId);
        openknx.logger.logWithValues("  T0H=%d ns  T0L=%d ns  T1H=%d ns  T1L=%d ns", t0h, t0l, t1h, t1l);
        if (resetUs > 0)
            openknx.logger.logWithValues("  Reset=%d µs", (int)resetUs);

        if (!strip->setCustomTiming(t0h, t0l, t1h, t1l, resetUs))
        {
            openknx.logger.log("ERROR: Failed to set custom timing!");
            openknx.logger.log("  Custom timing only supported for PIO Serial (RP2040/RP2350) and RMT (ESP32) drivers");
            return true;
        }

        openknx.logger.log("Custom timing set successfully!");
        openknx.logger.log("  RP2040/RP2350 (PIO): derives clkdiv from T1H; T0H/T0L/T1L follow fixed 3:7:6:4 ratio.");
        openknx.logger.log("  ESP32 (RMT):         all four values applied independently.");
        strip->clear();
        strip->show();
        return true;
    }

    // Special subcommand: freq <kHz> -> set bitrate directly (mirrors the ETS "Timing" field)
    if (strcmp(modeStr, "freq") == 0)
    {
        unsigned int freqKhz = 0;
        // Parse kHz after "freq": args = "<id> freq <kHz>"
        int parsed = sscanf(args.c_str(), "%*d %*s %u", &freqKhz);
        if (parsed < 1 || freqKhz < 400 || freqKhz > 1200)
        {
            openknx.logger.log("ERROR: Usage: neo phys timing <id> freq <kHz>");
            openknx.logger.log("  Bitrate in kHz (400-1200). Standard = 800; Clones oft 760-790.");
            openknx.logger.log("  Example: neo phys timing 0 freq 775");
            return true;
        }

        // 3:7:6:4 ratio; only T1H matters on PIO (T1H_ns = 600000 / kHz)
        uint16_t t1h = (uint16_t)(600000UL / freqKhz);
        uint16_t t0h = (uint16_t)(t1h / 2);
        uint16_t t0l = (uint16_t)((uint32_t)t1h * 7 / 6);
        uint16_t t1l = (uint16_t)((uint32_t)t1h * 4 / 6);

        openknx.logger.logWithValues("Setting bitrate for strip [%d]: %u kHz (T1H=%d ns)", stripId, (int)freqKhz, (int)t1h);
        if (!strip->setCustomTiming(t0h, t0l, t1h, t1l, 0))
        {
            openknx.logger.log("ERROR: Failed to set timing!");
            openknx.logger.log("  Only 1-Wire strips (PIO/RMT). SPI LEDs (APA102/...) use their own SPI clock.");
            return true;
        }
        openknx.logger.log("Bitrate set. NOTE: not persistent across ETS download - set 'Timing' in the ETS parameter instead.");
        strip->clear();
        strip->show();
        return true;
    }

    // Special subcommand: reset -> revert to AUTO timing
    if (strcmp(modeStr, "reset") == 0)
    {
        openknx.logger.logWithValues("Reverting strip [%d] to AUTO timing...", stripId);
        if (!strip->clearCustomTiming())
        {
            openknx.logger.log("ERROR: Failed to restore protocol timing.");
            return true;
        }
        openknx.logger.log("Timing reverted to AUTO.");
        strip->clear();
        strip->show();
        return true;
    }

    // Special subcommand: qualify (or scan alias) -> start clone timing qualify
    if (strcmp(modeStr, "qualify") == 0 || strcmp(modeStr, "scan") == 0)
    {
        return processPhysTimingScanCommand(stripId);
    }

    // Special subcommand: tune [subArgs] -> live timing tuner (enter or control)
    if (strcmp(modeStr, "tune") == 0)
    {
        // Extract everything after "<id> tune " as sub-arguments
        const char* p = args.c_str();
        while (*p && *p != ' ') p++;   // skip id
        while (*p == ' ') p++;         // skip spaces
        while (*p && *p != ' ') p++;   // skip "tune"
        while (*p == ' ') p++;         // skip spaces
        return processPhysTimingTuneCommand((uint32_t)stripId, std::string(p));
    }

    // Special subcommand: profile <N> -> apply a clone profile permanently
    if (strcmp(modeStr, "profile") == 0)
    {
        int profileIdx = -1;
        sscanf(args.c_str(), "%*d %*s %d", &profileIdx);
        if (profileIdx < 0 || profileIdx >= (int)kCloneProfileCount)
        {
            openknx.logger.logWithValues("ERROR: Usage: neo phys timing <id> profile <0-%d>", (int)kCloneProfileCount - 1);
            openknx.logger.log("  Use 'neo phys timings' to list all clone profiles.");
            return true;
        }
        return processPhysTimingProfileCommand(stripId, (uint8_t)profileIdx);
    }

    // Plain number: 0-15 = ETS Timing value (same kHz table), >15 = direct kHz
    if (modeStr[0] >= '0' && modeStr[0] <= '9')
    {
        static const uint16_t kTimingFreqTable[16] = {
            800, 960, 640, 680, 720, 760, 840, 880, 920, 750, 765, 770, 775, 780, 785, 790};
        int num = atoi(modeStr);
        uint16_t freqKhz;
        if (num >= 0 && num <= 15)
            freqKhz = kTimingFreqTable[num]; // 0-15 = ETS 'Timing' dropdown value
        else if (num >= 400 && num <= 1200)
            freqKhz = (uint16_t)num; // >15 = direct frequency in kHz (e.g. 799)
        else
        {
            openknx.logger.log("ERROR: Use 0-15 (ETS Timing value) or 400-1200 (direct kHz, e.g. 799).");
            return true;
        }
        const uint16_t t1h = (uint16_t)(600000UL / freqKhz);
        const uint16_t t0h = (uint16_t)(t1h / 2);
        const uint16_t t0l = (uint16_t)((uint32_t)t1h * 7 / 6);
        const uint16_t t1l = (uint16_t)((uint32_t)t1h * 4 / 6);
        openknx.logger.logWithValues("Setting timing for strip [%d]: %u kHz (T1H=%d ns)",
                                     stripId, (int)freqKhz, (int)t1h);
        if (!strip->setCustomTiming(t0h, t0l, t1h, t1l, 0))
        {
            openknx.logger.log("ERROR: Failed to set timing! (1-Wire strips only; SPI LEDs use SPI clock)");
            return true;
        }
        uint8_t* buffer = strip->getBuffer();
        if (buffer && _scanSavedBuffer.size() == strip->getBufferSize())
            memcpy(buffer, _scanSavedBuffer.data(), _scanSavedBuffer.size());
        _scanSavedBuffer.clear();
        strip->show();
        return true;
    }

    TimingMode newMode = parseTimingMode(modeStr);

    openknx.logger.logWithValues("Setting timing mode for strip [%d] to %s...", stripId, modeStr);

    if (!strip->setTimingMode(newMode))
    {
        openknx.logger.log("ERROR: Failed to set timing mode!");
        openknx.logger.log("  Timing mode change only supported for PIO Serial drivers (WS2812B, SK6812)");
        return true;
    }

    openknx.logger.log("Timing mode changed successfully!");
    openknx.logger.log("  Note: LEDs will be cleared after timing change");

    // Clear LEDs after timing change to ensure clean state
    strip->clear();
    strip->show();

    return true;
}

// ============================================================================
/**
 * @brief Start clone timing qualify for the given strip (non-blocking).
 *
 * Saves current timing, then kicks off the state machine in loop().
 * Each of the 6 clone profiles is shown for kScanColorDurationMs ms on the FULL
 * strip, then waits for user input (next/apply/stop) or auto-advances after
 * kScanWaitTimeoutMs ms.  After all profiles the original timing is restored.
 * Apply a found profile with: neo phys timing <id> profile <N>
 */
bool NeoPixel::processPhysTimingScanCommand(uint32_t stripId)
{
    if (_scanPhase != ScanPhase::IDLE)
    {
        openknx.logger.log("ERROR: A qualify scan is already running! Use 'neo scan stop' to abort.");
        return true;
    }

    auto strip = _manager ? _manager->getStrip(stripId) : nullptr;
    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Strip [%d] not found!", (int)stripId);
        return true;
    }

    auto* cfg = strip->getConfig();
    SerialStripConfig* sCfg = (cfg && cfg->isSerialConfig())
                                  ? static_cast<SerialStripConfig*>(cfg)
                                  : nullptr;
    if (!sCfg)
    {
        openknx.logger.log("ERROR: Clone scan only supported for serial strips (WS2812B/SK6812).");
        return true;
    }

    // Save original timing so we can restore it afterwards
    _scanSavedT0H   = sCfg->getT0H();
    _scanSavedT0L   = sCfg->getT0L();
    _scanSavedT1H   = sCfg->getT1H();
    _scanSavedT1L   = sCfg->getT1L();
    _scanSavedReset = sCfg->getResetTime();
    _scanSavedMode  = sCfg->getTimingMode();

    _scanStripId    = stripId;
    _scanProfileIdx = 0; // will be incremented at first PAUSE→SHOW transition

    openknx.logger.log("");
    openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    openknx.logger.logWithValues("  Clone Timing Qualify — Strip [%d]", (int)stripId);
    openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    openknx.logger.log("");
    openknx.logger.logWithValues("  Qualifying %d profiles — %ds show + %ds input-wait each.",
                                 (int)kCloneProfileCount,
                                 (int)(kScanColorDurationMs / 1000),
                                 (int)(kScanWaitTimeoutMs / 1000));
    openknx.logger.log("  Watch the ENTIRE STRIP while every candidate sends the same stress pattern.");
    openknx.logger.log("  Commands: neo scan next | neo scan apply | neo scan stop");
    openknx.logger.log("");
    openknx.logger.log("  Profile │ Payload │ Waveform/Reset");
    openknx.logger.log("  ────────┼─────────┼──────────────────────────────────────");
    for (uint8_t i = 0; i < kCloneProfileCount; i++)
    {
        const CloneTimingProfile& p = kCloneProfiles[i];
        openknx.logger.logWithValues("    %d     │ stress │ %s", (int)i, p.desc);
    }
    openknx.logger.log("");
    openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    openknx.logger.log("");

    uint8_t* buffer = strip->getBuffer();
    const size_t bufferSize = strip->getBufferSize();
    if (!buffer || bufferSize == 0)
    {
        openknx.logger.log("ERROR: Strip has no writable frame buffer.");
        return true;
    }
    _scanSavedBuffer.assign(buffer, buffer + bufferSize);

    // Apply profile 0 immediately; the state machine will start from SHOW_COLOR
    const CloneTimingProfile& first = kCloneProfiles[0];
    if (!applyCloneTimingProfile(strip, first) ||
        !writeCloneTimingStressPayload(strip) || !strip->show())
    {
        openknx.logger.log("ERROR: Failed to apply or transmit the first timing candidate.");
        _scanSavedBuffer.clear();
        return true;
    }

    openknx.logger.logWithValues("Profile 1/%d: %s — %s",
                                 (int)kCloneProfileCount, first.name, first.desc);

    _scanPhaseStart    = millis();
    _scanPromptPrinted = false;
    _scanPhase         = ScanPhase::SHOW_COLOR;

    return true;
}

// ============================================================================
/**
 * @brief Apply a clone timing profile for the current boot.
 */
bool NeoPixel::processPhysTimingProfileCommand(uint32_t stripId, uint8_t profileIdx)
{
    auto strip = _manager ? _manager->getStrip(stripId) : nullptr;
    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Strip [%d] not found!", (int)stripId);
        return true;
    }

    auto* cfg = strip->getConfig();
    SerialStripConfig* sCfg = (cfg && cfg->isSerialConfig())
                                  ? static_cast<SerialStripConfig*>(cfg) : nullptr;
    if (!sCfg)
    {
        openknx.logger.log("ERROR: Profile command only supported for serial strips.");
        return true;
    }

    const CloneTimingProfile& p = kCloneProfiles[profileIdx];
    openknx.logger.logWithValues("Applying clone profile %d (%s) to strip [%d]...",
                                 (int)profileIdx, p.name, (int)stripId);
    openknx.logger.logWithValues("  %s", p.desc);

    if (!applyCloneTimingProfile(strip, p))
    {
        openknx.logger.log("ERROR: This timing profile is not supported by the selected backend.");
        return true;
    }

    if (!writeCloneTimingStressPayload(strip) || !strip->show())
    {
        openknx.logger.log("ERROR: Profile applied, but the test frame could not be transmitted.");
        return true;
    }

    openknx.logger.log("Profile applied for the current boot only.");
    openknx.logger.log("  Timing overrides are not stored in flash; configure ETS Timing for a permanent setting.");
    openknx.logger.log("  To revert: neo phys timing <id> reset");

    return true;
}

// ============================================================================
/**
 * @brief Handle 'next / apply / stop' during an active clone timing qualify scan.
 *
 * Called from processCommand() (user input) and from loopTimingScan() on timeout.
 * @param cmd  "next" | "apply" | "stop"
 */
bool NeoPixel::processScanControlCommand(const std::string& cmd)
{
    if (_scanPhase == ScanPhase::IDLE)
    {
        openknx.logger.log("INFO: No qualify scan is currently active.");
        return true;
    }

    auto strip = _manager ? _manager->getStrip(_scanStripId) : nullptr;
    if (!strip) { _scanPhase = ScanPhase::IDLE; return true; }

    auto* cfg  = strip->getConfig();
    SerialStripConfig* sCfg = (cfg && cfg->isSerialConfig())
                                  ? static_cast<SerialStripConfig*>(cfg) : nullptr;

    auto restoreOriginalTiming = [&]() {
        if (sCfg)
        {
            if (_scanSavedMode == TimingMode::CUSTOM)
                strip->setCustomTiming(_scanSavedT0H, _scanSavedT0L,
                                       _scanSavedT1H, _scanSavedT1L, _scanSavedReset);
            else
            {
                sCfg->setTiming(0, 0, 0, 0);
                sCfg->setResetTime(_scanSavedReset);
                sCfg->setTimingMode(_scanSavedMode);
                strip->setTimingMode(_scanSavedMode);
            }
        }
        strip->clear();
        strip->show();
        _scanPhase      = ScanPhase::IDLE;
        _lastUpdateTime = millis();
    };

    // ── apply ────────────────────────────────────────────────────────────────
    if (cmd == "apply")
    {
        openknx.logger.logWithValues("Applying profile %d (%s) permanently ...",
                                     (int)_scanProfileIdx, kCloneProfiles[_scanProfileIdx].name);
        processPhysTimingProfileCommand(_scanStripId, _scanProfileIdx);
        _scanPhase      = ScanPhase::IDLE;
        _lastUpdateTime = millis();
        return true;
    }

    // ── stop ─────────────────────────────────────────────────────────────────
    if (cmd == "stop")
    {
        openknx.logger.log("Qualify scan aborted. Restoring original timing ...");
        restoreOriginalTiming();
        openknx.logger.log("Original timing restored.");
        return true;
    }

    // ── next ─────────────────────────────────────────────────────────────────
    if (cmd == "next")
    {
        strip->clear();
        strip->show();
        _scanProfileIdx++;

        if (_scanProfileIdx >= kCloneProfileCount)
        {
            openknx.logger.log("");
            openknx.logger.log("Qualify complete. All profiles shown. Original timing restored.");
            restoreOriginalTiming();
            openknx.logger.log("If a profile lit the full strip, apply it with:");
            openknx.logger.logWithValues("  neo phys timing %d profile <N>", (int)_scanStripId);
            openknx.logger.log("");
            return true;
        }

        // Brief pause; loopTimingScan/PAUSE will apply the next profile
        _scanPhaseStart    = millis();
        _scanPromptPrinted = false;
        _scanPhase         = ScanPhase::PAUSE;
        return true;
    }

    return false;
}

// ============================================================================
/**
 * @brief Live timing tuner — entry point AND control for a single strip.
 *
 * Called for every `neo phys timing <id> tune [subArgs]` command.
 *
 *   subArgs empty    → enter tuner (or show status if already active for this strip)
 *   subArgs "show"   → print current live values
 *   subArgs "save"   → compatibility alias for done; timing remains RAM-only
 *   subArgs "done"   → keep timing (no flash write), exit
 *   subArgs "abort"  → restore original timing, exit
 *   subArgs "t1h +50"→ adjust T1H by +50 ns (absolute or +/- relative)
 *
 * The strip ID is always explicit — multiple strips can be tuned simultaneously.
 * loop() skips normal updates while any tuner is active (_activeTuners non-empty).
 */
bool NeoPixel::processPhysTimingTuneCommand(uint32_t stripId, const std::string& subArgs)
{
    if (_scanPhase != ScanPhase::IDLE)
    {
        openknx.logger.log("ERROR: A qualify scan is running. Use 'neo scan stop' first.");
        return true;
    }

    auto strip = _manager ? _manager->getStrip(stripId) : nullptr;
    if (!strip)
    {
        openknx.logger.logWithValues("ERROR: Strip [%d] not found!", (int)stripId);
        return true;
    }

    auto* cfg = strip->getConfig();
    SerialStripConfig* sCfg = (cfg && cfg->isSerialConfig())
                                  ? static_cast<SerialStripConfig*>(cfg) : nullptr;
    if (!sCfg)
    {
        openknx.logger.log("ERROR: Live tuner only supported for serial strips (WS2812B/SK6812).");
        return true;
    }

    bool isActive = (_activeTuners.find(stripId) != _activeTuners.end());

    // ── ENTER (no subArgs or strip not yet active) ────────────────────────────
    if (subArgs.empty() && !isActive)
    {
        TunerState ts;
        ts.savedT0H    = sCfg->getT0H();
        ts.savedT0L    = sCfg->getT0L();
        ts.savedT1H    = sCfg->getT1H();
        ts.savedT1L    = sCfg->getT1L();
        ts.savedResetUs = sCfg->getResetTime();
        ts.savedMode   = sCfg->getTimingMode();
        auto eff = sCfg->getEffectiveTimings(strip->getProtocol());
        ts.liveT0H     = eff.t0h;
        ts.liveT0L     = eff.t0l;
        ts.liveT1H     = eff.t1h;
        ts.liveT1L     = eff.t1l;
        ts.liveResetUs = eff.resetUs;
        _activeTuners[stripId] = ts;

        // Light full strip white
        uint16_t ledCount = strip->getLedCount();
        for (uint16_t i = 0; i < ledCount; i++)
            strip->setPixel(i, 255, 255, 255);
        strip->show();

        openknx.logger.log("");
        openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        openknx.logger.logWithValues("  [EXPERT] Clone Timing Tuner — Strip [%d]", (int)stripId);
        openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        openknx.logger.log("  Strip is lit WHITE. Every change applies instantly.");
        openknx.logger.log("  If all LEDs light up → that timing works for your strip!");
        openknx.logger.log("");
        openknx.logger.log("  NeoPixel 1-wire protocol — what each value means:");
        openknx.logger.log("    T0H / T0L = HIGH / LOW duration for a '0' bit (ns)");
        openknx.logger.log("    T1H / T1L = HIGH / LOW duration for a '1' bit (ns)");
        openknx.logger.log("    Reset     = min. LOW pause between two frames (µs)");
        openknx.logger.log("");
#ifdef ARDUINO_ARCH_RP2040
        openknx.logger.log("  Platform: RP2040/RP2350 (PIO driver)");
        openknx.logger.log("    Fixed 3:7:6:4 cycle ratio — only T1H sets the bitrate.");
        openknx.logger.log("    T0H/T0L/T1L are derived automatically.");
        openknx.logger.log("    Start here:  neo phys timing 0 tune t1h +50");
        openknx.logger.log("    Latch fix:   neo phys timing 0 tune reset 280");
#endif
#ifdef ARDUINO_ARCH_ESP32
        openknx.logger.log("  Platform: ESP32 (RMT driver)");
        openknx.logger.log("    All four values are applied independently.");
        openknx.logger.log("    Start here:  neo phys timing 0 tune t1h +50");
        openknx.logger.log("    Also try:    neo phys timing 0 tune t0h 350");
        openknx.logger.log("    Latch fix:   neo phys timing 0 tune reset 280");
#endif
        openknx.logger.log("");
        openknx.logger.logWithValues("  T0H = %4d ns    T0L = %4d ns", (int)ts.liveT0H, (int)ts.liveT0L);
        openknx.logger.logWithValues("  T1H = %4d ns    T1L = %4d ns", (int)ts.liveT1H, (int)ts.liveT1L);
        openknx.logger.logWithValues("  Reset = %d µs", (int)ts.liveResetUs);
        openknx.logger.log("");
        openknx.logger.logWithValues("  Commands (strip %d):", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune t1h +50", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune reset <µs>", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune show", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune save (alias for done)", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune done", (int)stripId);
        openknx.logger.logWithValues("    neo phys timing %d tune abort", (int)stripId);
        openknx.logger.log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        openknx.logger.log("");
        return true;
    }

    // ── STATUS (no subArgs but already active) ────────────────────────────────
    if (subArgs.empty() && isActive)
    {
        openknx.logger.logWithValues("INFO: Tuner for strip [%d] is already active. Use:", (int)stripId);
        openknx.logger.logWithValues("  neo phys timing %d tune show   -> current values", (int)stripId);
        openknx.logger.logWithValues("  neo phys timing %d tune abort  -> restore & exit", (int)stripId);
        return true;
    }

    // All control commands require an active tuner for this strip
    if (!isActive)
    {
        openknx.logger.logWithValues("ERROR: No tuner active for strip [%d]. Start with:", (int)stripId);
        openknx.logger.logWithValues("  neo phys timing %d tune", (int)stripId);
        return true;
    }

    TunerState& ts = _activeTuners[stripId];

    // ── save ──────────────────────────────────────────────────────────────────
    if (subArgs == "save")
    {
        if (!strip->setCustomTiming(ts.liveT0H, ts.liveT0L, ts.liveT1H, ts.liveT1L, ts.liveResetUs))
        {
            openknx.logger.log("ERROR: Failed to apply timing; tuner remains open.");
            return true;
        }
        _activeTuners.erase(stripId);
        _lastUpdateTime = millis();
        openknx.logger.logWithValues("Strip [%d]: timing kept for this boot. Tuner closed.", (int)stripId);
        openknx.logger.log("  Timing overrides are not stored in flash; configure ETS Timing permanently.");
        return true;
    }

    // ── done ──────────────────────────────────────────────────────────────────
    if (subArgs == "done")
    {
        if (!strip->setCustomTiming(ts.liveT0H, ts.liveT0L, ts.liveT1H, ts.liveT1L, ts.liveResetUs))
        {
            openknx.logger.log("ERROR: Failed to apply timing; tuner remains open.");
            return true;
        }
        _activeTuners.erase(stripId);
        _lastUpdateTime = millis();
        openknx.logger.logWithValues("Strip [%d]: tuner closed, timing kept (not written to flash).", (int)stripId);
        openknx.logger.logWithValues("  To save: neo phys timing %d custom %d %d %d %d %d",
                                     (int)stripId,
                                     (int)ts.liveT0H, (int)ts.liveT0L,
                                     (int)ts.liveT1H, (int)ts.liveT1L, (int)ts.liveResetUs);
        return true;
    }

    // ── abort ─────────────────────────────────────────────────────────────────
    if (subArgs == "abort" || subArgs == "stop")
    {
        if (ts.savedMode == TimingMode::CUSTOM)
            strip->setCustomTiming(ts.savedT0H, ts.savedT0L, ts.savedT1H, ts.savedT1L, ts.savedResetUs);
        else
        {
            sCfg->setTiming(0, 0, 0, 0);
            sCfg->setResetTime(ts.savedResetUs);
            sCfg->setTimingMode(ts.savedMode);
            strip->setTimingMode(ts.savedMode);
        }
        strip->clear();
        strip->show();
        _activeTuners.erase(stripId);
        _lastUpdateTime = millis();
        openknx.logger.logWithValues("Strip [%d]: original timing restored. Tuner closed.", (int)stripId);
        return true;
    }

    // ── show ──────────────────────────────────────────────────────────────────
    if (subArgs == "show")
    {
        openknx.logger.log("");
        openknx.logger.logWithValues("  [Strip %d — Tuner active]", (int)stripId);
        openknx.logger.logWithValues("  T0H = %4d ns    T0L = %4d ns", (int)ts.liveT0H, (int)ts.liveT0L);
        openknx.logger.logWithValues("  T1H = %4d ns    T1L = %4d ns", (int)ts.liveT1H, (int)ts.liveT1L);
        openknx.logger.logWithValues("  Reset = %d µs", (int)ts.liveResetUs);
        openknx.logger.log("");
        return true;
    }

    // ── parameter adjustment: "t1h +50", "reset 280", … ──────────────────────
    char   paramBuf[16] = {};
    char   valBuf[16]   = {};
    if (sscanf(subArgs.c_str(), "%15s %15s", paramBuf, valBuf) < 2)
    {
        openknx.logger.log("ERROR: Usage: neo phys timing <id> tune <param> <value|+delta|-delta>");
        openknx.logger.log("  Params: t0h  t0l  t1h  t1l  reset");
        return true;
    }

    bool isRelative = (valBuf[0] == '+' || valBuf[0] == '-');
    int  delta      = atoi(valBuf);

    uint16_t* target16  = nullptr;
    uint32_t* target32  = nullptr;
    const char* paramName = paramBuf;

    if      (strcmp(paramBuf, "t0h")   == 0) { target16 = &ts.liveT0H; }
    else if (strcmp(paramBuf, "t0l")   == 0) { target16 = &ts.liveT0L; }
    else if (strcmp(paramBuf, "t1h")   == 0) { target16 = &ts.liveT1H; }
    else if (strcmp(paramBuf, "t1l")   == 0) { target16 = &ts.liveT1L; }
    else if (strcmp(paramBuf, "reset") == 0 || strcmp(paramBuf, "rst") == 0)
                                             { target32 = &ts.liveResetUs; }
    else
    {
        openknx.logger.logWithValues("ERROR: Unknown param '%s'. Use: t0h t0l t1h t1l reset", paramBuf);
        return true;
    }

    if (target16)
    {
        int newVal = isRelative ? ((int)*target16 + delta) : delta;
        if (newVal <   50) newVal =   50;
        if (newVal > 5000) newVal = 5000;
        *target16 = (uint16_t)newVal;
    }
    else
    {
        int newVal = isRelative ? ((int)*target32 + delta) : delta;
        if (newVal <   10) newVal =   10;
        if (newVal > 1000) newVal = 1000;
        *target32 = (uint32_t)newVal;
    }

    if (!strip->setCustomTiming(ts.liveT0H, ts.liveT0L, ts.liveT1H, ts.liveT1L, ts.liveResetUs))
    {
        openknx.logger.log("ERROR: Failed to apply timing (not supported on this driver).");
        return true;
    }

    // Re-light strip white so user gets instant visual feedback
    uint16_t ledCount = strip->getLedCount();
    for (uint16_t i = 0; i < ledCount; i++)
        strip->setPixel(i, 255, 255, 255);
    strip->show();

    openknx.logger.logWithValues("  [Strip %d] %s = %d  |  T0H=%d T0L=%d T1H=%d T1L=%d Reset=%dµs",
                                 (int)stripId, paramName,
                                 target16 ? (int)*target16 : (int)*target32,
                                 (int)ts.liveT0H, (int)ts.liveT0L,
                                 (int)ts.liveT1H, (int)ts.liveT1L, (int)ts.liveResetUs);
    return true;
}


// ============================================================================
/**
 * @brief Process 'neo phys config' command router
 */
bool NeoPixel::processPhysConfigCommand(const std::string& args)
{
    // Parse: <id> <subcommand> [params...]
    std::istringstream iss(args);
    uint32_t stripId;
    std::string subCmd;

    if (!(iss >> stripId >> subCmd))
    {
        openknx.logger.log("ERROR: Usage: neo phys config <id> <info|dummy|frames|pattern|brightness|detect>");
        return true;
    }

    if (subCmd == "info")
    {
        return processPhysConfigInfoCommand(stripId);
    }
    else if (subCmd == "dummy")
    {
        int modeInt;
        if (!(iss >> modeInt))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> dummy <0-2>");
            return true;
        }
        return processPhysConfigDummyCommand(stripId, (uint8_t)modeInt);
    }
    else if (subCmd == "frames")
    {
        int startInt, endInt;
        if (!(iss >> startInt >> endInt))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> frames <start> <end>");
            return true;
        }
        return processPhysConfigFramesCommand(stripId, (uint8_t)startInt, (uint8_t)endInt);
    }
    else if (subCmd == "pattern")
    {
        std::string patternStr;
        if (!(iss >> patternStr))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> pattern <0x00|0xFF>");
            return true;
        }
        uint8_t pattern = (uint8_t)strtol(patternStr.c_str(), nullptr, 0);
        return processPhysConfigPatternCommand(stripId, pattern);
    }
    else if (subCmd == "brightness")
    {
        int brightnessInt;
        if (!(iss >> brightnessInt))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> brightness <0-31>");
            return true;
        }
        return processPhysConfigBrightnessCommand(stripId, (uint8_t)brightnessInt);
    }
    else if (subCmd == "freq" || subCmd == "frequency")
    {
        float freqMHz;
        if (!(iss >> freqMHz))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> freq <MHz> (e.g. 7.5, 10, 15)");
            return true;
        }
        return processPhysConfigFrequencyCommand(stripId, (uint32_t)(freqMHz * 1000000.0f));
    }
    else if (subCmd == "delay")
    {
        int delayUs;
        if (!(iss >> delayUs))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> delay <us> (0-1000)");
            return true;
        }
        return processPhysConfigDelayCommand(stripId, (uint32_t)delayUs);
    }
    else if (subCmd == "autodetect")
    {
        std::string onOff;
        if (!(iss >> onOff))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> autodetect <on|off>");
            return true;
        }
        bool enable = (onOff == "on" || onOff == "1" || onOff == "true");
        return processPhysConfigAutoDetectCommand(stripId, enable);
    }
    else if (subCmd == "detect")
    {
        return processPhysConfigDetectCommand(stripId);
    }
    else if (subCmd == "skipfirst")
    {
        int count;
        if (!(iss >> count))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> skipfirst <count>");
            return true;
        }
        return processPhysConfigSkipFirstCommand(stripId, (uint8_t)count);
    }
    else if (subCmd == "skipmask")
    {
        std::string maskCmd;
        if (!(iss >> maskCmd))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> skipmask <init|clear|set|list>");
            return true;
        }
        return processPhysConfigSkipMaskCommand(stripId, maskCmd, iss);
    }
    else if (subCmd == "levelshifter" || subCmd == "ls")
    {
        std::string typeStr;
        if (!(iss >> typeStr))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> levelshifter <none|txs0108|hct125|ahct125>");
            return true;
        }
        LevelShifterType lsType;
        if (typeStr == "none" || typeStr == "0")
            lsType = LevelShifterType::NONE;
        else if (typeStr == "txs0108" || typeStr == "txs0108e" || typeStr == "1")
            lsType = LevelShifterType::TXS0108E;
        else if (typeStr == "hct125" || typeStr == "74hct125" || typeStr == "2")
            lsType = LevelShifterType::SN74HCT125;
        else if (typeStr == "ahct125" || typeStr == "74ahct125" || typeStr == "3")
            lsType = LevelShifterType::SN74AHCT125;
        else
        {
            openknx.logger.logWithValues("ERROR: Unknown type '%s'. Use: none | txs0108 | hct125 | ahct125", typeStr.c_str());
            return true;
        }
        return processPhysConfigLevelShifterCommand(stripId, lsType);
    }
    else
    {
        openknx.logger.log("ERROR: Unknown config subcommand. Use 'neo phys ?' for help.");
        return true;
    }
}

/**
 * @brief Process 'neo phys config <id> info' command
 */
bool NeoPixel::processPhysConfigInfoCommand(uint32_t stripId)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    openknx.logger.log("");
    printSectionSeparator();
    openknx.logger.logWithPrefixAndValues("", "  Strip Configuration - Strip %d", stripId);
    printSectionSeparator();

    // Get config
    auto* cfg = strip->getConfig();
    if (!cfg)
    {
        openknx.logger.log("ERROR: No config available for this strip");
        return false;
    }

    // Check if SPI or Serial
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    SerialStripConfig* serialCfg = cfg->isSerialConfig() ? static_cast<SerialStripConfig*>(cfg) : nullptr;

    if (spiCfg)
    {
        // Display SPI config
        uint8_t hwBrightness = spiCfg->getHwBrightness();
        uint8_t dummyMode = spiCfg->getDummyLedMode();
        uint8_t startFrames = spiCfg->getStartFrameCount();
        uint8_t endFrames = spiCfg->getEndFrameCount();
        uint8_t endPattern = spiCfg->getEndFramePattern();
        bool autoDetect = spiCfg->getAutoDetectChip();
        LedProtocol detected = spiCfg->getDetectedChip();

        openknx.logger.log("Type: SPI Strip (APA102/SK9822)");
        openknx.logger.log("");

        // Hardware Info (from driver)
        auto driver = strip->getDriver();
        if (driver)
        {
#ifdef ARDUINO_ARCH_RP2040
            uint32_t frequency = spiCfg->getSpiFrequency();
            auto spiDriver = dynamic_cast<PIO_NeoPixel_SPI*>(driver);
            if (spiDriver)
            {
                PIO pio = spiDriver->getPio();
                const char* pioName = (pio == pio0) ? "PIO0" : (pio == pio1) ? "PIO1"
                                                                             : "PIO2";
                int dmaChannel = spiDriver->getDmaChannel();
                float clkdiv = spiDriver->getClkdiv();

                openknx.logger.logWithValues("Pins: CLK=GPIO%d, MOSI=GPIO%d",
                                             spiDriver->getClkPin(), spiDriver->getMosiPin());

                if (dmaChannel >= 0)
                {
                    openknx.logger.logWithValues("Hardware: %s/SM%d (SPI), DMA Ch%d",
                                                 pioName, spiDriver->getStateMachine(), dmaChannel);
                }
                else
                {
                    openknx.logger.logWithValues("Hardware: %s/SM%d (SPI, no DMA)",
                                                 pioName, spiDriver->getStateMachine());
                }

                openknx.logger.logWithValues("SPI Frequency: %d MHz (clkdiv: %.2f)",
                                             frequency / 1000000, clkdiv);
                openknx.logger.log("");
            }
#endif // ARDUINO_ARCH_RP2040
        }

        uint8_t minBright = spiCfg->getHwBrightnessMin();
        uint8_t maxBright = spiCfg->getHwBrightnessMax();
        uint8_t defBright = spiCfg->getHwBrightnessDefault();
        openknx.logger.logWithValues("HW Brightness: %d (range: %d-%d, default: %d)", hwBrightness, minBright, maxBright, defBright);

        openknx.logger.logWithValues("Dummy LED Mode: %d (%s)",
                                     dummyMode,
                                     dummyMode == 0 ? "None" : (dummyMode == 1 ? "Physical" : "Virtual"));

        openknx.logger.logWithValues("Start Frames: %d", startFrames);

        openknx.logger.logWithValues("End Frames: %d", endFrames);

        openknx.logger.logWithValues("End Frame Pattern: 0x%02X (%s)",
                                     endPattern,
                                     endPattern == 0x00 ? "APA102" : "SK9822");

        openknx.logger.logWithValues("Auto-Detect: %s", autoDetect ? "Enabled" : "Disabled");

        const char* detectedName = "Unknown";
        if (detected == LedProtocol::APA102) detectedName = "APA102";
        else if (detected == LedProtocol::SK9822)
            detectedName = "SK9822";
        openknx.logger.logWithValues("Detected Chip: %s", detectedName);

        openknx.logger.log("");
        openknx.logger.log("Tip: Clone chips often mislabeled - use 'neo phys config <id> detect' to verify");
    }
    else if (serialCfg)
    {
        // Display Serial config
        TimingMode timingMode = serialCfg->getTimingMode();
        const char* modeName = getTimingModeName(timingMode);

        // Resolve effective timing: custom values when set, otherwise protocol defaults
        LedProtocol protocol = strip->getProtocol();
        auto eff = serialCfg->getEffectiveTimings(protocol);
        const char* autoLabel = eff.isCustom ? "" : " (auto)";

        openknx.logger.log("Type: Serial Strip (WS2812B/SK6812)");
        openknx.logger.log("");

        openknx.logger.logWithValues("Timing Mode:       %s (%d)", modeName, (int)timingMode);
        openknx.logger.logWithValues("T0H (0-bit high):  %d ns%s", (int)eff.t0h, autoLabel);
        openknx.logger.logWithValues("T0L (0-bit low):   %d ns%s", (int)eff.t0l, autoLabel);
        openknx.logger.logWithValues("T1H (1-bit high):  %d ns%s", (int)eff.t1h, autoLabel);
        openknx.logger.logWithValues("T1L (1-bit low):   %d ns%s", (int)eff.t1l, autoLabel);
        openknx.logger.logWithValues("Reset Time:        %d µs%s", (int)eff.resetUs, autoLabel);

        // Level-shifter
        LevelShifterType ls = serialCfg->getLevelShifter();
        const char* lsName;
        switch (ls)
        {
            case LevelShifterType::TXS0108E:    lsName = "TXS0108E (bidirect., pull=off, drive+)"; break;
            case LevelShifterType::SN74HCT125:  lsName = "74HCT125 (unidirect. buffer)";          break;
            case LevelShifterType::SN74AHCT125: lsName = "74AHCT125 (unidirect. buffer, faster)"; break;
            default:                            lsName = "none";                                    break;
        }
        openknx.logger.logWithValues("Level Shifter:     %s", lsName);

        openknx.logger.log("");
        if (eff.isCustom)
        {
            openknx.logger.log("Tip: Use 'neo phys timing <id> reset' to revert to AUTO timing");
        }
        else
        {
            openknx.logger.log("Tip: Use 'neo phys timing <id> custom <t0h> <t0l> <t1h> <t1l>' to set custom timing for clone LEDs");
        }
    }
    else
    {
        openknx.logger.log("ERROR: Unknown config type");
        return false;
    }

    // Display common config (skipFirstLeds, skipMask)
    uint8_t skipFirst = cfg->getSkipFirstLeds();
    if (skipFirst > 0)
    {
        openknx.logger.logWithValues("Skip First LEDs: %d (forced to black)", skipFirst);
    }

    if (cfg->hasSkipMask())
    {
        uint16_t skipCount = cfg->getSkipMaskCount();
        openknx.logger.logWithValues("Skip Mask: Active (%d LEDs marked)", skipCount);
        openknx.logger.log("  Use 'neo phys config <id> skipmask list' to see details");
    }

    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo phys config <id> dummy <mode>' command
 */
bool NeoPixel::processPhysConfigDummyCommand(uint32_t stripId, uint8_t mode)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    if (mode > 2)
    {
        openknx.logger.log("ERROR: Dummy mode must be 0 (none), 1 (physical), or 2 (virtual)");
        return true;
    }

    // Get SPI config and set dummy mode
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Strip is not SPI type!");
        openknx.logger.log("  Only supported for APA102/SK9822 SPI strips");
        return true;
    }

    // Check if already initialized
    if (strip->isInitialized())
    {
        openknx.logger.log("WARNING: Dummy LED mode change requires strip recreation!");
        openknx.logger.log("  The strip is already initialized, which means the buffer is already allocated.");
        openknx.logger.log("  Changing dummy LED mode requires buffer re-allocation.");
        openknx.logger.log("");
        openknx.logger.log("  Solution: Restart device or recreate strip:");
        openknx.logger.logWithValues("    1. neo phys del %d", stripId);
        openknx.logger.log("    2. Create new strip with correct dummy LED mode");
        openknx.logger.logWithValues("    3. neo phys config %d dummy %d", stripId, mode);
        openknx.logger.log("");
        openknx.logger.log("  Or: Simply restart the device after setting this value.");
        openknx.logger.log("");

        // Still set the mode in config so it's applied on next restart/recreation
        spiCfg->setDummyLedMode(mode);
        openknx.logger.logWithValues("Dummy LED mode set to: %d (%s) - will take effect on next strip creation",
                                     mode, mode == 0 ? "None" : (mode == 1 ? "Physical" : "Virtual"));
        return true;
    }

    // Strip not initialized yet - we can safely change the mode
    spiCfg->setDummyLedMode(mode);
    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        return true;
    }

    openknx.logger.logWithValues("Dummy LED mode set to: %d (%s)",
                                 mode,
                                 mode == 0 ? "None" : (mode == 1 ? "Physical" : "Virtual"));
    openknx.logger.log("SUCCESS: Config applied - strip will use this mode when initialized");

    return true;
}

/**
 * @brief Process 'neo phys config <id> frames <start> <end>' command
 */
bool NeoPixel::processPhysConfigFramesCommand(uint32_t stripId, uint8_t start, uint8_t end)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    if (start < 1 || start > 8)
    {
        openknx.logger.log("ERROR: Start frame count must be 1-8");
        return true;
    }

    if (end < 1 || end > 80)
    {
        openknx.logger.log("ERROR: End frame count must be 1-80");
        return true;
    }

    // Get SPI config and set frame counts
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Strip is not SPI type!");
        openknx.logger.log("  Only supported for APA102/SK9822 SPI strips");
        return true;
    }

    spiCfg->setStartFrameCount(start);
    spiCfg->setEndFrameCount(end);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        openknx.logger.log("  Only supported for APA102/SK9822 SPI strips");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "Frame counts set: start=%d, end=%d", start, end);
    openknx.logger.log("Note: Start frame changes require strip re-creation");

    return true;
}

/**
 * @brief Process 'neo phys config <id> pattern <0x00|0xFF>' command
 */
bool NeoPixel::processPhysConfigPatternCommand(uint32_t stripId, uint8_t pattern)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    if (pattern != 0x00 && pattern != 0xFF)
    {
        openknx.logger.log("ERROR: End frame pattern must be 0x00 (APA102) or 0xFF (SK9822)");
        return true;
    }

    // Get SPI config and set pattern
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Strip is not SPI type!");
        openknx.logger.log("  Only supported for APA102/SK9822 SPI strips");
        return true;
    }

    spiCfg->setEndFramePattern(pattern);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to set end frame pattern!");
        openknx.logger.log("  Only supported for APA102/SK9822 SPI strips");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "End frame pattern set to: 0x%02X (%s)",
                                          pattern,
                                          pattern == 0x00 ? "APA102" : "SK9822");

    return true;
}

/**
 * @brief Process 'neo phys config <id> brightness <value>' command
 */
bool NeoPixel::processPhysConfigBrightnessCommand(uint32_t stripId, uint8_t brightness)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    // Get config - brightness only available for SPI strips
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Hardware brightness only available for SPI strips (APA102/SK9822)!");
        openknx.logger.log("  Serial strips (WS2812B/SK6812) use software brightness");
        return true;
    }

    const uint8_t minBright = spiCfg->getHwBrightnessMin();
    const uint8_t maxBright = spiCfg->getHwBrightnessMax();

    if (brightness < minBright || brightness > maxBright)
    {
        openknx.logger.logWithPrefixAndValues("", "ERROR: Brightness must be %d-%d (protocol limit)", minBright, maxBright);
        return true;
    }

    spiCfg->setHwBrightness(brightness);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "Hardware brightness set to: %d (range: %d-%d)", brightness, minBright, maxBright);
    openknx.logger.log("Note: Takes effect immediately on next setPixel() call");

    return true;
}

/**
 * @brief Process 'neo phys config <id> freq <MHz>' command
 */
bool NeoPixel::processPhysConfigFrequencyCommand(uint32_t stripId, uint32_t frequencyHz)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    // Get config - frequency only available for SPI strips
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: SPI frequency only available for SPI strips (APA102/SK9822)!");
        openknx.logger.log("  Serial strips (WS2812B/SK6812) use fixed timing");
        return true;
    }

    // Validate frequency range (1 MHz to 25 MHz typical)
    if (frequencyHz < 1000000 || frequencyHz > 25000000)
    {
        openknx.logger.log("ERROR: Frequency must be 1-25 MHz");
        openknx.logger.log("  Typical: 7.5 MHz (safe), 10 MHz (standard), 15-20 MHz (fast)");
        return true;
    }

    spiCfg->setSpiFrequency(frequencyHz);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "SPI frequency set to: %.1f MHz", frequencyHz / 1000000.0f);
    openknx.logger.log("Note: Takes effect immediately");
    openknx.logger.log("Tip: Higher frequencies = faster updates, but may cause issues on long cables");

    return true;
}

/**
 * @brief Process 'neo phys config <id> delay <us>' command
 */
bool NeoPixel::processPhysConfigDelayCommand(uint32_t stripId, uint32_t delayUs)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    // Get config - delay only available for SPI strips
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Start frame delay only available for SPI strips (APA102/SK9822)!");
        return true;
    }

    // Validate range
    if (delayUs > 1000)
    {
        openknx.logger.log("ERROR: Delay must be 0-1000 microseconds");
        return true;
    }

    spiCfg->setStartFrameDelayUs(delayUs);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "Start frame delay set to: %d us", delayUs);
    openknx.logger.log("Note: Takes effect immediately on next show() call");

    return true;
}

/**
 * @brief Process 'neo phys config <id> autodetect <on|off>' command
 */
bool NeoPixel::processPhysConfigAutoDetectCommand(uint32_t stripId, bool enable)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    // Get config - autodetect only available for SPI strips
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Auto-detection only available for SPI strips (APA102/SK9822)!");
        return true;
    }

    spiCfg->setAutoDetectChip(enable);

    if (!strip->applyConfig())
    {
        openknx.logger.log("ERROR: Failed to apply config!");
        return true;
    }

    openknx.logger.logWithPrefixAndValues("", "Auto-detection %s", enable ? "ENABLED" : "DISABLED");
    if (enable)
    {
        openknx.logger.log("Note: Chip type will be auto-detected on next init()");
        openknx.logger.log("Tip: Use 'neo phys config <id> detect' to detect now");
    }
    else
    {
        openknx.logger.log("Note: Using manual chip configuration (end frame pattern)");
    }

    return true;
}

/**
 * @brief Process 'neo phys config <id> detect' command
 */
bool NeoPixel::processPhysConfigDetectCommand(uint32_t stripId)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    LedProtocol proto = strip->getProtocol();
    if (proto != LedProtocol::APA102 && proto != LedProtocol::SK9822)
    {
        openknx.logger.log("ERROR: Chip detection only available for APA102/SK9822 strips!");
        return true;
    }

    openknx.logger.log("");
    openknx.logger.log("Starting chip auto-detection...");
    openknx.logger.log("Watch LEDs: RED → GREEN flash test");
    openknx.logger.log("");

    // Get SPI config and run detection
    auto* cfg = strip->getConfig();
    SpiStripConfig* spiCfg = cfg->isSpiConfig() ? static_cast<SpiStripConfig*>(cfg) : nullptr;
    if (!spiCfg)
    {
        openknx.logger.log("ERROR: Strip is not SPI type!");
        return true;
    }

    LedProtocol detected = spiCfg->detectChipType(strip);
    strip->applyConfig(); // Apply detected settings

    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    const char* chipName = (detected == LedProtocol::APA102) ? "APA102" : "SK9822";
    openknx.logger.logWithPrefixAndValues("", "Detected chip type: %s", chipName);
    openknx.logger.color(0);

    uint8_t endPattern = spiCfg->getEndFramePattern();
    openknx.logger.logWithPrefixAndValues("", "End frame pattern: 0x%02X", endPattern);

    openknx.logger.log("");
    openknx.logger.log("Note: Detection is heuristic-based (LED count + pattern test)");
    openknx.logger.log("      Use 'neo phys config <id> pattern <0x00|0xFF>' for manual override");

    return true;
}
/**
 * @brief Process 'neo phys config <id> skipfirst <count>' command
 */
bool NeoPixel::processPhysConfigSkipFirstCommand(uint32_t stripId, uint8_t count)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    auto* cfg = strip->getConfig();
    if (!cfg)
    {
        openknx.logger.log("ERROR: No config available for this strip");
        return true;
    }

    cfg->setSkipFirstLeds(count);
    openknx.logger.logWithPrefixAndValues("", "Skip first %d LEDs (forced to black)", count);

    return true;
}

/**
 * @brief Process 'neo phys config <id> levelshifter <none|txs0108>' command
 *
 * Configures the level-shifter type for the strip's data line.
 * On TXS0108E: RP2040 disables internal pull-up/down; ESP32 boosts drive to 40mA + FLOAT.
 * Setting is applied via applyConfig() immediately and persisted to flash.
 */
bool NeoPixel::processPhysConfigLevelShifterCommand(uint32_t stripId, LevelShifterType type)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    auto* cfg = strip->getConfig();
    if (!cfg || !cfg->isSerialConfig())
    {
        openknx.logger.log("ERROR: Level-shifter config only applies to serial strips (WS2812B/SK6812)");
        return true;
    }

    auto* serialCfg = static_cast<SerialStripConfig*>(cfg);
    serialCfg->setLevelShifter(type);

    // Apply immediately via applyConfig
    auto* drv = strip->getDriver();
    if (drv) drv->applyConfig(cfg);

    openknx.flash.save();

    const char* typeName;
    switch (type)
    {
        case LevelShifterType::TXS0108E:    typeName = "TXS0108E";   break;
        case LevelShifterType::SN74HCT125:  typeName = "74HCT125";   break;
        case LevelShifterType::SN74AHCT125: typeName = "74AHCT125";  break;
        default:                            typeName = "none";        break;
    }
    openknx.logger.logWithValues("Level-shifter set to: %s (applied)", typeName);

#ifdef ARDUINO_ARCH_RP2040
    if (type == LevelShifterType::TXS0108E)
        openknx.logger.log("  RP2040: internal pull-up/down disabled on data pin");
    else if (type == LevelShifterType::SN74HCT125 || type == LevelShifterType::SN74AHCT125)
        openknx.logger.log("  RP2040: no GPIO changes (4mA drive sufficient for logic input)");
#endif
#ifdef ARDUINO_ARCH_ESP32
    if (type == LevelShifterType::TXS0108E)
        openknx.logger.log("  ESP32: drive strength boosted to 40mA, pull mode = FLOATING");
    else if (type == LevelShifterType::SN74HCT125 || type == LevelShifterType::SN74AHCT125)
        openknx.logger.log("  ESP32: no GPIO changes (20mA drive sufficient for logic input)");
#endif

    return true;
}

/**
 * @brief Process 'neo phys config <id> skipmask <init|clear|set|list>' command
 */
bool NeoPixel::processPhysConfigSkipMaskCommand(uint32_t stripId, const std::string& maskCmd, std::istringstream& iss)
{
    auto strip = _manager->getStrip(stripId);
    if (!strip)
    {
        openknx.logger.log("ERROR: Strip ID not found!");
        return true;
    }

    auto* cfg = strip->getConfig();
    if (!cfg)
    {
        openknx.logger.log("ERROR: No config available for this strip");
        return true;
    }

    if (maskCmd == "init")
    {
        uint16_t ledCount = strip->getLedCount();
        cfg->initSkipMask(ledCount);
        openknx.logger.logWithPrefixAndValues("", "Skip mask initialized for %d LEDs", ledCount);
        openknx.logger.log("Use 'neo phys config <id> skipmask set <index> <0|1>' to mark LEDs");
    }
    else if (maskCmd == "clear")
    {
        cfg->clearSkipMask();
        openknx.logger.log("Skip mask cleared and memory freed");
    }
    else if (maskCmd == "set")
    {
        uint16_t index;
        int value;
        if (!(iss >> index >> value))
        {
            openknx.logger.log("ERROR: Usage: neo phys config <id> skipmask set <index> <0|1>");
            return true;
        }

        if (!cfg->hasSkipMask())
        {
            openknx.logger.log("ERROR: Skip mask not initialized. Use 'skipmask init' first");
            return true;
        }

        cfg->setLedSkip(index, value != 0);
        const char* status = (value != 0) ? "SKIPPED" : "ENABLED";
        openknx.logger.logWithPrefixAndValues("", "LED#%d: %s", index, status);
    }
    else if (maskCmd == "list")
    {
        if (!cfg->hasSkipMask())
        {
            openknx.logger.log("Skip mask not initialized");
            return true;
        }

        uint16_t count = cfg->getSkipMaskCount();
        openknx.logger.log("");
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logWithPrefixAndValues("", "Skip Mask - %d LEDs marked for skipping", count);
        openknx.logger.color(0);

        if (count == 0)
        {
            openknx.logger.log("No LEDs marked for skipping");
        }
        else
        {
            openknx.logger.log("Skipped LEDs:");
            uint16_t ledCount = strip->getLedCount();
            for (uint16_t i = 0; i < ledCount; i++)
            {
                if (cfg->isLedSkipped(i))
                {
                    openknx.logger.logWithPrefixAndValues("  ", "LED#%d", i);
                }
            }
        }
    }
    else
    {
        openknx.logger.log("ERROR: Unknown skipmask command. Use init|clear|set|list");
    }

    return true;
}
