// Device-only part of TimeZones: reading zones.csv from LittleFS.
// Kept separate so TimeZones.cpp stays host-testable.

#include <Arduino.h>
#include <LittleFS.h>

#include "TimeZones.h"

namespace TimeZones {
    namespace {
        // zones.csv is ~15 KB; refuse anything absurd.
        constexpr size_t MAX_FILE_BYTES = 64 * 1024;
    }

    bool loadFromFile(const char* path) {
        File file = LittleFS.open(path, "r");
        if (!file) {
            Serial.printf("[TZ] %s not found\n", path);
            return false;
        }

        const size_t size = file.size();
        if (size == 0 || size > MAX_FILE_BYTES) {
            Serial.printf("[TZ] %s has unexpected size %u\n", path, (unsigned)size);
            file.close();
            return false;
        }

        char* buffer = static_cast<char*>(malloc(size));
        if (buffer == nullptr) {
            Serial.println("[TZ] out of memory reading zones");
            file.close();
            return false;
        }

        const size_t read = file.read(reinterpret_cast<uint8_t*>(buffer), size);
        file.close();

        const bool ok = read == size && loadFromText(buffer, size);
        free(buffer);

        if (ok) {
            Serial.printf(
                "[TZ] loaded %d zones from %s (%d lines skipped)\n",
                count(),
                path,
                skippedLines()
            );
        } else {
            Serial.printf("[TZ] failed to parse %s\n", path);
        }

        return ok;
    }
}
