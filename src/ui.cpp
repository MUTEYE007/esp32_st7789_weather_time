#include "ui.h"
#include "config.h"
#include "display.h"
#include "weather.h"
#include "gb2312_font.h"
#include <WiFi.h>
#include <time.h>

static int textWidth16(const char *p);

void drawStatusHeader() {
  fillArea(0, STATUS_Y, SCREEN_W, STATUS_H, COLOR_BG);

  int nameEndX = PAD_LEFT;
  drawGB16(PAD_LEFT, STATUS_Y, weatherName.c_str(), COLOR_GOLD, COLOR_BG);
  nameEndX += textWidth16(weatherName.c_str());

  drawWiFiBars(SCREEN_W - PAD_RIGHT - 20, STATUS_Y + 2, state.wifiConnected);

  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_MUTED);
    tft->setCursor(nameEndX, STATUS_Y + 3);
    tft->printf(" %02d:%02d", ti->tm_hour, ti->tm_min);
  }
}

void updateStatusTime() {
  if (!state.timeSynced) return;
  time_t t = timeClient->getEpochTime();
  struct tm *ti = localtime(&t);
  int nameEndX = PAD_LEFT + textWidth16(weatherName.c_str());
  tft->setTextSize(1);
  tft->setTextColor(COLOR_MUTED, COLOR_BG);
  tft->setCursor(nameEndX, STATUS_Y + 3);
  tft->printf(" %02d:%02d", ti->tm_hour, ti->tm_min);
}

static int textWidth16(const char *p) {
  int w = 0;
  while (*p) {
    if ((*p & 0xE0) == 0xE0) { p += 3; w += 16; }
    else if (*p & 0x80) { p += 2; w += 16; }
    else { p++; w += 8; }
  }
  return w;
}

void drawWeatherIcon(int cx, int cy, int code) {
  if (code == 100 || code == 150) {
    tft->fillCircle(cx, cy, 10, COLOR_YELLOW);
    tft->drawLine(cx, cy - 14, cx, cy - 18, COLOR_YELLOW);
    tft->drawLine(cx, cy + 14, cx, cy + 18, COLOR_YELLOW);
    tft->drawLine(cx - 14, cy, cx - 18, cy, COLOR_YELLOW);
    tft->drawLine(cx + 14, cy, cx + 18, cy, COLOR_YELLOW);
    tft->drawLine(cx - 10, cy - 10, cx - 13, cy - 13, COLOR_YELLOW);
    tft->drawLine(cx + 10, cy + 10, cx + 13, cy + 13, COLOR_YELLOW);
    tft->drawLine(cx + 10, cy - 10, cx + 13, cy - 13, COLOR_YELLOW);
    tft->drawLine(cx - 10, cy + 10, cx - 13, cy + 13, COLOR_YELLOW);
  } else if (code >= 101 && code <= 104) {
    tft->fillCircle(cx - 10, cy + 2, 8, COLOR_CLOUD);
    tft->fillCircle(cx, cy - 2, 10, COLOR_CLOUD);
    tft->fillCircle(cx + 10, cy + 2, 8, COLOR_CLOUD);
    tft->fillRect(cx - 10, cy - 2, 20, 14, COLOR_CLOUD);
  } else if ((code >= 300 && code <= 399) || (code >= 400 && code <= 499)) {
    tft->fillCircle(cx - 8, cy - 2, 7, COLOR_CLOUD);
    tft->fillCircle(cx, cy - 5, 9, COLOR_CLOUD);
    tft->fillCircle(cx + 8, cy - 2, 7, COLOR_CLOUD);
    tft->fillRect(cx - 8, cy - 5, 16, 10, COLOR_CLOUD);
    tft->drawLine(cx - 8, cy + 8, cx - 10, cy + 14, COLOR_RAIN);
    tft->drawLine(cx, cy + 8, cx - 2, cy + 14, COLOR_RAIN);
    tft->drawLine(cx + 8, cy + 8, cx + 6, cy + 14, COLOR_RAIN);
  } else if (code >= 500 && code <= 599) {
    tft->fillCircle(cx - 8, cy - 2, 7, COLOR_CLOUD);
    tft->fillCircle(cx, cy - 5, 9, COLOR_CLOUD);
    tft->fillCircle(cx + 8, cy - 2, 7, COLOR_CLOUD);
    tft->fillRect(cx - 8, cy - 5, 16, 10, COLOR_CLOUD);
    tft->drawLine(cx - 8, cy + 8, cx - 8, cy + 12, COLOR_PRIMARY);
    tft->drawLine(cx, cy + 8, cx, cy + 12, COLOR_PRIMARY);
    tft->drawLine(cx + 8, cy + 8, cx + 8, cy + 12, COLOR_PRIMARY);
  } else {
    tft->fillCircle(cx - 8, cy, 8, COLOR_CLOUD);
    tft->fillCircle(cx, cy - 3, 10, COLOR_CLOUD);
    tft->fillCircle(cx + 8, cy, 8, COLOR_CLOUD);
    tft->fillRect(cx - 8, cy - 3, 16, 11, COLOR_CLOUD);
  }
}

