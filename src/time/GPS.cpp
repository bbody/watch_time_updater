#include <Arduino.h>
#include <HardwareSerial.h>
#include <sys/time.h>
#include <time.h>

#include "GPS.h"
#include "TimeFormat.h"

namespace GPS {
    namespace {
        // GPS connector sits on the top header.
        // Not the board's RX/TX pins (GPIO 3/1, UART0/USB) or 16/17 (WROVER PSRAM).
        constexpr uint8_t GPS_RX_PIN = 35;   // MCU RX ← GPS TX
        constexpr uint8_t GPS_PPS_PIN = 39;  // VN, GPS PPS
        // GPS RX wire lands on GPIO 34 (input-only), so nothing is sent to the
        // GPS. The UART still needs a TX pin: use 23, which is unconnected.
        // Don't pass -1 — the core may fall back to a default pin (27 = TFT DC).
        constexpr uint8_t GPS_TX_PIN = 23;
        constexpr uint32_t GPS_BAUD = 9600;

        constexpr int MIN_SATELLITES = 4;
        constexpr uint32_t FIX_STALE_MS = 2500;
        constexpr size_t NMEA_MAX = 96;

        HardwareSerial gpsSerial(1);

        bool gpsScreenActive = false;
        bool uartStarted = false;
        bool ppsAttached = false;
        bool seenNmea = false;

        int satelliteCount = 0;
        int fixQuality = 0;
        bool rmcActive = false;

        bool timeValid = false;
        time_t fixEpochSec = 0;
        uint32_t fixReceivedMillis = 0;

        bool positionValid = false;
        double latitude = 0.0;
        double longitude = 0.0;

        volatile bool ppsPending = false;
        uint32_t lastPpsMillis = 0;

        void IRAM_ATTR onPpsPulse() {
            ppsPending = true;
        }

        int parseNmeaField(const char* line, int index, char* out, size_t outLen) {
            if (index < 0 || outLen == 0) {
                return -1;
            }

            out[0] = '\0';

            if (line[0] != '$') {
                return -1;
            }

            int field = 0;
            size_t outIdx = 0;

            for (const char* p = line + 1; *p != '\0' && *p != '*'; p++) {
                if (*p == ',') {
                    if (field == index) {
                        out[outIdx] = '\0';
                        return static_cast<int>(outIdx);
                    }

                    field++;
                    outIdx = 0;
                    continue;
                }

                if (field == index && outIdx + 1 < outLen) {
                    out[outIdx++] = *p;
                }
            }

            if (field == index) {
                out[outIdx] = '\0';
                return static_cast<int>(outIdx);
            }

            return -1;
        }

        bool rmcSentence(const char* line) {
            return strncmp(line, "$GPRMC", 6) == 0
                || strncmp(line, "$GNRMC", 6) == 0
                || strncmp(line, "$GLRMC", 6) == 0;
        }

        bool ggaSentence(const char* line) {
            return strncmp(line, "$GPGGA", 6) == 0
                || strncmp(line, "$GNGGA", 6) == 0
                || strncmp(line, "$GLGGA", 6) == 0;
        }

        int parseUtcFieldToInt(const char* field, int width) {
            if (field == nullptr || static_cast<int>(strlen(field)) < width) {
                return -1;
            }

            int value = 0;

            for (int i = 0; i < width; i++) {
                if (field[i] < '0' || field[i] > '9') {
                    return -1;
                }

                value = value * 10 + (field[i] - '0');
            }

            return value;
        }

