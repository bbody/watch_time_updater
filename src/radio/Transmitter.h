#pragma once

#include "../app/ModeManager.h"

namespace Transmitter {
    void begin();
    void stop();

    // Call from Selected phase when the active time source may be valid.
    void update(Mode mode);
}
