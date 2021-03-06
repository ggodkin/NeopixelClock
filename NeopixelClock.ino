// Based on AceTime and Adafruit_NeoMatrix libraries and examples.
//
// Change log:
//      Credits to NeoMatrix and AceTime libraries
//      change display only when minute changes
//      add leading 0 to minutes
//      only take positive NTP values
//      Reduce : spacing and blink it
//      OTA
//      Garage status indicator
//      MQTT enable
//      FastLED
//      Updated version of AceTime
// TODO indicate failed NTP and Failed WiFi requests
// TODO Web control? Time Zone, WiFi credentials, Node Red...
// TODO RTC?
// TODO adjust light intecity based on ambient light

#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>

#if !defined(ESP32) && !defined(ESP8266)
  #error This sketch works only for the ESP8266 and ESP32
#endif

#include <AceRoutine.h>
#include <AceTime.h>
#include <AceTimeClock.h>

#define DEBUG 1

#if DEBUG == 1
  #define debug(x) Serial.print(x)
  #define debugln(x) Serial.println(x)
#else
  #define debug(x)
  #define debugln(x)
#endif 

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
volatile boolean garageDoorClosedStatus = false;

unsigned int brightness = 1;

using namespace ace_time;
using namespace ace_time::clock;

acetime_t prevSeconds = 0;

static const char SSID[] = SSID1;
static const char PASSWORD[] = PWD1;

//const char* mqtt_server = mqttServer1;
const int mqttPort = mqttPort1;
const char* mqttUser = mqttUser1;
const char* mqttPassword = mqttPassword1;

static BasicZoneProcessor chicagoProcessor;
static NtpClock ntpClock;
static SystemClockLoop systemClock(&ntpClock, nullptr /*backup*/);

#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
char msgOut[MSG_BUFFER_SIZE];
int value = 0;
String msgStr;

#define PIN D3
#define mw 32
#define mh 8
#define NUMMATRIX (mw*mh)

CRGB matrixleds[NUMMATRIX];

FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(matrixleds, mw, mh, 
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
    NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
 
const uint16_t colors[] = {
  matrix->Color(255, 0, 0), matrix->Color(0, 255, 0), matrix->Color(0, 0, 255)
};

IPAddress server(mqttServer1);

WiFiClient espClient;
PubSubClient client(server, mqttPort1, espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  debug("Message arrived [");
  debug(topic);
  debug("] ");
  for (int i = 0; i < length; i++) {
    debug((char)payload[i]);
  }
  debugln();

  if ((char)payload[0] == '1') {
    garageDoorClosedStatus = true;
  } else {
    garageDoorClosedStatus = false;
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    debug("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUser1, mqttPassword1)) {
      debugln("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("cmnd/NeopixelClock/GarageDoorClosed");
    } else {
      debug("failed, rc=");
      debug(client.state());
      debugln(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  Serial.begin(115200);
  
  FastLED.addLeds<NEOPIXEL,PIN>(matrixleds, NUMMATRIX); 
  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(brightness);
  matrix->setTextColor(colors[1]);
  matrix->print("Setup");
  debug("Setup");
  matrix->show();
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    matrix->fillScreen(0);
    matrix->setCursor(0, 0);
    matrix->setTextColor(colors[2]);
    matrix->print("noWIFI");
    debug("No WiFi");
    matrix->show();
    delay(1000);
    ESP.restart();
  }

  auto chicagoTz = TimeZone::forZoneInfo(
                     &zonedb::kZoneAmerica_Chicago, &chicagoProcessor);

  acetime_t nowSeconds;

  matrix->fillScreen(0);
  matrix->setCursor(0, 0);
  matrix->setTextColor(colors[1]);
  matrix->print("WiFiOk");
  debug("WiFi ok");
  matrix->show();
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
  
//  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

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
    msgStr = (String(chicagoTime.hour()) + " : " + String(chicagoTime.minute()));
    //Serial.println(msgStr);
    msgStr.toCharArray(msgOut,MSG_BUFFER_SIZE);
    snprintf (msg, MSG_BUFFER_SIZE, msgOut, value);
    client.publish("WatchBroom/Time", msg);
    debugln(msg);
  }

  if (nowSeconds != prevSeconds) {
    cursorOn = not cursorOn;
    prevSeconds = nowSeconds;

    brightness = analogRead(A0)/5;
    if (brightness == 0) {
      brightness = 1;
    }
    matrix->setBrightness(brightness);
    msgStr = (String(brightness));
    msgStr.toCharArray(msgOut,MSG_BUFFER_SIZE);
    snprintf (msg, MSG_BUFFER_SIZE, msgOut, value);
    client.publish("WatchBroom/Brightness", msg);
    matrix->setCursor(11, 0);
    
    if (cursorOn){
      matrix->setTextColor(colors[2]);
    } else {
      matrix->setTextColor(colors[3]);
    }
  
    matrix->print(":");
    matrix->show();
    displayGarageClosed(garageDoorClosedStatus);
   }
 
   prevMinutes = currentMinutes;

}

 

void displayTime(int dispHours, int dispMinutes) {

  matrix->fillScreen(0);
  if (dispHours < 10) {
    matrix->setCursor(6, 0);
  } else {
    matrix->setCursor(0, 0);
  }
  matrix->setTextColor(colors[2]);
  String localMinutes;
  if (dispMinutes < 10) {
    localMinutes = "0" + String(dispMinutes);
  }else {
    localMinutes = String(dispMinutes);
  }
  matrix->print(String(dispHours));
  matrix->setCursor(16, 0);
  matrix->print(localMinutes);
  matrix->show();
  displayGarageClosed(garageDoorClosedStatus);
}

void displayGarageClosed(boolean closedInd) {
  int bmx = 28;
  int bmy = 0;
  int dimx = 3;
  int dimy = 3;
  if (closedInd) {
    matrix->fillRect(bmx, bmy, dimx, dimy, colors[1]);
   } else {
    matrix->drawRect(bmx, bmy, dimx, dimy, colors[0]);
    matrix->fillRect(bmx+1, bmy+1, 1, 1, 0);
    matrix->fillRect(bmx+1, bmy+2, 1, 1, 0);
  }
  matrix->show();
}

 
