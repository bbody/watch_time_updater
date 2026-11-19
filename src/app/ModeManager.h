#pragma once

enum class Mode {
    GPS,
    WiFi,
    Settings,
    Ethernet
};

namespace ModeManager {
    void begin();
    void next();
    void previous();
    void persist();
    void revertToSaved();
    void resetToDefault();
    Mode current();
    const char* name(Mode mode);
    bool isTimeSource(Mode mode);
}
