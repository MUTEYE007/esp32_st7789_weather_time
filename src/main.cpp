#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "ui.h"

static unsigned long lastWifiTry = 0;

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

void networkTask(void *pvParameters) {
  initWiFi();
  lastWifiTry = millis();

  for (int i = 0; i < 60; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      state.wifiConnected = true;
      break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  while (1) {
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (now - lastWifiTry > 5000) {
        WiFi.reconnect();
        lastWifiTry = now;
      }
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      if (!state.wifiConnected) {
        state.wifiConnected = true;
      }
      xSemaphoreGive(dataMutex);
    }

    if (!state.timeSynced && (now - state.lastNtpAttempt > 30000 || state.lastNtpAttempt == 0)) {
      initNTP();
    }

    if (state.timeSynced) {
      timeClient->update();
    }

    if (state.timeSynced && state.wifiConnected &&
        (now - state.lastWeatherFetch > WEATHER_INTERVAL_MS || state.lastWeatherFetch == 0)) {
      networkBusy = true;
      fetchWeather();
      fetchDaily();
      fetchHourly();
      weatherUpdated = true;
      networkBusy = false;
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        state.lastWeatherFetch = millis();
        xSemaphoreGive(dataMutex);
      }
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void uiTask(void *pvParameters) {
  showBootScreen("ESP32 Mini TV");
  int y = 40;
  showBootLine(y, "Starting...", COLOR_LABEL);
  vTaskDelay(300 / portTICK_PERIOD_MS);

  int loadingFrame = 0;
  int lastUiSec = -1;
  int infoLastSec = -1;
  int lastBtnState = HIGH;
  bool uiReady = false;

  while (1) {
    unsigned long now = millis();
    int curBtn = digitalRead(BTN_PIN);

    if (curBtn == LOW && lastBtnState == HIGH) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        state.showingSystemInfo = !state.showingSystemInfo;
        if (state.showingSystemInfo) state.systemInfoDirty = true;
        xSemaphoreGive(dataMutex);
      }
      if (state.showingSystemInfo) {
        drawSystemInfo();
      } else {
        drawFullUI();
      }
    }
    lastBtnState = curBtn;

    if (state.showingSystemInfo) {
      int curSec = -1;
      if (state.timeSynced) {
        time_t t = timeClient->getEpochTime();
        struct tm *ti = localtime(&t);
        curSec = ti->tm_sec;
      } else {
        curSec = (now / 1000) % 60;
      }
      if (curSec != infoLastSec) {
        drawSystemInfo();
        infoLastSec = curSec;
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    if (networkBusy) {
      drawLoadingFrame(loadingFrame);
      loadingFrame = (loadingFrame + 1) % 8;
    }

    if (weatherUpdated) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        weatherUpdated = false;
        xSemaphoreGive(dataMutex);
      }
      if (!uiReady) {
        uiReady = true;
        drawFullUI();
      } else {
        fillArea(PAD_LEFT, DETAIL_Y, CONTENT_W, CHART_Y - DETAIL_Y + CHART_H, COLOR_BG);
        drawWeatherSection();
        drawDetailSection();
        drawHourlyChart();
      }
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

    if (curSec != lastUiSec) {
      lastUiSec = curSec;
      drawClockSection();
      drawStatusHeader();
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Desktop Mini TV (FreeRTOS)");

  state.wifiConnected = false;
  state.timeSynced = false;
  state.ntpTried = false;
  state.weatherLoaded = false;
  state.showingSystemInfo = false;
  state.systemInfoDirty = true;
  state.lastWeatherFetch = 0;
  state.lastNtpAttempt = 0;
  strcpy(state.ntpFailReason, "");
  strcpy(state.ntpServer, "");
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  dataMutex = xSemaphoreCreateMutex();
  networkBusy = false;
  weatherUpdated = false;

  initDisplay();
  pinMode(BTN_PIN, INPUT_PULLUP);

  xTaskCreatePinnedToCore(
    networkTask, "network", 8192, NULL, 1, NULL, 0
  );

  xTaskCreatePinnedToCore(
    uiTask, "ui", 8192, NULL, 2, NULL, 1
  );

  vTaskDelete(NULL);
}

void loop() {}
