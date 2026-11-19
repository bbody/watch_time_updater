#pragma once

#include <sys/time.h>
#include <time.h>

#include "TimeSource.h"

namespace TimeFormat {
    // Interpret tm fields as UTC and return Unix epoch (ESP32 has no timegm()).
    time_t epochUtcFromTm(struct tm* utc);

    // Convert UTC wall time to JST for JJY encoding (display remains UTC).
    bool utcSnapshotToTm(const Time::ClockSnapshot& utc, struct tm& out);

    bool utcTimevalToJstTm(const struct timeval& utc, struct tm& jst);
}
