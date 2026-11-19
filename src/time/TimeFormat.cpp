#include <Arduino.h>

#include "TimeFormat.h"

namespace TimeFormat {
    namespace {
        constexpr time_t JST_OFFSET_SEC = 9 * 3600;

        time_t snapshotToEpochUtc(const Time::ClockSnapshot& utc) {
            struct tm t = {};
            t.tm_year = utc.year - 1900;
            t.tm_mon = utc.month - 1;
            t.tm_mday = utc.day;
            t.tm_hour = utc.hours;
            t.tm_min = utc.minutes;
            t.tm_sec = utc.seconds;

            return epochUtcFromTm(&t);
        }
    }

    time_t epochUtcFromTm(struct tm* utc) {
        setenv("TZ", "UTC0", 1);
        tzset();
        return mktime(utc);
    }

    bool utcSnapshotToTm(const Time::ClockSnapshot& utc, struct tm& out) {
        struct timeval tv = {
            .tv_sec = snapshotToEpochUtc(utc),
            .tv_usec = utc.microseconds,
        };

        return utcTimevalToJstTm(tv, out);
    }

    bool utcTimevalToJstTm(const struct timeval& utc, struct tm& jst) {
        const time_t jstEpoch = utc.tv_sec + JST_OFFSET_SEC;
        gmtime_r(&jstEpoch, &jst);
        return true;
    }
}
