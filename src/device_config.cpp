#include "device_config.h"

#include <Preferences.h>

#include "secrets.h"

namespace {

constexpr const char* NVS_NAMESPACE = "device";

constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
constexpr const char* KEY_WIFI_PASSWORD = "wifi_pass";

constexpr const char* KEY_MQTT_SERVER = "mqtt_server";
constexpr const char* KEY_MQTT_PORT = "mqtt_port";
constexpr const char* KEY_MQTT_USERNAME = "mqtt_user";
constexpr const char* KEY_MQTT_PASSWORD = "mqtt_pass";

Preferences preferences;

void copyString(
    char* destination,
    size_t destinationSize,
    const char* source
) {
    if (destinationSize == 0) {
        return;
    }

    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }

    strncpy(
        destination,
        source,
        destinationSize - 1
    );

    destination[destinationSize - 1] = '\0';
}

} // namespace

bool DeviceConfig::begin() {

    // Start with the existing compiled-in configuration.
    // This preserves current behavior for a device that has never
    // been provisioned through NVS.
    copyString(
        _wifiSsid,
        sizeof(_wifiSsid),
        WIFI_SSID
    );

    copyString(
        _wifiPassword,
        sizeof(_wifiPassword),
        WIFI_PASSWORD
    );

    copyString(
        _mqttServer,
        sizeof(_mqttServer),
        MQTT_SERVER
    );

    _mqttPort = MQTT_PORT;

    copyString(
        _mqttUsername,
        sizeof(_mqttUsername),
        MQTT_USERNAME
    );

    copyString(
        _mqttPassword,
        sizeof(_mqttPassword),
        MQTT_PASSWORD
    );

    _loadedFromNvs = false;

    if (!preferences.begin(NVS_NAMESPACE, true)) {
        Serial.println("DeviceConfig: NVS unavailable, using defaults");
        return false;
    }

    const bool hasWifiSsid =
        preferences.isKey(KEY_WIFI_SSID);

    const bool hasWifiPassword =
        preferences.isKey(KEY_WIFI_PASSWORD);

    const bool hasMqttServer =
        preferences.isKey(KEY_MQTT_SERVER);

    const bool hasMqttPort =
        preferences.isKey(KEY_MQTT_PORT);

    const bool hasMqttUsername =
        preferences.isKey(KEY_MQTT_USERNAME);

    const bool hasMqttPassword =
        preferences.isKey(KEY_MQTT_PASSWORD);

    const bool hasCompleteConfiguration =
        hasWifiSsid &&
        hasWifiPassword &&
        hasMqttServer &&
        hasMqttPort &&
        hasMqttUsername &&
        hasMqttPassword;

    if (hasCompleteConfiguration) {

        preferences.getString(
            KEY_WIFI_SSID,
            _wifiSsid,
            sizeof(_wifiSsid)
        );

        preferences.getString(
            KEY_WIFI_PASSWORD,
            _wifiPassword,
            sizeof(_wifiPassword)
        );

        preferences.getString(
            KEY_MQTT_SERVER,
            _mqttServer,
            sizeof(_mqttServer)
        );

        _mqttPort =
            preferences.getUShort(
                KEY_MQTT_PORT,
                MQTT_PORT
            );

        preferences.getString(
            KEY_MQTT_USERNAME,
            _mqttUsername,
            sizeof(_mqttUsername)
        );

        preferences.getString(
            KEY_MQTT_PASSWORD,
            _mqttPassword,
            sizeof(_mqttPassword)
        );

        _loadedFromNvs = true;
    }

    preferences.end();

    if (_loadedFromNvs) {
        Serial.println("DeviceConfig: loaded from NVS");
    } else {
        Serial.println("DeviceConfig: using compiled-in defaults");
    }

    return _loadedFromNvs;
}

const char* DeviceConfig::wifiSsid() const {
    return _wifiSsid;
}

const char* DeviceConfig::wifiPassword() const {
    return _wifiPassword;
}

const char* DeviceConfig::mqttServer() const {
    return _mqttServer;
}

uint16_t DeviceConfig::mqttPort() const {
    return _mqttPort;
}

const char* DeviceConfig::mqttUsername() const {
    return _mqttUsername;
}

const char* DeviceConfig::mqttPassword() const {
    return _mqttPassword;
}

bool DeviceConfig::loadedFromNvs() const {
    return _loadedFromNvs;
}