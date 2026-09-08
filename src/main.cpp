// NeopixelClock
//
// PlatformIO / ESP32 migration.
//
// Current architecture:
//   - Timekeeper: native ESP32 clock, SNTP, AceTime timezone conversion
//   - FastLED_NeoMatrix for the 32x8 display
//   - PubSubClient for MQTT
//   - ArduinoOTA for OTA updates
//
// Power-management and further modularization will be added later.

#include <Arduino.h>

#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>

#include "config.h"
#include "secrets.h"
#include "timekeeper.h"
#include "display.h"
#include "power_manager.h"
#include "device_config.h"
#include "config_portal.h"

#define DEBUG 1

#if DEBUG == 1
    #define debug(x)   Serial.print(x)
    #define debugln(x) Serial.println(x)
#else
    #define debug(x)
    #define debugln(x)
#endif

PowerManager powerManager;
DeviceConfig deviceConfig;
ConfigPortal configPortal;
bool configurationMode = false;

// -----------------------------------------------------------------------------
// WiFi / MQTT
// -----------------------------------------------------------------------------

const char* ssid = nullptr;
const char* password = nullptr;

WiFiClient espClient;

PubSubClient client(
    espClient
);

// -----------------------------------------------------------------------------
// Timekeeper
// -----------------------------------------------------------------------------

Timekeeper timekeeper;

// -----------------------------------------------------------------------------
// Display state
// -----------------------------------------------------------------------------

Display display;

bool cursorOn = true;

volatile bool garageDoorClosedStatus = false;

// -----------------------------------------------------------------------------
// Outage state
// -----------------------------------------------------------------------------

bool outageMode = false;
bool outageDisplayOn = true;
bool outageNetworkOff = false;
uint32_t outageDisplayStateSince = 0;

constexpr OutageProfile ACTIVE_OUTAGE_PROFILE = OUTAGE_PROFILES[0];

// -----------------------------------------------------------------------------
// OTA state
// -----------------------------------------------------------------------------

bool otaMode = false;

// -----------------------------------------------------------------------------
// MQTT state
// -----------------------------------------------------------------------------

constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;

uint32_t lastMqttReconnectAttempt = 0;

// -----------------------------------------------------------------------------
// MQTT messages
// -----------------------------------------------------------------------------

constexpr size_t MSG_BUFFER_SIZE = 50;

char msg[MSG_BUFFER_SIZE];
char msgOut[MSG_BUFFER_SIZE];

String msgStr;

// -----------------------------------------------------------------------------
// LED matrix
// -----------------------------------------------------------------------------

CRGB matrixleds[NUM_LEDS];

FastLED_NeoMatrix* matrix =
    new FastLED_NeoMatrix(
        matrixleds,
        MATRIX_WIDTH,
        MATRIX_HEIGHT,
        NEO_MATRIX_TOP +
        NEO_MATRIX_LEFT +
        NEO_MATRIX_COLUMNS +
        NEO_MATRIX_ZIGZAG
    );

const uint32_t colors[] = {
    matrix->Color(255, 0, 0),
    matrix->Color(0, 255, 0),
    matrix->Color(0, 0, 255),
    matrix->Color(0, 0, 0)
};

// -----------------------------------------------------------------------------
// Function declarations
// -----------------------------------------------------------------------------

void callback(
    char* topic,
    byte* payload,
    unsigned int length
);

void reconnect();
void updateOtaMode();

void displayTime(
    int dispHours,
    int dispMinutes
);

void displayGarageClosed(
    bool closedInd
);

void redrawDisplay();

// -----------------------------------------------------------------------------
// MQTT callback
// -----------------------------------------------------------------------------

void callback(
    char* topic,
    byte* payload,
    unsigned int length
) {

    debug("Message arrived [");
    debug(topic);
    debug("] ");

    for (unsigned int i = 0; i < length; i++) {
        debug((char)payload[i]);
    }

    debugln();

    if (length > 0 && payload[0] == '1') {
        garageDoorClosedStatus = true;
    } else {
        garageDoorClosedStatus = false;
    }
}

// -----------------------------------------------------------------------------
// MQTT reconnect
// -----------------------------------------------------------------------------

