#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "ui.h"
#include "page_manager.h"
#include "config_server.h"

static unsigned long lastWifiTry = 0;
static bool configServerStarted = false;

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
  initWiFiWithProvisioning();
  lastWifiTry = millis();

  if (state.wifiConnected) {
    initNTP();
  }

  while (1) {
    unsigned long now = millis();

    if (state.provisioningMode) {
      wifiManager.process();
      if (WiFi.status() == WL_CONNECTED) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          state.provisioningMode = false;
          state.wifiConnected = true;
          xSemaphoreGive(dataMutex);
        }
      } else if (millis() - state.bootTime > 185000) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          state.provisioningMode = false;
          xSemaphoreGive(dataMutex);
        }
        WiFi.mode(WIFI_STA);
        WiFi.begin();
      }
      esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

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

    if (!configServerStarted && state.wifiConnected) {
      startConfigServer();
      configServerStarted = true;
    }

    handleConfigClient();

    processNTP();

    if (!state.timeSynced && (now - state.lastNtpAttempt > 30000 || state.lastNtpAttempt == 0)) {
      initNTP();
    }

    if (!state.locationResolved) {
      resolveLocation();
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
      fetchWeatherWarnings();
      networkBusy = false;
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        weatherUpdated = true;
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
  int lastBtnStateLong = HIGH;
  int lastBtnStateShort = HIGH;
  bool uiReady = false;
  unsigned long lastFullDraw = 0;
  unsigned long btnPressStart = 0;
  bool edgeGlowShown = false;
  bool provScreenDrawn = false;

  while (1) {
    unsigned long now = millis();
    int curBtnLong = digitalRead(BTN_LONG_PIN);

    if (curBtnLong == LOW) {
      if (lastBtnStateLong == HIGH) {
        btnPressStart = now;
        edgeGlowShown = false;
      } else {
        float progress = (float)(now - btnPressStart) / 3000.0f;
        if (progress > 1.0f) progress = 1.0f;
        drawLongPressBar(progress);
      }
      if (!edgeGlowShown) {
        drawEdgeGlow(true);
        edgeGlowShown = true;
      }
      if ((now - btnPressStart) >= 3000) {
        fillArea(PAD_LEFT, 100, CONTENT_W, 20, COLOR_BG);
        tft->setTextColor(COLOR_ACCENT);
        tft->setTextSize(1);
        tft->setCursor(PAD_LEFT, 100);
        tft->print("Resetting WiFi...");
        wifiManager.resetSettings();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        ESP.restart();
      }
    } else if (curBtnLong == HIGH && lastBtnStateLong == LOW) {
      drawEdgeGlow(false);
    }
    lastBtnStateLong = curBtnLong;

    int curBtnShort = digitalRead(BTN_SHORT_PIN);
    if (curBtnShort == HIGH && lastBtnStateShort == LOW) {
      bool wasForceWarn = state.forceWarnActive;
      bool leavingWarning = state.showingWarning;
      pageNext();

      switch (getCurrentPage()) {
        case PAGE_WARNING:
          warning_page::drawWarningPage();
          break;
        case PAGE_MINUTELY:
          minutely_page::drawMinutelyPage();
          if (wasForceWarn || leavingWarning || !minutely.valid ||
              (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS) {
            fetchMinutelyPrecipitation();
            if (getCurrentPage() == PAGE_MINUTELY) {
              minutely_page::drawMinutelyPage();
            }
          }
          break;
        case PAGE_SYSTEM_INFO:
          state.systemInfoDirty = true;
          system_page::drawSystemInfo();
          break;
        case PAGE_MAIN:
          weatherUpdated = false;
          main_page::drawFullUI();
          lastFullDraw = millis();
          break;
      }
    }
    lastBtnStateShort = curBtnShort;

    time_t cachedEpoch = 0;
    struct tm cachedTm;
    if (state.timeSynced) {
      cachedEpoch = timeClient->getEpochTime();
      localtime_r(&cachedEpoch, &cachedTm);
    }

    if (state.provisioningMode) {
      if (!provScreenDrawn) {
        drawProvisioningScreen(state.apName, state.apIP);
        provScreenDrawn = true;
      }
      updateProvisioningFrame(loadingFrame);
      loadingFrame = (loadingFrame + 1) % 16;
      if (WiFi.status() == WL_CONNECTED) {
        tft->fillScreen(COLOR_BG);
        fillArea(0, 0, SCREEN_W, STATUS_H + 4, COLOR_GREEN);
        tft->setTextColor(COLOR_PRIMARY);
        tft->setTextSize(1);
        tft->setCursor((SCREEN_W - 11 * 6) / 2, 5);
        tft->print("WiFi Connected");
        tft->setTextColor(COLOR_LABEL);
        tft->setTextSize(1);
        tft->setCursor(PAD_LEFT, 40);
        tft->print("Connected to:");
        tft->setTextColor(COLOR_PRIMARY);
        tft->setTextSize(2);
        tft->setCursor(PAD_LEFT, 56);
        tft->print(WiFi.SSID());
        tft->setTextColor(COLOR_LABEL);
        tft->setTextSize(1);
        tft->setCursor(PAD_LEFT, 90);
        tft->print("IP:");
        tft->setTextColor(COLOR_PRIMARY);
        tft->setTextSize(2);
        tft->setCursor(PAD_LEFT, 106);
        tft->print(WiFi.localIP().toString().c_str());
        tft->setTextColor(COLOR_ACCENT);
        tft->setTextSize(1);
        tft->setCursor(PAD_LEFT, 150);
        tft->print("Starting up, please wait...");
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (state.showingWarning) {
      if (state.forceWarnActive) {
        unsigned long elapsed = millis() - state.forceWarnStartMs;
        float progress = 1.0f - (float)elapsed / 30000.0f;
        drawWarnProgressBar(progress);

        if (isForceWarningExpired()) {
          advanceOrDismissForceWarnings();
          switch (getCurrentPage()) {
            case PAGE_WARNING:
              warning_page::drawWarningPage();
              break;
            case PAGE_MINUTELY:
              minutely_page::drawMinutelyPage();
              break;
            case PAGE_SYSTEM_INFO:
              state.systemInfoDirty = true;
              system_page::drawSystemInfo();
              break;
            case PAGE_MAIN:
              weatherUpdated = false;
              main_page::drawFullUI();
              lastFullDraw = millis();
              break;
          }
          vTaskDelay(100 / portTICK_PERIOD_MS);
          continue;
        }
      }
      warning_page::updateWarningScroll();
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (state.showingMinutely) {
      if (minutely.valid && (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS) {
        fetchMinutelyPrecipitation();
        minutely_page::drawMinutelyPage();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (state.showingSystemInfo) {
      int curSec;
      if (state.timeSynced) {
        curSec = cachedTm.tm_sec;
      } else {
        curSec = (now / 1000) % 60;
      }
      if (curSec != infoLastSec) {
        system_page::drawSystemInfo();
        infoLastSec = curSec;
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    if (minutely.valid && (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS) {
      fetchMinutelyPrecipitation();
    }

    if (networkBusy) {
      main_page::drawLoadingFrame(loadingFrame);
      loadingFrame = (loadingFrame + 1) % 8;
    }

    if (weatherUpdated && (now - lastFullDraw > 2000)) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        weatherUpdated = false;
        xSemaphoreGive(dataMutex);
      }
      uiReady = true;
      main_page::drawFullUI();
      lastFullDraw = millis();
    }

    if (warningCount == 0 && state.forceWarnShownCount > 0) {
      state.forceWarnShownCount = 0;
    }

    if (tryStartForceWarnings()) {
      warning_page::drawWarningPage();
    }

    int curSec, curMin, curHour;
    if (state.timeSynced) {
      curSec = cachedTm.tm_sec;
      curMin = cachedTm.tm_min;
      curHour = cachedTm.tm_hour;
    } else {
      curSec = (now / 1000) % 60;
      curMin = (now / 60000) % 60;
      curHour = (now / 3600000) % 24;
    }

    if (curSec != lastUiSec) {
      lastUiSec = curSec;
      main_page::updateStatusTime();
      main_page::updateClockTime(curHour, curMin, curSec);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  state = AppState{};
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  dataMutex = xSemaphoreCreateMutex();
  networkBusy = false;
  weatherUpdated = false;

  initDisplay();

  pinMode(BTN_LONG_PIN, INPUT_PULLUP);
  pinMode(BTN_SHORT_PIN, INPUT_PULLUP);

  disableCore0WDT();

  xTaskCreatePinnedToCore(
    networkTask, "network", 8192, NULL, 1, NULL, 0
  );

  vTaskDelay(100 / portTICK_PERIOD_MS);

  if (state.provisioningMode) {
    drawProvisioningScreen(state.apName, state.apIP);
  }

  xTaskCreatePinnedToCore(
    uiTask, "ui", 8192, NULL, 2, NULL, 1
  );

  vTaskDelete(NULL);
}

void loop() {}
