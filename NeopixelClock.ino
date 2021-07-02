// Based on AceTime and Adafruit_NeoMatrix libraries and examples.
//
// Change log:
//      Credits to NeoMatrix and AceTime libraries
//      change display only when minute changes
//      add leading 0 to minutes
//      only take positive NTP values
//      Reduce : spacing and blink it
//      OTA
// TODO indicate failed NTP and Failed WiFi requests
// TODO MQTT enable
// TODO Web control? Time Zone, WiFi credentials, Node Red...
// TODO RTC?
// TODO Garage status indicator
// TODO adjust light intecity based on ambient light

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#if !defined(ESP32) && !defined(ESP8266)
  #error This sketch works only for the ESP8266 and ESP32
#endif

#include <AceRoutine.h>
#include <AceTime.h>

// File in your library path to hold setings not to be shared with public
// in the following format
// #define SSID1 "Enter your SSID"
// #define PWD1 "Enter your network pasword"
// it will be extended with MQTT credentials

#include <home_wifi_multi.h>
const char* ssid = SSID1;
const char* password = PWD1;

unsigned int prevMinutes = 0;
boolean cursorOn = true;

using namespace ace_time;
using namespace ace_time::clock;

acetime_t prevSeconds = 0;

static const char SSID[] = SSID1;
static const char PASSWORD[] = PWD1;

static BasicZoneProcessor chicagoProcessor;
static NtpClock ntpClock;
static SystemClockLoop systemClock(&ntpClock, nullptr /*backup*/);

#define PIN 3

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN,
                            NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);
 
const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255)
};

void setup() {
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  matrix.setTextColor(colors[1]);
  matrix.print("Setup");
  matrix.show();
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    matrix.fillScreen(0);
    matrix.setCursor(0, 0);
    matrix.setTextColor(colors[2]);
    matrix.print("noWIFI");
    matrix.show();
    delay(5000);
    ESP.restart();
  }

  auto chicagoTz = TimeZone::forZoneInfo(
                     &zonedb::kZoneAmerica_Chicago, &chicagoProcessor);

  acetime_t nowSeconds;

  matrix.fillScreen(0);
  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.Color(127,127,0));
  matrix.print("WiFiOk");
  matrix.show();
  delay(1000);

  //ntpClock.setup(SSID, PASSWORD);
  ntpClock.setup();

  if (!ntpClock.isSetup()) {
    return;
  }

  nowSeconds = ntpClock.getNow();
  if (nowSeconds > 0 ) {
    systemClock.setNow(nowSeconds);
  } else {
    return;
  }
  
  ArduinoOTA.begin();

}

void loop() {

  if (millis() < 300000) {
      ArduinoOTA.handle();
  }

  unsigned int currentMinutes = 0;

  systemClock.loop();
  auto chicagoTz = TimeZone::forZoneInfo(
                     &zonedb::kZoneAmerica_Chicago, &chicagoProcessor);
  acetime_t nowSeconds;
  auto chicagoTime = ZonedDateTime::forEpochSeconds(nowSeconds, chicagoTz);
  nowSeconds = systemClock.getNow();
  chicagoTime = ZonedDateTime::forEpochSeconds(nowSeconds, chicagoTz);
  currentMinutes = chicagoTime.minute();
  if (currentMinutes != prevMinutes) {
    displayTime(chicagoTime.hour(), currentMinutes );
  }

  if (nowSeconds != prevSeconds) {
    cursorOn = not cursorOn;
    prevSeconds = nowSeconds;
  }

  matrix.setCursor(11, 0);
  
  if (cursorOn){
    matrix.setTextColor(colors[2]);
  } else {
    matrix.setTextColor(colors[3]);
  }
  
  matrix.print(":");
  matrix.show();
  prevMinutes = currentMinutes;

}

 

void displayTime(int dispHours, int dispMinutes) {

  matrix.fillScreen(0);
  if (dispHours < 10) {
    matrix.setCursor(6, 0);
  } else {
    matrix.setCursor(0, 0);
  }
  matrix.setTextColor(colors[2]);
  String localMinutes;
  if (dispMinutes < 10) {
    localMinutes = "0" + String(dispMinutes);
  }else {
    localMinutes = String(dispMinutes);
  }
  matrix.print(String(dispHours));
  matrix.setCursor(16, 0);
  matrix.print(localMinutes);
  matrix.show();

}

 
