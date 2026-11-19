#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wps.h>

#include "WiFiConnect.h"
#include "../storage/Persistence.h"
#include "../time/NTP.h"

namespace WiFiConnect {
    namespace {
        constexpr uint32_t WPS_TIMEOUT_MS = 120000;
        constexpr uint32_t CONNECT_TIMEOUT_MS = 30000;

        ScreenState state = ScreenState::NotConfigured;
        uint32_t stateStartedAt = 0;
        bool wpsSessionActive = false;
        bool wifiScreenActive = false;
        bool pendingSessionStart = false;
        bool wifiEventHandlerRegistered = false;

        volatile bool wpsErSuccess = false;
        volatile bool wpsErFailed = false;
        volatile bool wpsErTimeout = false;

        void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
            (void)info;

            switch (event) {
                case ARDUINO_EVENT_WPS_ER_SUCCESS:
                    wpsErSuccess = true;
                    Serial.println("WPS succeeded");
                    break;

                case ARDUINO_EVENT_WPS_ER_FAILED:
                    wpsErFailed = true;
                    Serial.println("WPS failed");
                    break;

                case ARDUINO_EVENT_WPS_ER_TIMEOUT:
                    wpsErTimeout = true;
                    Serial.println("WPS timed out");
                    break;

                default:
                    break;
            }
        }

        void ensureWifiEventHandler() {
            if (wifiEventHandlerRegistered) {
                return;
            }

            WiFi.onEvent(onWiFiEvent);
            wifiEventHandlerRegistered = true;
        }

        bool readStoredSsid(char* out, size_t outLen) {
            wifi_config_t config = {};
            if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) {
                return false;
            }

            if (config.sta.ssid[0] == '\0') {
                return false;
            }

            strncpy(out, reinterpret_cast<const char*>(config.sta.ssid), outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }

        void stopWps() {
            if (!wpsSessionActive) {
                return;
            }

            esp_wifi_wps_disable();
            wpsSessionActive = false;
        }

        void setState(ScreenState next) {
            state = next;
            stateStartedAt = millis();
        }

        void startWps() {
            ensureWifiEventHandler();
            stopWps();

            wpsErSuccess = false;
            wpsErFailed = false;
            wpsErTimeout = false;

            WiFi.mode(WIFI_STA);
            WiFi.persistent(true);
            WiFi.disconnect(true, true);
            delay(100);

            esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);

            if (esp_wifi_wps_enable(&config) != ESP_OK) {
                Serial.println("esp_wifi_wps_enable failed");
                setState(ScreenState::Failed);
                return;
            }

            if (esp_wifi_wps_start(0) != ESP_OK) {
                esp_wifi_wps_disable();
                Serial.println("esp_wifi_wps_start failed");
                setState(ScreenState::Failed);
                return;
            }

