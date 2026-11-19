#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TimeZones.h"

namespace TimeZones {
    namespace {
        char* textBuffer = nullptr;   // owned copy of the CSV; zones point into it
        Zone* zones = nullptr;
        int zoneCount = 0;
        int skipped = 0;

        bool isNameChar(char c) {
            return isalnum(static_cast<unsigned char>(c))
                || c == '_' || c == '+' || c == '-' || c == '/';
        }

        bool isPosixChar(char c) {
            return isalnum(static_cast<unsigned char>(c))
                || strchr("<>+-:,./", c) != nullptr;
        }

        bool allChars(const char* s, bool (*pred)(char)) {
            if (*s == '\0') {
                return false;
            }

            for (; *s != '\0'; s++) {
                if (!pred(*s)) {
                    return false;
                }
            }

            return true;
        }

        // Read 1..maxDigits digits. Returns false if there are none.
        bool readNumber(const char*& p, int maxDigits, int* out) {
            int value = 0;
            int digits = 0;

            while (digits < maxDigits && isdigit(static_cast<unsigned char>(*p))) {
                value = value * 10 + (*p - '0');
                p++;
                digits++;
            }

            *out = value;
            return digits > 0;
        }

        // Parse one line `"name","posix"` in place (line is NUL-terminated,
        // without the newline). On success, name/posix point into the line.
        bool parseLine(char* line, Zone* zone) {
            char* p = line;

            while (*p == ' ' || *p == '\t') {
                p++;
            }

            if (*p++ != '"') {
                return false;
            }

            char* name = p;
            char* nameEnd = strchr(name, '"');
            if (nameEnd == nullptr || strncmp(nameEnd, "\",\"", 3) != 0) {
                return false;
            }

            char* posix = nameEnd + 3;
            char* posixEnd = strchr(posix, '"');
            if (posixEnd == nullptr) {
                return false;
            }

            for (const char* rest = posixEnd + 1; *rest != '\0'; rest++) {
                if (!isspace(static_cast<unsigned char>(*rest))) {
                    return false;
                }
            }

            *nameEnd = '\0';
            *posixEnd = '\0';

            int offset = 0;
            if (
                !allChars(name, isNameChar)
                || !allChars(posix, isPosixChar)
                || !parseStdOffset(posix, &offset)
            ) {
                return false;
            }

            zone->name = name;
            zone->posix = posix;
            zone->stdOffsetMin = static_cast<int16_t>(offset);
            return true;
        }

        int compareZones(const void* a, const void* b) {
            const Zone* za = static_cast<const Zone*>(a);
            const Zone* zb = static_cast<const Zone*>(b);

            if (za->stdOffsetMin != zb->stdOffsetMin) {
                return za->stdOffsetMin < zb->stdOffsetMin ? -1 : 1;
            }

            return strcmp(za->name, zb->name);
        }
    }

    bool parseStdOffset(const char* posix, int* offsetMinutesEast) {
        if (posix == nullptr) {
            return false;
        }

        const char* p = posix;

        // Standard-time name: <...> or 3+ letters.
        if (*p == '<') {
            const char* close = strchr(p, '>');
            if (close == nullptr || close == p + 1) {
                return false;
            }
            p = close + 1;
        } else {
            int letters = 0;
            while (isalpha(static_cast<unsigned char>(*p))) {
                p++;
                letters++;
            }
            if (letters < 3) {
                return false;
            }
        }

        int sign = 1;
        if (*p == '+' || *p == '-') {
            sign = (*p == '-') ? -1 : 1;
            p++;
        }

        int hours = 0;
        int minutes = 0;
        int seconds = 0;

        if (!readNumber(p, 2, &hours)) {
            return false;
        }

        if (*p == ':') {
            p++;
            if (!readNumber(p, 2, &minutes) || minutes > 59) {
                return false;
            }

            if (*p == ':') {
                p++;
                if (!readNumber(p, 2, &seconds) || seconds > 59) {
                    return false;
                }
            }
        }

        if (hours > 24) {
            return false;
        }

        // POSIX offsets are west-positive: "AEST-10" means UTC+10.
        *offsetMinutesEast = -sign * (hours * 60 + minutes);
        return true;
    }

