// Only built into the simulator firmware (`make sim`).
#ifdef WATCH_SIMULATOR

#include <Arduino.h>

#include "FakePositioning.h"

// The place is chosen at build time: `make sim SIM_LOCATION=SYDNEY`
// (default TOKYO) defines SIM_LOCATION_SYDNEY.
namespace Positioning {
    namespace {
        struct Place {
            const char* name;
            Position position;
        };

#if defined(SIM_LOCATION_TOKYO)
        // JJY's eastern transmitter area (40 kHz).
        constexpr Place PLACE = { "Tokyo", { 35.68124, 139.76712 } };
#elif defined(SIM_LOCATION_OSAKA)
        // JJY's western transmitter area (60 kHz).
        constexpr Place PLACE = { "Osaka", { 34.70249, 135.49595 } };
#elif defined(SIM_LOCATION_SYDNEY)
        constexpr Place PLACE = { "Sydney", { -33.85678, 151.21530 } };
#elif defined(SIM_LOCATION_BRISBANE)
        // Queensland: no daylight saving, unlike Sydney.
        constexpr Place PLACE = { "Brisbane", { -27.46888, 153.02362 } };
#elif defined(SIM_LOCATION_NEW_YORK)
        constexpr Place PLACE = { "New York", { 40.75800, -73.98550 } };
#elif defined(SIM_LOCATION_LONDON)
        // Greenwich: on the prime meridian.
        constexpr Place PLACE = { "London", { 51.47787, -0.00147 } };
#elif defined(SIM_LOCATION_AT_SEA)
        // South Pacific, far from land: no country in the zone lookup.
        constexpr Place PLACE = { "At sea", { -30.00000, -140.00000 } };
#else
#error "Unknown SIM_LOCATION. Use one of: TOKYO, OSAKA, SYDNEY, BRISBANE, NEW_YORK, LONDON, AT_SEA"
#endif
    }

    bool FakeSource::readPosition(Position& position) const {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Serial.printf(
                "[Sim] Fake position: %s (%.5f, %.5f)\n",
                PLACE.name,
                PLACE.position.latitude,
                PLACE.position.longitude
            );
        }

        position = PLACE.position;
        return true;
    }
}

#endif
