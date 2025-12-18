#ifdef OPENKNX_NEOPIXEL_TESTS
    #include "test/AnimationTest.h"
    #include "OpenKNX/Log/Logger.h"
    #include <Arduino.h>

// Rainbow lookup table
static uint8_t rainbowR[8];
static uint8_t rainbowG[8];
static uint8_t rainbowB[8];

void AnimationTest::initRainbowTable(uint8_t brightness)
{
    rainbowR[0] = brightness;
    rainbowG[0] = 0;
    rainbowB[0] = 0;
    rainbowR[1] = brightness;
    rainbowG[1] = brightness / 2;
    rainbowB[1] = 0;
    rainbowR[2] = brightness / 2;
    rainbowG[2] = brightness;
    rainbowB[2] = 0;
    rainbowR[3] = 0;
    rainbowG[3] = brightness;
    rainbowB[3] = 0;
    rainbowR[4] = 0;
    rainbowG[4] = brightness / 2;
    rainbowB[4] = brightness / 2;
    rainbowR[5] = 0;
    rainbowG[5] = 0;
    rainbowB[5] = brightness;
    rainbowR[6] = brightness / 2;
    rainbowG[6] = 0;
    rainbowB[6] = brightness;
    rainbowR[7] = brightness;
    rainbowG[7] = 0;
    rainbowB[7] = brightness / 2;
}

void AnimationTest::print_separator(const char* title)
{
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("═══════════════════════════════════════════════════");
    openknx.logger.color(0);
    if (title)
    {
        openknx.logger.logWithValues("  %s", title);
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("═══════════════════════════════════════════════════");
        openknx.logger.color(0);
    }
}
void AnimationTest::print_end_separator()
{
    openknx.logger.color(CONSOLE_HEADLINE_COLOR);
    openknx.logger.log("═══════════════════════════════════════════════════");
    openknx.logger.color(0);
    openknx.logger.log("");
}
bool AnimationTest::init(
    NeoPixelManager* manager,
    uint8_t pin1,
    uint8_t ledCount1,
    uint8_t pin2,
    uint8_t ledCount2,
    bool virtualMode,
    uint8_t brightness)
{
    if (_running)
    {
        logInfoP("AnimationTest already running!");
        return false;
    }

    logInfoP("Animation Test Initialization");
    logInfoP("Mode: %s", virtualMode ? "Virtual 9x8" : "Separate Strips");
    logInfoP("Hardware: GPIO%d=%d LEDs, GPIO%d=%d LEDs", pin1, ledCount1, pin2, ledCount2);
    logInfoP("Brightness: %d/255", brightness);

    _manager = manager;
    _pin1 = pin1;
    _pin2 = pin2;
    _ledCount1 = ledCount1;
    _ledCount2 = ledCount2;
    _virtualMode = virtualMode;
    _brightness = brightness;

    // Initialize rainbow table
    initRainbowTable(brightness);

    // Create physical strips
    _physStrip1 = _manager->addStrip(_pin1, _ledCount1, LedProtocol::WS2812B, DriverType::AUTO);
    if (!_physStrip1)
    {
        logErrorP("Failed to create physical strip 1!");
        return false;
    }

    _physStrip2 = _manager->addStrip(_pin2, _ledCount2, LedProtocol::WS2812B, DriverType::AUTO);
    if (!_physStrip2)
    {
        logErrorP("Failed to create physical strip 2!");
        return false;
    }

    // Create virtual strips
    _strip8 = _manager->addVirtualStrip(_ledCount1, ColorOrder::GRB);
    if (!_strip8 || !_manager->attachPhysicalToVirtual(_strip8, _physStrip1, 0))
    {
        logErrorP("Failed to create virtual strip 1!");
        return false;
    }

    _matrix8x8 = _manager->addVirtualStrip(_ledCount2, ColorOrder::GRB);
    if (!_matrix8x8 || !_manager->attachPhysicalToVirtual(_matrix8x8, _physStrip2, 0))
    {
        logErrorP("Failed to create virtual strip 2!");
        return false;
    }

    // NOW re-initialize manager with the newly added strips!
    if (!_manager->init())
    {
        logErrorP("Failed to re-initialize manager with test strips!");
        return false;
    }

    // Test initial LED
    if (_virtualMode)
    {
        _strip8->setPixel(0, _brightness, 0, 0);    // RED
        _matrix8x8->setPixel(0, 0, _brightness, 0); // GREEN
    }
    else
    {
        _strip8->setPixel(0, _brightness, 0, 0);    // RED
        _matrix8x8->setPixel(0, 0, _brightness, 0); // GREEN
    }
    _strip8->show();
    _matrix8x8->show();
    delay(100);

    // Reset stats
    _testStartTime = millis();
    _frameCount = 0;
    _lastUpdateTime = _testStartTime;
    _lastLogTime = _testStartTime;
    _minUpdateTime = UINT32_MAX;
    _maxUpdateTime = 0;
    _totalUpdateTime = 0;
    _animPhase = 0;
    _lastPhaseChange = _testStartTime;

    _running = true;
    logInfoP("AnimationTest Init COMPLETE");

    return true;
}

