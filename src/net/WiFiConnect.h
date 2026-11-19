#pragma once

namespace WiFiConnect {
    enum class ScreenState {
        NotConfigured,
        WpsWaiting,
        Connecting,
        Connected,
        Failed
    };

    void begin();
    void update();

    void onEnterWifiScreen();
    void onLeaveWifiScreen();
    void clearConfiguration();

    bool hasCredentials();
    bool shouldUsePressForWps();
    void handleButtonPress();

    ScreenState screenState();
    const char* statusLine1();
    const char* statusLine2();
}
