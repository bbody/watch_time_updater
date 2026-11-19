#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp32-hal-dac.h"
#endif

#include "Display.h"
#include "Icons.h"
#include "../app/ModeManager.h"
#include "../net/WiFiConnect.h"
#include "../time/GPS.h"
#include "../time/NTP.h"
#include "../time/TimeSource.h"
#include "../input/Button.h"

namespace Display {
    void showWifiScreen();
    void showGpsScreen();

    // Not TFT_* — Fri3d / some variants #define TFT_CS, TFT_DC, TFT_RST in pins_arduino.h
    constexpr uint8_t DISP_SCK  = 25;
    constexpr uint8_t DISP_MOSI = 26;
    constexpr uint8_t DISP_CS   = 14;
    constexpr uint8_t DISP_DC   = 27;
    constexpr uint8_t DISP_RST  = 18;

    constexpr int16_t SCREEN_W = 240;
    constexpr int16_t SCREEN_H = 240;
    constexpr uint32_t DISP_SPI_HZ = 12000000;
    constexpr bool DISP_INVERT = true;

    SPIClass dispSpi(HSPI);

    Adafruit_GC9A01A tft(
        &dispSpi,
        DISP_DC,
        DISP_CS,
        DISP_RST
    );

    namespace {
        constexpr uint32_t MODE_HOLD_MS = 2000;
        constexpr uint32_t ICON_PULSE_MS = 600;
        constexpr float ICON_PULSE_PEAK = 1.2f;

        constexpr int16_t HOLD_RING_RADIUS = 108;
        constexpr int16_t HOLD_START_DEG = 270;

        constexpr int16_t ICON_CY = 84;
        constexpr int16_t LABEL_Y = 128;
        constexpr int16_t SELECTION_BOTTOM_Y = 158;
        constexpr int16_t SELECTION_SIDE_W = 58;
        constexpr int16_t SELECTION_NAV_Y = 118;
        constexpr int16_t SELECTION_NAV_LEFT_X = SELECTION_SIDE_W / 2;
        constexpr int16_t SELECTION_NAV_RIGHT_X =
            SCREEN_W - SELECTION_SIDE_W / 2;
        constexpr int16_t SELECTION_ENTER_CY = 198;
        constexpr int16_t SELECTION_ENTER_R = 7;
        constexpr int16_t ICON_CLEAR_X = 46;
        constexpr int16_t ICON_CLEAR_Y = 50;
        constexpr int16_t ICON_CLEAR_W = 148;
        constexpr int16_t ICON_CLEAR_H = 98;

        bool modeHoldActive = false;
        uint32_t modeHoldStartedAt = 0;
        int lastHoldSweepDeg = -1;

        bool iconPulseActive = false;
        uint32_t iconPulseStartedAt = 0;
        float iconPulseScale = 1.0f;
        int lastRenderedPulseStep = -1;

        GFXcanvas16 iconCanvas(ICON_CLEAR_W, ICON_CLEAR_H);

        void stopIconPulse();

        void resetModeChangeHold() {
            modeHoldActive = false;
            lastHoldSweepDeg = -1;
        }

        int holdSweepFromElapsed(uint32_t elapsedMs, uint32_t durationMs) {
            if (elapsedMs >= durationMs) {
                return 360;
            }

            return (int)((360.0f * elapsedMs) / durationMs);
        }

        uint16_t colorForSweep(int sweepDeg) {
            float t = sweepDeg / 360.0f;
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }

            uint8_t r = 0;
            uint8_t g = 0;
            constexpr uint8_t b = 0;

            if (t <= 0.33f) {
                const float u = t / 0.33f;
                r = 255;
                g = (uint8_t)(165.0f * u);
            } else if (t <= 0.66f) {
                const float u = (t - 0.33f) / 0.33f;
                r = 255;
                g = (uint8_t)(165.0f + (255.0f - 165.0f) * u);
            } else {
                const float u = (t - 0.66f) / 0.34f;
                r = (uint8_t)(255.0f * (1.0f - u));
                g = 255;
            }

            return tft.color565(r, g, b);
        }

        void drawHoldRingAtSweep(int sweepDeg) {
            const int16_t cx = SCREEN_W / 2;
            const int16_t cy = SCREEN_H / 2;
            const int deg = HOLD_START_DEG - sweepDeg;
            const float rad = deg * DEG_TO_RAD;
            const uint16_t color = colorForSweep(sweepDeg);

            for (int thickness = -6; thickness <= 6; thickness++) {
                const int radius = HOLD_RING_RADIUS + thickness;
                const int x = cx + (int)(cosf(rad) * radius);
                const int y = cy - (int)(sinf(rad) * radius);
                tft.drawPixel(x, y, color);
            }
        }

