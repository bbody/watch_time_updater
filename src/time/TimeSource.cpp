#include "TimeSource.h"
#include "GPS.h"
#include "NTP.h"

namespace Time {
    Source& sourceForMode(Mode mode) {
        switch (mode) {
            case Mode::GPS:
                return GPS::instance();
            case Mode::WiFi:
            default:
                return NTP::instance();
        }
    }
}
