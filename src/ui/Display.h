#pragma once

#include "../app/ModeManager.h"

enum class ModeChangeHoldResult {
    Running,
    Completed
};

namespace Display {
    enum class SettingsItem {
        Info,
        Back
    };
    void begin();

    void showSelectionPreview(Mode mode);
    void refreshSelectionNavButtons();
    void updateSelectionScreen(Mode mode);
    void enterSelectedMode(Mode mode);

    void showSettingsMenu(SettingsItem item);
    void updateSettingsScreen(SettingsItem item);

    void showGpsScreen();
    void enterGpsUi();
    void enterWifiUi();
    void showWifiScreen();
    void updateWifiScreen();
    void updateGpsScreen();

    void beginIconPulse(Mode mode);
    void updateIconPulse(Mode mode);
    void cancelIconPulse();
    bool isIconPulseActive();

    void beginModeChangeHold();
    void cancelModeChangeHold();
    ModeChangeHoldResult updateModeChangeHold();
    bool isModeChangeHoldActive();

}
