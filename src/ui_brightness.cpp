#include "ui.h"
#include "ui_common.h"

namespace brightness_page {

static const int BR_BAR_X = 20;
static const int BR_BAR_Y = 100;
static const int BR_BAR_W = 200;
static const int BR_BAR_H = 20;

void drawBrightnessPage() {
    tft->fillScreen(COLOR_BG);

    fillArea(2, 2, SCREEN_W - 4, 16, COLOR_CLOCK);
    tft->setTextColor(COLOR_BG);
    tft->setTextSize(1);
    tft->setCursor(8, 5);
    tft->print("BRIGHTNESS");

    int cx = SCREEN_W / 2;
    tft->setTextColor(COLOR_LABEL);
    tft->setCursor(cx - 42, 40);
    tft->print("Hold to adjust");

    tft->drawRect(BR_BAR_X, BR_BAR_Y, BR_BAR_W, BR_BAR_H, COLOR_PRIMARY);

    tft->setTextSize(1);
    tft->setTextColor(COLOR_LABEL);
    tft->setCursor(cx - 60, BR_BAR_Y + 50);
    tft->print("Short press to exit");

    updateBrightnessBar(g_brightness, 0);
}

void updateBrightnessBar(uint8_t brightness, int8_t dir) {
    int fillW = (brightness * BR_BAR_W) / 255;
    fillArea(BR_BAR_X + 1, BR_BAR_Y + 1, BR_BAR_W - 2, BR_BAR_H - 2, COLOR_BG);
    if (fillW > 0) {
        fillArea(BR_BAR_X + 1, BR_BAR_Y + 1, fillW - 1, BR_BAR_H - 2, COLOR_CLOCK);
    }

    int cx = SCREEN_W / 2;
    fillArea(cx - 30, BR_BAR_Y + 26, 60, 20, COLOR_BG);
    int pct = (brightness * 100 + 127) / 255;
    tft->setTextColor(COLOR_PRIMARY);
    tft->setTextSize(2);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    tft->setCursor(cx - 18, BR_BAR_Y + 26);
    tft->print(buf);
}

} // namespace brightness_page