        // NMEA coordinate "ddmm.mmmm" (lat, 2 degree digits) or "dddmm.mmmm"
        // (lon, 3 degree digits) plus hemisphere → signed decimal degrees.
        bool parseCoordinate(
            const char* field,
            const char* hemisphere,
            int degreeDigits,
            double maxDegrees,
            double* out
        ) {
            if (static_cast<int>(strlen(field)) < degreeDigits + 2) {
                return false;
            }

            const int degrees = parseUtcFieldToInt(field, degreeDigits);
            if (degrees < 0) {
                return false;
            }

            char* end = nullptr;
            const double minutes = strtod(field + degreeDigits, &end);
            if (end == field + degreeDigits || *end != '\0' || minutes < 0 || minutes >= 60) {
                return false;
            }

            double value = degrees + minutes / 60.0;
            if (value > maxDegrees) {
                return false;
            }

            switch (hemisphere[0]) {
                case 'N':
                case 'E':
                    break;
                case 'S':
                case 'W':
                    value = -value;
                    break;
                default:
                    return false;
            }

            *out = value;
            return true;
        }

        // RMC fields 3-6: latitude, N/S, longitude, E/W. Only called for
        // active (status A) sentences.
        void parseRmcPosition(const char* line) {
            char latField[16] = {};
            char latHemi[4] = {};
            char lonField[16] = {};
            char lonHemi[4] = {};
            double lat = 0.0;
            double lon = 0.0;

            const bool ok =
                parseNmeaField(line, 3, latField, sizeof(latField)) > 0
                && parseNmeaField(line, 4, latHemi, sizeof(latHemi)) > 0
                && parseNmeaField(line, 5, lonField, sizeof(lonField)) > 0
                && parseNmeaField(line, 6, lonHemi, sizeof(lonHemi)) > 0
                && parseCoordinate(latField, latHemi, 2, 90.0, &lat)
                && parseCoordinate(lonField, lonHemi, 3, 180.0, &lon);

            if (!ok) {
                return;
            }

            latitude = lat;
            longitude = lon;
            positionValid = true;
        }

        bool parseRmcTime(const char* line) {
            char status[4] = {};
            char timeField[16] = {};
            char dateField[16] = {};

            if (parseNmeaField(line, 2, status, sizeof(status)) <= 0) {
                return false;
            }

            rmcActive = status[0] == 'A';

            if (!rmcActive) {
                return false;
            }

            parseRmcPosition(line);

            if (parseNmeaField(line, 1, timeField, sizeof(timeField)) <= 0) {
                return false;
            }

            if (parseNmeaField(line, 9, dateField, sizeof(dateField)) <= 0) {
                return false;
            }

            const int hours = parseUtcFieldToInt(timeField, 2);
            const int minutes = parseUtcFieldToInt(timeField + 2, 2);
            const int seconds = parseUtcFieldToInt(timeField + 4, 2);

            const int day = parseUtcFieldToInt(dateField, 2);
            const int month = parseUtcFieldToInt(dateField + 2, 2);
            int year = parseUtcFieldToInt(dateField + 4, 2);

            if (
                hours < 0 || minutes < 0 || seconds < 0
                || day < 1 || month < 1 || month > 12 || year < 0
            ) {
                return false;
            }

            year += 2000;

            struct tm utc = {};
            utc.tm_year = year - 1900;
            utc.tm_mon = month - 1;
            utc.tm_mday = day;
            utc.tm_hour = hours;
            utc.tm_min = minutes;
            utc.tm_sec = seconds;

            fixEpochSec = TimeFormat::epochUtcFromTm(&utc);
            fixReceivedMillis = millis();

            if (fixEpochSec < 0) {
                return false;
            }

            struct timeval tv = {
                .tv_sec = fixEpochSec,
                .tv_usec = 0,
            };
            settimeofday(&tv, nullptr);

            Serial.printf(
                "[GPS] RMC time %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                year,
                month,
                day,
                hours,
                minutes,
                seconds
            );

            return true;
        }

        void parseGga(const char* line) {
            char fixField[4] = {};
            char satField[4] = {};

            if (parseNmeaField(line, 6, fixField, sizeof(fixField)) > 0) {
                fixQuality = atoi(fixField);
            }

            if (parseNmeaField(line, 7, satField, sizeof(satField)) > 0) {
                satelliteCount = atoi(satField);
            }
        }

