#pragma once

#include <stdint.h>

class Persistence {
public:
    static void begin();
    static bool hasSavedMode();
    static uint8_t loadMode(uint8_t defaultValue);
    static void saveMode(uint8_t mode);
    static bool loadWifiConfigured();
    static void saveWifiConfigured(bool configured);
    static void clearAll();
};
