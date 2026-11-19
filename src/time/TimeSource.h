#pragma once

#include "../app/ModeManager.h"

namespace Time {
    struct ClockSnapshot {
        int year;
        int month;
        int day;
        int hours;
        int minutes;
        int seconds;
        int microseconds;
    };

    class Source {
    public:
        virtual ~Source() = default;

        virtual void begin() = 0;
        virtual void invalidate() = 0;
        virtual void update() = 0;
        virtual bool isTimeValid() const = 0;
        virtual bool readClock(ClockSnapshot& snapshot) const = 0;
    };

    Source& sourceForMode(Mode mode);
}
