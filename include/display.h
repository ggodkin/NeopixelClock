#pragma once

#include <stdint.h>

// Three-state network indicator used by the three status LEDs.
enum class NetworkStatus : uint8_t {
    ATTEMPTING,
    FAILED,
    CONNECTED
};

class Display {
public:
    void begin();

    void showTime(int hours, int minutes);
    void updateColon(bool on);
    void showGarageClosed(bool closed);
    void showNetworkStatus(
        NetworkStatus wifi,
        NetworkStatus ntp,
        NetworkStatus mqtt
    );
    void showMessage(const char* message);

    void clear();

private:
    void show();

    bool _cursorOn = true;
    NetworkStatus _wifiStatus = NetworkStatus::ATTEMPTING;
    NetworkStatus _ntpStatus = NetworkStatus::ATTEMPTING;
    NetworkStatus _mqttStatus = NetworkStatus::ATTEMPTING;
};