void AnimationTest::start()
{
    if (_running)
    {
        logInfoP("AnimationTest already running");
        return;
    }

    if (!_manager || !_strip8 || !_matrix8x8)
    {
        logErrorP("Cannot start - not initialized!");
        return;
    }

    logInfoP("AnimationTest starting...");

    // Reset stats
    _testStartTime = millis();
    _frameCount = 0;
    _lastUpdateTime = _testStartTime;
    _lastLogTime = _testStartTime;
    _minUpdateTime = UINT32_MAX;
    _maxUpdateTime = 0;
    _totalUpdateTime = 0;
    _animPhase = 0;
    _lastPhaseChange = _testStartTime;

    _running = true;
    logInfoP("AnimationTest started");
}

bool AnimationTest::loop()
{
    if (!_running || !_manager || !_strip8 || !_matrix8x8)
    {
        return false;
    }

    uint32_t now = millis();

    // Check update interval
    if ((now - _lastUpdateTime) < _updateInterval)
    {
        return false;
    }

    _lastUpdateTime = now;
    uint32_t updateStart = micros();

    // Run animation based on mode
    if (_virtualMode)
    {
        runVirtualAnimation();
    }
    else
    {
        runSeparateAnimation();
    }

    // Update manager
    _manager->update(_updateInterval);

    uint32_t updateTime = micros() - updateStart;
    _frameCount++;

    updatePerformanceStats(updateTime);

    // Periodic logging
    if ((now - _lastLogTime) >= _logInterval)
    {
        _lastLogTime = now;
        logPerformanceStats();
    }

    return true;
}

void AnimationTest::stop()
{
    if (!_running) return;

    logInfoP("AnimationTest stopping...");

    // Clear buffers and SEND to hardware!
    if (_strip8)
    {
        _strip8->clear();
        _strip8->show(); // ← Must send to hardware!
    }
    if (_matrix8x8)
    {
        _matrix8x8->clear();
        _matrix8x8->show(); // ← Must send to hardware!
    }

    _running = false;
    logInfoP("AnimationTest stopped");
}

void AnimationTest::cleanup()
{
    stop();

    _manager = nullptr;
    _strip8 = nullptr;
    _matrix8x8 = nullptr;
    _physStrip1 = nullptr;
    _physStrip2 = nullptr;
}

