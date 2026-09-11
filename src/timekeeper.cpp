#include "timekeeper.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include <esp_attr.h>

#include "config.h"

using namespace ace_time;

namespace {

constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.nist.gov";
constexpr const char* NTP_SERVER_3 = "time.google.com";

constexpr time_t MIN_VALID_UNIX_TIME = 1577836800; // 2020-01-01 UTC

constexpr char NVS_NAMESPACE[] = "timekeeper";
constexpr char NVS_KEY_UNIX[] = "lastUnix";

constexpr uint8_t TIME_ZONE_CACHE_SIZE = 2;

RTC_DATA_ATTR int64_t rtcLastKnownUnixSeconds = 0;

Preferences preferences;
bool preferencesOpen = false;

ExtendedZoneProcessorCache<TIME_ZONE_CACHE_SIZE> zoneProcessorCache;
ExtendedZoneManager zoneManager(
    zonedbx::kZoneAndLinkRegistrySize,
    zonedbx::kZoneAndLinkRegistry,
    zoneProcessorCache);

} // namespace

bool Timekeeper::begin(const char* timeZone) {
    _valid = false;
    _ntpStarted = false;
    _unixSeconds = 0;
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

    update();

    if (!_valid) {
        restoreRtcTime();
        update();
    }

    if (!_valid) {
        restoreNvsTime();
        update();
    }

    if (_valid) {
        Serial.printf(
            "Local time available: %04d-%02d-%02d %02d:%02d:%02d\n",
            _year, _month, _day, _hour, _minute, _second);
    } else {
        Serial.println("No valid local time available yet");
    }

    return true;
}

void Timekeeper::restoreRtcTime() {
    if (rtcLastKnownUnixSeconds < MIN_VALID_UNIX_TIME) {
        return;
    }

    timeval tv{};
    tv.tv_sec = static_cast<time_t>(rtcLastKnownUnixSeconds);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    Serial.print("Restored RTC time: ");
    Serial.println(rtcLastKnownUnixSeconds);
}

void Timekeeper::restoreNvsTime() {
    if (!preferencesOpen) {
        preferencesOpen = preferences.begin(NVS_NAMESPACE, false);
    }

    if (!preferencesOpen) {
        Serial.println("Timekeeper: NVS unavailable; cannot restore saved time");
        return;
    }

    const int64_t saved = preferences.getLong64(NVS_KEY_UNIX, 0);

    if (saved < MIN_VALID_UNIX_TIME) {
        Serial.println("Timekeeper: no valid saved time in NVS");
        return;
    }

    timeval tv{};
    tv.tv_sec = static_cast<time_t>(saved);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    rtcLastKnownUnixSeconds = saved;

    Serial.print("Restored time from NVS: ");
    Serial.println(saved);
}

void Timekeeper::saveRtcTime() {
    if (!_valid) {
        return;
    }

    rtcLastKnownUnixSeconds = _unixSeconds;

    if (!preferencesOpen) {
        preferencesOpen = preferences.begin(NVS_NAMESPACE, false);
    }

    if (preferencesOpen) {
        preferences.putLong64(NVS_KEY_UNIX, _unixSeconds);
    }
}

void Timekeeper::startNtp() {
    if (_ntpStarted) {
        return;
    }

    _ntpStarted = true;

    Serial.println("Starting native SNTP...");

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

    // Save the last-known time once per minute. This provides a fallback for
    // a later boot if RTC-retained memory has been lost.
    if (_unixSeconds / 60 != rtcLastKnownUnixSeconds / 60) {
        saveRtcTime();
    }

    ZonedDateTime localTime = ZonedDateTime::forUnixSeconds64(
        _unixSeconds, _timeZone);

    if (localTime.isError()) {
        _valid = false;
        _minuteChanged = false;
        _secondChanged = false;
        return;
    }

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