void reconnect() {

    if (outageNetworkOff || WiFi.status() != WL_CONNECTED) {
        return;
    }

    uint32_t nowMillis = millis();

    if (
        nowMillis - lastMqttReconnectAttempt <
        MQTT_RECONNECT_INTERVAL_MS
    ) {
        return;
    }

    lastMqttReconnectAttempt = nowMillis;

    debug("Attempting MQTT connection...");

    String clientId = "ESP32Client-";

    clientId += String(
        static_cast<uint32_t>(random(0xffff)),
        HEX
    );

    if (
        client.connect(
            clientId.c_str(),
            deviceConfig.mqttUsername(),
            deviceConfig.mqttPassword()
        )
    ) {

        debugln("connected");

        client.publish(
            "outTopic",
            "hello world"
        );

        client.subscribe(
            "cmnd/NeopixelClock/GarageDoorClosed"
        );

    } else {

        debug("failed, rc=");
        debugln(client.state());
    }
}

// -----------------------------------------------------------------------------
// Display redraw helper
// -----------------------------------------------------------------------------

void redrawDisplay() {

    if (!timekeeper.isValid()) {
        return;
    }

    display.showTime(
        timekeeper.hour(),
        timekeeper.minute()
    );

    display.showGarageClosed(
        garageDoorClosedStatus
    );
}

// -----------------------------------------------------------------------------
// OTA mode
// -----------------------------------------------------------------------------

