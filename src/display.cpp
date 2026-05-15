#include "display.h"
#include "config.h"
#include <SPI.h>
#include <WiFi.h>

SPIClass *vspi = nullptr;
Adafruit_ST7789 *tft = nullptr;

void initDisplay() {
  vspi = new SPIClass(VSPI);
  vspi->begin(TFT_SCK, -1, TFT_MOSI, -1);
  tft = new Adafruit_ST7789(vspi, TFT_CS, TFT_DC, TFT_RST);
  tft->init(SCREEN_W, SCREEN_H);
  tft->setRotation(1);
  tft->fillScreen(COLOR_BG);
  tft->setTextWrap(false);
}

void drawSectionLine(int y) {
  tft->drawFastHLine(PAD_LEFT, y, CONTENT_W, COLOR_LINE);
}

void drawLabel(int x, int y, const char *text) {
  tft->setCursor(x, y);
  tft->setTextColor(COLOR_LABEL);
  tft->setTextSize(1);
  tft->print(text);
}

void drawValue(int x, int y, const char *text) {
  tft->setCursor(x, y);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setTextSize(1);
  tft->print(text);
}

void fillArea(int x, int y, int w, int h, uint16_t color) {
  tft->fillRect(x, y, w, h, color);
}

void drawWiFiBars(int x, int y, bool connected) {
  if (connected) {
    int rssi = WiFi.RSSI();
    int bars = rssi > -50 ? 4 : rssi > -65 ? 3 : rssi > -80 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
      int bx = x + i * 5;
      int bh = 3 + i * 3;
      tft->fillRect(bx, y + 10 - bh, 3, bh, i < bars ? COLOR_GREEN : COLOR_LINE);
    }
  } else {
    tft->setTextSize(1);
    tft->setCursor(x, y + 2);
    tft->setTextColor(COLOR_ACCENT);
    tft->print("X");
  }
}