void drawWeatherSection() {
  fillArea(PAD_LEFT, WEATHER_Y, CONTENT_W, WEATHER_H, COLOR_BG);

  if (!weather.valid) {
    drawGB16(50, WEATHER_Y + 8, "不可用", COLOR_LABEL, COLOR_BG);
    return;
  }

  int iconCode = weather.weatherIcon.toInt();
  drawWeatherIcon(ICON_CX, ICON_CY, iconCode);

  tft->setTextSize(3);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(TEMP_X, TEMP_Y);
  tft->print(weather.temp);
  tft->setTextSize(1);
  tft->setCursor(TEMP_X + weather.temp.length() * 18, TEMP_Y + 6);
  tft->print("C");

  int cityEndX = CITY_X;
  drawGB16(CITY_X, CITY_Y, weatherName.c_str(), COLOR_GOLD, COLOR_BG);
  cityEndX += textWidth16(weatherName.c_str());
  drawGB16(cityEndX + 4, CITY_Y, weather.weatherText.c_str(), COLOR_PRIMARY, COLOR_BG);

  if (weather.tempMax.length() > 0) {
    tft->setCursor(HILO_X, HILO_Y1);
    tft->setTextSize(2);
    tft->setTextColor(COLOR_PRIMARY);
    tft->print("H:");
    tft->setTextColor(COLOR_AMBER);
    tft->print(weather.tempMax);

    tft->setCursor(HILO_X, HILO_Y2);
    tft->setTextColor(COLOR_PRIMARY);
    tft->print("L:");
    tft->setTextColor(COLOR_CLOCK);
    tft->print(weather.tempMin);
  }
}

void drawClockSection() {
  fillArea(PAD_LEFT, CLOCK_Y, CONTENT_W, CLOCK_H, COLOR_BG);

  int h, m, s;
  char dateBuf[24] = "";

  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    h = ti->tm_hour;
    m = ti->tm_min;
    s = ti->tm_sec;
    static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    sprintf(dateBuf, "%04d-%02d-%02d %s",
      ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
      days[ti->tm_wday]);
  } else if (state.ntpTried) {
    unsigned long ms = millis() - state.bootTime;
    s = (ms / 1000) % 60;
    m = (ms / 60000) % 60;
    h = (ms / 3600000) % 24;
    strcpy(dateBuf, state.ntpFailReason);
  } else {
    tft->setTextColor(COLOR_ACCENT);
    tft->setTextSize(2);
    tft->setCursor(50, CLOCK_Y + 20);
    tft->print("Syncing...");
    return;
  }

  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);

  int strW = strlen(buf) * 30;
  int cx = (SCREEN_W - strW) / 2;

  tft->setTextSize(5);
  tft->setTextColor(COLOR_CLOCK);
  tft->setCursor(cx, CLOCK_TEXT_Y);
  tft->print(buf);

  char secBuf[3];
  sprintf(secBuf, "%02d", s);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(cx + strW + SEC_X_OFFSET, SEC_Y);
  tft->print(secBuf);

  int dateW = strlen(dateBuf) * 12;
  int dcx = (SCREEN_W - dateW) / 2;
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(dcx, DATE_Y);
  tft->print(dateBuf);
}

void updateClockTime(int h, int m, int s) {
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  int strW = strlen(buf) * 30;
  int cx = (SCREEN_W - strW) / 2;

  tft->setTextSize(5);
  tft->setTextColor(COLOR_CLOCK, COLOR_BG);
  tft->setCursor(cx, CLOCK_TEXT_Y);
  tft->print(buf);

  char secBuf[3];
  sprintf(secBuf, "%02d", s);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL, COLOR_BG);
  tft->setCursor(cx + strW + SEC_X_OFFSET, SEC_Y);
  tft->print(secBuf);
}