void updateOtaMode() {

    const bool otaRequested =
        digitalRead(OTA_ENABLE_PIN) == LOW;

    const bool shouldBeOtaMode =
        otaRequested && !outageMode && !outageNetworkOff;

    if (shouldBeOtaMode == otaMode) {
        return;
    }

    otaMode = shouldBeOtaMode;

    if (otaMode) {
        display.showMessage("OTA");
        debugln("OTA mode enabled");
    } else {
        redrawDisplay();
        debugln("OTA mode disabled");
    }
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {

    Serial.begin(115200);

    delay(100);

    debugln();
    debugln("NeopixelClock ESP32 starting...");

    // -------------------------------------------------------------------------
    // OTA enable input
    // -------------------------------------------------------------------------

    pinMode(OTA_ENABLE_PIN, INPUT_PULLUP);

    // -------------------------------------------------------------------------
    // Power manager
    // -------------------------------------------------------------------------

    powerManager.begin();

    // -------------------------------------------------------------------------
    // Display
    // -------------------------------------------------------------------------

    display.begin();

    debugln("Display setup");

    // -------------------------------------------------------------------------
    // Device configuration
    // -------------------------------------------------------------------------

    pinMode(CONFIG_ENABLE_PIN, INPUT_PULLUP);

    const bool configurationRequested =
        digitalRead(CONFIG_ENABLE_PIN) == LOW;

    configurationMode = configurationRequested;

    deviceConfig.begin();

    if (configurationMode) {

        display.showMessage("CONFIG");

        debugln("Configuration mode requested");

        if (!configPortal.begin(deviceConfig)) {
            debugln("Configuration portal failed");

            while (true) {
                delay(1000);
            }
        }

        return;
    }

    ssid = deviceConfig.wifiSsid();
    password = deviceConfig.wifiPassword();

    // -------------------------------------------------------------------------
    // WiFi
    // -------------------------------------------------------------------------

    debugln("Before WiFi.mode()");

    WiFi.mode(WIFI_STA);

    debugln("After WiFi.mode()");

    WiFi.begin(
        ssid,
        password
    );

    debugln("After WiFi.begin()");

    uint32_t wifiStart = millis();

    while (WiFi.status() != WL_CONNECTED) {

        delay(250);

        debug(".");

        if (millis() - wifiStart >= 15000) {

            debugln();
            debugln("WiFi connection timeout");

            matrix->fillScreen(0);
            matrix->setCursor(0, 0);
            matrix->setTextColor(colors[2]);
            matrix->print("noWIFI");
            matrix->show();

            delay(2000);

            ESP.restart();
        }
    }

    debugln();

    debug("WiFi connected. IP: ");
    debugln(WiFi.localIP());

    // -------------------------------------------------------------------------
    // WiFi status
    // -------------------------------------------------------------------------

    matrix->fillScreen(0);

    matrix->setCursor(0, 0);

    matrix->setTextColor(colors[1]);

    matrix->print("WiFiOk");

    matrix->show();

    delay(1000);

    // -------------------------------------------------------------------------
    // Timekeeper / native SNTP
    // -------------------------------------------------------------------------

    timekeeper.begin();

    // -------------------------------------------------------------------------
    // Initial time display
    // -------------------------------------------------------------------------

    redrawDisplay();

    // -------------------------------------------------------------------------
    // OTA
    // -------------------------------------------------------------------------

    ArduinoOTA.begin();

    debugln("OTA ready");

    // -------------------------------------------------------------------------
    // MQTT
    // -------------------------------------------------------------------------

    client.setServer(
        deviceConfig.mqttServer(),
        deviceConfig.mqttPort()
    );

    client.setCallback(callback);

    debugln("Setup complete");

    updateOtaMode();
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop() {

    // Configuration mode is selected only at boot. No normal networking,
    // OTA, clock, or outage processing is performed while the portal runs.
    if (configurationMode) {
        configPortal.handle();
        delay(2);
        return;
    }

    // -------------------------------------------------------------------------
    // Power management
    // -------------------------------------------------------------------------

    powerManager.update();

    if (powerManager.powerLost()) {

        outageMode = true;
        outageDisplayOn = true;
        outageNetworkOff = false;
        outageDisplayStateSince = millis();

        otaMode = false;

        debugln("Outage started");
    }

    if (powerManager.powerRestored()) {

        outageMode = false;
        outageDisplayOn = true;
        outageNetworkOff = false;
        outageDisplayStateSince = millis();

        debugln("Outage ended");

        WiFi.mode(WIFI_STA);
        WiFi.begin(
            ssid,
            password
        );

        lastMqttReconnectAttempt = millis() - MQTT_RECONNECT_INTERVAL_MS;

        redrawDisplay();
    }

    if (outageMode) {

        uint32_t nowMillis = millis();

        if (
            outageDisplayOn &&
            nowMillis - outageDisplayStateSince >=
                ACTIVE_OUTAGE_PROFILE.displayOnMs
        ) {

            outageDisplayOn = false;
            outageDisplayStateSince = nowMillis;
            display.clear();

            debugln("Outage display off");

            if (!outageNetworkOff) {

                if (client.connected()) {
                    client.disconnect();
                }

                WiFi.mode(WIFI_OFF);
                outageNetworkOff = true;

                debugln("Outage network off");
            }
        }
        else if (
            !outageDisplayOn &&
            nowMillis - outageDisplayStateSince >=
                ACTIVE_OUTAGE_PROFILE.intervalMs
        ) {

            outageDisplayOn = true;
            outageDisplayStateSince = nowMillis;

            redrawDisplay();

            debugln("Outage display on");
        }
    }

    // -------------------------------------------------------------------------
    // OTA mode
    // -------------------------------------------------------------------------

    updateOtaMode();

    // -------------------------------------------------------------------------
    // MQTT
    // -------------------------------------------------------------------------

    if (!outageNetworkOff) {

        if (!client.connected()) {
            reconnect();
        }

        client.loop();
    }

    // -------------------------------------------------------------------------
    // OTA
    // -------------------------------------------------------------------------

    if (
        otaMode &&
        !outageNetworkOff &&
        WiFi.status() == WL_CONNECTED
    ) {
        ArduinoOTA.handle();
    }

    // -------------------------------------------------------------------------
    // Clock
    // -------------------------------------------------------------------------

    timekeeper.update();

    if (!timekeeper.isValid()) {
        return;
    }

    const bool displayUpdatesAllowed =
        !otaMode &&
        (!outageMode || outageDisplayOn);

    // -------------------------------------------------------------------------
    // Minute changed
    // -------------------------------------------------------------------------

    if (timekeeper.minuteChanged()) {

        if (displayUpdatesAllowed) {
            display.showTime(
                timekeeper.hour(),
                timekeeper.minute()
            );

            display.showGarageClosed(
                garageDoorClosedStatus
            );
        }

        msgStr =
            String(timekeeper.hour()) +
            " : " +
            String(timekeeper.minute());

        msgStr.toCharArray(
            msgOut,
            MSG_BUFFER_SIZE
        );

        snprintf(
            msg,
            MSG_BUFFER_SIZE,
            "%s",
            msgOut
        );

        if (!outageNetworkOff) {
            client.publish(
                "WatchBroom/Time",
                msg
            );
        }

        debugln(msg);
    }

    // -------------------------------------------------------------------------
    // Once-per-second processing
    // -------------------------------------------------------------------------

    if (timekeeper.secondChanged()) {

        cursorOn = !cursorOn;

        if (displayUpdatesAllowed) {
            display.updateColon(cursorOn);

            display.showGarageClosed(
                garageDoorClosedStatus
            );
        }
    }
}
