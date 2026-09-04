#include "timekeeper.h"

#include <Arduino.h>
#include <time.h>

#include <AceTime.h>

using namespace ace_time;

namespace {

BasicZoneProcessor denverProcessor;

constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
constexpr const char* NTP_SERVER_3 = "time.google.com";

constexpr uint32_t NTP_TIMEOUT_MS = 15000;

} // namespace

bool Timekeeper::begin() {

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

    while (
        !ntpValid &&
        millis() - ntpStart < NTP_TIMEOUT_MS
    ) {

        ntpValid = getLocalTime(
            &timeinfo,
            1000
        );

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

    Serial.print("System time: ");
    Serial.println(timeBuffer);

    update();

    if (_valid) {
        Serial.print("Denver time: ");

        Serial.print(_year);
        Serial.print("-");
        Serial.print(_month);
        Serial.print("-");
        Serial.print(_day);
        Serial.print(" ");

        Serial.print(_hour);
        Serial.print(":");

        if (_minute < 10) {
            Serial.print("0");
        }

        Serial.print(_minute);
        Serial.print(":");

        if (_second < 10) {
            Serial.print("0");
        }

        Serial.println(_second);
    }

    return _valid;
}

void Timekeeper::update() {

    time_t now;

    time(&now);

    _unixSeconds =
        static_cast<int64_t>(now);

    auto denverTz =
        TimeZone::forZoneInfo(
            &zonedb::kZoneAmerica_Denver,
            &denverProcessor
        );

    auto denverTime =
        ZonedDateTime::forUnixSeconds64(
            _unixSeconds,
            denverTz
        );

    _year = denverTime.year();
    _month = denverTime.month();
    _day = denverTime.day();

    _hour = denverTime.hour();
    _minute = denverTime.minute();
    _second = denverTime.second();

    _minuteChanged =
        (_minute != _previousMinute);

    _secondChanged =
        (_unixSeconds != _previousSecond);

    _previousMinute = _minute;
    _previousSecond = _unixSeconds;

    _valid = true;
}

bool Timekeeper::isValid() const {
    return _valid;
}

int Timekeeper::year() const {
    return _year;
}

int Timekeeper::month() const {
    return _month;
}

int Timekeeper::day() const {
    return _day;
}

int Timekeeper::hour() const {
    return _hour;
}

int Timekeeper::minute() const {
    return _minute;
}

int Timekeeper::second() const {
    return _second;
}

int64_t Timekeeper::unixSeconds() const {
    return _unixSeconds;
}

bool Timekeeper::minuteChanged() const {
    return _minuteChanged;
}

bool Timekeeper::secondChanged() const {
    return _secondChanged;
}