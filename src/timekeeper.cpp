#include "timekeeper.h"

#include <Arduino.h>
#include <time.h>

#include "config.h"

using namespace ace_time;

namespace {

constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
constexpr const char* NTP_SERVER_3 = "time.google.com";

constexpr uint32_t NTP_TIMEOUT_MS = 15000;

// The configuration portal allows the user to select any IANA timezone name,
// so use AceTime's complete extended registry rather than compiling in a
// single timezone.
constexpr uint8_t TIME_ZONE_CACHE_SIZE = 2;

ExtendedZoneProcessorCache<TIME_ZONE_CACHE_SIZE> zoneProcessorCache;
ExtendedZoneManager zoneManager(
    zonedbx::kZoneAndLinkRegistrySize,
    zonedbx::kZoneAndLinkRegistry,
    zoneProcessorCache);

} // namespace

bool Timekeeper::begin(const char* timeZone) {

    if (timeZone == nullptr || timeZone[0] == '\0') {
        Serial.println("Timekeeper: invalid time zone");
        return false;
    }

    _timeZone = zoneManager.createForZoneName(timeZone);

    if (_timeZone.isError()) {
        Serial.print("Timekeeper: invalid time zone '"
                     );
        Serial.print(timeZone);
        Serial.println("', falling back to compiled-in default");

        _timeZone = zoneManager.createForZoneName(TIME_ZONE);

        if (_timeZone.isError()) {
            Serial.println("Timekeeper: compiled-in time zone is invalid");
            return false;
        }
    }

    Serial.print("Time zone: ");
    Serial.println(_timeZone.getName());
    Serial.println("Starting native SNTP...");

    // SNTP supplies UTC epoch seconds. AceTime performs the local-time
    // conversion below, so no ESP32 libc TZ configuration is required.
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

    Serial.println("NTP synchronized!");

    update();

    if (_valid) {
        Serial.print("Local time: ");
        Serial.printf(
            "%04d-%02d-%02d %02d:%02d:%02d\n",
            _year,
            _month,
            _day,
            _hour,
            _minute,
            _second);
    }

    return _valid;
}

void Timekeeper::update() {

    time_t now;
    time(&now);

    _unixSeconds = static_cast<int64_t>(now);

    // Convert the UTC epoch supplied by SNTP using the configured AceTime
    // timezone. This applies the correct DST rules for the selected IANA zone.
    ZonedDateTime localTime = ZonedDateTime::forUnixSeconds64(
        _unixSeconds,
        _timeZone);

    if (!localTime.isError()) {
        _year = localTime.year();
        _month = localTime.month();
        _day = localTime.day();

        _hour = localTime.hour();
        _minute = localTime.minute();
        _second = localTime.second();

        _minuteChanged = (_minute != _previousMinute);
        _secondChanged = (_unixSeconds != _previousSecond);

        _previousMinute = _minute;
        _previousSecond = _unixSeconds;

        _valid = true;
    } else {
        _valid = false;
    }
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
