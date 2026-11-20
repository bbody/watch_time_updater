#pragma once

#include "TimeSource.h"
#include "../location/Positioning.h"

namespace GPS {
    enum class ScreenState {
        Searching,
        Acquiring,
        Ready
    };

    class Source final : public Time::Source, public Positioning::Source {
    public:
        void begin() override;
        void invalidate() override;
        void update() override;
        bool isTimeValid() const override;
        bool readClock(Time::ClockSnapshot& snapshot) const override;

        void onEnterGpsScreen();
        void onLeaveGpsScreen();

        // Last position from an active RMC sentence, while the fix is valid.
        bool readPosition(Positioning::Position& position) const override;

        ScreenState screenState() const;
        const char* statusLine1() const;
        const char* statusLine2() const;
    };

    Source& instance();

    void begin();
    void onEnterGpsScreen();
    void onLeaveGpsScreen();
    void update();
    ScreenState screenState();
    const char* statusLine1();
    const char* statusLine2();
}
