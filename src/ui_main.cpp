#include "ui.h"
#include "ui_common.h"
#include <WiFi.h>
#include <time.h>

namespace main_page {

static void drawCloudCluster(int cx, int cy, int r1, int cy1, int r2, int cy2, int r3, int cy3, int rx, int ry, int rw, int rh) {
    tft->fillCircle(cx - 8, cy1, r1, COLOR_WHITE);
    tft->fillCircle(cx, cy2, r2, COLOR_WHITE);
    tft->fillCircle(cx + 8, cy3, r3, COLOR_WHITE);
    tft->fillRect(rx, ry, rw, rh, COLOR_WHITE);
}

void drawWeatherIcon(int cx, int cy, int code) {
    if (code == 100 || code == 150) {
        tft->fillCircle(cx, cy, 10, COLOR_WHITE);
        tft->drawLine(cx, cy - 14, cx, cy - 18, COLOR_WHITE);
        tft->drawLine(cx, cy + 14, cx, cy + 18, COLOR_WHITE);
        tft->drawLine(cx - 14, cy, cx - 18, cy, COLOR_WHITE);
        tft->drawLine(cx + 14, cy, cx + 18, cy, COLOR_WHITE);
        tft->drawLine(cx - 10, cy - 10, cx - 13, cy - 13, COLOR_WHITE);
        tft->drawLine(cx + 10, cy + 10, cx + 13, cy + 13, COLOR_WHITE);
        tft->drawLine(cx + 10, cy - 10, cx + 13, cy - 13, COLOR_WHITE);
        tft->drawLine(cx - 10, cy + 10, cx - 13, cy + 13, COLOR_WHITE);
    } else if (code >= 101 && code <= 104) {
        tft->fillCircle(cx - 10, cy + 2, 8, COLOR_WHITE);
        tft->fillCircle(cx, cy - 2, 10, COLOR_WHITE);
        tft->fillCircle(cx + 10, cy + 2, 8, COLOR_WHITE);
        tft->fillRect(cx - 10, cy - 2, 20, 14, COLOR_WHITE);
    } else if ((code >= 300 && code <= 399) || (code >= 400 && code <= 499)) {
        drawCloudCluster(cx, cy, 7, cy - 2, 9, cy - 5, 7, cy - 2, cx - 8, cy - 5, 16, 10);
        tft->drawLine(cx - 8, cy + 8, cx - 10, cy + 14, COLOR_WHITE);
        tft->drawLine(cx, cy + 8, cx - 2, cy + 14, COLOR_WHITE);
        tft->drawLine(cx + 8, cy + 8, cx + 6, cy + 14, COLOR_WHITE);
    } else if (code >= 500 && code <= 599) {
        drawCloudCluster(cx, cy, 7, cy - 2, 9, cy - 5, 7, cy - 2, cx - 8, cy - 5, 16, 10);
        tft->drawLine(cx - 8, cy + 8, cx - 8, cy + 12, COLOR_WHITE);
        tft->drawLine(cx, cy + 8, cx, cy + 12, COLOR_WHITE);
        tft->drawLine(cx + 8, cy + 8, cx + 8, cy + 12, COLOR_WHITE);
    } else {
        drawCloudCluster(cx, cy, 8, cy, 10, cy - 3, 8, cy, cx - 8, cy - 3, 16, 11);
    }
}

void drawStatusHeader() {
    fillArea(0, STATUS_Y, SCREEN_W, STATUS_H, COLOR_BG);

    drawGB16(PAD_LEFT, STATUS_Y, weatherName.c_str(), COLOR_GOLD, COLOR_BG);
    drawWiFiBars(SCREEN_W - PAD_RIGHT - 20, STATUS_Y + 2, state.wifiConnected);

    int nameEndX = PAD_LEFT + textWidth16(weatherName.c_str());

    tft->setTextSize(1);
    tft->setTextColor(COLOR_MUTED, COLOR_BG);
    tft->setCursor(nameEndX, STATUS_Y + 3);

    if (state.timeSynced) {
        time_t t = timeClient->getEpochTime();
        struct tm *ti = localtime(&t);
        tft->printf(" %02d:%02d", ti->tm_hour, ti->tm_min);
    } else {
        tft->print(" --:--");
    }

    // Next weather update (W) and next alert check (A) — English abbreviations
    String wNext = nextTimeStr(state.lastWeatherFetch, state.weatherIntervalMs);
    String aNext = nextTimeStr(state.lastWarningFetch, WARN_INTERVAL_MS);
    tft->print(" W");
    tft->print(wNext);
    tft->print(" A");
    tft->print(aNext);
}

void updateStatusTime() {
    if (!state.timeSynced) return;

    int nameEndX = PAD_LEFT + textWidth16(weatherName.c_str());

    tft->setTextSize(1);
    tft->setTextColor(COLOR_MUTED, COLOR_BG);
    tft->setCursor(nameEndX, STATUS_Y + 3);

    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    tft->printf(" %02d:%02d", ti->tm_hour, ti->tm_min);

    // Next weather update (W) and next alert check (A)
    String wNext = nextTimeStr(state.lastWeatherFetch, state.weatherIntervalMs);
    String aNext = nextTimeStr(state.lastWarningFetch, WARN_INTERVAL_MS);
    tft->print(" W");
    tft->print(wNext);
    tft->print(" A");
    tft->print(aNext);
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

static const char *wdCN[] = {"日","一","二","三","四","五","六"};

void drawClockSection() {
    // Always draw card background first
    tft->fillRoundRect(CLOCK_CARD_X, CLOCK_CARD_Y, CLOCK_CARD_W, CLOCK_CARD_H,
                       CLOCK_CARD_R, COL_CLOCK_CARD);

    int h, m, s;

    if (state.timeSynced) {
        time_t t = timeClient->getEpochTime();
        struct tm *ti = localtime(&t);
        h = ti->tm_hour;
        m = ti->tm_min;
        s = ti->tm_sec;
    } else if (state.ntpTried) {
        unsigned long ms = millis() - state.bootTime;
        s = (ms / 1000) % 60;
        m = (ms / 60000) % 60;
        h = (ms / 3600000) % 24;
    } else {
        tft->setTextColor(COLOR_ACCENT, COL_CLOCK_CARD);
        tft->setTextSize(2);
        tft->setCursor(56, CLOCK_CARD_Y + 22);
        tft->print("Syncing...");
        return;
    }

    // Centering: HH(60) + ':'(30) + MM(60) + gap(6) + SS(24) = 180px
    int totalW = 5 * 30 + 6 + 2 * 12;
    int cx = (SCREEN_W - totalW) / 2;

    char hBuf[3], mBuf[3], sBuf[3];
    sprintf(hBuf, "%02d", h);
    sprintf(mBuf, "%02d", m);
    sprintf(sBuf, "%02d", s);

    // HH
    tft->setTextSize(5);
    tft->setTextColor(COL_CLOCK_HM, COL_CLOCK_CARD);
    tft->setCursor(cx, CLOCK_HM_Y);
    tft->print(hBuf);

    // Colon (always visible here; flashing handled by updateClockTime)
    tft->setTextColor(COL_CLOCK_COLON, COL_CLOCK_CARD);
    tft->setCursor(cx + 2 * 30, CLOCK_HM_Y);
    tft->print(":");

    // MM
    tft->setTextColor(COL_CLOCK_HM, COL_CLOCK_CARD);
    tft->setCursor(cx + 3 * 30, CLOCK_HM_Y);
    tft->print(mBuf);

    // SS
    tft->setTextSize(2);
    tft->setTextColor(COL_CLOCK_SS, COL_CLOCK_CARD);
    tft->setCursor(cx + 5 * 30 + 6, CLOCK_SS_Y);
    tft->print(sBuf);

    // Date
    if (state.timeSynced) {
        time_t t = timeClient->getEpochTime();
        struct tm *ti = localtime(&t);
        char dateBuf[32];
        sprintf(dateBuf, "%04d年%02d月%02d日 星期%s",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            wdCN[ti->tm_wday]);
        int dateW = textWidth16(dateBuf);
        int dcx = (SCREEN_W - dateW) / 2;
        drawGB16(dcx, CLOCK_DATE_Y, dateBuf, COL_CLOCK_DATE, COL_CLOCK_CARD);
    } else {
        int dw = strlen(state.ntpFailReason) * 12;
        tft->setTextSize(2);
        tft->setTextColor(COLOR_MUTED, COL_CLOCK_CARD);
        tft->setCursor((SCREEN_W - dw) / 2, CLOCK_DATE_Y);
        tft->print(state.ntpFailReason);
    }
}

void updateClockTime(int h, int m, int s) {
    static bool colonVisible = true;
    colonVisible = !colonVisible;

    int totalW = 5 * 30 + 6 + 2 * 12;
    int cx = (SCREEN_W - totalW) / 2;

    char hBuf[3], mBuf[3], sBuf[3];
    sprintf(hBuf, "%02d", h);
    sprintf(mBuf, "%02d", m);
    sprintf(sBuf, "%02d", s);

    // HH
    tft->setTextSize(5);
    tft->setTextColor(COL_CLOCK_HM, COL_CLOCK_CARD);
    tft->setCursor(cx, CLOCK_HM_Y);
    tft->print(hBuf);

    // Colon — flash every second
    int colonX = cx + 2 * 30;
    if (colonVisible) {
        tft->setTextColor(COL_CLOCK_COLON, COL_CLOCK_CARD);
        tft->setCursor(colonX, CLOCK_HM_Y);
        tft->print(":");
    } else {
        fillArea(colonX, CLOCK_HM_Y, 30, 40, COL_CLOCK_CARD);
    }

    // MM
    tft->setTextColor(COL_CLOCK_HM, COL_CLOCK_CARD);
    tft->setCursor(cx + 3 * 30, CLOCK_HM_Y);
    tft->print(mBuf);

    // SS
    tft->setTextSize(2);
    tft->setTextColor(COL_CLOCK_SS, COL_CLOCK_CARD);
    tft->setCursor(cx + 5 * 30 + 6, CLOCK_SS_Y);
    tft->print(sBuf);

    // Date
    if (state.timeSynced) {
        time_t t = timeClient->getEpochTime();
        struct tm *ti = localtime(&t);
        char dateBuf[32];
        sprintf(dateBuf, "%04d年%02d月%02d日 星期%s",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            wdCN[ti->tm_wday]);
        int dateW = textWidth16(dateBuf);
        int dcx = (SCREEN_W - dateW) / 2;
        drawGB16(dcx, CLOCK_DATE_Y, dateBuf, COL_CLOCK_DATE, COL_CLOCK_CARD);
    }
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
            tft->drawFastVLine(px, py, CHART_DATA_BOT - py, COLOR_GRID);
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

} // namespace main_page
