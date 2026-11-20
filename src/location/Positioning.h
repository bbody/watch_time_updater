#pragma once

// Where the device is, used to detect its time zone.
// On the device the position comes from the GPS. In a simulator build
// (WATCH_SIMULATOR, see `make sim`) it's a fixed position, because Wokwi
// has no GPS.
namespace Positioning {
    struct Position {
        double latitude;   // degrees, north positive
        double longitude;  // degrees, east positive
    };

    class Source {
    public:
        virtual ~Source() = default;

        // Fills `position` and returns true when a current position is known.
        virtual bool readPosition(Position& position) const = 0;
    };

    // The position source for this build.
    Source& active();
}