            wpsSessionActive = true;
            setState(ScreenState::WpsWaiting);
            Serial.println("WPS started — press WPS on your router");
        }

        void startConnect() {
            stopWps();
            WiFi.mode(WIFI_STA);
            WiFi.persistent(true);
            WiFi.begin();
            setState(ScreenState::Connecting);
            Serial.println("WiFi connecting with saved credentials");
        }
    }

    void begin() {
        ensureWifiEventHandler();
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        setState(ScreenState::NotConfigured);
    }

    bool hasCredentials() {
        char ssid[33] = {};
        return readStoredSsid(ssid, sizeof(ssid));
    }

    ScreenState screenState() {
        return state;
    }

    const char* statusLine1() {
        if (!hasCredentials()) {
            return "WiFi not connected";
        }

        switch (state) {
            case ScreenState::NotConfigured:
            case ScreenState::WpsWaiting:
                return "WiFi not connected";
            case ScreenState::Connecting:
                return "Connecting…";
            case ScreenState::Connected:
                return "WiFi connected";
            case ScreenState::Failed:
                return "Connection failed";
        }

        return "";
    }

    const char* statusLine2() {
        if (!hasCredentials()) {
            return "Press WPS to connect";
        }

        switch (state) {
            case ScreenState::NotConfigured:
            case ScreenState::WpsWaiting:
                return "Press WPS to connect";
            case ScreenState::Connecting:
                return "Please wait";
            case ScreenState::Connected: {
                static char line[40] = {};
                char ssid[33] = {};
                if (readStoredSsid(ssid, sizeof(ssid))) {
                    snprintf(line, sizeof(line), "%s", ssid);
                } else {
                    snprintf(line, sizeof(line), "%s", WiFi.SSID().c_str());
                }
                return line;
            }
            case ScreenState::Failed:
                return "Press WPS to retry";
        }

        return "";
    }

    void onEnterWifiScreen() {
        wifiScreenActive = true;
        pendingSessionStart = true;
        setState(ScreenState::NotConfigured);
    }

    void onLeaveWifiScreen() {
        wifiScreenActive = false;
        pendingSessionStart = false;
        stopWps();
        WiFi.disconnect();
        setState(ScreenState::NotConfigured);
    }

    void clearConfiguration() {
        onLeaveWifiScreen();
        WiFi.persistent(true);
        WiFi.disconnect(true, true);
        NTP::invalidate();
        setState(ScreenState::NotConfigured);
    }

    void update() {
        if (!wifiScreenActive) {
            return;
        }

        if (pendingSessionStart) {
            pendingSessionStart = false;
            Serial.println("[WiFi] session start");

            if (hasCredentials()) {
                startConnect();
            } else {
                startWps();
            }

            return;
        }

        const uint32_t now = millis();

        if (state == ScreenState::WpsWaiting) {
            if (wpsErFailed || wpsErTimeout) {
                wpsErFailed = false;
                wpsErTimeout = false;
                stopWps();
                setState(ScreenState::Failed);
                return;
            }

            if (wpsErSuccess) {
                wpsErSuccess = false;
                stopWps();
                Persistence::saveWifiConfigured(true);

                char ssid[33] = {};
                if (readStoredSsid(ssid, sizeof(ssid))) {
                    Serial.printf("WPS credentials saved: %s\n", ssid);
                } else {
                    Serial.println("WPS credentials saved");
                }

                startConnect();
                return;
            }

            if (WiFi.status() == WL_CONNECTED) {
                stopWps();
                Persistence::saveWifiConfigured(true);
                setState(ScreenState::Connected);
                Serial.printf("WiFi connected: %s\n", WiFi.SSID().c_str());
                return;
            }

            if (now - stateStartedAt >= WPS_TIMEOUT_MS) {
                stopWps();
                setState(ScreenState::Failed);
                Serial.println("WPS timed out");
            }
            return;
        }

        if (state == ScreenState::Connecting) {
            if (WiFi.status() == WL_CONNECTED) {
                setState(ScreenState::Connected);
                Serial.printf("WiFi connected: %s\n", WiFi.SSID().c_str());
                return;
            }

            if (now - stateStartedAt >= CONNECT_TIMEOUT_MS) {
                setState(ScreenState::Failed);
                Serial.println("WiFi connect timed out");
            }
            return;
        }

        if (state == ScreenState::Connected && WiFi.status() != WL_CONNECTED) {
            if (hasCredentials()) {
                startConnect();
            } else {
                setState(ScreenState::Failed);
            }
        }
    }

    bool shouldUsePressForWps() {
        if (!wifiScreenActive) {
            return false;
        }

        return !hasCredentials()
            || state == ScreenState::NotConfigured
            || state == ScreenState::Failed
            || state == ScreenState::WpsWaiting;
    }

    void handleButtonPress() {
        if (!wifiScreenActive) {
            return;
        }

        if (hasCredentials()) {
            startConnect();
            return;
        }

        startWps();
    }
}
