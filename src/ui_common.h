#pragma once

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "gb2312_font.h"

inline int textWidth16(const char *p) {
    int w = 0;
    while (*p) {
        if ((*p & 0xE0) == 0xE0) { p += 3; w += 16; }
        else if (*p & 0x80) { p += 2; w += 16; }
        else { p++; w += 8; }
    }
    return w;
}
