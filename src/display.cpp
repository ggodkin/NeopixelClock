#include "display.h"

#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include "config.h"

namespace {

CRGB matrixleds[NUM_LEDS];

FastLED_NeoMatrix matrix(
    matrixleds,
    MATRIX_WIDTH,
    MATRIX_HEIGHT,
    NEO_MATRIX_TOP +
    NEO_MATRIX_LEFT +
    NEO_MATRIX_COLUMNS +
    NEO_MATRIX_ZIGZAG
);

// Preserve the existing display colors and add yellow for status attempts.
const uint32_t colors[] = {
    matrix.Color(255, 0, 0),
    matrix.Color(0, 255, 0),
    matrix.Color(0, 0, 255),
    matrix.Color(0, 0, 0),
    matrix.Color(255, 255, 0)
};

constexpr uint8_t BRIGHTNESS = 1;

constexpr int GARAGE_X = 28;
constexpr int GARAGE_Y = 0;
constexpr int GARAGE_WIDTH = 3;
constexpr int GARAGE_HEIGHT = 3;

// Rightmost column, bottom three LEDs:
//   y=5: WiFi
//   y=6: NTP
//   y=7: MQTT
constexpr int STATUS_X = 31;
constexpr int WIFI_STATUS_Y = 5;
constexpr int NTP_STATUS_Y = 6;
constexpr int MQTT_STATUS_Y = 7;

CRGB statusColor(NetworkStatus status) {
    switch (status) {
        case NetworkStatus::ATTEMPTING:
            return CRGB(255, 255, 0);
        case NetworkStatus::FAILED:
            return CRGB(255, 0, 0);
        case NetworkStatus::CONNECTED:
            return CRGB(0, 0, 255);
    }

    return CRGB::Black;
}

void applyNetworkStatusIndicators() {
    // Write the status pixels directly into the FastLED buffer. This keeps
    // the indicators independent of the GFX drawing operations used for the
    // clock, colon, garage indicator, and messages.
    matrixleds[matrix.XY(STATUS_X, WIFI_STATUS_Y)] = statusColor(
        NetworkStatus::ATTEMPTING
    );
}

} // namespace

void Display::begin() {
    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(matrixleds, NUM_LEDS);
    matrix.begin();
    matrix.setTextWrap(false);
    matrix.setBrightness(BRIGHTNESS);
    matrix.setTextColor(colors[1]);
    matrix.print("Setup");
    show();
    delay(1000);
    matrix.fillScreen(0);
    show();
}

void Display::showTime(int hours, int minutes) {
    matrix.fillScreen(0);

    if (hours < 10) {
        matrix.setCursor(6, 0);
    } else {
        matrix.setCursor(0, 0);
    }

    matrix.setTextColor(colors[2]);

    String localMinutes;
    if (minutes < 10) {
        localMinutes = "0" + String(minutes);
    } else {
        localMinutes = String(minutes);
    }

    matrix.print(String(hours));
    matrix.setCursor(16, 0);
    matrix.print(localMinutes);
    show();
}

void Display::updateColon(bool on) {
    matrix.setCursor(11, 0);
    matrix.setTextColor(on ? colors[2] : colors[3]);
    matrix.print(":");
    show();
}

void Display::showGarageClosed(bool closed) {
    if (closed) {
        matrix.fillRect(GARAGE_X, GARAGE_Y, GARAGE_WIDTH, GARAGE_HEIGHT, colors[1]);
    } else {
        matrix.drawRect(GARAGE_X, GARAGE_Y, GARAGE_WIDTH, GARAGE_HEIGHT, colors[0]);
        matrix.fillRect(GARAGE_X + 1, GARAGE_Y + 1, 1, 1, 0);
        matrix.fillRect(GARAGE_X + 1, GARAGE_Y + 2, 1, 1, 0);
    }
    show();
}

void Display::showNetworkStatus(
    NetworkStatus wifi,
    NetworkStatus ntp,
    NetworkStatus mqtt
) {
    _wifiStatus = wifi;
    _ntpStatus = ntp;
    _mqttStatus = mqtt;
    show();
}

void Display::showMessage(const char* message) {
    matrix.fillScreen(0);
    matrix.setCursor(0, 0);
    matrix.setTextColor(colors[1]);
    matrix.print(message);
    show();
}

void Display::clear() {
    matrix.fillScreen(0);
    show();
}

void Display::show() {
    // Status indicators are applied last, after all other GFX drawing, so
    // clearing/redrawing the clock cannot erase them.
    matrixleds[matrix.XY(STATUS_X, WIFI_STATUS_Y)] = statusColor(_wifiStatus);
    matrixleds[matrix.XY(STATUS_X, NTP_STATUS_Y)] = statusColor(_ntpStatus);
    matrixleds[matrix.XY(STATUS_X, MQTT_STATUS_Y)] = statusColor(_mqttStatus);
    matrix.show();
}
