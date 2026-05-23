#include "ui.h"
#include "ui_common.h"

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

    uint16_t lineColor = COLOR_PRIMARY;
    uint16_t pointColor = COLOR_PRIMARY;
    uint16_t gridColor = COLOR_GRID;
    uint16_t colorSmall = 0x1ACF;
    uint16_t colorMed  = 0x445F;
    uint16_t colorLarge = 0xE21F;

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
