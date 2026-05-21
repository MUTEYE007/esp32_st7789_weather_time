#pragma once

#include <Adafruit_ST7789.h>

extern Adafruit_ST7789 *tft;

void initDisplay();
void drawSectionLine(int y);
void drawLabel(int x, int y, const char *text);
void fillArea(int x, int y, int w, int h, uint16_t color);
void drawWiFiBars(int x, int y, bool connected);
void drawProvisioningScreen(const char *apName, const char *apIP);
void updateProvisioningFrame(int frame);
void drawLongPressRing(int cx, int cy, float progress);
void animateWipe();
