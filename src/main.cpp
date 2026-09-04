// NeopixelClock
//
// Initial PlatformIO / ESP32 migration.
//
// Based on AceTime, FastLED_NeoMatrix and PubSubClient.
//
// This version intentionally preserves the basic behavior of the original
// ESP8266 project. Power-management and modularization will be added later.

#include <Arduino.h>

#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>

#include <AceRoutine.h>
#include <AceTime.h>
#include <AceTimeClock.h>

#include "config.h"
#include "secrets.h"

#define DEBUG 1

#if DEBUG == 1
    #define debug(x)   Serial.print(x)
    #define debugln(x) Serial.println(x)
#else
    #define debug(x)
    #define debugln(x)
#endif

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
// Time
// -----------------------------------------------------------------------------

using namespace ace_time;
using namespace ace_time::clock;

acetime_t prevSeconds = 0;

static BasicZoneProcessor denverProcessor;
static NtpClock ntpClock;
static SystemClockLoop systemClock(&ntpClock, nullptr);

// -----------------------------------------------------------------------------
// Display state
// -----------------------------------------------------------------------------

unsigned int prevMinutes = 0;

bool cursorOn = true;
volatile bool garageDoorClosedStatus = false;

uint8_t brightness = 1;

// -----------------------------------------------------------------------------
// MQTT messages
// -----------------------------------------------------------------------------

constexpr size_t MSG_BUFFER_SIZE = 50;

char msg[MSG_BUFFER_SIZE];
char msgOut[MSG_BUFFER_SIZE];

int value = 0;

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

// RGB colors.
//
// The original sketch had only 3 entries but accessed colors[3] when
// turning the colon off. That was an out-of-bounds access.
//
// We give the fourth entry its intended value: black/off.
//
const uint32_t colors[] = {
    matrix->Color(255, 0, 0),  // 0 = red
    matrix->Color(0, 255, 0),  // 1 = green
    matrix->Color(0, 0, 255),  // 2 = blue
    matrix->Color(0, 0, 0)     // 3 = off
};

// -----------------------------------------------------------------------------
// Function declarations
// -----------------------------------------------------------------------------

void callback(char* topic, byte* payload, unsigned int length);

void reconnect();

void displayTime(
    int dispHours,
    int dispMinutes
);

