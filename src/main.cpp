// NeopixelClock
//
// PlatformIO / ESP32 migration.
//
// Based on AceTime, FastLED_NeoMatrix and PubSubClient.
//
// Current architecture:
//   - ESP32 native system clock
//   - Native SNTP for time synchronization
//   - AceTime for America/Denver timezone/DST conversion
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

#include <AceRoutine.h>
#include <AceTime.h>
#include <AceTimeClock.h>

#include <time.h>

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

static BasicZoneProcessor denverProcessor;

int64_t prevSeconds = 0;

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
// The fourth entry is intentionally black/off.
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

void callback(
    char* topic,
    byte* payload,
    unsigned int length
);

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
    // It will be replaced with non-blocking reconnect logic during
    // the network/MQTT refactor.

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
    // Time zone
    // -------------------------------------------------------------------------

    auto denverTz =
        TimeZone::forZoneInfo(
            &zonedb::kZoneAmerica_Denver,
            &denverProcessor
        );
        
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
    // Native ESP32 SNTP
    // -------------------------------------------------------------------------
    //
    // The ESP32 system clock is the fundamental time source.
    //
    // SNTP synchronizes/corrects the system clock.
    // AceTime is then used only for timezone/DST conversion.
    //
    // We intentionally do NOT use AceTime's NtpClock here.
    //

    debugln("Starting native SNTP...");

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov",
        "time.google.com"
    );

    debugln("Waiting for NTP time...");

    struct tm timeinfo;

    uint32_t ntpStart = millis();

    bool ntpValid = false;

    while (
        !ntpValid &&
        millis() - ntpStart < 15000
    ) {

        ntpValid = getLocalTime(
            &timeinfo,
            1000
        );

        if (!ntpValid) {
            debug(".");
        }
    }

    debugln();

    if (!ntpValid) {

        debugln("NTP synchronization timeout");

        // Do not reset the ESP32.
        //
        // The main loop can continue running, and a later NTP
        // synchronization can be added during the network refactor.

    } else {

        char timeBuffer[64];

        strftime(
            timeBuffer,
            sizeof(timeBuffer),
            "%Y-%m-%d %H:%M:%S",
            &timeinfo
        );

        debugln("NTP synchronized!");

        debug("System time: ");
        debugln(timeBuffer);
    }

    // -------------------------------------------------------------------------
    // Initial time display
    // -------------------------------------------------------------------------

    if (ntpValid) {

        time_t now;

        time(&now);

        auto denverTz =
            TimeZone::forZoneInfo(
                &zonedb::kZoneAmerica_Denver,
                &denverProcessor
            );

        auto denverTime =
            ZonedDateTime::forUnixSeconds64(
                static_cast<int64_t>(now),
                denverTz
            );

        debug("Denver time: ");

        debug(denverTime.year());
        debug("-");
        debug(denverTime.month());
        debug("-");
        debug(denverTime.day());
        debug(" ");

        debug(denverTime.hour());
        debug(":");

        if (denverTime.minute() < 10) {
            debug("0");
        }

        debug(denverTime.minute());
        debug(":");

        if (denverTime.second() < 10) {
            debug("0");
        }

        debugln(denverTime.second());

        displayTime(
            denverTime.hour(),
            denverTime.minute()
        );

        prevMinutes = denverTime.minute();

        prevSeconds =
            static_cast<int64_t>(now);
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
    //
    // Temporary migration behavior.
    //
    // This is still blocking when the MQTT broker is unavailable.
    // We will replace this with non-blocking reconnect behavior later.
    //

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
    //
    // The ESP32 system clock is continuously maintained by the ESP32.
    // SNTP provides synchronization/correction.
    //
    // No AceTime SystemClockLoop is required.
    //

    time_t now;

    time(&now);

    auto denverTz =
        TimeZone::forZoneInfo(
            &zonedb::kZoneAmerica_Denver,
            &denverProcessor
        );

    auto denverTime =
        ZonedDateTime::forUnixSeconds64(
            static_cast<int64_t>(now),
            denverTz
        );

    unsigned int currentMinutes =
        denverTime.minute();

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

        prevMinutes = currentMinutes;
    }

    // -------------------------------------------------------------------------
    // Once-per-second processing
    // -------------------------------------------------------------------------

    int64_t nowSeconds =
        static_cast<int64_t>(now);

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