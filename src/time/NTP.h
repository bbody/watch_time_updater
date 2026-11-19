#pragma once

#include "TimeSource.h"

namespace NTP {
    class Source final : public Time::Source {
    public:
        void begin() override;
        void invalidate() override;
        void update() override;
        bool isTimeValid() const override;
        bool readClock(Time::ClockSnapshot& snapshot) const override;
    };

    Source& instance();

    void begin();
    void invalidate();
    void update();
    bool isTimeValid();
    bool readClock(Time::ClockSnapshot& snapshot);
}