void displayGarageClosed(
    bool closedInd
);

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

    // Temporary migration behavior.
    //
    // This retains the original blocking reconnect behavior.
    // We will replace this with non-blocking reconnect logic in the
    // network/MQTT refactor.

    while (!client.connected()) {

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
            debug(client.state());

            debugln(" try again in 5 seconds");

            delay(5000);
        }
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
    // Display
    // -------------------------------------------------------------------------

    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(
        matrixleds,
        NUM_LEDS
    );

    matrix->begin();

    matrix->setTextWrap(false);

    matrix->setBrightness(brightness);

    matrix->setTextColor(colors[1]);

    matrix->print("Setup");

    debugln("Display setup");

    matrix->show();

    delay(1000);

    // -------------------------------------------------------------------------
    // WiFi
    // -------------------------------------------------------------------------

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        ssid,
        password
    );

    debug("Connecting to WiFi");

    while (
        WiFi.waitForConnectResult() != WL_CONNECTED
    ) {

        matrix->fillScreen(0);

        matrix->setCursor(0, 0);

        matrix->setTextColor(colors[2]);

        matrix->print("noWIFI");

        debugln("No WiFi");

        matrix->show();

        delay(1000);

        ESP.restart();
    }

    debugln();
    debug("WiFi connected. IP: ");
    debugln(WiFi.localIP());

    // -------------------------------------------------------------------------
    // Time zone
    // -------------------------------------------------------------------------

    auto denverTz =
        TimeZone::forZoneInfo(
            &zonedb::kZoneAmerica_Denver,
            &denverProcessor
        );

    (void) denverTz;

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
    // NTP
    // -------------------------------------------------------------------------

    ntpClock.setup();

    if (!ntpClock.isSetup()) {

        debugln("NTP setup failed");

        return;
    }

    acetime_t nowSeconds = ntpClock.getNow();

    if (nowSeconds > 0) {

        systemClock.setNow(nowSeconds);

        debug("NTP time: ");
        debugln(nowSeconds);

    } else {

        debugln("NTP did not return valid time");

        return;
    }

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
    // MQTT
    // -------------------------------------------------------------------------

    if (!client.connected()) {
        reconnect();
    }

    client.loop();

    // -------------------------------------------------------------------------
    // OTA
    // -------------------------------------------------------------------------
    //
    // Unlike the original sketch, OTA is serviced continuously.
    // This is intentional and will remain the desired behavior.
    //

    ArduinoOTA.handle();

    // -------------------------------------------------------------------------
    // Clock
    // -------------------------------------------------------------------------

    unsigned int currentMinutes = 0;

    systemClock.loop();

    auto denverTz =
        TimeZone::forZoneInfo(
            &zonedb::kZoneAmerica_Denver,
            &denverProcessor
        );

    acetime_t nowSeconds = systemClock.getNow();

    auto denverTime =
        ZonedDateTime::forEpochSeconds(
            nowSeconds,
            denverTz
        );

    currentMinutes = denverTime.minute();

    // -------------------------------------------------------------------------
    // Minute changed
    // -------------------------------------------------------------------------

    if (currentMinutes != prevMinutes) {

        displayTime(
            denverTime.hour(),
            currentMinutes
        );

        msgStr =
            String(denverTime.hour()) +
            " : " +
            String(denverTime.minute());

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

    if (nowSeconds != prevSeconds) {

        cursorOn = !cursorOn;

        prevSeconds = nowSeconds;

        // Ambient-light handling is intentionally NOT migrated yet.
        //
        // The ESP8266 A0 pin has no equivalent clean-sheet hardware mapping.
        // We'll decide on the ambient-light sensor input as part of the
        // hardware design rather than silently assigning a GPIO.

        matrix->setCursor(11, 0);

        if (cursorOn) {
            matrix->setTextColor(colors[2]);
        } else {
            matrix->setTextColor(colors[3]);
        }

        matrix->print(":");

        matrix->show();

        displayGarageClosed(
            garageDoorClosedStatus
        );
    }

    prevMinutes = currentMinutes;
}

// -----------------------------------------------------------------------------
// Display time
// -----------------------------------------------------------------------------

void displayTime(
    int dispHours,
    int dispMinutes
) {

    matrix->fillScreen(0);

    if (dispHours < 10) {
        matrix->setCursor(6, 0);
    } else {
        matrix->setCursor(0, 0);
    }

    matrix->setTextColor(colors[2]);

    String localMinutes;

    if (dispMinutes < 10) {
        localMinutes =
            "0" +
            String(dispMinutes);
    } else {
        localMinutes =
            String(dispMinutes);
    }

    matrix->print(
        String(dispHours)
    );

    matrix->setCursor(16, 0);

    matrix->print(
        localMinutes
    );

    matrix->show();

    displayGarageClosed(
        garageDoorClosedStatus
    );
}

// -----------------------------------------------------------------------------
// Garage indicator
// -----------------------------------------------------------------------------

void displayGarageClosed(
    bool closedInd
) {

    constexpr int bmx = 28;
    constexpr int bmy = 0;

    constexpr int dimx = 3;
    constexpr int dimy = 3;

    if (closedInd) {

        matrix->fillRect(
            bmx,
            bmy,
            dimx,
            dimy,
            colors[1]
        );

    } else {

        matrix->drawRect(
            bmx,
            bmy,
            dimx,
            dimy,
            colors[0]
        );

        matrix->fillRect(
            bmx + 1,
            bmy + 1,
            1,
            1,
            0
        );

        matrix->fillRect(
            bmx + 1,
            bmy + 2,
            1,
            1,
            0
        );
    }

    matrix->show();
}