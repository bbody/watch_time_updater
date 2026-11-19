// Host unit tests for src/time/TimeZones. Run with `make test`.
// Usage: test_timezones path/to/zones.csv

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/time/TimeZones.h"

static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                \
                    __FILE__, __LINE__, #cond);                         \
            failures++;                                                 \
        }                                                               \
    } while (0)

static const char* nameAt(int index) {
    const TimeZones::Zone* zone = TimeZones::at(index);
    return zone ? zone->name : "(null)";
}

static int offsetOf(const char* name) {
    const TimeZones::Zone* zone = TimeZones::at(TimeZones::indexOf(name));
    return zone ? zone->stdOffsetMin : -9999;
}

static bool loadText(const char* text) {
    return TimeZones::loadFromText(text, strlen(text));
}

static char* readFile(const char* path, size_t* length) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = static_cast<char*>(malloc(size > 0 ? size : 1));
    *length = fread(buffer, 1, size, f);
    fclose(f);
    return buffer;
}

static void testParseStdOffset() {
    int m = 0;
    CHECK(TimeZones::parseStdOffset("AEST-10AEDT,M10.1.0,M4.1.0/3", &m) && m == 600);
    CHECK(TimeZones::parseStdOffset("EST5EDT,M3.2.0,M11.1.0", &m) && m == -300);
    CHECK(TimeZones::parseStdOffset("IST-5:30", &m) && m == 330);
    CHECK(TimeZones::parseStdOffset("<+0545>-5:45", &m) && m == 345);
    CHECK(TimeZones::parseStdOffset("NST3:30NDT,M3.2.0,M11.1.0", &m) && m == -210);
    CHECK(TimeZones::parseStdOffset("<-12>12", &m) && m == -720);
    CHECK(TimeZones::parseStdOffset("UTC0", &m) && m == 0);
    CHECK(TimeZones::parseStdOffset("GMT+0", &m) && m == 0);

    CHECK(!TimeZones::parseStdOffset("", &m));
    CHECK(!TimeZones::parseStdOffset("AB5", &m));        // name too short
    CHECK(!TimeZones::parseStdOffset("<>5", &m));        // empty <> name
    CHECK(!TimeZones::parseStdOffset("EST", &m));        // no offset
    CHECK(!TimeZones::parseStdOffset("EST5:", &m));      // colon, no minutes
    CHECK(!TimeZones::parseStdOffset("EST5:75", &m));    // minutes > 59
    CHECK(!TimeZones::parseStdOffset("EST99", &m));      // hours > 24
    CHECK(!TimeZones::parseStdOffset(nullptr, &m));
}

static void testFormatFixedOffset() {
    char buf[16];
    TimeZones::formatFixedOffset(10, buf, sizeof(buf));
    CHECK(strcmp(buf, "<+10>-10") == 0);
    TimeZones::formatFixedOffset(-5, buf, sizeof(buf));
    CHECK(strcmp(buf, "<-05>5") == 0);
    TimeZones::formatFixedOffset(0, buf, sizeof(buf));
    CHECK(strcmp(buf, "UTC0") == 0);

    int m = 0;
    TimeZones::formatFixedOffset(10, buf, sizeof(buf));
    CHECK(TimeZones::parseStdOffset(buf, &m) && m == 600);
    TimeZones::formatFixedOffset(-5, buf, sizeof(buf));
    CHECK(TimeZones::parseStdOffset(buf, &m) && m == -300);
}

static void testMalformedInput() {
    // Empty input.
    CHECK(!TimeZones::loadFromText("", 0));
    CHECK(!TimeZones::isLoaded());
    CHECK(TimeZones::count() == 0);
    CHECK(TimeZones::at(0) == nullptr);
    CHECK(TimeZones::indexNearestOffset(0) == -1);
    CHECK(TimeZones::indexFromLongitude(150) == -1);

    // Nothing valid.
    CHECK(!loadText("hello\nworld\n"));
    CHECK(TimeZones::skippedLines() == 2);

    // Bad lines skipped, good ones kept; CRLF, blank lines, no trailing newline.
    CHECK(loadText(
        "\"Asia/Tokyo\",\"JST-9\"\r\n"
        "\r\n"
        "garbage\n"
        "\"Bad Name\",\"UTC0\"\n"                 // space in name
        "\"Evil/Zone\",\"UTC0\\\"; x\"\n"         // quote/semicolon in rule
        "\"No/Offset\",\"EST\"\n"                 // unparseable rule
        "\"Extra/Field\",\"UTC0\",\"x\"\n"        // trailing junk
        "\"America/New_York\",\"EST5EDT,M3.2.0,M11.1.0\""));
    CHECK(TimeZones::count() == 2);
    CHECK(TimeZones::skippedLines() == 5);
    CHECK(strcmp(nameAt(0), "America/New_York") == 0);   // sorted by offset
    CHECK(strcmp(nameAt(1), "Asia/Tokyo") == 0);

    // Reloading replaces the table.
    CHECK(loadText("\"Etc/UTC\",\"UTC0\"\n"));
    CHECK(TimeZones::count() == 1);
    CHECK(TimeZones::indexOf("Asia/Tokyo") == -1);
}

