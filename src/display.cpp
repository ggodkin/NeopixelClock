#include "display.h"

#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include "config.h"

namespace {

// -----------------------------------------------------------------------------
// LED matrix
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Display colors
//
// Preserve the existing colors:
//   0 = red
//   1 = green
//   2 = blue
//   3 = off
// -----------------------------------------------------------------------------

const uint32_t colors[] = {
    matrix.Color(255, 0, 0),
    matrix.Color(0, 255, 0),
    matrix.Color(0, 0, 255),
    matrix.Color(0, 0, 0)
};

constexpr uint8_t BRIGHTNESS = 1;

// -----------------------------------------------------------------------------
// Garage indicator
// -----------------------------------------------------------------------------

constexpr int GARAGE_X = 28;
constexpr int GARAGE_Y = 0;

constexpr int GARAGE_WIDTH = 3;
constexpr int GARAGE_HEIGHT = 3;

} // namespace

// -----------------------------------------------------------------------------
// Begin
// -----------------------------------------------------------------------------

void Display::begin() {

    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(
        matrixleds,
        NUM_LEDS
    );

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

// -----------------------------------------------------------------------------
// Show time
// -----------------------------------------------------------------------------

void Display::showTime(
    int hours,
    int minutes
) {

    matrix.fillScreen(0);

    if (hours < 10) {
        matrix.setCursor(6, 0);
    } else {
        matrix.setCursor(0, 0);
    }

    matrix.setTextColor(colors[2]);

    String localMinutes;

    if (minutes < 10) {
        localMinutes =
            "0" +
            String(minutes);
    } else {
        localMinutes =
            String(minutes);
    }

    matrix.print(
        String(hours)
    );

    matrix.setCursor(16, 0);

    matrix.print(
        localMinutes
    );

    show();
}

// -----------------------------------------------------------------------------
// Colon
// -----------------------------------------------------------------------------

void Display::updateColon(
    bool on
) {

    matrix.setCursor(11, 0);

    if (on) {
        matrix.setTextColor(colors[2]);
    } else {
        matrix.setTextColor(colors[3]);
    }

    matrix.print(":");

    show();
}

// -----------------------------------------------------------------------------
// Garage indicator
// -----------------------------------------------------------------------------

void Display::showGarageClosed(
    bool closed
) {

    if (closed) {

        matrix.fillRect(
            GARAGE_X,
            GARAGE_Y,
            GARAGE_WIDTH,
            GARAGE_HEIGHT,
            colors[1]
        );

    } else {

        matrix.drawRect(
            GARAGE_X,
            GARAGE_Y,
            GARAGE_WIDTH,
            GARAGE_HEIGHT,
            colors[0]
        );

        matrix.fillRect(
            GARAGE_X + 1,
            GARAGE_Y + 1,
            1,
            1,
            0
        );

        matrix.fillRect(
            GARAGE_X + 1,
            GARAGE_Y + 2,
            1,
            1,
            0
        );
    }

    show();
}

// -----------------------------------------------------------------------------
// Clear
// -----------------------------------------------------------------------------

void Display::clear() {

    matrix.fillScreen(0);

    show();
}

// -----------------------------------------------------------------------------
// Hardware update
// -----------------------------------------------------------------------------

void Display::show() {

    matrix.show();
}