    void clear() {
        free(zones);
        free(textBuffer);
        zones = nullptr;
        textBuffer = nullptr;
        zoneCount = 0;
        skipped = 0;
    }

    bool loadFromText(const char* text, size_t length) {
        clear();

        if (text == nullptr || length == 0) {
            return false;
        }

        textBuffer = static_cast<char*>(malloc(length + 1));
        if (textBuffer == nullptr) {
            return false;
        }

        memcpy(textBuffer, text, length);
        textBuffer[length] = '\0';

        int maxLines = 1;
        for (size_t i = 0; i < length; i++) {
            if (textBuffer[i] == '\n') {
                maxLines++;
            }
        }

        zones = static_cast<Zone*>(malloc(sizeof(Zone) * maxLines));
        if (zones == nullptr) {
            clear();
            return false;
        }

        char* line = textBuffer;
        while (line != nullptr && *line != '\0') {
            char* next = strchr(line, '\n');
            if (next != nullptr) {
                *next = '\0';
                next++;
            }

            // Blank lines (including a trailing "\r") aren't errors.
            bool blank = true;
            for (const char* c = line; *c != '\0'; c++) {
                if (!isspace(static_cast<unsigned char>(*c))) {
                    blank = false;
                    break;
                }
            }

            if (!blank) {
                if (parseLine(line, &zones[zoneCount])) {
                    zoneCount++;
                } else {
                    skipped++;
                }
            }

            line = next;
        }

        if (zoneCount == 0) {
            const int skippedLines = skipped;
            clear();
            skipped = skippedLines;
            return false;
        }

        qsort(zones, zoneCount, sizeof(Zone), compareZones);
        return true;
    }

    bool isLoaded() {
        return zoneCount > 0;
    }

    int skippedLines() {
        return skipped;
    }

    int count() {
        return zoneCount;
    }

    const Zone* at(int index) {
        if (index < 0 || index >= zoneCount) {
            return nullptr;
        }

        return &zones[index];
    }

    int indexOf(const char* name) {
        if (name == nullptr) {
            return -1;
        }

        for (int i = 0; i < zoneCount; i++) {
            if (strcmp(zones[i].name, name) == 0) {
                return i;
            }
        }

        return -1;
    }

    int indexNearestOffset(int offsetMinutes) {
        if (zoneCount == 0) {
            return -1;
        }

        int best = 0;
        int bestDistance = abs(zones[0].stdOffsetMin - offsetMinutes);

        // Sorted by offset: the first zone at the best distance wins.
        for (int i = 1; i < zoneCount; i++) {
            const int distance = abs(zones[i].stdOffsetMin - offsetMinutes);

            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }

        return best;
    }

    int offsetHoursFromLongitude(double longitudeDeg) {
        int hours = static_cast<int>(lround(longitudeDeg / 15.0));

        if (hours < -12) {
            hours = -12;
        } else if (hours > 12) {
            hours = 12;
        }

        return hours;
    }

    int indexFromLongitude(double longitudeDeg) {
        const int hours = offsetHoursFromLongitude(longitudeDeg);

        if (hours == 0) {
            return indexOf("Etc/UTC");
        }

        // Etc/GMT signs are inverted: "Etc/GMT-10" is UTC+10.
        char name[16];
        snprintf(name, sizeof(name), "Etc/GMT%+d", -hours);
        return indexOf(name);
    }

    void formatFixedOffset(int offsetHours, char* out, size_t outLen) {
        if (out == nullptr || outLen == 0) {
            return;
        }

        if (offsetHours == 0) {
            snprintf(out, outLen, "UTC0");
            return;
        }

        // e.g. +10 → "<+10>-10" (POSIX offset sign is inverted).
        snprintf(out, outLen, "<%+03d>%d", offsetHours, -offsetHours);
    }
}