static void testRealTable(const char* path) {
    size_t length = 0;
    char* text = readFile(path, &length);
    CHECK(text != nullptr);
    if (text == nullptr) {
        return;
    }

    CHECK(TimeZones::loadFromText(text, length));
    free(text);

    CHECK(TimeZones::count() > 400);
    CHECK(TimeZones::skippedLines() == 0);

    // Sorted by standard offset, then name.
    for (int i = 1; i < TimeZones::count(); i++) {
        const TimeZones::Zone* a = TimeZones::at(i - 1);
        const TimeZones::Zone* b = TimeZones::at(i);
        CHECK(a->stdOffsetMin < b->stdOffsetMin
              || (a->stdOffsetMin == b->stdOffsetMin && strcmp(a->name, b->name) < 0));
    }

    CHECK(TimeZones::at(0)->stdOffsetMin == -12 * 60);
    CHECK(TimeZones::at(TimeZones::count() - 1)->stdOffsetMin == 14 * 60);

    const int sydney = TimeZones::indexOf("Australia/Sydney");
    CHECK(sydney >= 0);
    CHECK(strcmp(TimeZones::at(sydney)->posix, "AEST-10AEDT,M10.1.0,M4.1.0/3") == 0);
    CHECK(offsetOf("Australia/Sydney") == 600);
    CHECK(offsetOf("Asia/Kolkata") == 330);
    CHECK(offsetOf("Asia/Kathmandu") == 345);
    CHECK(offsetOf("America/New_York") == -300);
    CHECK(offsetOf("America/St_Johns") == -210);
    CHECK(offsetOf("Pacific/Chatham") == 765);
    CHECK(offsetOf("Asia/Tokyo") == 540);
    CHECK(TimeZones::indexOf("Nowhere/Atlantis") == -1);
    CHECK(TimeZones::indexOf(nullptr) == -1);

    // Nearest offset: lands on the first zone of the group.
    CHECK(TimeZones::at(TimeZones::indexNearestOffset(600))->stdOffsetMin == 600);
    CHECK(TimeZones::at(TimeZones::indexNearestOffset(-2000))->stdOffsetMin == -720);
    CHECK(TimeZones::at(TimeZones::indexNearestOffset(2000))->stdOffsetMin == 840);
    const int utc = TimeZones::indexNearestOffset(0);
    CHECK(TimeZones::at(utc)->stdOffsetMin == 0);
    CHECK(utc == 0 || TimeZones::at(utc - 1)->stdOffsetMin < 0);

    // Longitude estimate.
    CHECK(TimeZones::offsetHoursFromLongitude(151.2) == 10);   // Sydney
    CHECK(TimeZones::offsetHoursFromLongitude(-0.1) == 0);     // London
    CHECK(TimeZones::offsetHoursFromLongitude(-74.0) == -5);   // New York
    CHECK(TimeZones::offsetHoursFromLongitude(180.0) == 12);
    CHECK(TimeZones::offsetHoursFromLongitude(-180.0) == -12);
    CHECK(strcmp(nameAt(TimeZones::indexFromLongitude(151.2)), "Etc/GMT-10") == 0);
    CHECK(strcmp(nameAt(TimeZones::indexFromLongitude(-74.0)), "Etc/GMT+5") == 0);
    CHECK(strcmp(nameAt(TimeZones::indexFromLongitude(2.0)), "Etc/UTC") == 0);
    for (int lon = -180; lon <= 180; lon++) {
        CHECK(TimeZones::indexFromLongitude(lon) >= 0);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s zones.csv\n", argv[0]);
        return 2;
    }

    testParseStdOffset();
    testFormatFixedOffset();
    testMalformedInput();
    testRealTable(argv[1]);

    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("test_timezones: all checks passed (%d zones in %s)\n", TimeZones::count(), argv[1]);
    return 0;
}
