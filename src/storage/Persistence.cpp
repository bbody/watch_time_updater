#include <Preferences.h>

#include "Persistence.h"

namespace {
    Preferences prefs;

    constexpr char PREFS_NAMESPACE[] = "watch";
    constexpr char PREFS_KEY_MODE[] = "mode";
    constexpr char PREFS_KEY_WIFI[] = "wifi_ok";
}

void Persistence::begin() {
    prefs.begin(PREFS_NAMESPACE, false);
}

bool Persistence::hasSavedMode() {
    return prefs.isKey(PREFS_KEY_MODE);
}

uint8_t Persistence::loadMode(uint8_t defaultValue) {
    if (!prefs.isKey(PREFS_KEY_MODE)) {
        return defaultValue;
    }

    return prefs.getUChar(PREFS_KEY_MODE, defaultValue);
}

void Persistence::saveMode(uint8_t mode) {
    prefs.putUChar(PREFS_KEY_MODE, mode);
}

bool Persistence::loadWifiConfigured() {
    return prefs.getBool(PREFS_KEY_WIFI, false);
}

void Persistence::saveWifiConfigured(bool configured) {
    prefs.putBool(PREFS_KEY_WIFI, configured);
}

void Persistence::clearAll() {
    prefs.clear();
}
