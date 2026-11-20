#include <Arduino.h>

#include "AppFlow.h"
#include "ModeManager.h"
#include "../input/Button.h"
#include "../location/Positioning.h"
#include "../net/WiFiConnect.h"
#include "../storage/Persistence.h"
#include "../ui/Display.h"
#include "../radio/Transmitter.h"
#include "../time/GPS.h"
#include "../time/NTP.h"
#include "../time/TimeZones.h"
#include "../time/ZoneLookup.h"

namespace AppFlow {
    namespace {
        Phase phase = Phase::Selection;
        WiFiConnect::ScreenState lastWifiScreenState =
            WiFiConnect::ScreenState::NotConfigured;
        GPS::ScreenState lastGpsScreenState = GPS::ScreenState::Searching;
        bool wifiResetConfirmActive = false;
        Display::SettingsItem settingsItem = Display::SettingsItem::Info;
        bool gpsZoneDetected = false;

        // Once per GPS session: position → time zone + country.
        // For now this only logs the result; applying it comes later.
        void detectZoneFromGps() {
            if (gpsZoneDetected) {
                return;
            }

            Positioning::Position position = {};
            if (!Positioning::active().readPosition(position)) {
                return;
            }

            gpsZoneDetected = true;
            const double lat = position.latitude;
            const double lon = position.longitude;

            ZoneLookup::Result zone;
            if (ZoneLookup::lookup(static_cast<float>(lat), static_cast<float>(lon), zone)) {
                const TimeZones::Zone* rule = TimeZones::at(TimeZones::indexOf(zone.timezone));

                Serial.printf(
                    "[Zone] %.5f, %.5f → %s (%s, %s), TZ \"%s\"\n",
                    lat,
                    lon,
                    zone.timezone,
                    zone.countryCode[0] != '\0' ? zone.countryCode : "--",
                    zone.countryName[0] != '\0' ? zone.countryName : "at sea",
                    rule != nullptr ? rule->posix : "?"
                );
                return;
            }

            // No database (or no match): fall back to the longitude estimate.
            const int hours = TimeZones::offsetHoursFromLongitude(lon);
            char posix[16];
            TimeZones::formatFixedOffset(hours, posix, sizeof(posix));
            Serial.printf(
                "[Zone] %.5f, %.5f → no ZoneDetect match; longitude estimate UTC%+d (\"%s\")\n",
                lat,
                lon,
                hours,
                posix
            );
        }

        void cancelWifiResetConfirm() {
            Display::cancelModeChangeHold();
            wifiResetConfirmActive = false;

            if (ModeManager::current() == Mode::WiFi) {
                Display::showWifiScreen();
                lastWifiScreenState = WiFiConnect::screenState();
            }

            Serial.println("WiFi reset cancelled");
        }

        void beginWifiResetConfirm() {
            wifiResetConfirmActive = true;
            Display::beginModeChangeHold();
            Serial.println("Hold to confirm WiFi reset");
        }

        void enterSelected() {
            phase = Phase::Selected;

            const Mode mode = ModeManager::current();
            Serial.printf("Selected mode: %s\n", ModeManager::name(mode));

            Display::enterSelectedMode(mode);

            if (mode == Mode::WiFi) {
                lastWifiScreenState = WiFiConnect::screenState();
            } else if (mode == Mode::GPS) {
                lastGpsScreenState = GPS::screenState();
                gpsZoneDetected = false;
            }
        }

        void enterSelection(bool advanceMode) {
            phase = Phase::Selection;
            Display::cancelIconPulse();
            Display::cancelModeChangeHold();

            if (advanceMode) {
                ModeManager::next();
            }

            Display::showSelectionPreview(ModeManager::current());

            Serial.printf(
                "Selection mode: %s\n",
                ModeManager::name(ModeManager::current())
            );
        }

        void enterSettings() {
            phase = Phase::Settings;
            settingsItem = Display::SettingsItem::Info;
            Display::showSettingsMenu(settingsItem);
            Serial.println("Settings: Info");
        }

        void cycleSettingsItem(int delta) {
            const int itemCount = 2;
            int index = static_cast<int>(settingsItem);
            index = (index + delta + itemCount) % itemCount;
            settingsItem = static_cast<Display::SettingsItem>(index);
            Display::showSettingsMenu(settingsItem);
            Serial.printf(
                "Settings: %s\n",
                settingsItem == Display::SettingsItem::Info ? "Info" : "Back"
            );
        }