        void logFixStatus() {
            static bool lastTimeValid = false;
            static int lastSatelliteCount = -1;
            static int lastFixQuality = -1;
            static bool lastRmcActive = false;

            if (
                timeValid == lastTimeValid
                && satelliteCount == lastSatelliteCount
                && fixQuality == lastFixQuality
                && rmcActive == lastRmcActive
            ) {
                return;
            }

            Serial.printf(
                "[GPS] status rmc=%c fixQ=%d sats=%d timeValid=%d\n",
                rmcActive ? 'A' : 'V',
                fixQuality,
                satelliteCount,
                timeValid ? 1 : 0
            );

            lastTimeValid = timeValid;
            lastSatelliteCount = satelliteCount;
            lastFixQuality = fixQuality;
            lastRmcActive = rmcActive;
        }

        void evaluateFixValidity() {
            const bool fixOk = rmcActive
                && fixQuality > 0
                && satelliteCount >= MIN_SATELLITES
                && fixEpochSec > 0;

            timeValid = fixOk;

            if (!fixOk) {
                logFixStatus();
                return;
            }

            fixReceivedMillis = millis();
            logFixStatus();
        }

        void handleNmeaLine(const char* line) {
            seenNmea = true;
            Serial.printf("[GPS] %s\n", line);

            if (rmcSentence(line)) {
                parseRmcTime(line);
            } else if (ggaSentence(line)) {
                parseGga(line);
            }

            evaluateFixValidity();
        }

        void ensurePpsAttached() {
            if (ppsAttached) {
                return;
            }

            pinMode(GPS_PPS_PIN, INPUT);
            attachInterrupt(
                digitalPinToInterrupt(GPS_PPS_PIN),
                onPpsPulse,
                RISING
            );
            ppsAttached = true;
            Serial.printf("[GPS] PPS interrupt on GPIO %u\n", GPS_PPS_PIN);
        }

        void ensureUartStarted() {
            if (uartStarted) {
                return;
            }

            Serial.printf(
                "[GPS] starting UART RX=%u TX=%u baud=%u\n",
                GPS_RX_PIN,
                GPS_TX_PIN,
                GPS_BAUD
            );
            Serial.flush();

            // Use both UART pins so the driver initializes reliably; leave ESP TX
            // disconnected from the module RX if the module was held idle before.
            gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
            uartStarted = true;

            Serial.printf("[GPS] UART ready (disconnect ESP TX from GPS RX if no NMEA)\n");
            Serial.flush();
        }

        void resetFixState() {
            seenNmea = false;
            satelliteCount = 0;
            fixQuality = 0;
            rmcActive = false;
            timeValid = false;
            fixEpochSec = 0;
            fixReceivedMillis = 0;
            positionValid = false;
        }
    }

    Source& instance() {
        static Source s;
        return s;
    }

    void Source::begin() {
        setenv("TZ", "UTC0", 1);
        tzset();
        resetFixState();
    }

    void Source::invalidate() {
        timeValid = false;
    }

    void Source::onEnterGpsScreen() {
        gpsScreenActive = true;
        ensureUartStarted();
        ensurePpsAttached();
    }

    void Source::onLeaveGpsScreen() {
        gpsScreenActive = false;

        if (ppsAttached) {
            detachInterrupt(digitalPinToInterrupt(GPS_PPS_PIN));
            ppsAttached = false;
        }

        if (uartStarted) {
            gpsSerial.end();
            uartStarted = false;
            Serial.println("[GPS] UART stopped (left GPS mode)");
        }
    }

    void Source::update() {
        if (!uartStarted || !gpsScreenActive) {
            return;
        }

        static uint32_t lastHeartbeatMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastHeartbeatMs >= 5000) {
            lastHeartbeatMs = nowMs;
            const int rxPending = gpsSerial.available();
            Serial.printf(
                "[GPS] heartbeat nmea=%s sats=%d fix=%s rx_pending=%d\n",
                seenNmea ? "yes" : "no",
                satelliteCount,
                timeValid ? "valid" : "none",
                rxPending < 0 ? 0 : rxPending
            );
        }

