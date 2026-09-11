#include "device_config.h"

#include <Preferences.h>

#include "secrets.h"
#include "config.h"

namespace {

constexpr const char* NVS_NAMESPACE = "device";

constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
constexpr const char* KEY_WIFI_PASSWORD = "wifi_pass";

constexpr const char* KEY_MQTT_SERVER = "mqtt_server";
constexpr const char* KEY_MQTT_PORT = "mqtt_port";
constexpr const char* KEY_MQTT_USERNAME = "mqtt_user";
constexpr const char* KEY_MQTT_PASSWORD = "mqtt_pass";

constexpr const char* KEY_TIME_ZONE = "time_zone";

Preferences preferences;

void copyString(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0) {
        return;
    }

    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
}

} // namespace

bool DeviceConfig::begin() {
    // Start with the compiled-in defaults. Individual NVS keys below can
    // override these independently.
    copyString(_wifiSsid, sizeof(_wifiSsid), WIFI_SSID);
    copyString(_wifiPassword, sizeof(_wifiPassword), WIFI_PASSWORD);
    copyString(_mqttServer, sizeof(_mqttServer), MQTT_SERVER);
    _mqttPort = MQTT_PORT;
    copyString(_mqttUsername, sizeof(_mqttUsername), MQTT_USERNAME);
    copyString(_mqttPassword, sizeof(_mqttPassword), MQTT_PASSWORD);
    copyString(_timeZone, sizeof(_timeZone), TIME_ZONE);

    _loadedFromNvs = false;

    if (!preferences.begin(NVS_NAMESPACE, true)) {
        Serial.println("DeviceConfig: NVS unavailable, using defaults");
        Serial.print("DeviceConfig: timezone = ");
        Serial.println(_timeZone);
        return false;
    }

    // Load each setting independently. In particular, the timezone must not
    // depend on every network setting being present in NVS.
    bool loadedAny = false;

    if (preferences.isKey(KEY_WIFI_SSID)) {
        preferences.getString(KEY_WIFI_SSID, _wifiSsid, sizeof(_wifiSsid));
        loadedAny = true;
    }

    if (preferences.isKey(KEY_WIFI_PASSWORD)) {
        preferences.getString(KEY_WIFI_PASSWORD, _wifiPassword, sizeof(_wifiPassword));
        loadedAny = true;
    }

    if (preferences.isKey(KEY_MQTT_SERVER)) {
        preferences.getString(KEY_MQTT_SERVER, _mqttServer, sizeof(_mqttServer));
        loadedAny = true;
    }

    if (preferences.isKey(KEY_MQTT_PORT)) {
        _mqttPort = preferences.getUShort(KEY_MQTT_PORT, MQTT_PORT);
        loadedAny = true;
    }

    if (preferences.isKey(KEY_MQTT_USERNAME)) {
        preferences.getString(KEY_MQTT_USERNAME, _mqttUsername, sizeof(_mqttUsername));
        loadedAny = true;
    }

    if (preferences.isKey(KEY_MQTT_PASSWORD)) {
        preferences.getString(KEY_MQTT_PASSWORD, _mqttPassword, sizeof(_mqttPassword));
        loadedAny = true;
    }

    if (preferences.isKey(KEY_TIME_ZONE)) {
        preferences.getString(KEY_TIME_ZONE, _timeZone, sizeof(_timeZone));
        loadedAny = true;
    }

    preferences.end();

    _loadedFromNvs = loadedAny;

    if (_loadedFromNvs) {
        Serial.println("DeviceConfig: loaded from NVS");
    } else {
        Serial.println("DeviceConfig: using compiled-in defaults");
    }

    Serial.print("DeviceConfig: timezone = ");
    Serial.println(_timeZone);

    return _loadedFromNvs;
}

bool DeviceConfig::save(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServer,
    uint16_t mqttPort,
    const char* mqttUsername,
    const char* mqttPassword,
    const char* timeZone
) {
    if (wifiSsid == nullptr ||
        wifiPassword == nullptr ||
        mqttServer == nullptr ||
        mqttUsername == nullptr ||
        mqttPassword == nullptr ||
        timeZone == nullptr ||
        mqttPort == 0) {
        return false;
    }

    if (strlen(wifiSsid) >= STRING_LENGTH ||
        strlen(wifiPassword) >= STRING_LENGTH ||
        strlen(mqttServer) >= STRING_LENGTH ||
        strlen(mqttUsername) >= STRING_LENGTH ||
        strlen(mqttPassword) >= STRING_LENGTH ||
        strlen(timeZone) >= STRING_LENGTH) {
        return false;
    }

    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("DeviceConfig: unable to open NVS for writing");
        return false;
    }

    const bool success =
        preferences.putString(KEY_WIFI_SSID, wifiSsid) > 0 &&
        preferences.putString(KEY_WIFI_PASSWORD, wifiPassword) > 0 &&
        preferences.putString(KEY_MQTT_SERVER, mqttServer) > 0 &&
        preferences.putUShort(KEY_MQTT_PORT, mqttPort) > 0 &&
        preferences.putString(KEY_MQTT_USERNAME, mqttUsername) > 0 &&
        preferences.putString(KEY_MQTT_PASSWORD, mqttPassword) > 0 &&
        preferences.putString(KEY_TIME_ZONE, timeZone) > 0;

    preferences.end();

    if (!success) {
        Serial.println("DeviceConfig: NVS write failed");
        return false;
    }

    copyString(_wifiSsid, sizeof(_wifiSsid), wifiSsid);
    copyString(_wifiPassword, sizeof(_wifiPassword), wifiPassword);
    copyString(_mqttServer, sizeof(_mqttServer), mqttServer);
    _mqttPort = mqttPort;
    copyString(_mqttUsername, sizeof(_mqttUsername), mqttUsername);
    copyString(_mqttPassword, sizeof(_mqttPassword), mqttPassword);
    copyString(_timeZone, sizeof(_timeZone), timeZone);
    _loadedFromNvs = true;

    Serial.println("DeviceConfig: configuration saved to NVS");
    Serial.print("DeviceConfig: saved timezone = ");
    Serial.println(_timeZone);
    return true;
}

const char* DeviceConfig::wifiSsid() const { return _wifiSsid; }
const char* DeviceConfig::wifiPassword() const { return _wifiPassword; }
const char* DeviceConfig::mqttServer() const { return _mqttServer; }
uint16_t DeviceConfig::mqttPort() const { return _mqttPort; }
const char* DeviceConfig::mqttUsername() const { return _mqttUsername; }
const char* DeviceConfig::mqttPassword() const { return _mqttPassword; }
const char* DeviceConfig::timeZone() const { return _timeZone; }
bool DeviceConfig::loadedFromNvs() const { return _loadedFromNvs; }