        void updateSettings() {
            bool itemChanged = false;

            if (Button::wasPressed(Button::Id::Right)) {
                cycleSettingsItem(-1);
                itemChanged = true;
            }

            if (Button::wasPressed(Button::Id::Left)) {
                cycleSettingsItem(1);
                itemChanged = true;
            }

            if (itemChanged) {
                return;
            }

            if (Button::wasPressed(Button::Id::Enter)) {
                if (settingsItem == Display::SettingsItem::Back) {
                    enterSelection(false);
                    return;
                }
            }

            Display::updateSettingsScreen(settingsItem);
        }

        void clearWifiConfig() {
            WiFiConnect::clearConfiguration();
            Persistence::saveWifiConfigured(false);

            if (ModeManager::current() == Mode::WiFi) {
                Display::enterWifiUi();
                lastWifiScreenState = WiFiConnect::screenState();
            }

            Serial.println("WiFi configuration cleared");
        }

        void updateSelected() {
            if (wifiResetConfirmActive) {
                if (!Button::isHeld()) {
                    cancelWifiResetConfirm();
                    return;
                }

                if (
                    Display::updateModeChangeHold()
                    == ModeChangeHoldResult::Completed
                ) {
                    Display::cancelModeChangeHold();
                    wifiResetConfirmActive = false;
                    clearWifiConfig();
                }

                return;
            }

            if (
                Button::wasLongPressed() &&
                ModeManager::current() == Mode::WiFi
            ) {
                beginWifiResetConfirm();
                return;
            }

            if (Button::wasPressed(Button::Id::Enter)) {
                enterSelection(false);
                return;
            }

            const Mode mode = ModeManager::current();

            if (mode == Mode::GPS) {
                const GPS::ScreenState gpsState = GPS::screenState();

                if (gpsState != lastGpsScreenState) {
                    Display::showGpsScreen();
                    lastGpsScreenState = gpsState;
                } else {
                    Display::updateGpsScreen();
                }

                detectZoneFromGps();
            } else if (mode == Mode::WiFi) {
                WiFiConnect::update();
                NTP::update();

                const WiFiConnect::ScreenState state =
                    WiFiConnect::screenState();

                if (state != lastWifiScreenState) {
                    Display::showWifiScreen();
                    lastWifiScreenState = state;
                } else {
                    Display::updateWifiScreen();
                }
            }

            // Transmitter::update(mode);  // JJY emitting disabled
        }

        void updateSelection() {
            const Mode mode = ModeManager::current();
            bool modeChanged = false;

            if (Button::wasPressed(Button::Id::Right)) {
                ModeManager::previous();
                modeChanged = true;
            }

            if (Button::wasPressed(Button::Id::Left)) {
                ModeManager::next();
                modeChanged = true;
            }

            if (modeChanged) {
                Display::showSelectionPreview(ModeManager::current());
                Serial.printf(
                    "Preview: %s\n",
                    ModeManager::name(ModeManager::current())
                );
            }

            if (Button::wasPressed(Button::Id::Enter)) {
                if (ModeManager::current() == Mode::Settings) {
                    enterSettings();
                    return;
                }

                ModeManager::persist();
                enterSelected();
                return;
            }

            Display::updateSelectionScreen(mode);
        }

    }

    void begin() {
        Display::cancelIconPulse();
        Display::cancelModeChangeHold();

        if (Persistence::hasSavedMode()) {
            enterSelected();
            return;
        }

        phase = Phase::Selection;
        Display::showSelectionPreview(ModeManager::current());

        Serial.printf(
            "Selection mode: %s\n",
            ModeManager::name(ModeManager::current())
        );
    }

    void update() {
        if (
            phase == Phase::Selected
            && ModeManager::current() == Mode::GPS
        ) {
            GPS::update();
        }

        if (phase != Phase::Selected) {
            // Transmitter::stop();  // JJY emitting disabled
        }

        switch (phase) {
            case Phase::Selection:
                updateSelection();
                break;
            case Phase::Settings:
                updateSettings();
                break;
            case Phase::Selected:
                updateSelected();
                break;
        }
    }

    Phase currentPhase() {
        return phase;
    }
}
