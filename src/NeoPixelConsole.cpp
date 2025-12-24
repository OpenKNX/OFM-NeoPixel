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

// Standard library includes
#include <sstream>

// Effect system includes
#include "effects/Effect.h"
#include "effects/EffectPool.h"
#include "effects/GarageDoorEffect.h"

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

// ============================================================================
// Console Command Interface
// ============================================================================
/**
 * @brief Show help for console commands
 */
void NeoPixel::showHelp()
{
    openknx.console.printHelpLine("neo", "NeoPixel LED Control Module. Use 'neo ?' for more.");
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
        openknx.console.printHelpLine("neo virt add ?", "Create virtual strip (RGB or RGBW)");
        openknx.console.printHelpLine("neo virt del <i>", "Delete virtual strip by ID");

        printHelpSectionHeader("Segment Management");
        openknx.console.printHelpLine("neo seg ?", "Show detailed Segment commands");
        openknx.console.printHelpLine("neo seg list", "List all segments");
        openknx.console.printHelpLine("neo seg add ?", "Create segment on virtual strip");
        openknx.console.printHelpLine("neo seg del <i>", "Delete segment by ID");

        printHelpSectionHeader("Power Management");
        openknx.console.printHelpLine("neo power ?", "Show detailed Power commands");
        openknx.console.printHelpLine("neo power status", "Show current consumption and power limit");
        openknx.console.printHelpLine("neo  |off", "Enable/disable current limiting");

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
        openknx.console.printHelpLine("timings", "List all available timing modes");
        openknx.console.printHelpLine("timing <i>", "Show current timing mode for strip");
        openknx.console.printHelpLine("timing <i> <mode>", "Set timing (auto|legacy|slow5-20|fast5-25)");
        openknx.console.printHelpLine("timing <i> info", "Show detailed timing information");
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
        printDetailHelpEnd();
        return true;
    }

    // Detail help: VirtualStrip
    if (command == "neo virt ?" || command == "neo virt help")
    {
        printDetailHelpHeader("VirtualStrip Commands");
        openknx.console.printHelpLine("list", "List all virtual strips");
        openknx.console.printHelpLine("add <n> [type]", "Create virtual strip (RGB or RGBW, default: RGB)");
        openknx.console.printHelpLine("del <i>", "Delete virtual strip by ID");
        openknx.console.printHelpLine("attach <v> <p>", "Attach physical strip to virtual strip");
        openknx.console.printHelpLine("detach <v>", "Detach physical strip from virtual strip");
        printDetailHelpSeparator();
        printDetailHelpParameter("<i>=ID, <n>=LED Count, <v>=Virtual Strip ID, <p>=Physical Strip ID");
        printDetailHelpExample("neo virt add 72 rgb      Create RGB virtual strip with 72 LEDs");
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
        printDetailHelpSeparator();
        printDetailHelpParameter("<i>=Segment ID, <v>=Virtual Strip ID, <start>/<end>=LED Position");
        printDetailHelpExample("neo seg add 0 0 35       Create segment on virtual strip 0, LEDs 0-35");
        printDetailHelpExample("neo seg pause 0          Pause segment 0 (freeze animation)");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Power
    if (command == "neo power ?" || command == "neo power help")
    {
        printDetailHelpHeader("Power Management Commands");
        openknx.console.printHelpLine("status", "Show current consumption and power limit");
        openknx.console.printHelpLine("limit <mA>", "Set maximum current limit (e.g., 5000 for 5A)");
        openknx.console.printHelpLine("profile <type>", "Set LED profile: ws2812b|sk6812|apa102|conservative");
        openknx.console.printHelpLine("on|off", "Enable/disable current limiting");
        printDetailHelpSeparator();
        printDetailHelpParameter("<mA>=Milliampere, <type>=Profile Type");
        printDetailHelpExample("neo power limit 5000     Set maximum current to 5A (5000mA)");
        printDetailHelpExample("neo power profile ws2812b   Use WS2812B power profile");
        printDetailHelpExample("neo power on             Enable current limiting");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Effect
    if (command == "neo effect ?" || command == "neo effect help")
    {
        printDetailHelpHeader("Effect Commands");
        openknx.console.printHelpLine("effects", "List all available effects");
        openknx.console.printHelpLine("effect <s> <eff>", "Assign effect to segment");
        openknx.console.printHelpLine("effect config <s>", "Show effect parameters");
        openknx.console.printHelpLine("effect config <s> get <i>", "Get parameter value");
        openknx.console.printHelpLine("effect config <s> set <i> <v>", "Set parameter value");
        openknx.console.printHelpLine("garage <s> <phase>", "GarageDoor: 0=OPENING 1=RUNWAY 2=DONE 3=STOP");
        printDetailHelpSeparator();
        printDetailHelpParameter("<s>=Segment ID, <eff>=Effect Name, <i>=Parameter Index, <v>=Value");
        printDetailHelpExample("neo effect 0 rainbow     Assign rainbow effect to segment 0");
        printDetailHelpExample("neo effect config 0      Show all parameters of segment 0 effect");
        printDetailHelpExample("neo effect config 0 set 0 100   Set parameter 0 to value 100");
        printDetailHelpEnd();
        return true;
    }

    // Detail help: Color
    if (command == "neo color ?" || command == "neo color help")
    {
        printDetailHelpHeader("Color Commands");
        openknx.console.printHelpLine("color <s> <r> <g> <b> [w]", "Set segment color (0-255, w optional for RGBW)");
        printDetailHelpSeparator();
        printDetailHelpParameter("<s>=Segment ID, <r>=Red, <g>=Green, <b>=Blue, <w>=White (0-255)");
        printDetailHelpExample("neo color 0 255 0 0      Set segment 0 to red");
        printDetailHelpExample("neo color 1 0 255 128 200   Set RGBW segment 1 to green+blue+white");
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

    return false;
}

// ============================================================================
// Command Handlers
// ============================================================================
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
    // ESP32-S3: RMT Channels
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
        uint32_t targetFPS = 1000 / _updateInterval;
        openknx.logger.logWithValues("  Target FPS:      %d Hz (%d ms)", targetFPS, _updateInterval);

        // Show actual FPS if performance data available
        if (g_perfTracker.hasData())
        {
            float actualFPS = g_perfTracker.getCurrentFPS(_updateInterval);
            uint32_t avgUpdateTime = g_perfTracker.getAverageTime();
            float cpuLoad = (avgUpdateTime * targetFPS / 1000000.0f) * 100.0f;

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
        openknx.logger.log("  Use 'neo add <gpio_pin> <led_count>' to create one.");
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
                                             "WS2812B", // TODO: Get protocol name
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
    using namespace OpenKNX::NeoPixel;

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

    // Calculate statistics
    uint32_t avgUpdateTime = g_perfTracker.getAverageTime();
    float currentFPS = g_perfTracker.getCurrentFPS(_updateInterval);
    uint32_t uptime = g_perfTracker.getUptimeSeconds();
    uint32_t targetFPS = 1000 / _updateInterval;
    float cpuLoad = (avgUpdateTime * targetFPS / 1000000.0f) * 100.0f;

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
    openknx.logger.logWithValues("  Target FPS:      %lu Hz", targetFPS);
    openknx.logger.logWithValues("  Min Update:      %lu µs",
                                 g_perfTracker.minUpdateTime == UINT32_MAX ? 0 : g_perfTracker.minUpdateTime);
    openknx.logger.logWithValues("  Max Update:      %lu µs", g_perfTracker.maxUpdateTime);
    openknx.logger.logWithValues("  Avg Update:      %lu µs", avgUpdateTime);

    // Calculate time budget
    uint32_t frameBudget = _updateInterval * 1000; // µs per frame
    float budgetUsed = (avgUpdateTime * 100.0f) / frameBudget;
    openknx.logger.logWithValues("  Frame Budget:    %lu µs (%.1f%% used)", frameBudget, budgetUsed);
    openknx.logger.log("");

    // CPU Load Analysis
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("CPU Load:");
    openknx.logger.color(0);

    openknx.logger.logWithValues("  CPU Usage:       %.2f%%", cpuLoad);
    openknx.logger.logWithValues("  Free CPU:        %.2f%%", 100.0f - cpuLoad);

    // Throughput calculation (LEDs updated per second)
    uint32_t throughput = totalLEDs * currentFPS;
    openknx.logger.logWithValues("  Throughput:      %lu LEDs/sec", throughput);

    if (avgUpdateTime < 500)
    {
        openknx.logger.log("  Status:          DMA working perfectly!");
    }
    else if (avgUpdateTime < 1000)
    {
        openknx.logger.log("  Status:          OK - Highly optimized.");
    }
    else if (avgUpdateTime < 2000)
    {
        openknx.logger.log("  Status:          OK - PIO mode optimized.");
    }
    else if (avgUpdateTime < 5000)
    {
        openknx.logger.log("  Status:          NOK - Check for blocking calls.");
    }
    else
    {
        openknx.logger.log("  Status:          To Slow! Check configuration!");
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
                    case ColorOrder::RGB: colorOrder = "RGB"; break;
                    case ColorOrder::RBG: colorOrder = "RBG"; break;
                    case ColorOrder::GRB: colorOrder = "GRB"; break;
                    case ColorOrder::BGR: colorOrder = "BGR"; break;
                    case ColorOrder::GBR: colorOrder = "GBR"; break;
                    case ColorOrder::BRG: colorOrder = "BRG"; break;
                    case ColorOrder::RGBW: colorOrder = "RGBW"; break;
                    case ColorOrder::GRBW: colorOrder = "GRBW"; break;
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
        openknx.logger.log("  SPI (APA102):            neo phys add <clk> <count> apa102 <data>");
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
        openknx.logger.log("ID │ Total LEDs │ Order │ Attached │ Status");
        openknx.logger.log("───┼────────────┼───────┼──────────┼────────");

        for (uint32_t i = 0; i < count; i++)
        {
            auto vstrip = _manager->getVirtualStrip(i);
            if (vstrip)
            {
                // Display buffer format (RGB vs RGBW)
                const char* bufferFormat = vstrip->hasWhiteChannel() ? "RGBW" : "RGB";

                openknx.logger.logWithValues("%2d │ %10d │ %5s │ %8s │ %s",
                                             i,
                                             vstrip->getLedCount(),
                                             bufferFormat,
                                             vstrip->getPhysicalStripCount() > 0 ? "Yes" : "No",
                                             "OK" // TODO: Add proper status check
                );
            }
        }
    }

    printSectionSeparator();
    openknx.logger.log("");

    return true;
}

/**
 * @brief Process 'neo virt add <leds> [type]' command
 * Type: RGB (3 bytes/LED) or RGBW (4 bytes/LED), default: RGB
 *
 * VirtualStrip stores pixels in RGB/RGBW format internally.
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
        openknx.logger.log("       type: RGB (default) or RGBW");
        openknx.logger.log("       Note: VirtualStrip stores pixels in RGB/RGBW format");
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
        else
        {
            openknx.logger.log("ERROR: Invalid type! Use: RGB or RGBW");
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
        openknx.logger.log("ID │ Range     │ State   │ Effect │ Effect Name      │ Color R:G:B (W)");
        openknx.logger.log("───┼───────────┼─────────┼────────┼──────────────────┼───────────────────");
        for (uint32_t i = 0; i < count; i++)
        {
            auto seg = _manager->getSegment(i);
            if (seg)
            {
                auto effect = seg->getEffect();
                auto effectName = effect ? effect->getName() : "None";
                auto& config = seg->getConfig();
                const char* state = seg->isPaused() ? "Paused" : "Running";

                openknx.logger.logWithValues("%2d │ %3d - %3d │ %-7s │ %-6s │ %-16s │ %3d:%3d:%3d (%3d)",
                                             i,
                                             seg->getStartLed(),
                                             seg->getEndLed(),
                                             state,
                                             effect ? "Set" : "N/A",
                                             effect ? effectName : "N/A",
                                             (config.primaryRGBW >> 24) & 0xFF, // Red from RGBW
                                             (config.primaryRGBW >> 16) & 0xFF, // Green from RGBW
                                             (config.primaryRGBW >> 8) & 0xFF,  // Blue from RGBW
                                             config.primaryRGBW & 0xFF          // White from RGBW
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
    openknx.logger.log("Use 'neo effect <seg> <eff>' to assign");
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
    uint32_t value;

    // Parse: <seg> or <seg> get <idx> or <seg> set <idx> <val>
    int parsed = sscanf(args.c_str(), "%d %7s %d %u", &segId, cmd, &paramIdx, &value);

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
            uint32_t val = effect->getParameter(seg, i);
            uint32_t def = effect->getParameterDefault(i);
            openknx.logger.logWithValues("  [%d] %-15s = %u (default: %u)", i, name, val, def);
        }
        return true;
    }

    // Get single parameter
    if (strcmp(cmd, "get") == 0 && parsed >= 3)
    {
        if (paramIdx >= count)
        {
            openknx.logger.logWithValues("ERROR: Index %d out of range (0-%d)", paramIdx, count - 1);
            return true;
        }
        uint32_t val = effect->getParameter(seg, paramIdx);
        openknx.logger.logWithValues("%s.%s = %u",
                                     effect->getName(),
                                     effect->getParameterName(paramIdx),
                                     val);
        return true;
    }

    // Set parameter
    if (strcmp(cmd, "set") == 0 && parsed >= 4)
    {
        if (paramIdx >= count)
        {
            openknx.logger.logWithValues("ERROR: Index %d out of range (0-%d)", paramIdx, count - 1);
            return true;
        }
        effect->setParameter(seg, paramIdx, value);
        openknx.logger.logWithValues("Set %s.%s = %u",
                                     effect->getName(),
                                     effect->getParameterName(paramIdx),
                                     value);
        return true;
    }

    openknx.logger.log("Usage: neo effect config <seg> [get/set <idx> [val]]");
    return true;
}

/**
 * @brief Process 'neo effect <str_action> <seg> <eff>' command
 * Assign or control effects on a specific segment
 * str_action values: set, stop, clear, pause, resume
 *     set: assign effect to segment
 *     stop: stop effect (pauses and clears segment)
 *     clear: remove effect from segment
 *     pause: pause effect (freezes current state)
 *     resume: resume paused effect
 * seg: segment ID
 * eff: effect ID
 */
bool NeoPixel::processEffectCommand(const std::string& args)
{
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <str_action> <seg_id> <effect_id>
    // str_action values: set, stop, clear, pause, resume
    std::string action;
    int segId, effId;

    // ToDo: check if is empty or with argument ?
    if (args.empty() || args.compare("?") == 0)
    {
        openknx.logger.log("ERROR: Usage: neo effect <seg_id> <effect_id>");
        openknx.logger.log("Use 'neo effects' to see available effects");
        openknx.logger.log("");
        return true;
    }

    // action and segId are mandatory
    char _action[7] = "";
    if (sscanf(args.c_str(), "%s %d", _action, &segId) != 2)
    {
        openknx.logger.log("ERROR! Action and Segment ID must be provided!");
        return true;
    }
    action = std::string(_action);

    if (action.compare("set") == 0 && sscanf(args.c_str(), "%*s %d %d", &segId, &effId) != 2)
    {
        openknx.logger.log("ERROR! Action 'set' requires Segment ID and Effect ID!");
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
    if (action.compare("set") == 0)
    {
        Effect* effect = nullptr;
        switch (effId) // Get effect from pool - matches ETS enumeration values
        {
            case 0: // Solid
                effect = EffectPool::getSolid();
                break;
            case 1: // Wipe
                effect = EffectPool::getWipe();
                break;
            case 2: // Rainbow
                effect = EffectPool::getRainbow();
                break;
            case 3: // Pride2015
                effect = EffectPool::getPride();
                break;
            case 4: // Confetti
                effect = EffectPool::getConfetti();
                break;
            case 5: // Juggle
                effect = EffectPool::getJuggle();
                break;
            case 6: // BPM
                effect = EffectPool::getBPM();
                break;
            case 7: // Cylon
                effect = EffectPool::getCylon();
                break;
            case 8: // SK6812Test
                effect = EffectPool::getRGBWTest();
                break;
            case 9: // GarageDoor
                effect = EffectPool::getGarageDoor();
                break;
#ifndef NEOPIXEL_MINIMAL_EFFECTS
            case 10: // Fire
                effect = EffectPool::getFire();
                break;
            case 11: // Theater Chase
                effect = EffectPool::getTheaterChase();
                break;
            case 12: // Theater Chase Rainbow
                effect = EffectPool::getTheaterChaseRainbow();
                break;
            case 13: // Sinelon
                effect = EffectPool::getSinelon();
                break;
            case 14: // Twinkle
                effect = EffectPool::getTwinkle();
                break;
            case 15: // Sparkle
                effect = EffectPool::getSparkle();
                break;
            case 16: // Breathing
                effect = EffectPool::getBreathing();
                break;
            case 17: // Strobe
                effect = EffectPool::getStrobe();
                break;
            case 18: // Pulse
                effect = EffectPool::getPulse();
                break;
            case 19: // Comet
                effect = EffectPool::getComet();
                break;
            case 20: // Meteor
                effect = EffectPool::getMeteor();
                break;
#endif
            default:
                openknx.logger.logWithValues("ERROR: Effect ID %d not found!", effId);
                openknx.logger.log("       Use 'neo effects' to see available effects");
                return true;
        }
        if (!effect)
        {
            openknx.logger.log("ERROR: Failed to get effect from pool!");
            return true;
        }

        // Assign effect to segment, replacing existing one if necessary.
        if (hasEffect)
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
    }
    else if (action.compare("clear") == 0)

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
    if (!_initialized || !_manager)
    {
        openknx.logger.log("ERROR: NeoPixel module not initialized!");
        return true;
    }

    // Parse arguments: <seg_id> <phase>
    int segId, phase;
    if (sscanf(args.c_str(), "%d %d", &segId, &phase) != 2)
    {
        openknx.logger.log("ERROR: Usage: neo garage <seg_id> <phase>");
        openknx.logger.log("       Phases: 0=OPENING, 1=RUNWAY, 2=COMPLETED, 3=STOPPED");
        return true;
    }

    // Validate phase
    if (phase < 0 || phase > 3)
    {
        openknx.logger.log("ERROR: Phase must be 0-3 (OPENING/RUNWAY/COMPLETED/STOPPED)");
        return true;
    }

    // Get segment
    auto seg = _manager->getSegment(segId);
    if (!seg)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] not found!", segId);
        return true;
    }

    // Get GarageDoorEffect from segment
    auto effect = seg->getEffect();
    if (!effect)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] has no effect assigned!", segId);
        openknx.logger.log("       Use 'neo effect <seg> 9' to assign GarageDoor effect");
        return true;
    }

    // Check if it's GarageDoorEffect (no RTTI, check by name)
    GarageDoorEffect* garageEffect = (strcmp(effect->getName(), "GarageDoor") == 0) ? static_cast<GarageDoorEffect*>(effect) : nullptr;
    if (!garageEffect)
    {
        openknx.logger.logWithValues("ERROR: Segment [%d] does not have GarageDoor effect!", segId);
        openknx.logger.logWithValues("       Current effect: %s", effect->getName());
        openknx.logger.log("       Use 'neo effect <seg> 9' to assign GarageDoor effect");
        return true;
    }

    // Set phase for this segment
    GaragePhase newPhase = (GaragePhase)phase;
    garageEffect->setSegmentPhase(seg, newPhase);

    const char* phaseNames[] = {"OPENING", "RUNWAY", "COMPLETED", "STOPPED"};
    openknx.logger.logWithValues("Segment [%d] GarageDoor phase set to: %s", segId, phaseNames[phase]);

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

    // Set software brightness
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

    // neo power (show status)
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
        printHelpSectionHeader("Power Management Status");

        // Current limiting status
        openknx.logger.logWithValues("Status:           %s", pm->isEnabled() ? "ENABLED" : "DISABLED");
        openknx.logger.logWithValues("Max Current:      %u mA (%.2f A)", pm->getMaxCurrent(), pm->getMaxCurrent() / 1000.0f);

        // LED profile
        const char* profileName = "CUSTOM";
        LedCurrentProfile profile = pm->getLedProfile();
        if (profile == LedProfiles::WS2812B) profileName = "WS2812B";
        else if (profile == LedProfiles::SK6812_RGBW)
            profileName = "SK6812 RGBW";
        else if (profile == LedProfiles::APA102)
            profileName = "APA102";
        else if (profile == LedProfiles::CONSERVATIVE)
            profileName = "CONSERVATIVE";

        openknx.logger.logWithValues("LED Profile:      %s (R:%umA G:%umA B:%umA W:%umA)",
                                     profileName, profile.redMA, profile.greenMA,
                                     profile.blueMA, profile.whiteMA);

        // Current consumption - REQUESTED (before limiting)
        float totalPower = _manager->getTotalPowerWatts();
        uint32_t requestedCurrent = pm->getLastCalculatedCurrent();
        openknx.logger.logWithValues("Requested Power:  %.2f W @ 5V", totalPower);
        openknx.logger.logWithValues("Requested Current:%u mA", requestedCurrent);

        // Actual consumption - AFTER limiting
        float actualPower = pm->getActualPowerWatts();
        uint32_t actualCurrent = pm->getActualCurrent();
        openknx.logger.logWithValues("Actual Power:     %.2f W @ 5V", actualPower);
        openknx.logger.logWithValues("Actual Current:   %u mA", actualCurrent);

        // Show if limiting is active
        if (pm->isEnabled() && requestedCurrent > pm->getMaxCurrent())
        {
            float scale = (float)actualCurrent / requestedCurrent;
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.logWithValues("WARNING: CURRENT LIMITING ACTIVE - Brightness scaled to %.1f%%", scale * 100.0f);
            openknx.logger.color(0);
        }

        printSectionSeparator();
        openknx.logger.log("");
        openknx.logger.end();
        return true;
    }

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

        if (!found)
        {
            openknx.logger.log("[ERROR] Unknown profile. Use: ws2812b|sk6812|apa102|conservative");
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
    openknx.logger.log("[ERROR] Unknown power command. Use: neo power status|limit|profile|on|off");
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
    const int totalWidth = 77;  // Max console width (safe buffer size)
    int titleLen = strlen(title);
    int sidesWidth = totalWidth - titleLen - 2;  // 2 spaces around title
    int leftWidth = sidesWidth / 2;
    int rightWidth = sidesWidth - leftWidth;
    
    // Build header with dynamic border
    char header[80];
    int pos = 0;
    for (int i = 0; i < leftWidth && pos < 79; i++) header[pos++] = '=';
    if (pos < 79) header[pos++] = ' ';
    for (int i = 0; i < titleLen && pos < 79; i++) header[pos++] = title[i];
    if (pos < 79) header[pos++] = ' ';
    for (int i = 0; i < rightWidth && pos < 79; i++) header[pos++] = '=';
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

    openknx.logger.log("ID │ Mode Name       │ Target Bitrate │ Description");
    openknx.logger.log("───┼─────────────────┼────────────────┼───────────────────────────");
    openknx.logger.log(" 0 │ AUTO            │ 800 kHz        │ Auto-detect (default)");
    openknx.logger.log(" 1 │ AUTO_LEGACY     │ 960 kHz*       │ WS2812C/D onboard LEDs");
    openknx.logger.log(" 2 │ SLOW_20PCT      │ 640 kHz        │ -20% for signal issues");
    openknx.logger.log(" 3 │ SLOW_15PCT      │ 680 kHz        │ -15%");
    openknx.logger.log(" 4 │ SLOW_10PCT      │ 720 kHz        │ -10%");
    openknx.logger.log(" 5 │ SLOW_5PCT       │ 760 kHz        │ -5%");
    openknx.logger.log(" 6 │ FAST_5PCT       │ 840 kHz        │ +5% performance boost");
    openknx.logger.log(" 7 │ FAST_10PCT      │ 880 kHz        │ +10%");
    openknx.logger.log(" 8 │ FAST_15PCT      │ 920 kHz        │ +15%");
    openknx.logger.log(" 9 │ FAST_20PCT      │ 960 kHz        │ +20%");
    openknx.logger.log("10 │ FAST_25PCT      │ 1000 kHz       │ +25% maximum");
    openknx.logger.log("");
    openknx.logger.log("* LEGACY mode uses fixed clkdiv, actual bitrate depends on CPU frequency");
    openknx.logger.log("");

    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("Usage Examples:");
    openknx.logger.color(0);
    openknx.logger.log("  neo phys timing 0             -> Show current timing for strip 0");
    openknx.logger.log("  neo phys timing 0 auto        -> Set to AUTO mode");
    openknx.logger.log("  neo phys timing 0 legacy      -> Set to AUTO_LEGACY");
    openknx.logger.log("  neo phys timing 0 fast25      -> Set to FAST_25PCT (1 MHz)");
    openknx.logger.log("  neo phys timing 0 info        -> Detailed timing information");

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
        openknx.logger.log("ERROR: Usage: neo phys timing <id> [mode|info]");
        return true;
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
        uint8_t timingMode = static_cast<uint8_t>(serialCfg->getTimingMode());
        uint16_t t0h = serialCfg->getT0H();
        uint16_t t0l = serialCfg->getT0L();
        uint16_t t1h = serialCfg->getT1H();
        uint16_t t1l = serialCfg->getT1L();
        uint32_t reset = serialCfg->getResetTime();

        openknx.logger.log("Type: Serial Strip (WS2812B/SK6812)");
        openknx.logger.log("");

        openknx.logger.logWithValues("Timing Mode: %d", (int)timingMode);
        openknx.logger.logWithValues("T0H (0-bit high): %d ns", t0h);
        openknx.logger.logWithValues("T0L (0-bit low): %d ns", t0l);
        openknx.logger.logWithValues("T1H (1-bit high): %d ns", t1h);
        openknx.logger.logWithValues("T1L (1-bit low): %d ns", t1l);
        openknx.logger.logWithValues("Reset Time: %d µs", reset);

        openknx.logger.log("");
        openknx.logger.log("Tip: Use 'neo phys timing <id> auto' to auto-detect optimal timing");
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