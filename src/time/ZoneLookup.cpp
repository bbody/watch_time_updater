#include <stdio.h>
#include <string.h>

#include "ZoneLookup.h"

namespace ZoneLookup {
    namespace {
        void copyField(char* out, size_t outLen, const char* value) {
            snprintf(out, outLen, "%s", value != nullptr ? value : "");
        }

        bool isHit(ZDLookupResult result) {
            return result == ZD_LOOKUP_IN_ZONE
                || result == ZD_LOOKUP_ON_BORDER_VERTEX
                || result == ZD_LOOKUP_ON_BORDER_SEGMENT;
        }
    }

    bool lookupIn(const ZoneDetect* database, float latitude, float longitude, Result& out) {
        memset(&out, 0, sizeof(out));

        if (database == nullptr) {
            return false;
        }

        ZoneDetectResult* results = ZDLookup(database, latitude, longitude, nullptr);
        if (results == nullptr) {
            return false;
        }

        bool found = false;

        for (int i = 0; results[i].lookupResult != ZD_LOOKUP_END; i++) {
            const ZoneDetectResult& r = results[i];

            if (!isHit(r.lookupResult) || r.data == nullptr) {
                continue;
            }

            const char* prefix = nullptr;
            const char* id = nullptr;

            for (unsigned f = 0; f < r.numFields; f++) {
                const char* name = r.fieldNames[f];
                const char* value = r.data[f];

                if (strcmp(name, "TimezoneIdPrefix") == 0) {
                    prefix = value;
                } else if (strcmp(name, "TimezoneId") == 0) {
                    id = value;
                } else if (strcmp(name, "CountryAlpha2") == 0) {
                    copyField(out.countryCode, sizeof(out.countryCode), value);
                } else if (strcmp(name, "CountryName") == 0) {
                    copyField(out.countryName, sizeof(out.countryName), value);
                }
            }

            if (id == nullptr || id[0] == '\0') {
                memset(&out, 0, sizeof(out));
                continue;
            }

            snprintf(
                out.timezone,
                sizeof(out.timezone),
                "%s%s",
                prefix != nullptr ? prefix : "",
                id
            );
            found = true;
            break;
        }

        ZDFreeResults(results);
        return found;
    }
}
