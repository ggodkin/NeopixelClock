#include "timekeeper.h"

#include <Arduino.h>
#include <time.h>

namespace {

constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
constexpr const char* NTP_SERVER_3 = "time.google.com";

constexpr uint32_t NTP_TIMEOUT_MS = 15000;

} // namespace

bool Timekeeper::begin(const char* timeZone) {

    if (timeZone == nullptr || timeZone[0] == '\0') {
        Serial.println("Timekeeper: invalid time zone");
        return false;
    }

    // The ESP32 C library uses the TZ environment variable for local-time
    // conversion, including daylight-saving rules for supported zone names.
    setenv("TZ", timeZone, 1);
    tzset();

    Serial.print("Time zone: ");
    Serial.println(timeZone);
    Serial.println("Starting native SNTP...");

    configTime(
        0,
        0,
        NTP_SERVER_1,
        NTP_SERVER_2,
        NTP_SERVER_3
    );

    Serial.println("Waiting for NTP time...");

    struct tm timeinfo;
    uint32_t ntpStart = millis();
    bool ntpValid = false;

    while (!ntpValid && millis() - ntpStart < NTP_TIMEOUT_MS) {
        ntpValid = getLocalTime(&timeinfo, 1000);

        if (!ntpValid) {
            Serial.print(".");
        }
    }

    Serial.println();

    if (!ntpValid) {
        Serial.println("NTP synchronization timeout");
        return false;
    }

    char timeBuffer[64];

    strftime(
        timeBuffer,
        sizeof(timeBuffer),
        "%Y-%m-%d %H:%M:%S",
        &timeinfo
    );

    Serial.println("NTP synchronized!");
    Serial.print("Local time: ");
    Serial.println(timeBuffer);

    update();

    return _valid;
}

void Timekeeper::update() {

    time_t now;
    time(&now);

    _unixSeconds = static_cast<int64_t>(now);

    struct tm localTime;

    if (localtime_r(&now, &localTime) == nullptr) {
        _valid = false;
        return;
    }

    _year = localTime.tm_year + 1900;
    _month = localTime.tm_mon + 1;
    _day = localTime.tm_mday;

    _hour = localTime.tm_hour;
    _minute = localTime.tm_min;
    _second = localTime.tm_sec;

    _minuteChanged = (_minute != _previousMinute);
    _secondChanged = (_unixSeconds != _previousSecond);

    _previousMinute = _minute;
    _previousSecond = _unixSeconds;

    _valid = true;
}

bool Timekeeper::isValid() const { return _valid; }
int Timekeeper::year() const { return _year; }
int Timekeeper::month() const { return _month; }
int Timekeeper::day() const { return _day; }
int Timekeeper::hour() const { return _hour; }
int Timekeeper::minute() const { return _minute; }
int Timekeeper::second() const { return _second; }
int64_t Timekeeper::unixSeconds() const { return _unixSeconds; }
bool Timekeeper::minuteChanged() const { return _minuteChanged; }
bool Timekeeper::secondChanged() const { return _secondChanged; }
