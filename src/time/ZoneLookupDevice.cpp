// Device-only part of ZoneLookup: memory-map the "zonedetect" partition.

#include <Arduino.h>
#include <esp_partition.h>

#include "ZoneLookup.h"

namespace ZoneLookup {
    namespace {
        constexpr const char* PARTITION_LABEL = "zonedetect";

        ZoneDetect* database = nullptr;
        esp_partition_mmap_handle_t mapHandle = 0;
        bool attempted = false;
    }

    bool begin() {
        if (database != nullptr) {
            return true;
        }

        if (attempted) {
            return false;
        }
        attempted = true;

        const esp_partition_t* partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_ANY,
            PARTITION_LABEL
        );

        if (partition == nullptr) {
            Serial.println("[Zone] no 'zonedetect' partition (partitions.csv not flashed?)");
            return false;
        }

        const void* mapped = nullptr;
        const esp_err_t err = esp_partition_mmap(
            partition,
            0,
            partition->size,
            ESP_PARTITION_MMAP_DATA,
            &mapped,
            &mapHandle
        );

        if (err != ESP_OK) {
            Serial.printf("[Zone] mmap failed: %s\n", esp_err_to_name(err));
            return false;
        }

        // ZoneDetect files start with "PLB"; erased flash is 0xFF.
        if (memcmp(mapped, "PLB", 3) != 0) {
            Serial.println("[Zone] no database in partition (run `make uploadfs`)");
            esp_partition_munmap(mapHandle);
            mapHandle = 0;
            return false;
        }

        // ZoneDetect only reads the buffer; the cast drops const for its API.
        database = ZDOpenDatabaseFromMemory(const_cast<void*>(mapped), partition->size);

        if (database == nullptr) {
            Serial.println("[Zone] database header invalid");
            esp_partition_munmap(mapHandle);
            mapHandle = 0;
            return false;
        }

        Serial.printf(
            "[Zone] database ready (%u KB partition at 0x%x)\n",
            (unsigned)(partition->size / 1024),
            (unsigned)partition->address
        );
        return true;
    }

    bool isReady() {
        return database != nullptr;
    }

    bool lookup(float latitude, float longitude, Result& out) {
        return lookupIn(database, latitude, longitude, out);
    }
}
