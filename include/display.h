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

    // Single coherent clock redraw: digits + colon + garage + status LEDs,
    // followed by exactly one FastLED show(). Prefer this over the
    // individual helpers to avoid intermediate frames and multi-show bursts
    // that can glitch WS2812 under main power (WiFi active).
    void showClock(int hours, int minutes, bool colonOn, bool garageClosed);

    // Lightweight helpers kept for status-only / message paths.
    void showNetworkStatus(
        NetworkStatus wifi,
        NetworkStatus ntp,
        NetworkStatus mqtt
    );
    void showMessage(const char* message);

    void clear();

private:
    void show();

    void drawTimeDigits(int hours, int minutes);
    void drawColon(bool on);
    void drawGarage(bool closed);

    bool _cursorOn = true;
    NetworkStatus _wifiStatus = NetworkStatus::ATTEMPTING;
    NetworkStatus _ntpStatus = NetworkStatus::ATTEMPTING;
    NetworkStatus _mqttStatus = NetworkStatus::ATTEMPTING;
};