void drawDetailSection() {
  fillArea(PAD_LEFT, DETAIL_Y, CONTENT_W, DETAIL_H, COLOR_BG);

  if (!weather.valid) return;

  drawGB16(PAD_LEFT, DETAIL_ROW1_Y, "体感", COLOR_LABEL, COLOR_BG);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(PAD_LEFT + 33, DETAIL_ROW1_Y + 4);
  tft->print(":");
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(PAD_LEFT + 44, DETAIL_ROW1_Y);
  tft->print(weather.feelsLike);
  tft->print("C");

  drawGB16(120, DETAIL_ROW1_Y, "湿度", COLOR_LABEL, COLOR_BG);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(153, DETAIL_ROW1_Y + 4);
  tft->print(":");
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(164, DETAIL_ROW1_Y);
  tft->print(weather.humidity);
  tft->print("%");

  int l3 = PAD_LEFT + 32;
  drawGB16(PAD_LEFT, DETAIL_ROW2_Y, "更新", COLOR_LABEL, COLOR_BG);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(l3 + 1, DETAIL_ROW2_Y + 4);
  tft->print(":");
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(l3 + 12, DETAIL_ROW2_Y);
  if (weather.updateTime.length() >= 16) {
    tft->print(weather.updateTime.substring(11, 16));
  }

  int l4 = 136;
  drawGB16(120, DETAIL_ROW2_Y, "风", COLOR_LABEL, COLOR_BG);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(l4 + 1, DETAIL_ROW2_Y + 4);
  tft->print(":");
  int wdX = l4 + 12;
  drawGB16(wdX, DETAIL_ROW2_Y, weather.windDir.c_str(), COLOR_PRIMARY, COLOR_BG);
  int wdEnd = wdX + textWidth16(weather.windDir.c_str());
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(wdEnd + 2, DETAIL_ROW2_Y);
  tft->print(weather.windScale);
  drawGB16(wdEnd + 2 + weather.windScale.length() * 12, DETAIL_ROW2_Y, "级", COLOR_PRIMARY, COLOR_BG);
}

void drawHourlyChart() {
  fillArea(PAD_LEFT, CHART_Y, CONTENT_W, CHART_H, COLOR_BG);

  if (!hourly.valid) return;

  int minT = hourly.tempInt[0], maxT = hourly.tempInt[0];
  for (int i = 1; i < HOUR_COUNT; i++) {
    if (hourly.tempInt[i] < minT) minT = hourly.tempInt[i];
    if (hourly.tempInt[i] > maxT) maxT = hourly.tempInt[i];
  }
  if (maxT == minT) { maxT = minT + 1; minT = minT - 1; }

  for (int gy = CHART_LINE1_Y; gy <= CHART_LINE3_Y; gy += (CHART_LINE3_Y - CHART_LINE1_Y) / 2) {
    for (int gx = CHART_START_X - 4; gx < CHART_START_X + (CHART_POINTS - 1) * CHART_COL_W + 4; gx += 4) {
      tft->drawPixel(gx, gy, COLOR_GRID);
    }
  }

  int ptsX[HOUR_COUNT], ptsY[HOUR_COUNT];
  for (int i = 0; i < HOUR_COUNT; i++) {
    ptsX[i] = CHART_START_X + i * CHART_COL_W;
    ptsY[i] = CHART_DATA_BOT - ((hourly.tempInt[i] - minT) * (CHART_DATA_BOT - CHART_DATA_TOP) / (maxT - minT));
  }

  for (int i = 1; i < HOUR_COUNT; i++) {
    int x0 = ptsX[i - 1], y0 = ptsY[i - 1];
    int x1 = ptsX[i], y1 = ptsY[i];
    for (int px = x0; px <= x1; px++) {
      int py = y0 + (y1 - y0) * (px - x0) / (x1 - x0);
      for (int fy = py; fy <= CHART_DATA_BOT; fy++) {
        tft->drawPixel(px, fy, COLOR_GRID);
      }
    }
  }

  for (int i = 1; i < HOUR_COUNT; i++) {
    tft->drawLine(ptsX[i - 1], ptsY[i - 1], ptsX[i], ptsY[i], COLOR_ACCENT);
  }

  for (int i = 0; i < HOUR_COUNT; i++) {
    tft->fillCircle(ptsX[i], ptsY[i], CHART_PT_R, COLOR_ACCENT);

    tft->setTextSize(1);
    tft->setTextColor(COLOR_LABEL);
    int labelW = hourly.hourLabel[i].length() * 6;
    tft->setCursor(ptsX[i] - labelW / 2, CHART_LABEL_Y);
    tft->print(hourly.hourLabel[i]);

    char tempBuf[6];
    sprintf(tempBuf, "%dC", hourly.tempInt[i]);
    int tempW = strlen(tempBuf) * 6;
    tft->setTextColor(COLOR_PRIMARY);
    tft->setCursor(ptsX[i] - tempW / 2, CHART_TEMP_Y);
    tft->print(tempBuf);
  }
}

