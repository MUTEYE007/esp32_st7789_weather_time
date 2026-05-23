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

void fillArea(int x, int y, int w, int h, uint16_t color) {
  tft->fillRect(x, y, w, h, color);
}

static const int RSSI_EXCELLENT = -50;
static const int RSSI_GOOD      = -65;
static const int RSSI_FAIR      = -80;

void drawWiFiBars(int x, int y, bool connected) {
  if (connected) {
    int rssi = WiFi.RSSI();
    int bars = rssi > RSSI_EXCELLENT ? 4 : rssi > RSSI_GOOD ? 3 : rssi > RSSI_FAIR ? 2 : 1;
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

void drawProvisioningScreen(const char *apName, const char *apIP) {
  tft->fillScreen(COLOR_BG);

  fillArea(0, 0, SCREEN_W, STATUS_H + 4, COLOR_ACCENT);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setTextSize(1);
  tft->setCursor((SCREEN_W - 11 * 6) / 2, 5);
  tft->print("WiFi Config");

  int cx = SCREEN_W / 2;

  tft->fillCircle(cx, 50, 3, COLOR_LABEL);
  tft->drawLine(cx - 4, 42, cx + 4, 42, COLOR_LABEL);
  tft->drawLine(cx - 8, 34, cx + 8, 34, COLOR_LABEL);
  tft->drawLine(cx - 12, 26, cx + 12, 26, COLOR_LABEL);

  tft->setTextSize(1);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(PAD_LEFT, 72);
  tft->print("AP:");
  tft->setTextColor(COLOR_CLOCK);
  tft->setTextSize(1);
  tft->setCursor(PAD_LEFT + 18, 72);
  tft->print(apName);

  tft->setTextSize(1);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(PAD_LEFT, 90);
  tft->print("IP:");
  tft->setTextColor(COLOR_PRIMARY);
  tft->setTextSize(1);
  tft->setCursor(PAD_LEFT + 18, 90);
  tft->print(apIP);

  drawSectionLine(116);

  tft->setTextSize(1);
  tft->setTextColor(COLOR_MUTED);
  tft->setCursor(cx - 42, 122);
  tft->print("== Operation ==");

  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(PAD_LEFT, 140);
  tft->print("1. Connect phone to this WiFi");
  tft->setCursor(PAD_LEFT, 154);
  tft->print("2. Open phone browser");
  tft->setCursor(PAD_LEFT, 168);
  tft->print("3. Config page opens auto");

  fillArea(PAD_LEFT, 184, CONTENT_W, 1, COLOR_LINE);

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(cx - 66, 196);
  tft->print("Waiting for connection...");

  tft->setTextColor(COLOR_ACCENT);
  tft->setTextSize(1);
  tft->setCursor(cx - 68, 228);
  tft->print("Hold BOOT 3s to reset WiFi");
}

void updateProvisioningFrame(int frame) {
  int cx = SCREEN_W / 2;
  int cy = 212;
  int spacing = 10;
  int dotR = 3;
  for (int i = 0; i < 4; i++) {
    int phase = (frame + i) % 4;
    uint16_t c;
    if (phase == 0) c = COLOR_ACCENT;
    else if (phase == 1) c = COLOR_MUTED;
    else c = COLOR_LINE;
    tft->fillCircle(cx - 3 * spacing / 2 + i * spacing, cy, dotR, c);
  }
}

static const uint16_t GLOW_EDGE  = 0xFFE0;
static const uint16_t GLOW_BAR   = 0xFB00;
static const uint16_t GLOW_BORDER = 0xE526;

void drawEdgeGlow(bool show) {
  uint16_t c = show ? GLOW_EDGE : COLOR_BG;
  fillArea(0, 0, SCREEN_W, 2, c);
  fillArea(0, SCREEN_H - 2, SCREEN_W, 2, c);
  fillArea(0, 2, 2, SCREEN_H - 4, c);
  fillArea(SCREEN_W - 2, 2, 2, SCREEN_H - 4, c);
}

void drawLongPressBar(float progress) {
  const int barW = CONTENT_W;
  const int barH = 8;
  const int barY = SCREEN_H - barH - 6;
  const int barX = PAD_LEFT;
  fillArea(barX, barY - 12, barW, barH + 16, COLOR_BG);
  tft->setTextSize(1);
  tft->setTextColor(GLOW_EDGE, COLOR_BG);
  tft->setCursor(barX, barY - 10);
  tft->print("Hold to reset WiFi...");
  tft->drawRect(barX, barY, barW, barH, GLOW_BORDER);
  int activeW = (int)(barW * progress);
  if (activeW > 0) {
    fillArea(barX, barY, activeW, barH, GLOW_BAR);
  }
}

void animateWipe() {
  const int BLOCKS = 6;
  int blockH = (SCREEN_H + BLOCKS - 1) / BLOCKS;
  for (int i = BLOCKS - 1; i >= 0; i--) {
    int y = i * blockH;
    int h = (y + blockH <= SCREEN_H) ? blockH : (SCREEN_H - y);
    fillArea(0, y, SCREEN_W, h, COLOR_BG);
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}

void drawWarnProgressBar(float progress) {
  const int barW = CONTENT_W;
  const int barH = 5;
  const int barY = SCREEN_H - barH - 4;
  const int barX = PAD_LEFT;
  fillArea(barX, barY, barW, barH, COLOR_BG);
  int activeW = (int)(barW * progress);
  if (activeW > 0) {
    fillArea(barX, barY, activeW, barH, COLOR_ACCENT);
  }
}
