#include "Positioning.h"

#ifdef WATCH_SIMULATOR
#include "FakePositioning.h"
#else
#include "../time/GPS.h"
#endif

namespace Positioning {
    Source& active() {
#ifdef WATCH_SIMULATOR
        static FakeSource fake;
        return fake;
#else
        return GPS::instance();
#endif
    }
}
