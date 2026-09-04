#pragma once

#include <stdint.h>

class Display {
public:
    void begin();

    void showTime(int hours, int minutes);
    void updateColon(bool on);
    void showGarageClosed(bool closed);

    void clear();

private:
    void show();

    bool _cursorOn = true;
};