#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "ui.h"

static void showBootScreen(const char *title) {
  tft->fillScreen(COLOR_BG);
  fillArea(2, 2, SCREEN_W - 4, STATUS_H, COLOR_ACCENT);
  tft->setTextColor(COLOR_PRIMARY);
  tft->setTextSize(1);
  tft->setCursor(8, 5);
  tft->print(title);
}

static void showBootLine(int y, const char *text, uint16_t color) {
  fillArea(PAD_LEFT, y, CONTENT_W, 12, COLOR_BG);
  tft->setTextColor(color);
  tft->setTextSize(1);
  tft->setCursor(PAD_LEFT, y);
  tft->print(text);
}

static void bootWiFi() {
  showBootScreen("NETWORK STARTUP");
  int y = 28;
  showBootLine(y, "WiFi connecting...", COLOR_PRIMARY);
  y += 24;

  initWiFi();

  if (state.wifiConnected) {
    showBootLine(y, "Connected!", COLOR_GREEN);
    y += 12;
    char ipBuf[32];
    sprintf(ipBuf, "IP: %s", WiFi.localIP().toString().c_str());
    showBootLine(y, ipBuf, COLOR_CLOCK);
  } else {
    showBootLine(y, "Connection failed!", COLOR_ACCENT);
  }
  delay(1200);
}

static void bootNTP() {
  int y = 100;
  if (!state.wifiConnected) {
    showBootLine(y, "NTP skipped (no WiFi)", COLOR_ACCENT);
    delay(800);
    return;
  }

  showBootLine(y, "NTP syncing...", COLOR_LABEL);
  initNTP();

  if (state.timeSynced) {
    showBootLine(y, "NTP synced!", COLOR_GREEN);
  } else {
    showBootLine(y, "NTP failed - local time", COLOR_ACCENT);
  }
  delay(600);
}

static int loadingFrame = 0;

static void refreshWeatherWithUI() {
  for (int i = 0; i < 12; i++) {
    drawLoadingFrame(loadingFrame);
    loadingFrame = (loadingFrame + 1) % 8;
    delay(80);
  }
  fetchWeather();
  fetchDaily();
  fetchHourly();
  drawWeatherSection();
  drawDetailSection();
  drawHourlyChart();
  state.lastWeatherFetch = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Desktop Mini TV Starting...");

  state.wifiConnected = false;
  state.timeSynced = false;
  state.ntpTried = false;
  state.weatherLoaded = false;
  state.showingSystemInfo = false;
  state.lastWeatherFetch = 0;
  state.lastSecond = -1;
  state.lastMinute = -1;
  state.lastBtnState = HIGH;
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  initDisplay();
  pinMode(BTN_PIN, INPUT_PULLUP);

  bootWiFi();
  bootNTP();
  refreshWeatherWithUI();
  drawFullUI();
}

void loop() {
  unsigned long now = millis();

  int curBtn = digitalRead(BTN_PIN);
  if (curBtn == LOW && state.lastBtnState == HIGH) {
    state.showingSystemInfo = !state.showingSystemInfo;
    if (state.showingSystemInfo) drawSystemInfo();
    else drawFullUI();
  }
  state.lastBtnState = curBtn;

  if (state.showingSystemInfo) { delay(50); return; }

  if (state.wifiConnected && state.timeSynced) {
    timeClient->update();
  }

  if (!state.timeSynced && !state.ntpTried && state.wifiConnected) {
    initNTP();
    if (state.timeSynced) drawFullUI();
    else drawClockSection();
  }

  int curSec = -1, curMin = -1;
  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    curSec = ti->tm_sec;
    curMin = ti->tm_min;
  } else {
    curSec = (now / 1000) % 60;
    curMin = (now / 60000) % 60;
  }

  if (curMin != state.lastMinute || !state.weatherLoaded) {
    state.lastMinute = curMin;
    if (now - state.lastWeatherFetch > WEATHER_INTERVAL_MS || state.lastWeatherFetch == 0) {
      if (state.wifiConnected) {
        refreshWeatherWithUI();
      }
    }
  }

  if (curSec != state.lastSecond) {
    state.lastSecond = curSec;
    drawClockSection();
    drawStatusHeader();
  }

  if (WiFi.status() != WL_CONNECTED && state.wifiConnected) {
    state.wifiConnected = false;
    drawError("WiFi Disconnected");
    WiFi.reconnect();
  } else if (WiFi.status() == WL_CONNECTED && !state.wifiConnected) {
    state.wifiConnected = true;
    if (!state.timeSynced) initNTP();
    state.lastWeatherFetch = 0;
    drawWeatherSection();
    drawStatusHeader();
  }

  delay(50);
}
