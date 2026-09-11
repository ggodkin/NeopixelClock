#pragma once

#include <stdint.h>

#include <AceTime.h>

class Timekeeper {
public:
    // Configure the timezone and restore/use any locally available time.
    // Does not wait for NTP.
    bool begin(const char* timeZone);

    // Start native SNTP once. Returns immediately.
    void startNtp();

    // Update the displayed time from the ESP32 system clock.
    void update();

    bool isValid() const;
    bool ntpStarted() const;

    int year() const;
    int month() const;
    int day() const;

    int hour() const;
    int minute() const;
    int second() const;

    int64_t unixSeconds() const;

    bool minuteChanged() const;
    bool secondChanged() const;

private:
    void restoreRtcTime();
    void saveRtcTime();

    bool _valid = false;
    bool _ntpStarted = false;

    int64_t _unixSeconds = 0;

    int _year = 0;
    int _month = 0;
    int _day = 0;

    int _hour = 0;
    int _minute = 0;
    int _second = 0;

    int _previousMinute = -1;
    int64_t _previousSecond = -1;

    bool _minuteChanged = false;
    bool _secondChanged = false;

    ace_time::TimeZone _timeZone;
};
