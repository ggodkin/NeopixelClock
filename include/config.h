#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// NeoPixel display
// -----------------------------------------------------------------------------

constexpr uint8_t LED_DATA_PIN = 18;

constexpr uint8_t MATRIX_WIDTH = 32;
constexpr uint8_t MATRIX_HEIGHT = 8;

constexpr uint16_t NUM_LEDS =
    MATRIX_WIDTH * MATRIX_HEIGHT;


// -----------------------------------------------------------------------------
// Main power detection
// -----------------------------------------------------------------------------
//
// USB 5V is monitored through a voltage divider:
//
//     USB 5V
//        |
//       12k
//        |
//        +------ GPIO34
//        |
//       22k
//        |
//       GND
//
// GPIO34 is an ESP32 ADC1 input-only pin.
//
// Measured behavior:
//   USB present: ADC raw approximately 4040-4080
//   USB absent:  ADC raw approximately 0-3
//
// We therefore use raw ADC thresholds with substantial hysteresis.
//

constexpr uint8_t MAIN_POWER_SENSE_PIN = 34;

// USB is considered definitely present above this ADC value.
constexpr uint16_t MAIN_POWER_PRESENT_THRESHOLD = 3000;

// USB is considered definitely absent below this ADC value.
constexpr uint16_t MAIN_POWER_ABSENT_THRESHOLD = 1000;

// A new state must remain continuously detected for this long
// before the state transition is accepted.
constexpr uint32_t POWER_PRESENT_CONFIRM_MS = 1000;
constexpr uint32_t POWER_ABSENT_CONFIRM_MS = 1000;


// -----------------------------------------------------------------------------
// Battery sensing
// -----------------------------------------------------------------------------

// Reserved for future battery-voltage monitoring.
constexpr uint8_t BATTERY_SENSE_PIN = 35;


// -----------------------------------------------------------------------------
// Time zone
// -----------------------------------------------------------------------------

constexpr const char* TIME_ZONE = "America/Denver";


// -----------------------------------------------------------------------------
// Outage profiles
// -----------------------------------------------------------------------------
//
// afterMs:
//     Time after entering an outage before this profile becomes active.
//
// displayOnMs:
//     How long the display remains on during an outage activity period.
//
// intervalMs:
//     How long the display remains off between outage activity periods.
//
// During the outage the display therefore cycles as:
//
//     2 seconds ON -> 30 seconds OFF -> repeat
//
// These values are the initial fixed outage profile. Adaptive profiles and
// sleep behavior will be added separately.
//

struct OutageProfile {
    uint32_t afterMs;
    uint32_t displayOnMs;
    uint32_t intervalMs;
};

constexpr OutageProfile OUTAGE_PROFILES[] = {
    {0UL, 2000UL, 30000UL}
};

constexpr size_t NUM_OUTAGE_PROFILES =
    sizeof(OUTAGE_PROFILES) / sizeof(OUTAGE_PROFILES[0]);

