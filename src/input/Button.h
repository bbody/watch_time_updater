#pragma once

namespace Button {
    enum class Id {
        Left,
        Enter,
        Right
    };

    void begin();
    void update();

    bool wasPressed(Id id);
    bool wasLongPressed(Id id);
    bool isHeld(Id id);

    bool wasPressed();
    bool wasLongPressed();
    bool isHeld();
}