void drawSystemInfo() {
  if (state.systemInfoDirty) {
    state.systemInfoDirty = false;
    animateWipe();

    fillArea(2, 2, SCREEN_W - 4, 16, COLOR_CLOCK);
    tft->setTextColor(COLOR_BG);
    tft->setTextSize(1);
    tft->setCursor(8, 5);
    tft->print("SYSTEM STATUS");

    int y = 24;
    int lh = 12;

    tft->setTextSize(1);
    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print("[ ESP32 ]");
    y += lh;

    drawLabel(8, y, "Chip: ");
    tft->setTextColor(COLOR_PRIMARY);
    tft->print(ESP.getChipModel());
    tft->print(" rev");
    tft->print(ESP.getChipRevision());
    y += lh;

    drawLabel(8, y, "Flash: ");
    tft->setTextColor(COLOR_PRIMARY);
    tft->print(ESP.getFlashChipSize() / (1024 * 1024));
    tft->print("MB  Free: ");
    tft->print(ESP.getFreeHeap() / 1024);
    tft->print("KB");
    y += lh;

    drawLabel(8, y, "Uptime: ");
    tft->setTextColor(COLOR_PRIMARY);
    unsigned long up = millis() / 1000;
    tft->print(up / 3600);
    tft->print("h ");
    tft->print((up % 3600) / 60);
    tft->print("m ");
    tft->print(up % 60);
    tft->print("s");
    y += lh + 4;

    drawSectionLine(y);
    y += 4;

    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print("[ WiFi ]");
    y += lh;

    drawLabel(8, y, "SSID: ");
    tft->setTextColor(COLOR_PRIMARY);
    tft->print(WiFi.SSID());
    y += lh;

    drawLabel(8, y, "IP:   ");
    tft->setTextColor(state.wifiConnected ? COLOR_GREEN : COLOR_ACCENT);
    if (state.wifiConnected) tft->print(WiFi.localIP().toString());
    else tft->print("--.--.--.--");
    y += lh;

    drawLabel(8, y, "GW:   ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.wifiConnected) tft->print(WiFi.gatewayIP().toString());
    else tft->print("--.--.--.--");
    y += lh;

    drawLabel(8, y, "RSSI: ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.wifiConnected) {
      tft->print(WiFi.RSSI());
      tft->print("dBm");
    } else {
      tft->print("N/A");
    }
    y += lh + 4;

    drawSectionLine(y);
    y += 4;

    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print("[ NTP ]");
    y += lh;

    drawLabel(8, y, "Status: ");
    tft->setTextColor(state.timeSynced ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(state.timeSynced ? "Synced" : "Failed");
    y += lh;

    drawLabel(8, y, "Server: ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.timeSynced) tft->print(state.ntpServer);
    else tft->print("--");
    y += lh;

    drawLabel(8, y, "Time:  ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.timeSynced) {
      time_t t = timeClient->getEpochTime();
      struct tm *ti = localtime(&t);
      char buf[20];
      sprintf(buf, "%04d-%02d-%02d %02d:%02d",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour, ti->tm_min);
      tft->print(buf);
    } else {
      tft->print("Not available");
    }
    y += lh + 4;

    drawSectionLine(y);
    y += 4;

    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print("[ Weather API ]");
    y += lh;

    drawLabel(8, y, "Now: ");
    tft->setTextColor(weather.valid ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(weather.valid ? "OK" : "N/A");
    drawLabel(120, y, "3d: ");
    tft->setTextColor(weather.tempMax.length() > 0 ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(weather.tempMax.length() > 0 ? "OK" : "N/A");
    y += lh;

    drawLabel(8, y, "24h: ");
    tft->setTextColor(hourly.valid ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(hourly.valid ? "OK" : "N/A");

    drawLabel(120, y, "Mem: ");
    tft->setTextColor(COLOR_PRIMARY);
    tft->print(ESP.getFreeHeap() / 1024);
    tft->print("KB");
    y += lh;

    drawSectionLine(y);
    y += 2;
    tft->setTextColor(COLOR_LABEL);
    tft->setTextSize(1);
    tft->setCursor(8, y);
    tft->print("Press BOOT to return");

  } else {
    int lh = 12;

    fillArea(8, 298, CONTENT_W, lh + 16, COLOR_BG);
    int y = 301;
    drawLabel(8, y, "Uptime: ");
    tft->setTextColor(COLOR_PRIMARY);
    unsigned long up = millis() / 1000;
    tft->print(up / 3600);
    tft->print("h ");
    tft->print((up % 3600) / 60);
    tft->print("m ");
    tft->print(up % 60);
    tft->print("s");

    y = 350;
    fillArea(PAD_LEFT, y, CONTENT_W, 412 - y, COLOR_BG);
    tft->setTextColor(COLOR_CLOCK);
    tft->setTextSize(1);
    tft->setCursor(8, y);
    tft->print("[ NTP ]");
    y += lh;

    drawLabel(8, y, "Status: ");
    tft->setTextColor(state.timeSynced ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(state.timeSynced ? "Synced" : "Failed");
    y += lh;

    drawLabel(8, y, "Server: ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.timeSynced) tft->print(state.ntpServer);
    else tft->print("--");
    y += lh;

    drawLabel(8, y, "Time:  ");
    tft->setTextColor(COLOR_PRIMARY);
    if (state.timeSynced) {
      time_t t = timeClient->getEpochTime();
      struct tm *ti = localtime(&t);
      char buf[20];
      sprintf(buf, "%04d-%02d-%02d %02d:%02d",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour, ti->tm_min);
      tft->print(buf);
    } else {
      tft->print("Not available");
    }
    y += lh + 4;

    drawSectionLine(y);
    y += 4;

    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print("[ Weather API ]");
    y += lh;

    drawLabel(8, y, "Now: ");
    tft->setTextColor(weather.valid ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(weather.valid ? "OK" : "N/A");
    drawLabel(120, y, "3d: ");
    tft->setTextColor(weather.tempMax.length() > 0 ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(weather.tempMax.length() > 0 ? "OK" : "N/A");
    y += lh;

    drawLabel(8, y, "24h: ");
    tft->setTextColor(hourly.valid ? COLOR_GREEN : COLOR_ACCENT);
    tft->print(hourly.valid ? "OK" : "N/A");

    drawLabel(120, y, "Mem: ");
    tft->setTextColor(COLOR_PRIMARY);
    tft->print(ESP.getFreeHeap() / 1024);
    tft->print("KB");
  }
}

void drawLoadingFrame(int frame) {
  int cx = SCREEN_W / 2;
  int cy = DETAIL_Y + DETAIL_H / 2;
  int radius = 6;

  fillArea(cx - 25, cy - 8, 50, 20, COLOR_BG);

  for (int i = 0; i < 8; i++) {
    float angle = (i * 45) * 3.14159 / 180.0;
    int px = cx + (int)(radius * cos(angle));
    int py = cy + (int)(radius * sin(angle));
    int dist = (i - frame + 8) % 8;
    uint16_t c = (dist == 0) ? COLOR_CLOCK : (dist == 1) ? COLOR_ACCENT : COLOR_LINE;
    tft->drawPixel(px, py, c);
  }

  tft->setTextSize(1);
  tft->setTextColor(COLOR_LABEL, COLOR_BG);
  tft->setCursor(cx - 24, cy + 10);
  tft->print("Loading");
}

// ---- Warning page scroll state ----
static int warnScrollY = 0;
static int warnDescLineCount = 0;
static int warnHLineCount = 0;
static int warnDescStartY = 0;
static int warnDescViewH = 0;
static bool warnScrollActive = false;
static unsigned long warnLastScrollMs = 0;

#define WARN_HEADLINE_Y   50
#define WARN_SCROLL_DELAY 3000
#define WARN_LINE_H       20
#define WARN_DOT_Y        228
#define MAX_WARN_LINES    30

static String warnHLines[MAX_WARN_LINES];
static String warnDLines[MAX_WARN_LINES];

static uint16_t severityColor(const String &sev) {
  if (sev == "extreme") return WARN_COLOR_RED;
  if (sev == "severe")  return WARN_COLOR_ORANGE;
  if (sev == "moderate") return WARN_COLOR_YELLOW;
  return WARN_COLOR_BLUE;
}

static uint16_t contrastColor(uint16_t bgColor) {
  int r = (bgColor >> 11) & 0x1F;
  int g = (bgColor >> 5) & 0x3F;
  int b = bgColor & 0x1F;
  int brightness = (r * 299 + g * 587 + b * 114) / 1000;
  return brightness > 20 ? 0x0000 : 0xFFFF;
}

static String wrapText16(const char *text, int maxW) {
  String r;
  int px = 0;
  for (int i = 0; text[i];) {
    uint8_t b = (uint8_t)text[i];
    if (b == '\n') { r += '\n'; i++; px = 0; continue; }
    int cw, cl;
    if (b < 0x80)      { cw = 8;  cl = 1; }
    else if ((b & 0xE0) == 0xC0) { cw = 16; cl = 2; }
    else if ((b & 0xF0) == 0xE0) { cw = 16; cl = 3; }
    else               { cw = 8;  cl = 1; }
    if (px + cw > maxW) { r += '\n'; px = 0; }
    for (int j = 0; j < cl && text[i + j]; j++) r += text[i + j];
    i += cl;
    px += cw;
  }
  return r;
}

static String truncateText16(const String &text, int maxPx) {
  String result;
  int px = 0;
  const char *p = text.c_str();
  int ellipsisW = 24;
  while (*p) {
    int cw = ((*p & 0xE0) == 0xE0) ? 16 : ((*p & 0x80) ? 16 : 8);
    int cl = ((*p & 0xE0) == 0xE0) ? 3 : ((*p & 0x80) ? 2 : 1);
    if (px + cw + ellipsisW > maxPx && result.length() > 0) {
      result += "...";
      break;
    }
    for (int j = 0; j < cl; j++) result += p[j];
    px += cw;
    p += cl;
  }
  return result;
}

static int countLines(const String &t) {
  int n = 1;
  for (int i = 0; i < t.length(); i++) if (t[i] == '\n') n++;
  return n;
}

static void splitLines(const String &text, String *out, int &count) {
  count = 0;
  int start = 0;
  for (int i = 0; i <= text.length(); i++) {
    if (i == text.length() || text[i] == '\n') {
      if (count < MAX_WARN_LINES) out[count++] = text.substring(start, i);
      start = i + 1;
    }
  }
}

static void redrawDots(int wi) {
  if (warningCount <= 1) return;
  fillArea(PAD_LEFT, WARN_DOT_Y - 4, CONTENT_W, 12, COLOR_BG);
  int spacing = 10;
  int totalW = warningCount * spacing;
  int startX = (SCREEN_W - totalW) / 2;
  for (int i = 0; i < warningCount; i++) {
    int dx = startX + i * spacing;
    if (i == wi) tft->fillCircle(dx, WARN_DOT_Y, 3, COLOR_CLOCK);
    else tft->fillCircle(dx, WARN_DOT_Y, 2, COLOR_LINE);
  }
}

static void redrawHeadline() {
  fillArea(PAD_LEFT, WARN_HEADLINE_Y, CONTENT_W, warnHLineCount * WARN_LINE_H, COLOR_BG);
  int y = WARN_HEADLINE_Y;
  for (int i = 0; i < warnHLineCount; i++) {
    drawGB16(PAD_LEFT, y, warnHLines[i].c_str(), COLOR_CLOCK, COLOR_BG);
    y += WARN_LINE_H;
  }
}

static void drawDescLines(int offsetY) {
  int viewTop = warnDescStartY;
  int viewBot = warnDescStartY + warnDescViewH;
  for (int i = 0; i < warnDescLineCount; i++) {
    int ly = viewTop + i * WARN_LINE_H - offsetY;
    if (ly >= viewTop && ly + WARN_LINE_H <= viewBot) {
      drawGB16(PAD_LEFT, ly, warnDLines[i].c_str(), COLOR_PRIMARY, COLOR_BG);
    }
  }
}

void drawWarningPage() {
  animateWipe();
  warnScrollY = 0;
  warnScrollActive = false;
  warnLastScrollMs = millis();

  fillArea(0, 0, SCREEN_W, 20, COLOR_ACCENT);
  drawGB16(4, 2, "天气预警", COLOR_PRIMARY, COLOR_ACCENT);
  if (warningCount > 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%s", warningCount, "条");
    drawGB16(80, 2, buf, COLOR_LABEL, COLOR_ACCENT);
  }
  drawWiFiBars(SCREEN_W - PAD_RIGHT - 20, 4, state.wifiConnected);

  if (warningCount == 0) {
    drawGB16(60, 90, "当前无预警", COLOR_GREEN, COLOR_BG);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_MUTED);
    tft->setCursor(60, 120);
    tft->print("No active warnings");
    return;
  }

  int wi = state.warningIndex;
  if (wi < 0 || wi >= warningCount) wi = 0;

  uint16_t sColor = severityColor(warnings[wi].severity);
  uint16_t textColor = contrastColor(sColor);
  fillArea(PAD_LEFT, 24, CONTENT_W, 20, sColor);
  drawGB16(PAD_LEFT + 4, 26, warnings[wi].eventName.c_str(), textColor, sColor);
  if (warnings[wi].senderName.length() > 0) {
    String truncated = truncateText16(warnings[wi].senderName, CONTENT_W - 80);
    drawGB16(PAD_LEFT + 80, 26, truncated.c_str(), textColor, sColor);
  }

  int maxW = CONTENT_W;
  String wHeadline;
  if (warnings[wi].headline.length() > 0) {
    wHeadline = wrapText16(warnings[wi].headline.c_str(), maxW);
  }
  warnHLineCount = 0;
  if (wHeadline.length() > 0) splitLines(wHeadline, warnHLines, warnHLineCount);

  String d = warnings[wi].description;
  if (d.length() > 400) d = d.substring(0, 400);
  String wDesc;
  warnDescLineCount = 0;
  if (d.length() > 0) {
    wDesc = wrapText16(d.c_str(), maxW);
    splitLines(wDesc, warnDLines, warnDescLineCount);
  }

  warnDescStartY = WARN_HEADLINE_Y + warnHLineCount * WARN_LINE_H;
  warnDescViewH = WARN_DOT_Y - warnDescStartY - 4;

  int totalDescH = warnDescLineCount * WARN_LINE_H;
  if (totalDescH > warnDescViewH) {
    warnScrollActive = true;
  }

  redrawHeadline();
  drawDescLines(0);

  if (warningCount > 1) {
    redrawDots(wi);
  }
}

void updateWarningScroll() {
  if (!warnScrollActive || warningCount == 0) return;

  unsigned long now = millis();
  if (now - warnLastScrollMs < WARN_SCROLL_DELAY) return;
  warnLastScrollMs = now;

  int maxScroll = warnDescLineCount * WARN_LINE_H - warnDescViewH + WARN_LINE_H;
  if (maxScroll < WARN_LINE_H) maxScroll = WARN_LINE_H;

  warnScrollY += WARN_LINE_H;
  if (warnScrollY > maxScroll) {
    warnScrollY = 0;
  }

  fillArea(PAD_LEFT, warnDescStartY, CONTENT_W, warnDescViewH, COLOR_BG);

  int wi = state.warningIndex;
  if (wi < 0 || wi >= warningCount) return;

  drawDescLines(warnScrollY);

  redrawHeadline();
  redrawDots(wi);
}

bool isWarningScrollNeeded() {
  return warnScrollActive;
}

void drawMinutelyPage() {
  animateWipe();

  fillArea(0, 0, SCREEN_W, 20, COLOR_ACCENT);
  drawGB16(4, 2, "分钟降水", COLOR_PRIMARY, COLOR_ACCENT);
  int cityX = 80;
  if (weatherName.length() > 0) {
    drawGB16(cityX, 2, weatherName.c_str(), COLOR_GOLD, COLOR_ACCENT);
    cityX += textWidth16(weatherName.c_str()) + 4;
  }
  if (warningCount > 0) {
    drawGB16(cityX, 2, "有预警", WARN_COLOR_ORANGE, COLOR_ACCENT);
  }
  drawWiFiBars(SCREEN_W - PAD_RIGHT - 20, 4, state.wifiConnected);

  if (!minutely.valid) {
    int cx = (SCREEN_W - 112) / 2;
    drawGB16(cx, 90, "分钟降水数据不可用", COLOR_MUTED, COLOR_BG);
    drawGB16(70, 130, "短按BOOT刷新", COLOR_LABEL, COLOR_BG);
    return;
  }

  int chartW = 202;
  int chartLeft = (SCREEN_W - chartW) / 2 + 4;
  int chartH = chartW * 9 / 16;
  int chartTop = 20 + (220 - chartH) / 2 + 26 + 4;

  if (minutely.summary.length() > 0) {
    String s = minutely.summary;
    int lineY = 22;
    int maxW = CONTENT_W;
    int i = 0;
    while (i < s.length() && lineY < 54) {
      int px = 0;
      int lineStart = i;
      while (i < s.length()) {
        uint8_t b = s[i];
        int cw;
        if (b < 0x80) { cw = 8; i++; }
        else if ((b & 0xE0) == 0xC0) { cw = 16; i += 2; }
        else if ((b & 0xF0) == 0xE0) { cw = 16; i += 3; }
        else { cw = 8; i++; }
        if (px + cw > maxW) {
          i -= (b < 0x80) ? 1 : ((b & 0xE0) == 0xC0) ? 2 : 3;
          break;
        }
        px += cw;
      }
      if (i == lineStart) i++;
      drawGB16(PAD_LEFT, lineY, s.substring(lineStart, i).c_str(), COLOR_CLOCK, COLOR_BG);
      lineY += 16;
    }
    chartTop = 60 + 4;
  }

  int chartBot = chartTop + chartH;
  if (chartBot > 208) {
    chartH = 208 - chartTop;
    chartBot = 208;
  }

  float maxPrecip = 0;
  for (int i = 0; i < MINUTELY_SLOTS; i++) {
    if (minutely.slots[i].precip > maxPrecip) maxPrecip = minutely.slots[i].precip;
  }
  if (maxPrecip < 0.5) maxPrecip = 0.5;

  uint16_t lineColor = 0xFFFF;
  uint16_t pointColor = 0xFFFF;
  uint16_t gridColor = 0x2104;
  uint16_t colorSmall = 0x04B0;
  uint16_t colorMed  = 0x06BF;
  uint16_t colorLarge = 0x001F;

  int py05 = chartBot - (int)(0.5f / maxPrecip * chartH);
  int py20 = chartBot - (int)(2.0f / maxPrecip * chartH);

  int ptsX[MINUTELY_SLOTS], ptsY[MINUTELY_SLOTS];
  for (int i = 0; i < MINUTELY_SLOTS; i++) {
    ptsX[i] = chartLeft + (int)((float)i / (MINUTELY_SLOTS - 1) * chartW);
    float ratio = minutely.slots[i].precip / maxPrecip;
    ptsY[i] = chartBot - (int)(ratio * chartH);
  }

  for (int x = chartLeft; x < chartLeft + chartW; x++) {
    int seg = (int)((float)(x - chartLeft) / chartW * (MINUTELY_SLOTS - 1));
    if (seg >= MINUTELY_SLOTS - 1) seg = MINUTELY_SLOTS - 2;
    float t = ((float)(x - chartLeft) / chartW * (MINUTELY_SLOTS - 1)) - seg;
    int ly = ptsY[seg] + (int)((ptsY[seg + 1] - ptsY[seg]) * t);
    if (ly < chartTop) ly = chartTop;

    int yFill = ly;
    if (py20 > chartTop && yFill < py20) {
      int bot = (py20 < chartBot) ? py20 : chartBot;
      if (yFill < bot) fillArea(x, yFill, 1, bot - yFill, colorLarge);
      yFill = bot;
    }
    if (py05 > chartTop && yFill < py05) {
      int bot = (py05 < chartBot) ? py05 : chartBot;
      if (yFill < bot) fillArea(x, yFill, 1, bot - yFill, colorMed);
      yFill = bot;
    }
    if (yFill < chartBot) {
      fillArea(x, yFill, 1, chartBot - yFill, colorSmall);
    }
  }

  for (int i = 1; i < MINUTELY_SLOTS; i++) {
    tft->drawLine(ptsX[i - 1], ptsY[i - 1], ptsX[i], ptsY[i], lineColor);
  }

  const int bands = 3;
  for (int v = 0; v <= bands; v++) {
    int py = chartBot - v * chartH / bands;
    tft->drawLine(chartLeft, py, chartLeft + chartW, py, gridColor);
  }
  const char* yLabels[] = {"小", "中", "大"};
  for (int v = 0; v < bands; v++) {
    int py = chartBot - (v * chartH / bands + chartH / (2 * bands));
    drawGB16(chartLeft - 17, py - 8, yLabels[v], COLOR_MUTED, COLOR_BG);
  }
  drawGB16(chartLeft - 17, chartTop - 8, "降水", COLOR_LABEL, COLOR_BG);

  tft->drawLine(chartLeft, chartBot, chartLeft + chartW, chartBot, COLOR_LINE);

  for (int i = 0; i < MINUTELY_SLOTS; i++) {
    tft->drawPixel(ptsX[i], ptsY[i], pointColor);
  }

  drawGB16(ptsX[0] - 16, chartBot + 4, "现在", COLOR_LABEL, COLOR_BG);
  drawGB16(ptsX[MINUTELY_SLOTS / 2] - 28, chartBot + 4, "1小时后", COLOR_LABEL, COLOR_BG);
  drawGB16(ptsX[MINUTELY_SLOTS - 1] - 56, chartBot + 4, "2小时后", COLOR_LABEL, COLOR_BG);

  float totalPrecip = 0, peakPrecip = 0;
  for (int i = 0; i < MINUTELY_SLOTS; i++) {
    totalPrecip += minutely.slots[i].precip;
    if (minutely.slots[i].precip > peakPrecip) peakPrecip = minutely.slots[i].precip;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "总降水: %.1fmm  峰值: %.1fmm", totalPrecip, peakPrecip);
  drawGB16(PAD_LEFT, 218, buf, COLOR_PRIMARY, COLOR_BG);
}

void drawFullUI() {
  animateWipe();
  drawHourlyChart();
  drawStatusHeader();
  drawSectionLine(LINE1_Y);
  drawWeatherSection();
  drawClockSection();
  drawSectionLine(LINE3_Y);
  drawDetailSection();
  drawSectionLine(LINE4_Y);
  fillArea(0, BAR_Y, SCREEN_W, BAR_H, COLOR_ACCENT);
}
