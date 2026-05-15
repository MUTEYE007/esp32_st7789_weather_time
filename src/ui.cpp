#include "ui.h"
#include "config.h"
#include "display.h"
#include "weather.h"
#include "hanzi_font.h"
#include <WiFi.h>
#include <time.h>

void drawStatusHeader() {
  fillArea(0, STATUS_Y, SCREEN_W, STATUS_H, COLOR_BG);

  tft->setTextSize(1);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(PAD_LEFT, STATUS_Y + 3);
  tft->print(WEATHER_NAME);

  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    char buf[8];
    sprintf(buf, " %02d:%02d", ti->tm_hour, ti->tm_min);
    tft->setTextColor(COLOR_MUTED);
    tft->print(buf);
  }

  drawWiFiBars(SCREEN_W - PAD_RIGHT - 20, STATUS_Y + 2, state.wifiConnected);
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
    drawHanziText(tft, 50, WEATHER_Y + 10, "不可用", COLOR_LABEL);
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

  drawHanziText(tft, CITY_X, CITY_Y, "福州", COLOR_CLOCK);

  if (weather.tempMax.length() > 0) {
    tft->setCursor(HILO_X, HILO_Y1);
    tft->setTextSize(1);
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

  drawHanziText(tft, WTEXT_X, WTEXT_Y, weather.weatherText.c_str(), COLOR_PRIMARY);
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
    strcpy(dateBuf, "NTP failed, local time");
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

void drawDetailSection() {
  fillArea(PAD_LEFT, DETAIL_Y, CONTENT_W, DETAIL_H, COLOR_BG);

  if (!weather.valid) return;

  drawHanziText(tft, PAD_LEFT, DETAIL_ROW1_Y, "体感", COLOR_LABEL);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(44, DETAIL_ROW1_Y);
  tft->print(weather.feelsLike);
  tft->print("C");

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(120, DETAIL_ROW1_Y);
  tft->setTextSize(1);
  tft->print("Hum ");
  tft->setTextColor(COLOR_PRIMARY);
  tft->print(weather.humidity);
  tft->print("%");

  drawHanziText(tft, PAD_LEFT, DETAIL_ROW2_Y, "更新", COLOR_LABEL);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setCursor(44, DETAIL_ROW2_Y);
  if (weather.updateTime.length() >= 16) {
    tft->print(weather.updateTime.substring(11, 16));
  }

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(120, DETAIL_ROW2_Y);
  tft->setTextSize(1);
  tft->print("Wind ");
  tft->setTextColor(COLOR_PRIMARY);
  tft->print(windDirToEn(weather.windDir));
  tft->print("-");
  tft->print(weather.windScale);
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
  tft->fillScreen(COLOR_BG);

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
  tft->print(WIFI_SSID);
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
}

void drawError(const char *msg) {
  fillArea(30, 100, 180, 30, COLOR_BG);
  tft->drawRect(30, 100, 180, 30, COLOR_ACCENT);
  tft->setTextColor(COLOR_ACCENT);
  tft->setTextSize(1);
  tft->setCursor(40, 110);
  tft->print(msg);
}

void drawLoadingFrame(int frame) {
  int cx = SCREEN_W / 2;
  int cy = DETAIL_Y + DETAIL_H / 2;
  int radius = 16;

  fillArea(PAD_LEFT, DETAIL_Y, CONTENT_W, DETAIL_H, COLOR_BG);

  for (int i = 0; i < 8; i++) {
    float angle = (i * 45) * 3.14159 / 180.0;
    int px = cx + (int)(radius * cos(angle));
    int py = cy + (int)(radius * sin(angle));
    int dist = (i - frame + 8) % 8;
    uint16_t c = (dist == 0) ? COLOR_CLOCK : (dist == 1) ? COLOR_ACCENT : COLOR_LINE;
    int r = (dist == 0) ? 3 : (dist == 1) ? 2 : 1;
    tft->fillCircle(px, py, r, c);
  }

  tft->setTextSize(1);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(cx - 24, cy + 22);
  tft->print("Loading");
}

void drawFullUI() {
  tft->fillScreen(COLOR_BG);
  drawStatusHeader();
  drawSectionLine(LINE1_Y);
  drawWeatherSection();
  drawSectionLine(LINE2_Y);
  drawClockSection();
  drawSectionLine(LINE3_Y);
  drawDetailSection();
  drawSectionLine(LINE4_Y);
  drawHourlyChart();
  fillArea(0, BAR_Y, SCREEN_W, BAR_H, COLOR_ACCENT);
}