void AnimationTest::runVirtualAnimation()
{
    uint32_t now = millis();

    // Change phase every 2 seconds
    if (now - _lastPhaseChange > 2000)
    {
        _animPhase = (_animPhase + 1) % 5;
        _lastPhaseChange = now;
        logDebugP("Virtual Animation Phase %d", _animPhase);
    }

    switch (_animPhase)
    {
        case 0:
        {
            // LED SWEEP (0-71)
            _strip8->clear();
            _matrix8x8->clear();

            uint8_t totalLed = (now / 100) % 72;
            if (totalLed < 64)
            {
                _matrix8x8->setPixel(totalLed, _brightness, _brightness, _brightness);
            }
            else
            {
                uint8_t virtualIdx = totalLed - 64;
                uint8_t physicalIdx = 7 - virtualIdx;
                _strip8->setPixel(physicalIdx, _brightness, _brightness, _brightness);
            }
            break;
        }

        case 1:
        {
            // RED ROWS (9 rows)
            _strip8->clear();
            _matrix8x8->clear();

            uint8_t row = (now / 500) % 9;
            if (row < 8)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx, _brightness, 0, 0);
                }
            }
            else
            {
                for (uint8_t i = 0; i < 8; i++)
                {
                    _strip8->setPixel(i, _brightness, 0, 0);
                }
            }
            break;
        }

        case 2:
        {
            // GREEN ROWS
            _strip8->clear();
            _matrix8x8->clear();

            uint8_t row = (now / 500) % 9;
            if (row < 8)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx, 0, _brightness, 0);
                }
            }
            else
            {
                for (uint8_t i = 0; i < 8; i++)
                {
                    _strip8->setPixel(i, 0, _brightness, 0);
                }
            }
            break;
        }

        case 3:
        {
            // BLUE ROWS
            _strip8->clear();
            _matrix8x8->clear();

            uint8_t row = (now / 500) % 9;
            if (row < 8)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx, 0, 0, _brightness);
                }
            }
            else
            {
                for (uint8_t i = 0; i < 8; i++)
                {
                    _strip8->setPixel(i, 0, 0, _brightness);
                }
            }
            break;
        }

        case 4:
        {
            // RAINBOW ROWS
            for (uint8_t row = 0; row < 8; row++)
            {
                uint8_t colorIdx = row % 8;
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx,
                                         rainbowR[colorIdx],
                                         rainbowG[colorIdx],
                                         rainbowB[colorIdx]);
                }
            }

            for (uint8_t i = 0; i < 8; i++)
            {
                _strip8->setPixel(i, rainbowR[0], rainbowG[0], rainbowB[0]);
            }
            break;
        }
    }
}

void AnimationTest::runSeparateAnimation()
{
    uint32_t now = millis();

    // Strip: Rainbow Chase
    uint8_t stripOffset = (now / 100) % 8;
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t colorIdx = (i + stripOffset) % 8;
        _strip8->setPixel(i, rainbowR[colorIdx], rainbowG[colorIdx], rainbowB[colorIdx]);
    }

    // Matrix: Color Test Phases
    if (now - _lastPhaseChange > 2000)
    {
        _animPhase = (_animPhase + 1) % 8;
        _lastPhaseChange = now;
        logDebugP("Matrix Test Phase %d", _animPhase);
    }

    switch (_animPhase)
    {
        case 0: // ALL RED
            for (uint8_t i = 0; i < 64; i++)
            {
                _matrix8x8->setPixel(i, _brightness, 0, 0);
            }
            break;

        case 1: // ALL GREEN
            for (uint8_t i = 0; i < 64; i++)
            {
                _matrix8x8->setPixel(i, 0, _brightness, 0);
            }
            break;

        case 2: // ALL BLUE
            for (uint8_t i = 0; i < 64; i++)
            {
                _matrix8x8->setPixel(i, 0, 0, _brightness);
            }
            break;

        case 3: // ALL WHITE
            for (uint8_t i = 0; i < 64; i++)
            {
                _matrix8x8->setPixel(i, _brightness, _brightness, _brightness);
            }
            break;

        case 4: // RAINBOW ROWS
            for (uint8_t row = 0; row < 8; row++)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx,
                                         rainbowR[row],
                                         rainbowG[row],
                                         rainbowB[row]);
                }
            }
            break;

        case 5: // RAINBOW COLUMNS
            for (uint8_t row = 0; row < 8; row++)
            {
                for (uint8_t col = 0; col < 8; col++)
                {
                    uint8_t ledIdx = row * 8 + col;
                    _matrix8x8->setPixel(ledIdx,
                                         rainbowR[col],
                                         rainbowG[col],
                                         rainbowB[col]);
                }
            }
            break;

        case 6:
        { // ROTATING PATTERN
            uint8_t offset = (now / 100) % 8;
            for (uint8_t i = 0; i < 64; i++)
            {
                uint8_t colorIdx = (i + offset) % 8;
                _matrix8x8->setPixel(i,
                                     rainbowR[colorIdx],
                                     rainbowG[colorIdx],
                                     rainbowB[colorIdx]);
            }
            break;
        }

        case 7: // ALL OFF
            _matrix8x8->clear();
            break;
    }
}

