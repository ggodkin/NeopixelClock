#include "config_portal.h"

#include <WiFi.h>
#include <WebServer.h>

#include "device_config.h"

namespace {

constexpr const char* AP_SSID = "NeopixelClock-Setup";

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);

String htmlEscape(const String& value) {
    String result;
    result.reserve(value.length() + 16);

    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += value[i]; break;
        }
    }

    return result;
}

} // namespace

bool ConfigPortal::begin(DeviceConfig& deviceConfig) {
    _deviceConfig = &deviceConfig;

    WiFi.mode(WIFI_AP);

    if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET)) {
        Serial.println("ConfigPortal: softAPConfig failed");
        return false;
    }

    if (!WiFi.softAP(AP_SSID)) {
        Serial.println("ConfigPortal: softAP start failed");
        return false;
    }

    server.on("/", HTTP_GET, [this]() {
        handleRoot();
    });

    server.on("/save", HTTP_POST, [this]() {
        handleSave();
    });

    server.onNotFound([]() {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    _running = true;

    Serial.println("Configuration mode enabled");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    return true;
}

void ConfigPortal::handle() {
    if (_running) {
        server.handleClient();
    }
}

void ConfigPortal::handleRoot() {
    if (_deviceConfig == nullptr) {
        server.send(500, "text/plain", "Configuration unavailable");
        return;
    }

    String html;

    html += "<!DOCTYPE html><html><head>";
    html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html += "<title>NeopixelClock Configuration</title></head><body>";
    html += "<h1>NeopixelClock</h1><h2>Configuration</h2>";
    html += "<form method=\"POST\" action=\"/save\">";

    html += "<p><label>Wi-Fi SSID<br>";
    html += "<input type=\"text\" name=\"wifi_ssid\" maxlength=\"127\" required value=\"";
    html += htmlEscape(String(_deviceConfig->wifiSsid()));
    html += "\"></label></p>";

    html += "<p><label>Wi-Fi Password<br>";
    html += "<input type=\"password\" name=\"wifi_password\" maxlength=\"127\" value=\"";
    html += htmlEscape(String(_deviceConfig->wifiPassword()));
    html += "\"></label></p>";

    html += "<hr>";

    html += "<p><label>MQTT Server<br>";
    html += "<input type=\"text\" name=\"mqtt_server\" maxlength=\"127\" required value=\"";
    html += htmlEscape(String(_deviceConfig->mqttServer()));
    html += "\"></label></p>";

    html += "<p><label>MQTT Port<br>";
    html += "<input type=\"number\" name=\"mqtt_port\" min=\"1\" max=\"65535\" required value=\"";
    html += String(_deviceConfig->mqttPort());
    html += "\"></label></p>";

    html += "<p><label>MQTT Username<br>";
    html += "<input type=\"text\" name=\"mqtt_username\" maxlength=\"127\" value=\"";
    html += htmlEscape(String(_deviceConfig->mqttUsername()));
    html += "\"></label></p>";

    html += "<p><label>MQTT Password<br>";
    html += "<input type=\"password\" name=\"mqtt_password\" maxlength=\"127\" value=\"";
    html += htmlEscape(String(_deviceConfig->mqttPassword()));
    html += "\"></label></p>";

    html += "<p><button type=\"submit\">Save Configuration</button></p>";
    html += "</form></body></html>";

    server.send(200, "text/html", html);
}

void ConfigPortal::handleSave() {
    if (_deviceConfig == nullptr) {
        server.send(500, "text/plain", "Configuration unavailable");
        return;
    }

    const char* fields[] = {
        "wifi_ssid",
        "wifi_password",
        "mqtt_server",
        "mqtt_port",
        "mqtt_username",
        "mqtt_password"
    };

    for (const char* field : fields) {
        if (!server.hasArg(field)) {
            server.send(400, "text/plain", "Missing configuration field");
            return;
        }
    }

    const String wifiSsid = server.arg("wifi_ssid");
    const String wifiPassword = server.arg("wifi_password");
    const String mqttServer = server.arg("mqtt_server");
    const String mqttPortString = server.arg("mqtt_port");
    const String mqttUsername = server.arg("mqtt_username");
    const String mqttPassword = server.arg("mqtt_password");

    const long mqttPort = mqttPortString.toInt();

    if (wifiSsid.length() == 0 ||
        mqttServer.length() == 0 ||
        mqttPort < 1 ||
        mqttPort > 65535) {
        server.send(400, "text/plain", "Invalid configuration");
        return;
    }

    if (!_deviceConfig->save(
            wifiSsid.c_str(),
            wifiPassword.c_str(),
            mqttServer.c_str(),
            static_cast<uint16_t>(mqttPort),
            mqttUsername.c_str(),
            mqttPassword.c_str())) {
        server.send(500, "text/plain", "Failed to save configuration");
        return;
    }

    Serial.println("Configuration saved. Restarting...");

    server.send(
        200,
        "text/html",
        "<!DOCTYPE html><html><head>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Saved</title></head><body>"
        "<h1>Configuration Saved</h1>"
        "<p>The device will restart now.</p>"
        "<p>Return the configuration switch to normal position before the device restarts.</p>"
        "</body></html>"
    );

    delay(1000);
    ESP.restart();
}