        if (ppsPending) {
            lastPpsMillis = millis();
            ppsPending = false;
        }

        static char line[NMEA_MAX] = {};
        static size_t lineLen = 0;

        while (gpsSerial.available() > 0) {
            const char c = static_cast<char>(gpsSerial.read());

            if (c == '\n' || c == '\r') {
                if (lineLen > 0) {
                    line[lineLen] = '\0';
                    handleNmeaLine(line);
                    lineLen = 0;
                }

                continue;
            }

            if (lineLen + 1 >= NMEA_MAX) {
                lineLen = 0;
                continue;
            }

            line[lineLen++] = c;
        }

        if (
            timeValid
            && millis() - fixReceivedMillis > FIX_STALE_MS
        ) {
            timeValid = false;
            Serial.println("[GPS] fix stale — time invalid");
        }
    }

    bool Source::isTimeValid() const {
        return timeValid;
    }

    bool Source::readClock(Time::ClockSnapshot& snapshot) const {
        if (!timeValid || fixEpochSec <= 0) {
            return false;
        }

        const uint32_t elapsedMs = millis() - fixReceivedMillis;
        const time_t nowSec = fixEpochSec + (elapsedMs / 1000);

        struct tm utc = {};
        gmtime_r(&nowSec, &utc);

        snapshot.year = utc.tm_year + 1900;
        snapshot.month = utc.tm_mon + 1;
        snapshot.day = utc.tm_mday;
        snapshot.hours = utc.tm_hour;
        snapshot.minutes = utc.tm_min;
        snapshot.seconds = utc.tm_sec;

        const uint32_t sincePps = millis() - lastPpsMillis;
        if (sincePps < 1100) {
            snapshot.microseconds = static_cast<int>(sincePps * 1000);
        } else {
            snapshot.microseconds = static_cast<int>((elapsedMs % 1000) * 1000);
        }

        return true;
    }

    bool Source::readPosition(Positioning::Position& position) const {
        if (!positionValid || !timeValid) {
            return false;
        }

        position.latitude = latitude;
        position.longitude = longitude;
        return true;
    }

    ScreenState Source::screenState() const {
        if (timeValid) {
            return ScreenState::Ready;
        }

        if (!seenNmea) {
            return ScreenState::Searching;
        }

        return ScreenState::Acquiring;
    }

    const char* Source::statusLine1() const {
        switch (screenState()) {
            case ScreenState::Ready:
                return "GPS time ready";
            case ScreenState::Acquiring:
                return "Acquiring GPS fix";
            case ScreenState::Searching:
            default:
                return "Waiting for GPS";
        }
    }

    const char* Source::statusLine2() const {
        static char line[32] = {};

        if (screenState() == ScreenState::Ready) {
            snprintf(line, sizeof(line), "%d satellites", satelliteCount);
            return line;
        }

        if (seenNmea) {
            snprintf(
                line,
                sizeof(line),
                "Sats: %d (need %d)",
                satelliteCount,
                MIN_SATELLITES
            );
            return line;
        }

        return "Searching satellites…";
    }

    void begin() {
        Serial.println("[GPS] begin (UART starts when GPS mode opens)");
        Serial.flush();
        instance().begin();
    }

    void onEnterGpsScreen() {
        instance().onEnterGpsScreen();
    }

    void onLeaveGpsScreen() {
        instance().onLeaveGpsScreen();
    }

    void update() {
        instance().update();
    }

    ScreenState screenState() {
        return instance().screenState();
    }

    const char* statusLine1() {
        return instance().statusLine1();
    }

    const char* statusLine2() {
        return instance().statusLine2();
    }
}
