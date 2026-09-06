// NeopixelClock
//
// PlatformIO / ESP32 migration.
//
// Current architecture:
//   - Timekeeper: native ESP32 clock, SNTP, AceTime timezone conversion
//   - Display: FastLED_NeoMatrix for the 32x8 display
//   - PubSubClient for MQTT
//   - ArduinoOTA for OTA updates
//   - PowerManager for confirmed USB power state

#include <Arduino.h>

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "config.h"
#include "secrets.h"
#include "timekeeper.h"
#include "display.h"
#include "power_manager.h"

#define DEBUG 1

#if DEBUG == 1
    #define debug(x)   Serial.print(x)
    #define debugln(x) Serial.println(x)
#else
    #define debug(x)
    #define debugln(x)
#endif

PowerManager powerManager;
Display display;
Timekeeper timekeeper;

// -----------------------------------------------------------------------------
// WiFi / MQTT
// -----------------------------------------------------------------------------

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WiFiClient espClient;

PubSubClient client(
    espClient
);

// -----------------------------------------------------------------------------
// Display state
// -----------------------------------------------------------------------------

bool cursorOn = true;

volatile bool garageDoorClosedStatus = false;

// -----------------------------------------------------------------------------
// Outage state
// -----------------------------------------------------------------------------

bool outageMode = false;
bool outageDisplayOn = true;
uint32_t outageStartedAt = 0;

constexpr OutageProfile ACTIVE_OUTAGE_PROFILE = OUTAGE_PROFILES[0];

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
// Function declarations
// -----------------------------------------------------------------------------

void callback(
    char* topic,
    byte* payload,
    unsigned int length
);

void reconnect();

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
            MQTT_USERNAME,
            MQTT_PASSWORD
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
// Setup
// -----------------------------------------------------------------------------

void setup() {

    Serial.begin(115200);

    delay(100);

    debugln();
    debugln("NeopixelClock ESP32 starting...");

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

            display.clear();

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

    display.clear();

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
        MQTT_SERVER,
        MQTT_PORT
    );

    client.setCallback(callback);

    debugln("Setup complete");
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop() {

    // -------------------------------------------------------------------------
    // Power management
    // -------------------------------------------------------------------------

    powerManager.update();

    // Confirmed power loss starts the outage display timer.
    if (powerManager.powerLost()) {

        outageMode = true;
        outageDisplayOn = true;
        outageStartedAt = millis();

        debugln("Outage started");
    }

    // Confirmed power restoration immediately returns to normal display mode.
    if (powerManager.powerRestored()) {

        outageMode = false;
        outageDisplayOn = true;

        debugln("Outage ended");

        redrawDisplay();
    }

    // After the configured display-on period, clear the display and remain
    // awake. Sleep and network shutdown are intentionally separate stages.
    if (
        outageMode &&
        outageDisplayOn &&
        millis() - outageStartedAt >= ACTIVE_OUTAGE_PROFILE.displayOnMs
    ) {

        outageDisplayOn = false;
        display.clear();

        debugln("Outage display off");
    }

    // -------------------------------------------------------------------------
    // MQTT
    // -------------------------------------------------------------------------

    if (!client.connected()) {
        reconnect();
    }

    client.loop();

    // -------------------------------------------------------------------------
    // OTA
    // -------------------------------------------------------------------------

    ArduinoOTA.handle();

    // -------------------------------------------------------------------------
    // Clock
    // -------------------------------------------------------------------------

    timekeeper.update();

    if (!timekeeper.isValid()) {
        return;
    }

    // Do not update the physical display while the outage display period has
    // expired. Timekeeping and networking continue unchanged for this test.
    const bool displayUpdatesAllowed =
        !outageMode || outageDisplayOn;

    // -------------------------------------------------------------------------
    // Minute changed
    // -------------------------------------------------------------------------

    if (timekeeper.minuteChanged()) {

        if (displayUpdatesAllowed) {
            redrawDisplay();
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

        client.publish(
            "WatchBroom/Time",
            msg
        );

        debugln(msg);
    }

    // -------------------------------------------------------------------------
    // Once-per-second processing
    // -------------------------------------------------------------------------

    if (timekeeper.secondChanged()) {

        cursorOn = !cursorOn;

        // Ambient-light handling is intentionally NOT migrated yet.
        //
        // The ESP8266 A0 pin has no equivalent clean-sheet hardware mapping.
        // We'll decide on the ambient-light sensor input as part of the
        // hardware design rather than silently assigning a GPIO.

        if (displayUpdatesAllowed) {

            display.updateColon(cursorOn);

            display.showGarageClosed(
                garageDoorClosedStatus
            );
        }
    }
}
