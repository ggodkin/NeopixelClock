#pragma once

#include <stdint.h>

class PowerManager {
public:
    void begin();
    void update();

    bool isMainPowerPresent() const;
    bool isOutage() const;

    bool powerStateChanged() const;
    bool powerLost() const;
    bool powerRestored() const;

    uint16_t rawAdc() const;
    float adcVoltage() const;
    float usbVoltage() const;

private:
    uint16_t _rawAdc = 0;
    float _adcVoltage = 0.0f;
    float _usbVoltage = 0.0f;

    // Confirmed power state.
    bool _mainPowerPresent = true;

    // State currently being considered for confirmation.
    bool _candidateState = true;
    uint32_t _stateChangeCandidateSince = 0;

    // One-update transition/event flags.
    bool _powerStateChanged = false;
    bool _powerRestored = false;
    bool _powerLost = false;

    // Limit normal diagnostic output.
    uint32_t _lastDebugPrint = 0;
};