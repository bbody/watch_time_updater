// Host tests for src/time/ZoneLookup against the built timezone16.bin.
// Usage: test_zonelookup path/to/timezone16.bin path/to/zones.csv

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/time/TimeZones.h"
#include "../src/time/ZoneLookup.h"

static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                \
                    __FILE__, __LINE__, #cond);                         \
            failures++;                                                 \
        }                                                               \
    } while (0)

struct Place {
    const char* label;
    float lat;
    float lon;
    const char* timezone;   // expected IANA zone
    const char* country;    // expected alpha-2, "" at sea
};

static const Place PLACES[] = {
    {"Sydney",        -33.8688f, 151.2093f, "Australia/Sydney",    "AU"},
    {"Perth",         -31.9523f, 115.8613f, "Australia/Perth",     "AU"},
    {"Adelaide",      -34.9285f, 138.6007f, "Australia/Adelaide",  "AU"},
    {"Auckland",      -36.8485f, 174.7633f, "Pacific/Auckland",    "NZ"},
    {"Tokyo",          35.6762f, 139.6503f, "Asia/Tokyo",          "JP"},
    {"Delhi",          28.6139f,  77.2090f, "Asia/Kolkata",        "IN"},
    {"Kathmandu",      27.7172f,  85.3240f, "Asia/Kathmandu",      "NP"},
    {"Madrid",         40.4168f,  -3.7038f, "Europe/Madrid",       "ES"},
    {"London",         51.5074f,  -0.1278f, "Europe/London",       "GB"},
    {"New York",       40.7128f, -74.0060f, "America/New_York",    "US"},
    {"Denver",         39.7392f,-104.9903f, "America/Denver",      "US"},
    {"St John's",      47.5615f, -52.7126f, "America/St_Johns",    "CA"},
    {"Mid-Pacific",     0.0f,   -150.0f,    "Etc/GMT+10",          ""},
    {"South Atlantic",-40.0f,    -20.0f,    "Etc/GMT+1",           ""},
};

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s timezone16.bin zones.csv\n", argv[0]);
        return 2;
    }

    ZoneDetect* db = ZDOpenDatabase(argv[1]);
    CHECK(db != nullptr);
    if (db == nullptr) {
        return 1;
    }
    CHECK(ZDGetTableType(db) == 'T');

    // zones.csv, to check every looked-up zone has a POSIX rule.
    FILE* f = fopen(argv[2], "rb");
    CHECK(f != nullptr);
    if (f != nullptr) {
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* text = static_cast<char*>(malloc(size));
        const size_t got = fread(text, 1, size, f);
        fclose(f);
        CHECK(TimeZones::loadFromText(text, got));
        free(text);
    }

    for (const Place& p : PLACES) {
        ZoneLookup::Result r;
        const bool found = ZoneLookup::lookupIn(db, p.lat, p.lon, r);
        printf("  %-15s %8.3f %9.3f → %-20s %-2s %s\n",
               p.label, p.lat, p.lon, r.timezone, r.countryCode, r.countryName);

        CHECK(found);
        if (strcmp(r.timezone, p.timezone) != 0) {
            fprintf(stderr, "  %s: expected %s, got %s\n", p.label, p.timezone, r.timezone);
            failures++;
        }
        if (strcmp(r.countryCode, p.country) != 0) {
            fprintf(stderr, "  %s: expected country '%s', got '%s'\n", p.label, p.country, r.countryCode);
            failures++;
        }
        if (TimeZones::indexOf(r.timezone) < 0) {
            fprintf(stderr, "  %s: %s has no POSIX rule in zones.csv\n", p.label, r.timezone);
            failures++;
        }
    }

    // Null database is handled.
    ZoneLookup::Result r;
    CHECK(!ZoneLookup::lookupIn(nullptr, 0, 0, r));
    CHECK(r.timezone[0] == '\0');

    ZDCloseDatabase(db);

    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("test_zonelookup: all checks passed (%zu places)\n", sizeof(PLACES) / sizeof(PLACES[0]));
    return 0;
}
