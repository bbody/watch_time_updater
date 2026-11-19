#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "NTP.h"

namespace NTP {
    namespace {
        bool sntpConfigured = false;
        bool timeValid = false;
    }

    Source& instance() {
        static Source s;
        return s;
    }

    void Source::begin() {
        sntpConfigured = false;
        timeValid = false;
    }

    void Source::invalidate() {
        timeValid = false;
    }

    void Source::update() {
        if (WiFi.status() != WL_CONNECTED) {
            timeValid = false;
            return;
        }

        if (!sntpConfigured) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            sntpConfigured = true;
        }

        struct tm timeinfo = {};
        if (
            getLocalTime(&timeinfo, 50)
            && timeinfo.tm_year >= (2020 - 1900)
        ) {
            timeValid = true;
        }
    }

    bool Source::isTimeValid() const {
        return timeValid;
    }

    bool Source::readClock(Time::ClockSnapshot& snapshot) const {
        if (!timeValid) {
            return false;
        }

        struct timeval tv = {};
        gettimeofday(&tv, nullptr);

        struct tm timeinfo = {};
        localtime_r(&tv.tv_sec, &timeinfo);

        snapshot.year = timeinfo.tm_year + 1900;
        snapshot.month = timeinfo.tm_mon + 1;
        snapshot.day = timeinfo.tm_mday;
        snapshot.hours = timeinfo.tm_hour;
        snapshot.minutes = timeinfo.tm_min;
        snapshot.seconds = timeinfo.tm_sec;
        snapshot.microseconds = static_cast<int>(tv.tv_usec);

        return true;
    }

    void begin() {
        instance().begin();
    }

    void invalidate() {
        instance().invalidate();
    }

    void update() {
        instance().update();
    }

    bool isTimeValid() {
        return instance().isTimeValid();
    }

    bool readClock(Time::ClockSnapshot& snapshot) {
        return instance().readClock(snapshot);
    }
}
