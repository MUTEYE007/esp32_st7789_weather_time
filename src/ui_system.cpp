#include "ui.h"
#include "ui_common.h"
#include <WiFi.h>
#include <time.h>

namespace system_page {

static const int SYS_LINE_H = 12;

static void drawSysSection(int &y, const char *title) {
    tft->setTextColor(COLOR_CLOCK);
    tft->setCursor(8, y);
    tft->print(title);
    y += SYS_LINE_H;
}

static void drawSysRow(int y, const char *label, const char *value, uint16_t color) {
    drawLabel(8, y, label);
    tft->setTextColor(color);
    tft->print(value);
}

static void drawSysSectionLine(int &y) {
    drawSectionLine(y);
    y += 4;
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

        drawSysSection(y, "[ ESP32 ]");

        drawSysRow(y, "Chip: ", (String(ESP.getChipModel()) + " rev" + ESP.getChipRevision()).c_str(), COLOR_PRIMARY);
        y += SYS_LINE_H;

        char flashBuf[32];
        snprintf(flashBuf, sizeof(flashBuf), "%uMB  Free: %uKB",
            ESP.getFlashChipSize() / (1024 * 1024), ESP.getFreeHeap() / 1024);
        drawSysRow(y, "Flash: ", flashBuf, COLOR_PRIMARY);
        y += SYS_LINE_H;

        tft->setTextColor(COLOR_PRIMARY);
        drawLabel(8, y, "Uptime: ");
        unsigned long up = millis() / 1000;
        tft->print(up / 3600);
        tft->print("h ");
        tft->print((up % 3600) / 60);
        tft->print("m ");
        tft->print(up % 60);
        tft->print("s");
        y += SYS_LINE_H + 4;

        // Firmware version (compile timestamp)
        drawSysRow(y, "Build: ", __DATE__ " " __TIME__, COLOR_PRIMARY);
        y += SYS_LINE_H;

        drawSysSectionLine(y);
        drawSysSection(y, "[ WiFi ]");

        drawSysRow(y, "SSID: ", WiFi.SSID().c_str(), COLOR_PRIMARY);
        y += SYS_LINE_H;

        drawSysRow(y, "IP:   ",
            state.wifiConnected ? WiFi.localIP().toString().c_str() : "--.--.--.--",
            state.wifiConnected ? COLOR_GREEN : COLOR_ACCENT);
        y += SYS_LINE_H;

        drawSysRow(y, "GW:   ",
            state.wifiConnected ? WiFi.gatewayIP().toString().c_str() : "--.--.--.--",
            COLOR_PRIMARY);
        y += SYS_LINE_H;

        drawLabel(8, y, "RSSI: ");
        tft->setTextColor(COLOR_PRIMARY);
        if (state.wifiConnected) {
            tft->print(WiFi.RSSI());
            tft->print("dBm");
        } else {
            tft->print("N/A");
        }
        y += SYS_LINE_H + 4;

        drawSysSectionLine(y);
        drawSysSection(y, "[ NTP ]");

        drawSysRow(y, "Status: ", state.timeSynced ? "Synced" : "Failed",
            state.timeSynced ? COLOR_GREEN : COLOR_ACCENT);
        y += SYS_LINE_H;

        drawSysRow(y, "Server: ", state.timeSynced ? state.ntpServer : "--", COLOR_PRIMARY);
        y += SYS_LINE_H;

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
        y += SYS_LINE_H + 4;

        drawSysSectionLine(y);
        drawSysSection(y, "[ Weather API ]");

        drawSysRow(y, "Now: ", weather.valid ? "OK" : "N/A",
            weather.valid ? COLOR_GREEN : COLOR_ACCENT);
        tft->setTextColor(weather.tempMax.length() > 0 ? COLOR_GREEN : COLOR_ACCENT);
        tft->setCursor(120, y);
        tft->print("3d: ");
        tft->print(weather.tempMax.length() > 0 ? "OK" : "N/A");
        y += SYS_LINE_H;

        drawSysRow(y, "24h: ", hourly.valid ? "OK" : "N/A",
            hourly.valid ? COLOR_GREEN : COLOR_ACCENT);
        tft->setTextColor(COLOR_PRIMARY);
        tft->setCursor(120, y);
        tft->print("Mem: ");
        tft->print(ESP.getFreeHeap() / 1024);
        tft->print("KB");
        y += SYS_LINE_H;

        drawSectionLine(y);
        y += 2;
        tft->setTextColor(COLOR_LABEL);
        tft->setTextSize(1);
        tft->setCursor(8, y);
        tft->print("Press BOOT to return");

    } else {
        fillArea(8, 298, CONTENT_W, SYS_LINE_H + 16, COLOR_BG);
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

        drawSysSection(y, "[ NTP ]");

        drawSysRow(y, "Status: ", state.timeSynced ? "Synced" : "Failed",
            state.timeSynced ? COLOR_GREEN : COLOR_ACCENT);
        y += SYS_LINE_H;

        drawSysRow(y, "Server: ", state.timeSynced ? state.ntpServer : "--", COLOR_PRIMARY);
        y += SYS_LINE_H;

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
        y += SYS_LINE_H + 4;

        drawSysSectionLine(y);
        drawSysSection(y, "[ Weather API ]");

        drawSysRow(y, "Now: ", weather.valid ? "OK" : "N/A",
            weather.valid ? COLOR_GREEN : COLOR_ACCENT);
        tft->setTextColor(weather.tempMax.length() > 0 ? COLOR_GREEN : COLOR_ACCENT);
        tft->setCursor(120, y);
        tft->print("3d: ");
        tft->print(weather.tempMax.length() > 0 ? "OK" : "N/A");
        y += SYS_LINE_H;

        drawSysRow(y, "24h: ", hourly.valid ? "OK" : "N/A",
            hourly.valid ? COLOR_GREEN : COLOR_ACCENT);
        tft->setTextColor(COLOR_PRIMARY);
        tft->setCursor(120, y);
        tft->print("Mem: ");
        tft->print(ESP.getFreeHeap() / 1024);
        tft->print("KB");
    }
}

} // namespace system_page
