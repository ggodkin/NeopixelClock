#include "timekeeper.h"

#include <Arduino.h>
#include <time.h>

#include "config.h"

using namespace ace_time;

namespace {

constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
constexpr const char* NTP_SERVER_3 = "time.google.com";

// Treat obviously uninitialized Unix time as invalid. The ESP32 system clock
// itself remains the source of time between NTP synchronizations.
constexpr time_t MIN_VALID_UNIX_TIME = 1577836800; // 2020-01-01 00:00:00 UTC

constexpr uint8_t TIME_ZONE_CACHE_SIZE = 2;

ExtendedZoneProcessorCache<TIME_ZONE_CACHE_SIZE> zoneProcessorCache;
ExtendedZoneManager zoneManager(
    zonedbx::kZoneAndLinkRegistrySize,
    zonedbx::kZoneAndLinkRegistry,
    zoneProcessorCache);

} // namespace

bool Timekeeper::begin(const char* timeZone) {
    _valid = false;
    _ntpStarted = false;
    _previousMinute = -1;
    _previousSecond = -1;
    _minuteChanged = false;
    _secondChanged = false;

    if (timeZone == nullptr || timeZone[0] == '\0') {
        Serial.println("Timekeeper: invalid time zone");
        return false;
    }

    Serial.print("Timekeeper: requested timezone = ");
    Serial.println(timeZone);

    _timeZone = zoneManager.createForZoneName(timeZone);

    if (_timeZone.isError()) {
        Serial.print("Timekeeper: invalid time zone '");
        Serial.print(timeZone);
        Serial.println("', falling back to compiled-in default");

        _timeZone = zoneManager.createForZoneName(TIME_ZONE);

        if (_timeZone.isError()) {
            Serial.println("Timekeeper: compiled-in time zone is invalid");
            return false;
        }
    }

    Serial.print("Timekeeper: resolved timezone = ");
    _timeZone.printTo(Serial);
    Serial.println();

    // Use an already-running ESP32 system clock immediately if it contains a
    // plausible time. NTP is started separately and never blocks the clock.
    update();

    if (_valid) {
        Serial.printf(
            "Existing system time: %04d-%02d-%02d %02d:%02d:%02d\n",
            _year,
            _month,
            _day,
            _hour,
            _minute,
            _second);
    } else {
        Serial.println("Existing system time is not valid yet");
    }

    return true;
}

void Timekeeper::startNtp() {
    if (_ntpStarted) {
        return;
    }

    _ntpStarted = true;

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
}

void Timekeeper::update() {
    if (_timeZone.isError()) {
        _valid = false;
        return;
    }

    time_t now;
    time(&now);

    if (now < MIN_VALID_UNIX_TIME) {
        _valid = false;
        _minuteChanged = false;
        _secondChanged = false;
        return;
    }

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
        _minuteChanged = false;
        _secondChanged = false;
    }
}

bool Timekeeper::isValid() const { return _valid; }
bool Timekeeper::ntpStarted() const { return _ntpStarted; }
int Timekeeper::year() const { return _year; }
int Timekeeper::month() const { return _month; }
int Timekeeper::day() const { return _day; }
int Timekeeper::hour() const { return _hour; }
int Timekeeper::minute() const { return _minute; }
int Timekeeper::second() const { return _second; }
int64_t Timekeeper::unixSeconds() const { return _unixSeconds; }
bool Timekeeper::minuteChanged() const { return _minuteChanged; }
bool Timekeeper::secondChanged() const { return _secondChanged; }
