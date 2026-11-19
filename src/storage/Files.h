#pragma once

// LittleFS on the "spiffs" data partition (0x290000, 1.375 MB on the default
// 4 MB layout). Populate it with `make uploadfs`, which builds the image from
// downloaded data (e.g. time zones) under build/fs/.
namespace Files {
    bool begin();
    bool isMounted();
}