void AnimationTest::updatePerformanceStats(uint32_t updateTime)
{
    if (updateTime < _minUpdateTime) _minUpdateTime = updateTime;
    if (updateTime > _maxUpdateTime) _maxUpdateTime = updateTime;
    _totalUpdateTime += updateTime;
}

void AnimationTest::logPerformanceStats()
{
    uint32_t avgUpdateTime = (_frameCount > 0) ? (_totalUpdateTime / _frameCount) : 0;

    print_separator("AnimationTest Performance Stats");
    openknx.logger.logWithValues("Frame %lu", _frameCount);
    openknx.logger.logWithValues("Update Time: min=%lu µs, max=%lu µs, avg=%lu µs",
                                 _minUpdateTime, _maxUpdateTime, avgUpdateTime);
    openknx.logger.logWithValues("CPU Load: %.2f%% @ 20 Hz", (avgUpdateTime * 20.0f / 1000000.0f) * 100.0f);

    bool strip1DMA = _physStrip1 ? _physStrip1->getCapabilities().supportsDMA : false;
    bool strip2DMA = _physStrip2 ? _physStrip2->getCapabilities().supportsDMA : false;

    openknx.logger.logWithValues("DMA: Strip1=%s, Strip2=%s",
                                 strip1DMA ? "YES" : "NO",
                                 strip2DMA ? "YES" : "NO");

    if (avgUpdateTime < 500)
    {
        openknx.logger.log("DMA working as expected");
    }
    else if (avgUpdateTime < 2000)
    {
        openknx.logger.log("PIO mode working, but consider DMA for better performance.");
    }
    else
    {
        openknx.logger.log("Very slow updates! Check for blocking calls!");
    }

    // Reset min/max
    _minUpdateTime = UINT32_MAX;
    _maxUpdateTime = 0;

    print_end_separator();
}

void AnimationTest::printStats()
{
    print_separator("AnimationTest Statistics");
    if (!_running)
    {
        openknx.logger.log("AnimationTest not running!");
        return;
    }

    uint32_t avgUpdateTime = (_frameCount > 0) ? (_totalUpdateTime / _frameCount) : 0;
    uint32_t uptime = (millis() - _testStartTime) / 1000;
    openknx.logger.logWithValues("Mode:         %s", _virtualMode ? "Virtual 9x8" : "Separate");
    openknx.logger.logWithValues("Uptime:       %lu seconds", uptime);
    openknx.logger.logWithValues("Frames:       %lu", _frameCount);
    openknx.logger.logWithValues("FPS:          %.2f", uptime > 0 ? (float)_frameCount / uptime : 0.0f);
    openknx.logger.logWithValues("Update Time:  %lu µs (avg)", avgUpdateTime);
    openknx.logger.logWithValues("Brightness:   %d/255", _brightness);
    print_end_separator();
}
#endif // OPENKNX_NEOPIXEL_TESTS
