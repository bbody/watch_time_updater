#pragma once

#include "Positioning.h"

namespace Positioning {
    // Always reports the same position, picked at build time with
    // SIM_LOCATION (see FakePositioning.cpp). Used in the simulator.
    class FakeSource final : public Source {
    public:
        bool readPosition(Position& position) const override;
    };
}
