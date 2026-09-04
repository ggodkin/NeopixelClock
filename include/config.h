#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------

constexpr uint8_t LED_DATA_PIN = 18;

constexpr uint8_t MATRIX_WIDTH  = 32;
constexpr uint8_t MATRIX_HEIGHT = 8;

constexpr uint16_t NUM_LEDS =
    MATRIX_WIDTH * MATRIX_HEIGHT;

// -----------------------------------------------------------------------------
// Main power detection
// -----------------------------------------------------------------------------
//
// GPIO34 is ADC1 and input-only.
// The actual ADC thresholds will be determined after the voltage divider
// is designed and measured.
//
// Leave these at zero until the power-sense hardware is implemented.
//

constexpr uint8_t MAIN_POWER_SENSE_PIN = 34;

constexpr uint16_t MAIN_POWER_PRESENT_THRESHOLD = 0;
constexpr uint16_t MAIN_POWER_ABSENT_THRESHOLD  = 0;

constexpr uint32_t POWER_PRESENT_CONFIRM_MS = 1000;
constexpr uint32_t POWER_ABSENT_CONFIRM_MS  = 1000;

// -----------------------------------------------------------------------------
// Battery voltage monitoring
// -----------------------------------------------------------------------------

constexpr uint8_t BATTERY_SENSE_PIN = 35;

// -----------------------------------------------------------------------------
// Time zone
// -----------------------------------------------------------------------------

constexpr const char* TIME_ZONE = "America/Denver";

// -----------------------------------------------------------------------------
// Outage display profiles
// -----------------------------------------------------------------------------
//
// These are deliberately configuration values rather than hard-coded logic.
// We will tune them after measuring the actual battery and LED current.
//

struct OutageProfile {
    uint32_t afterMs;
    uint32_t displayOnMs;
    uint32_t intervalMs;
};

constexpr OutageProfile OUTAGE_PROFILES[] = {
    // Initial profile:
    // display for 2 seconds, then wake/display every 60 seconds.
    { 0UL, 2000UL, 60000UL },
};

constexpr size_t NUM_OUTAGE_PROFILES =
    sizeof(OUTAGE_PROFILES) / sizeof(OUTAGE_PROFILES[0]);