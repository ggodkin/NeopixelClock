#pragma once

#include <Arduino.h>

class DeviceConfig {
public:
    bool begin();

    bool save(
        const char* wifiSsid,
        const char* wifiPassword,
        const char* mqttServer,
        uint16_t mqttPort,
        const char* mqttUsername,
        const char* mqttPassword
    );

    const char* wifiSsid() const;
    const char* wifiPassword() const;

    const char* mqttServer() const;
    uint16_t mqttPort() const;
    const char* mqttUsername() const;
    const char* mqttPassword() const;

    bool loadedFromNvs() const;

private:
    static constexpr size_t STRING_LENGTH = 128;

    char _wifiSsid[STRING_LENGTH] = {};
    char _wifiPassword[STRING_LENGTH] = {};

    char _mqttServer[STRING_LENGTH] = {};
    uint16_t _mqttPort = 1883;
    char _mqttUsername[STRING_LENGTH] = {};
    char _mqttPassword[STRING_LENGTH] = {};

    bool _loadedFromNvs = false;
};