        void drawHoldRingStartDot() {
            const int16_t cx = SCREEN_W / 2;
            const int16_t cy = SCREEN_H / 2;
            tft.fillCircle(
                cx,
                cy + HOLD_RING_RADIUS,
                7,
                colorForSweep(0)
            );
        }

        void drawHoldRingUpTo(int sweepDeg) {
            if (sweepDeg < 0) {
                return;
            }

            drawHoldRingStartDot();

            for (int sweep = 1; sweep <= sweepDeg; sweep++) {
                drawHoldRingAtSweep(sweep);
            }
        }

        void appendHoldRingSweep(int previousSweepDeg, int newSweepDeg) {
            if (newSweepDeg <= previousSweepDeg) {
                return;
            }

            if (previousSweepDeg < 0) {
                drawHoldRingStartDot();
                previousSweepDeg = 0;

                if (newSweepDeg == 0) {
                    return;
                }
            }

            for (int sweep = previousSweepDeg + 1; sweep <= newSweepDeg; sweep++) {
                drawHoldRingAtSweep(sweep);
            }
        }

        void drawCenteredText(const char* label, int16_t baselineY, uint8_t textSize) {
            if (label == nullptr) {
                label = "";
            }

            tft.setTextSize(textSize);
            tft.setTextColor(GC9A01A_WHITE);
            tft.setTextWrap(false);

            int16_t x1, y1;
            uint16_t width, height;

            tft.getTextBounds(label, 0, 0, &x1, &y1, &width, &height);

            tft.setCursor(
                (SCREEN_W - width) / 2 - x1,
                baselineY
            );
            tft.print(label);
        }

        void drawCenteredLabel(const char* label, int16_t baselineY) {
            drawCenteredText(label, baselineY, 2);
        }

