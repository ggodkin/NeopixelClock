#pragma once

#include <Arduino.h>

class DeviceConfig;

class ConfigPortal {
public:
    bool begin(DeviceConfig& deviceConfig);
    void handle();

private:
    DeviceConfig* _deviceConfig = nullptr;
    bool _running = false;

    void handleRoot();
    void handleSave();
};
