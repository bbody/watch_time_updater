#pragma once

#include <stddef.h>
#include <stdint.h>

// IANA time zone → POSIX TZ string lookup.
//
// Zones are loaded at runtime from a text file in posix_tz_db's zones.csv
// format (https://github.com/nayarsystems/posix_tz_db), one zone per line:
//     "Australia/Sydney","AEST-10AEDT,M10.1.0,M4.1.0/3"
// so the file can be replaced (uploaded or downloaded) without reflashing.
//
// After loading, zones are sorted by standard UTC offset, then name, so
// index ± 1 gives the neighbouring zones for a picker.
//
// Everything except loadFromFile() is free of Arduino dependencies and is
// unit-tested on the host (`make test`).
namespace TimeZones {
    constexpr const char* DEFAULT_PATH = "/tz/zones.csv";

    struct Zone {
        const char* name;       // IANA name, e.g. "Australia/Sydney"
        const char* posix;      // POSIX TZ, e.g. "AEST-10AEDT,M10.1.0,M4.1.0/3"
        int16_t stdOffsetMin;   // standard-time UTC offset, east positive
    };

    // Parse zones.csv text. Copies the text; malformed lines are skipped.
    // Replaces any previously loaded table. Returns false (and leaves the
    // table empty) if no valid zones were found or memory ran out.
    bool loadFromText(const char* text, size_t length);

    // Read and parse a zones.csv file from LittleFS (device only; the file
    // system must already be mounted).
    bool loadFromFile(const char* path = DEFAULT_PATH);

    void clear();
    bool isLoaded();

    // Lines skipped as malformed during the last load.
    int skippedLines();

    int count();

    // nullptr if index is out of range.
    const Zone* at(int index);

    // Index of the zone with this IANA name, or -1.
    int indexOf(const char* name);

    // Index of the first zone whose standard offset is closest to
    // offsetMinutes (ties go to the smaller offset), or -1 if none loaded.
    int indexNearestOffset(int offsetMinutes);

    // Estimated standard offset in whole hours from longitude: round(lon / 15),
    // clamped to -12..+12. Ignores political borders and daylight saving.
    int offsetHoursFromLongitude(double longitudeDeg);

    // Fixed-offset zone ("Etc/GMT-10" = UTC+10, "Etc/UTC" for 0) for the
    // longitude estimate, or -1 if it isn't in the loaded table.
    int indexFromLongitude(double longitudeDeg);

    // POSIX TZ for a fixed whole-hour offset, e.g. 10 → "<+10>-10", 0 → "UTC0".
    // Works without a loaded table.
    void formatFixedOffset(int offsetHours, char* out, size_t outLen);

    // Standard UTC offset (minutes, east positive) of a POSIX TZ string.
    // Returns false if it can't be parsed.
    bool parseStdOffset(const char* posix, int* offsetMinutesEast);
}
