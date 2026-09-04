#pragma once

#include <stdint.h>

class Timekeeper {
public:
    bool begin();
    void update();

    bool isValid() const;

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
    bool _valid = false;

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
};