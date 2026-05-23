#include "ui.h"
#include "ui_common.h"

namespace warning_page {

static int warnScrollY = 0;
static int warnDescLineCount = 0;
static int warnHLineCount = 0;
static int warnDescStartY = 0;
static int warnDescViewH = 0;
static bool warnScrollActive = false;
static unsigned long warnLastScrollMs = 0;

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
    int nameEndX = PAD_LEFT + 4;
    drawGB16(nameEndX, 26, warnings[wi].eventName.c_str(), textColor, sColor);
    nameEndX += textWidth16(warnings[wi].eventName.c_str()) + 4;
    if (warnings[wi].senderName.length() > 0) {
        int remainW = CONTENT_W - (nameEndX - PAD_LEFT);
        if (remainW > 16) {
            String truncated = truncateText16(warnings[wi].senderName, remainW);
            drawGB16(nameEndX, 26, truncated.c_str(), textColor, sColor);
        }
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

} // namespace warning_page
