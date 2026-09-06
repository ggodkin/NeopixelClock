#include "power_manager.h"

#include <Arduino.h>

#include "config.h"

namespace {

constexpr float R1 = 12000.0f;
constexpr float R2 = 22000.0f;

constexpr float DIVIDER_RATIO = (R1 + R2) / R2;
constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;

constexpr uint8_t ADC_SAMPLES = 16;

// Normal diagnostics are printed at most once per second.
constexpr uint32_t POWER_DEBUG_INTERVAL_MS = 1000UL;

}  // namespace


void PowerManager::begin() {
    pinMode(MAIN_POWER_SENSE_PIN, INPUT);

    analogReadResolution(12);

    Serial.println("PowerManager initialized");
    Serial.println("USB power sensing on GPIO34");
    Serial.println("Divider: 12k / 22k");

    // Establish the initial ADC reading and state.
    uint32_t total = 0;

    for (uint8_t i = 0; i < ADC_SAMPLES; ++i) {
        total += analogRead(MAIN_POWER_SENSE_PIN);
    }

    _rawAdc = static_cast<uint16_t>(total / ADC_SAMPLES);

    _adcVoltage =
        (static_cast<float>(_rawAdc) / 4095.0f) *
        ADC_REFERENCE_VOLTAGE;

    _usbVoltage = _adcVoltage * DIVIDER_RATIO;

    // At startup, establish the state immediately rather than waiting
    // through the normal confirmation period.
    if (_rawAdc >= MAIN_POWER_PRESENT_THRESHOLD) {
        _mainPowerPresent = true;
    } else if (_rawAdc <= MAIN_POWER_ABSENT_THRESHOLD) {
        _mainPowerPresent = false;
    } else {
        // Ambiguous startup reading: retain the safe default.
        _mainPowerPresent = true;
    }

    _candidateState = _mainPowerPresent;
    _stateChangeCandidateSince = millis();

    _powerStateChanged = false;
    _powerLost = false;
    _powerRestored = false;

    _lastDebugPrint = millis();

    Serial.print("Initial USB ADC raw: ");
    Serial.print(_rawAdc);
    Serial.print("  Main power: ");
    Serial.println(_mainPowerPresent ? "PRESENT" : "ABSENT");
}


void PowerManager::update() {
    const uint32_t now = millis();

    // Events are valid for only this update cycle.
    _powerStateChanged = false;
    _powerLost = false;
    _powerRestored = false;

    // ------------------------------------------------------------------
    // Read and average the ADC.
    // ------------------------------------------------------------------

    uint32_t total = 0;

    for (uint8_t i = 0; i < ADC_SAMPLES; ++i) {
        total += analogRead(MAIN_POWER_SENSE_PIN);
    }

    _rawAdc = static_cast<uint16_t>(total / ADC_SAMPLES);

    _adcVoltage =
        (static_cast<float>(_rawAdc) / 4095.0f) *
        ADC_REFERENCE_VOLTAGE;

    _usbVoltage = _adcVoltage * DIVIDER_RATIO;


    // ------------------------------------------------------------------
    // Convert the ADC reading into a candidate power state.
    //
    // The raw ADC value is used for detection. The calculated voltage is
    // informational only because the ESP32 ADC reference is not exactly
    // 3.3 V.
    //
    // Hysteresis:
    //
    //   >= PRESENT_THRESHOLD -> definitely present
    //   <= ABSENT_THRESHOLD  -> definitely absent
    //   between thresholds   -> retain current state
    // ------------------------------------------------------------------

    bool measuredState = _mainPowerPresent;

    if (_rawAdc >= MAIN_POWER_PRESENT_THRESHOLD) {
        measuredState = true;
    } else if (_rawAdc <= MAIN_POWER_ABSENT_THRESHOLD) {
        measuredState = false;
    }


    // ------------------------------------------------------------------
    // Candidate state handling.
    //
    // Whenever the measured state differs from the candidate, start a
    // new confirmation timer.
    // ------------------------------------------------------------------

    if (measuredState != _candidateState) {
        _candidateState = measuredState;
        _stateChangeCandidateSince = now;
    }


    // ------------------------------------------------------------------
    // Confirm a candidate state after the required stable period.
    // ------------------------------------------------------------------

    if (_candidateState != _mainPowerPresent) {
        const uint32_t confirmationTime =
            _candidateState
                ? POWER_PRESENT_CONFIRM_MS
                : POWER_ABSENT_CONFIRM_MS;

        if (now - _stateChangeCandidateSince >= confirmationTime) {

            const bool oldState = _mainPowerPresent;
            const bool newState = _candidateState;

            _mainPowerPresent = newState;

            _powerStateChanged = true;
            _powerLost = oldState && !newState;
            _powerRestored = !oldState && newState;

            // The confirmed state is now also the candidate state.
            _candidateState = _mainPowerPresent;
            _stateChangeCandidateSince = now;

            // Always report actual state transitions immediately.
            Serial.print("Main power ");
            Serial.println(
                _mainPowerPresent ? "RESTORED" : "LOST");
        }
    }


    // ------------------------------------------------------------------
    // Reduced diagnostic output.
    //
    // This runs only once per POWER_DEBUG_INTERVAL_MS instead of on
    // every pass through loop().
    // ------------------------------------------------------------------

    if (now - _lastDebugPrint >= POWER_DEBUG_INTERVAL_MS) {
        _lastDebugPrint = now;

        Serial.print("USB ADC raw: ");
        Serial.print(_rawAdc);

        Serial.print("  ADC voltage: ");
        Serial.print(_adcVoltage, 3);
        Serial.print(" V");

        Serial.print("  USB voltage: ");
        Serial.print(_usbVoltage, 3);
        Serial.print(" V");

        Serial.print("  Main power: ");
        Serial.println(
            _mainPowerPresent ? "PRESENT" : "ABSENT");
    }
}


bool PowerManager::isMainPowerPresent() const {
    return _mainPowerPresent;
}


bool PowerManager::isOutage() const {
    return !_mainPowerPresent;
}


bool PowerManager::powerStateChanged() const {
    return _powerStateChanged;
}


bool PowerManager::powerLost() const {
    return _powerLost;
}


bool PowerManager::powerRestored() const {
    return _powerRestored;
}


uint16_t PowerManager::rawAdc() const {
    return _rawAdc;
}


float PowerManager::adcVoltage() const {
    return _adcVoltage;
}


float PowerManager::usbVoltage() const {
    return _usbVoltage;
}