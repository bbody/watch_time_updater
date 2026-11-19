#include "src/app/AppFlow.h"
#include "src/app/ModeManager.h"
#include "src/net/WiFiConnect.h"
#include "src/storage/Files.h"
#include "src/storage/Persistence.h"
#include "src/time/TimeZones.h"
#include "src/time/ZoneLookup.h"
#include "src/radio/Transmitter.h"
#include "src/time/GPS.h"
#include "src/time/NTP.h"
#include "src/input/Button.h"
#include "src/ui/Display.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.printf(
        "[Mem] flash=%u KB, heap=%u KB (free %u KB), PSRAM=%u KB\n",
        (unsigned)(ESP.getFlashChipSize() / 1024),
        (unsigned)(ESP.getHeapSize() / 1024),
        (unsigned)(ESP.getFreeHeap() / 1024),
        (unsigned)(ESP.getPsramSize() / 1024)
    );

    Persistence::begin();

    if (!Files::begin() || !TimeZones::loadFromFile()) {
        Serial.println("[TZ] time zones unavailable; using UTC");
    }

    ZoneLookup::begin();
    ModeManager::begin();
    WiFiConnect::begin();
    NTP::begin();
    GPS::begin();
    // Transmitter::begin();  // JJY emitting disabled
    Display::begin();
    Button::begin();

    AppFlow::begin();

    Serial.println("Watch Time Updater ready");

    if (Persistence::hasSavedMode()) {
        Serial.printf(
            "Saved mode: %s (selected on boot)\n",
            ModeManager::name(ModeManager::current())
        );
    } else {
        Serial.println("No saved mode; selection defaults to GPS");
    }
}

void loop() {
    Button::update();
    AppFlow::update();
}