        // 7-segment patterns: a=top, b=tr, c=br, d=bot, e=bl, f=tl, g=mid
        constexpr uint8_t SEG7_DIGIT[10] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
        };

        int16_t seg7DigitWidth(int16_t digitHeight) {
            const int16_t thickness = digitHeight / 8;
            if (thickness < 2) {
                return digitHeight * 2 / 3;
            }

            const int16_t segWidth = (digitHeight * 3) / 5;
            return segWidth + thickness;
        }

        int16_t seg7StringWidth(const char* text, int16_t digitHeight) {
            const int16_t digitW = seg7DigitWidth(digitHeight);
            const int16_t spacing = 2;
            int16_t width = 0;
            int count = 0;

            for (const char* p = text; *p != '\0'; p++) {
                if (*p >= '0' && *p <= '9') {
                    count++;
                }
            }

            if (count == 0) {
                return 0;
            }

            width = count * digitW + (count - 1) * spacing;
            return width;
        }

        void drawSeg7Digit(
            Adafruit_GFX& gfx,
            int16_t x,
            int16_t y,
            uint8_t digit,
            int16_t digitHeight,
            uint16_t color
        ) {
            if (digit > 9) {
                return;
            }

            const uint8_t mask = SEG7_DIGIT[digit];
            const int16_t thickness = digitHeight / 8;

            if (thickness < 2) {
                return;
            }

            const int16_t segWidth = (digitHeight * 3) / 5;
            const int16_t half = (digitHeight - thickness) / 2;
            const int16_t innerH = half - thickness / 2;

            if (mask & 0x01) {
                gfx.fillRect(x + thickness, y, segWidth, thickness, color);
            }
            if (mask & 0x02) {
                gfx.fillRect(
                    x + thickness + segWidth,
                    y + thickness,
                    thickness,
                    innerH,
                    color
                );
            }
            if (mask & 0x04) {
                gfx.fillRect(
                    x + thickness + segWidth,
                    y + half + thickness / 2,
                    thickness,
                    innerH,
                    color
                );
            }
            if (mask & 0x08) {
                gfx.fillRect(
                    x + thickness,
                    y + digitHeight - thickness,
                    segWidth,
                    thickness,
                    color
                );
            }
            if (mask & 0x10) {
                gfx.fillRect(
                    x,
                    y + half + thickness / 2,
                    thickness,
                    innerH,
                    color
                );
            }
            if (mask & 0x20) {
                gfx.fillRect(x, y + thickness, thickness, innerH, color);
            }
            if (mask & 0x40) {
                gfx.fillRect(x + thickness, y + half, segWidth, thickness, color);
            }
        }

        void drawSeg7String(
            Adafruit_GFX& gfx,
            int16_t x,
            int16_t y,
            const char* text,
            int16_t digitHeight,
            uint16_t color
        ) {
            const int16_t digitW = seg7DigitWidth(digitHeight);
            const int16_t spacing = 2;
            int16_t cursorX = x;

            for (const char* p = text; *p != '\0'; p++) {
                if (*p < '0' || *p > '9') {
                    continue;
                }

                drawSeg7Digit(
                    gfx,
                    cursorX,
                    y,
                    static_cast<uint8_t>(*p - '0'),
                    digitHeight,
                    color
                );
                cursorX += digitW + spacing;
            }
        }

        // Set true to restore multi-row 7-segment clock (code kept below).
        constexpr bool USE_DIGITAL_CLOCK = false;

        constexpr int16_t CLOCK_DIGIT_H = 24;
        constexpr int16_t CLOCK_ROW_PITCH = 32;
        constexpr int16_t CLOCK_FIRST_Y = 42;

        constexpr int16_t ANALOG_CX = SCREEN_W / 2;
        constexpr int16_t ANALOG_CY = SCREEN_H / 2 + 2;
        constexpr int16_t ANALOG_R = 92;

        enum class TimeScreenKind {
            Unknown,
            Status,
            Syncing,
            Clock
        };

        TimeScreenKind timeScreenKind = TimeScreenKind::Unknown;
        Time::ClockSnapshot lastDrawnClock = {};

        int clockFieldValue(const Time::ClockSnapshot& clock, int row) {
            switch (row) {
                case 0:
                    return clock.year;
                case 1:
                    return clock.month;
                case 2:
                    return clock.day;
                case 3:
                    return clock.hours;
                case 4:
                    return clock.minutes;
                case 5:
                    return clock.seconds;
                default:
                    return 0;
            }
        }

        const char* clockFieldUnit(int row) {
            static const char* units[] = {
                "year", "month", "day", "hour", "min", "sec"
            };

            if (row < 0 || row > 5) {
                return "";
            }

            return units[row];
        }

        void formatClockFieldValue(char* out, size_t outLen, int row, int value) {
            if (row == 0) {
                snprintf(out, outLen, "%04d", value);
            } else {
                snprintf(out, outLen, "%02d", value);
            }
        }

        bool clocksEqual(const Time::ClockSnapshot& a, const Time::ClockSnapshot& b) {
            return a.year == b.year
                && a.month == b.month
                && a.day == b.day
                && a.hours == b.hours
                && a.minutes == b.minutes
                && a.seconds == b.seconds;
        }

        int16_t clockRowTopY(int row) {
            return CLOCK_FIRST_Y + CLOCK_ROW_PITCH * row;
        }

        void clearWifiClockRow(int row) {
            const int16_t y = clockRowTopY(row) - 2;
            tft.fillRect(0, y, SCREEN_W, CLOCK_ROW_PITCH + 4, GC9A01A_BLACK);
        }

        void drawWifiClockRow(
            const char* value,
            const char* unit,
            int16_t digitTopY,
            int16_t digitHeight
        ) {
            constexpr int16_t GAP = 8;

            const int16_t digitsW = seg7StringWidth(value, digitHeight);

            tft.setTextSize(1);
            tft.setTextColor(GC9A01A_WHITE);
            tft.setTextWrap(false);

            int16_t unitX1, unitY1;
            uint16_t unitW, unitH;
            tft.getTextBounds(unit, 0, 0, &unitX1, &unitY1, &unitW, &unitH);

            const int16_t totalW = digitsW + GAP + unitW;
            const int16_t startX = (SCREEN_W - totalW) / 2;

            drawSeg7String(tft, startX, digitTopY, value, digitHeight, GC9A01A_WHITE);

            const int16_t unitBaseline =
                digitTopY + (digitHeight + unitH) / 2 - unitY1;

            tft.setCursor(startX + digitsW + GAP, unitBaseline);
            tft.print(unit);
        }

        void drawWifiClockRowAtIndex(int row, int value) {
            char text[8] = {};
            formatClockFieldValue(text, sizeof(text), row, value);
            drawWifiClockRow(
                text,
                clockFieldUnit(row),
                clockRowTopY(row),
                CLOCK_DIGIT_H
            );
        }

        void analogHandAngles(
            const Time::ClockSnapshot& clock,
            float& hourAngle,
            float& minAngle,
            float& secAngle
        ) {
            secAngle = clock.seconds * 6.0f;
            minAngle = clock.minutes * 6.0f + clock.seconds * 0.1f;
            hourAngle =
                (clock.hours % 12) * 30.0f + clock.minutes * 0.5f;
        }

        void drawAnalogHub() {
            tft.fillCircle(ANALOG_CX, ANALOG_CY, 4, GC9A01A_WHITE);
            tft.fillCircle(ANALOG_CX, ANALOG_CY, 2, GC9A01A_BLACK);
        }

        void drawAnalogDateLine(const Time::ClockSnapshot& clock) {
            char dateLine[14] = {};
            snprintf(
                dateLine,
                sizeof(dateLine),
                "%04d-%02d-%02d",
                clock.year,
                clock.month,
                clock.day
            );
            constexpr int16_t baselineY = ANALOG_CY + ANALOG_R + 18;
            tft.fillRect(0, baselineY - 10, SCREEN_W, 14, GC9A01A_BLACK);
            drawCenteredText(dateLine, baselineY, 1);
        }

        void drawAnalogHand(
            float angleDeg,
            int16_t length,
            uint16_t color,
            int16_t thickness
        ) {
            const float rad = (angleDeg - 90.0f) * DEG_TO_RAD;
            const int16_t tipX =
                ANALOG_CX + static_cast<int16_t>(cosf(rad) * length);
            const int16_t tipY =
                ANALOG_CY + static_cast<int16_t>(sinf(rad) * length);

            if (thickness <= 1) {
                tft.drawLine(ANALOG_CX, ANALOG_CY, tipX, tipY, color);
                return;
            }

            const float perp = rad + static_cast<float>(M_PI / 2.0);
            const int16_t ox =
                static_cast<int16_t>(cosf(perp) * (thickness - 1));
            const int16_t oy =
                static_cast<int16_t>(sinf(perp) * (thickness - 1));

            tft.drawLine(
                ANALOG_CX - ox,
                ANALOG_CY - oy,
                tipX - ox,
                tipY - oy,
                color
            );
            tft.drawLine(ANALOG_CX, ANALOG_CY, tipX, tipY, color);
            tft.drawLine(
                ANALOG_CX + ox,
                ANALOG_CY + oy,
                tipX + ox,
                tipY + oy,
                color
            );
        }

        void drawAnalogClockFace() {
            tft.drawCircle(ANALOG_CX, ANALOG_CY, ANALOG_R, GC9A01A_WHITE);

            for (int tick = 0; tick < 60; tick++) {
                const float rad = (tick * 6.0f - 90.0f) * DEG_TO_RAD;
                const int inset = (tick % 5) == 0 ? 10 : 4;
                const int16_t x0 =
                    ANALOG_CX
                    + static_cast<int16_t>(cosf(rad) * (ANALOG_R - inset));
                const int16_t y0 =
                    ANALOG_CY
                    + static_cast<int16_t>(sinf(rad) * (ANALOG_R - inset));
                const int16_t x1 =
                    ANALOG_CX + static_cast<int16_t>(cosf(rad) * ANALOG_R);
                const int16_t y1 =
                    ANALOG_CY + static_cast<int16_t>(sinf(rad) * ANALOG_R);
                const uint16_t color =
                    (tick % 5) == 0 ? GC9A01A_WHITE : 0x3186;

                tft.drawLine(x0, y0, x1, y1, color);
            }
        }

        void drawAnalogHands(
            const Time::ClockSnapshot& clock,
            uint16_t hourColor,
            uint16_t minColor,
            uint16_t secColor
        ) {
            float hourAngle = 0.0f;
            float minAngle = 0.0f;
            float secAngle = 0.0f;
            analogHandAngles(clock, hourAngle, minAngle, secAngle);

            drawAnalogHand(hourAngle, ANALOG_R / 2, hourColor, 3);
            drawAnalogHand(
                minAngle,
                (ANALOG_R * 7) / 10,
                minColor,
                2
            );
            drawAnalogHand(
                secAngle,
                (ANALOG_R * 17) / 20,
                secColor,
                1
            );
        }

        void eraseAnalogHands(const Time::ClockSnapshot& clock) {
            drawAnalogHands(clock, GC9A01A_BLACK, GC9A01A_BLACK, GC9A01A_BLACK);
        }

        void drawAnalogClock(const Time::ClockSnapshot& clock) {
            tft.fillCircle(
                ANALOG_CX,
                ANALOG_CY,
                ANALOG_R + 10,
                GC9A01A_BLACK
            );
            drawAnalogClockFace();
            drawAnalogHands(
                clock,
                GC9A01A_WHITE,
                GC9A01A_WHITE,
                0xF800
            );
            drawAnalogHub();
            drawAnalogDateLine(clock);
        }

        void patchAnalogClock(const Time::ClockSnapshot& clock) {
            const bool dateChanged =
                clock.year != lastDrawnClock.year
                || clock.month != lastDrawnClock.month
                || clock.day != lastDrawnClock.day;

            eraseAnalogHands(lastDrawnClock);
            drawAnalogHands(
                clock,
                GC9A01A_WHITE,
                GC9A01A_WHITE,
                0xF800
            );
            drawAnalogHub();

            if (dateChanged) {
                drawAnalogDateLine(clock);
            }
        }

        void drawTimeClockFields(const Time::ClockSnapshot& clock) {
            if (USE_DIGITAL_CLOCK) {
                for (int row = 0; row < 6; row++) {
                    drawWifiClockRowAtIndex(
                        row,
                        clockFieldValue(clock, row)
                    );
                }
                return;
            }

            drawAnalogClock(clock);
        }

        void refreshTimeViewCache(Mode mode) {
            Time::Source& source = Time::sourceForMode(mode);

            if (mode == Mode::WiFi) {
                if (
                    WiFiConnect::screenState()
                    != WiFiConnect::ScreenState::Connected
                ) {
                    timeScreenKind = TimeScreenKind::Status;
                    return;
                }
            } else if (mode == Mode::GPS) {
                if (GPS::screenState() == GPS::ScreenState::Searching) {
                    timeScreenKind = TimeScreenKind::Status;
                    return;
                }

                if (GPS::screenState() == GPS::ScreenState::Acquiring) {
                    timeScreenKind = TimeScreenKind::Syncing;
                    return;
                }
            }

            Time::ClockSnapshot clock = {};
            if (source.readClock(clock)) {
                timeScreenKind = TimeScreenKind::Clock;
                lastDrawnClock = clock;
                return;
            }

            if (mode == Mode::WiFi) {
                timeScreenKind = TimeScreenKind::Syncing;
            } else {
                timeScreenKind = TimeScreenKind::Status;
            }
        }

        void patchTimeClockFields(const Time::ClockSnapshot& clock) {
            if (USE_DIGITAL_CLOCK) {
                for (int row = 0; row < 6; row++) {
                    const int value = clockFieldValue(clock, row);
                    const int previous = clockFieldValue(lastDrawnClock, row);

                    if (value == previous) {
                        continue;
                    }

                    clearWifiClockRow(row);
                    drawWifiClockRowAtIndex(row, value);
                }

                lastDrawnClock = clock;
                return;
            }

            if (clocksEqual(clock, lastDrawnClock)) {
                return;
            }

            patchAnalogClock(clock);
            lastDrawnClock = clock;
        }

        void drawWifiScreenContent() {
            if (WiFiConnect::screenState() == WiFiConnect::ScreenState::Connected) {
                Time::ClockSnapshot clock = {};

                if (NTP::readClock(clock)) {
                    drawTimeClockFields(clock);
                    return;
                }

                drawCenteredText("WiFi connected", 110, 1);
                drawCenteredText("Syncing time…", 130, 1);
                return;
            }

            constexpr int16_t ICON_CY = 78;

            Icons::drawWiFi(tft, SCREEN_W / 2, ICON_CY, GC9A01A_WHITE, 1.0f);
            drawCenteredText(WiFiConnect::statusLine1(), 132, 1);
            drawCenteredText(WiFiConnect::statusLine2(), 152, 1);
        }

        void drawGpsScreenContent() {
            if (GPS::screenState() == GPS::ScreenState::Ready) {
                Time::ClockSnapshot clock = {};

                if (GPS::instance().readClock(clock)) {
                    drawTimeClockFields(clock);
                    return;
                }
            }

            constexpr int16_t ICON_CY = 78;

            Icons::drawGps(tft, SCREEN_W / 2, ICON_CY, GC9A01A_WHITE, 1.0f);
            drawCenteredText(GPS::statusLine1(), 132, 1);
            drawCenteredText(GPS::statusLine2(), 152, 1);
        }

        void updateClockScreenForMode(Mode mode, bool clockActive, bool syncingActive) {
            Time::Source& source = Time::sourceForMode(mode);

            if (clockActive && source.isTimeValid()) {
                Time::ClockSnapshot clock = {};

                if (!source.readClock(clock)) {
                    return;
                }

                if (timeScreenKind != TimeScreenKind::Clock) {
                    if (mode == Mode::WiFi) {
                        showWifiScreen();
                    } else {
                        showGpsScreen();
                    }

                    return;
                }

                if (clocksEqual(clock, lastDrawnClock)) {
                    return;
                }

                patchTimeClockFields(clock);
                return;
            }

            if (syncingActive) {
                if (timeScreenKind != TimeScreenKind::Syncing) {
                    if (mode == Mode::WiFi) {
                        showWifiScreen();
                    } else {
                        showGpsScreen();
                    }
                }

                return;
            }

            if (timeScreenKind != TimeScreenKind::Status) {
                if (mode == Mode::WiFi) {
                    showWifiScreen();
                } else {
                    showGpsScreen();
                }
            }
        }

        void drawModeIconOnly(Adafruit_GFX& gfx, Mode mode, float iconScale, int16_t cx, int16_t cy) {
            switch (mode) {
                case Mode::WiFi:
                    Icons::drawWiFi(gfx, cx, cy, GC9A01A_WHITE, iconScale);
                    break;
                case Mode::GPS:
                    Icons::drawGps(gfx, cx, cy, GC9A01A_WHITE, iconScale);
                    break;
                case Mode::Settings:
                    Icons::drawGear(gfx, cx, cy, GC9A01A_WHITE, iconScale);
                    break;
                case Mode::Ethernet:
                    Icons::drawEthernet(gfx, cx, cy, GC9A01A_WHITE, iconScale);
                    break;
            }
        }

        void drawModeIconOnScreen(Mode mode, float iconScale) {
            drawModeIconOnly(tft, mode, iconScale, SCREEN_W / 2, ICON_CY);
        }

        void drawModeContent(Mode mode, float iconScale) {
            drawModeIconOnScreen(mode, iconScale);
            drawCenteredLabel(ModeManager::name(mode), LABEL_Y);
        }

        void blitIconPulse(Mode mode, float iconScale) {
            iconCanvas.fillScreen(0x0000);

            const int16_t canvasCenterX = (SCREEN_W / 2) - ICON_CLEAR_X;
            const int16_t canvasCenterY = ICON_CY - ICON_CLEAR_Y;

            drawModeIconOnly(
                iconCanvas,
                mode,
                iconScale,
                canvasCenterX,
                canvasCenterY
            );

            const uint16_t* buf = iconCanvas.getBuffer();
            if (buf == nullptr) {
                return;
            }

            tft.drawRGBBitmap(
                ICON_CLEAR_X,
                ICON_CLEAR_Y,
                buf,
                ICON_CLEAR_W,
                ICON_CLEAR_H
            );
        }

        void drawSelectionChrome() {
            tft.fillRect(
                0,
                0,
                SELECTION_SIDE_W,
                SELECTION_BOTTOM_Y,
                GC9A01A_WHITE
            );
            tft.fillRect(
                SCREEN_W - SELECTION_SIDE_W,
                0,
                SELECTION_SIDE_W,
                SELECTION_BOTTOM_Y,
                GC9A01A_WHITE
            );
            tft.fillRect(
                0,
                SELECTION_BOTTOM_Y,
                SCREEN_W,
                SCREEN_H - SELECTION_BOTTOM_Y,
                GC9A01A_WHITE
            );
        }

        void drawSelectionNavHints() {
            constexpr float NAV_SCALE = 1.2f;

            Icons::drawChevronLeft(
                tft,
                SELECTION_NAV_LEFT_X,
                SELECTION_NAV_Y,
                GC9A01A_BLACK,
                NAV_SCALE
            );
            Icons::drawChevronRight(
                tft,
                SELECTION_NAV_RIGHT_X,
                SELECTION_NAV_Y,
                GC9A01A_BLACK,
                NAV_SCALE
            );
            tft.fillCircle(
                SCREEN_W / 2,
                SELECTION_ENTER_CY,
                SELECTION_ENTER_R,
                GC9A01A_BLACK
            );
        }

        void paintSelectionNavChrome() {
            drawSelectionChrome();
            drawSelectionNavHints();
        }

        void drawSettingsItemIcon(Display::SettingsItem item) {
            const int16_t cx = SCREEN_W / 2;
            const int16_t cy = ICON_CY;

            switch (item) {
                case Display::SettingsItem::Info:
                    Icons::drawInfoMark(tft, cx, cy, GC9A01A_WHITE, 1.0f);
                    break;
                case Display::SettingsItem::Back:
                    Icons::drawBackArrow(tft, cx, cy, GC9A01A_WHITE, 1.0f);
                    break;
            }
        }

        void drawSettingsItemContent(Display::SettingsItem item) {
            drawSettingsItemIcon(item);

            switch (item) {
                case Display::SettingsItem::Info:
                    drawCenteredLabel("Info", LABEL_Y);
                    drawCenteredText("Brendon Body", LABEL_Y + 22, 1);
                    break;
                case Display::SettingsItem::Back:
                    drawCenteredLabel("Back", LABEL_Y);
                    break;
            }
        }

        void refreshSettingsFrame(Display::SettingsItem item) {
            tft.fillScreen(GC9A01A_BLACK);
            stopIconPulse();
            drawSettingsItemContent(item);
            paintSelectionNavChrome();
        }

        void refreshSelectionFrame(Mode mode) {
            tft.fillScreen(GC9A01A_BLACK);
            stopIconPulse();
            drawModeContent(mode, 1.0f);

            if (modeHoldActive && lastHoldSweepDeg >= 0) {
                drawHoldRingUpTo(lastHoldSweepDeg);
            }

            paintSelectionNavChrome();
        }

        float iconPulseScaleAt(uint32_t elapsedMs) {
            if (elapsedMs >= ICON_PULSE_MS) {
                return 1.0f;
            }

            const uint32_t halfMs = ICON_PULSE_MS / 2;

            if (elapsedMs < halfMs) {
                const float t = elapsedMs / (float)halfMs;
                return 1.0f + (ICON_PULSE_PEAK - 1.0f) * t;
            }

            const float t = (elapsedMs - halfMs) / (float)(ICON_PULSE_MS - halfMs);
            return ICON_PULSE_PEAK - (ICON_PULSE_PEAK - 1.0f) * t;
        }

        void stopIconPulse() {
            iconPulseActive = false;
            iconPulseScale = 1.0f;
            lastRenderedPulseStep = -1;
        }

        void startIconPulse() {
            iconPulseActive = true;
            iconPulseStartedAt = millis();
            iconPulseScale = 1.0f;
            lastRenderedPulseStep = -1;
        }

        int pulseStepFromElapsed(uint32_t elapsedMs) {
            return (int)(elapsedMs / 32);
        }

        void updateIconPulseAnimation(Mode mode) {
            if (!iconPulseActive) {
                return;
            }

            const uint32_t elapsed = millis() - iconPulseStartedAt;

            if (elapsed >= ICON_PULSE_MS) {
                iconPulseActive = false;
                iconPulseScale = 1.0f;
                lastRenderedPulseStep = -1;
                blitIconPulse(mode, 1.0f);
                return;
            }

            const int pulseStep = pulseStepFromElapsed(elapsed);

            if (pulseStep == lastRenderedPulseStep) {
                return;
            }

            iconPulseScale = iconPulseScaleAt(elapsed);
            lastRenderedPulseStep = pulseStep;
            blitIconPulse(mode, iconPulseScale);
        }
    }

    void begin() {
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
        dacDisable(DISP_SCK);
        dacDisable(DISP_MOSI);
#endif

        pinMode(DISP_RST, OUTPUT);
        pinMode(DISP_CS, OUTPUT);
        pinMode(DISP_DC, OUTPUT);
        digitalWrite(DISP_RST, HIGH);
        digitalWrite(DISP_CS, HIGH);
        digitalWrite(DISP_DC, HIGH);
        delay(20);

        // CS is driven by Adafruit_GC9A01A — do not assign as SPI hardware SS.
        const bool spiOk = dispSpi.begin(DISP_SCK, -1, DISP_MOSI, -1);

        Serial.printf(
            "[Display] HSPI %s | SCK=%u MOSI=%u CS=%u DC=%u RST=%u\n",
            spiOk ? "ok" : "FAIL",
            DISP_SCK,
            DISP_MOSI,
            DISP_CS,
            DISP_DC,
            DISP_RST
        );

        tft.begin(DISP_SPI_HZ);
        tft.setRotation(0);
        tft.invertDisplay(DISP_INVERT);

        tft.fillScreen(GC9A01A_WHITE);
        delay(100);
        tft.fillScreen(GC9A01A_BLACK);

        Serial.println("[Display] init done (brief white flash expected)");
    }

    void showSelectionPreview(Mode mode) {
        refreshSelectionFrame(mode);
    }

    void showSettingsMenu(SettingsItem item) {
        refreshSettingsFrame(item);
    }

    void updateSettingsScreen(SettingsItem item) {
        (void)item;
    }

    void refreshSelectionNavButtons() {
        paintSelectionNavChrome();
    }

    void updateSelectionScreen(Mode mode) {
        (void)mode;
    }

    void showGpsScreen() {
        tft.fillScreen(GC9A01A_BLACK);
        drawGpsScreenContent();
        refreshTimeViewCache(Mode::GPS);
    }

    void enterGpsUi() {
        GPS::onEnterGpsScreen();
        showGpsScreen();
    }

    void enterWifiUi() {
        showWifiScreen();
        WiFiConnect::onEnterWifiScreen();
    }

    void showWifiScreen() {
        tft.fillScreen(GC9A01A_BLACK);
        drawWifiScreenContent();
        refreshTimeViewCache(Mode::WiFi);
    }

    void updateWifiScreen() {
        const bool connected =
            WiFiConnect::screenState()
            == WiFiConnect::ScreenState::Connected;

        updateClockScreenForMode(
            Mode::WiFi,
            connected && NTP::isTimeValid(),
            connected
        );
    }

    void updateGpsScreen() {
        const GPS::ScreenState gpsState = GPS::screenState();

        updateClockScreenForMode(
            Mode::GPS,
            gpsState == GPS::ScreenState::Ready,
            gpsState == GPS::ScreenState::Acquiring
        );
    }

    void enterSelectedMode(Mode mode) {
        static Mode previousMode = Mode::GPS;

        if (previousMode == Mode::WiFi && mode != Mode::WiFi) {
            WiFiConnect::onLeaveWifiScreen();
        }

        if (previousMode == Mode::GPS && mode != Mode::GPS) {
            GPS::onLeaveGpsScreen();
        }

        previousMode = mode;

        if (mode == Mode::WiFi) {
            enterWifiUi();
        } else if (mode == Mode::GPS) {
            enterGpsUi();
        } else {
            tft.fillScreen(GC9A01A_BLACK);
        }
    }

    void cancelModeChangeHold() {
        resetModeChangeHold();
    }

    void beginModeChangeHold() {
        modeHoldActive = true;
        modeHoldStartedAt = millis();
        lastHoldSweepDeg = 0;
        drawHoldRingStartDot();
    }

    ModeChangeHoldResult updateModeChangeHold() {
        if (!modeHoldActive) {
            return ModeChangeHoldResult::Running;
        }

        const uint32_t elapsed = millis() - modeHoldStartedAt;

        if (elapsed >= MODE_HOLD_MS) {
            const int previousSweep = lastHoldSweepDeg;
            lastHoldSweepDeg = 360;
            appendHoldRingSweep(previousSweep, 360);

            resetModeChangeHold();
            return ModeChangeHoldResult::Completed;
        }

        const int sweepDeg = holdSweepFromElapsed(elapsed, MODE_HOLD_MS);

        if (sweepDeg != lastHoldSweepDeg) {
            const int previousSweep = lastHoldSweepDeg;
            lastHoldSweepDeg = sweepDeg;
            appendHoldRingSweep(previousSweep, sweepDeg);
        }

        return ModeChangeHoldResult::Running;
    }

    bool isModeChangeHoldActive() {
        return modeHoldActive;
    }

    void beginIconPulse(Mode mode) {
        tft.fillScreen(GC9A01A_BLACK);
        drawSelectionChrome();
        drawModeContent(mode, 1.0f);
        startIconPulse();
        blitIconPulse(mode, 1.0f);
        lastRenderedPulseStep = pulseStepFromElapsed(0);
    }

    void updateIconPulse(Mode mode) {
        updateIconPulseAnimation(mode);
    }

    void cancelIconPulse() {
        stopIconPulse();
    }

    bool isIconPulseActive() {
        return iconPulseActive;
    }
}
