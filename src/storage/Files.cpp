#include <Arduino.h>
#include <LittleFS.h>

#include "Files.h"

namespace Files {
    namespace {
        bool mounted = false;
    }

    bool begin() {
        // Don't format on failure: that would silently wipe the data files.
        mounted = LittleFS.begin(false);

        if (mounted) {
            Serial.printf(
                "[FS] LittleFS mounted (%u / %u bytes used)\n",
                (unsigned)LittleFS.usedBytes(),
                (unsigned)LittleFS.totalBytes()
            );
        } else {
            Serial.println("[FS] LittleFS mount failed (run `make uploadfs`)");
        }

        return mounted;
    }

    bool isMounted() {
        return mounted;
    }
}
