#include "ModeManager.h"
#include "../storage/Persistence.h"

namespace ModeManager {
    namespace {
        constexpr Mode SELECTION_MODES[] = {
            Mode::GPS,
            Mode::WiFi,
            Mode::Settings,
        };
        constexpr int SELECTION_COUNT =
            sizeof(SELECTION_MODES) / sizeof(SELECTION_MODES[0]);
        constexpr int PERSISTED_MODE_COUNT = 2;

        Mode selectedMode = Mode::GPS;

        bool isValidPersistedMode(int value) {
            return value >= 0 && value < PERSISTED_MODE_COUNT;
        }

        int selectionIndex(Mode mode) {
            for (int i = 0; i < SELECTION_COUNT; i++) {
                if (SELECTION_MODES[i] == mode) {
                    return i;
                }
            }

            return 0;
        }
    }

    bool isTimeSource(Mode mode) {
        return mode == Mode::GPS || mode == Mode::WiFi;
    }

    void begin() {
        if (!Persistence::hasSavedMode()) {
            selectedMode = Mode::GPS;
            return;
        }

        const int stored = Persistence::loadMode(
            static_cast<uint8_t>(Mode::GPS)
        );

        if (isValidPersistedMode(stored)) {
            selectedMode = static_cast<Mode>(stored);
        } else {
            selectedMode = Mode::GPS;
        }
    }

    void next() {
        const int index =
            (selectionIndex(selectedMode) + 1) % SELECTION_COUNT;
        selectedMode = SELECTION_MODES[index];
    }

    void previous() {
        const int index =
            (selectionIndex(selectedMode) + SELECTION_COUNT - 1)
            % SELECTION_COUNT;
        selectedMode = SELECTION_MODES[index];
    }

    void persist() {
        if (!isTimeSource(selectedMode)) {
            return;
        }

        Persistence::saveMode(static_cast<uint8_t>(selectedMode));
    }

    void revertToSaved() {
        const int stored = Persistence::loadMode(
            static_cast<uint8_t>(Mode::GPS)
        );

        if (isValidPersistedMode(stored)) {
            selectedMode = static_cast<Mode>(stored);
        } else {
            selectedMode = Mode::GPS;
        }
    }

    void resetToDefault() {
        selectedMode = Mode::GPS;
        Persistence::saveMode(static_cast<uint8_t>(Mode::GPS));
    }

    Mode current() {
        return selectedMode;
    }

    const char* name(Mode mode) {
        switch (mode) {
            case Mode::GPS:
                return "GPS";
            case Mode::WiFi:
                return "WiFi";
            case Mode::Settings:
                return "Settings";
            // case Mode::Ethernet: return "Ethernet";
            default:
                return "UNKNOWN";
        }
    }
}
