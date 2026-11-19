#pragma once

namespace AppFlow {
    enum class Phase {
        Selection,
        Settings,
        Selected
    };

    void begin();
    void update();

    Phase currentPhase();
}
