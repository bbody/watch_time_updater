#pragma once

#include "../third_party/zonedetect/zonedetect.h"

// GPS position → IANA time zone + country, using a ZoneDetect database
// (timezone16.bin from ZoneDetect's builder, built by `make zonedb`).
//
// On the device the database lives in the raw "zonedetect" flash partition
// and is memory-mapped (see partitions.csv); `make uploadfs` writes it.
namespace ZoneLookup {
    struct Result {
        char timezone[48];     // IANA name, e.g. "Australia/Sydney"
        char countryCode[3];   // ISO 3166 alpha-2, e.g. "AU"; "" at sea
        char countryName[48];  // e.g. "Australia"; "" at sea
    };

    // Device: map the partition and open the database. Safe to call again.
    bool begin();
    bool isReady();

    // Device: look up using the mapped database. False if not ready or the
    // point isn't in any zone.
    bool lookup(float latitude, float longitude, Result& out);

    // Look up in an already-open database (host-testable).
    bool lookupIn(const ZoneDetect* database, float latitude, float longitude, Result& out);
}
