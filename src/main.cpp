#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "ui.h"
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
  unsigned long lastFullDraw = 0;
  unsigned long btnPressStart = 0;
  int lastBtnDots = -1;
  bool provScreenDrawn = false;

  while (1) {
    unsigned long now = millis();
    int curBtn = digitalRead(BTN_PIN);

    if (curBtn == LOW) {
      if (lastBtnState == HIGH) {
        btnPressStart = now;
        lastBtnDots = -1;
      } else {
        int dots = (now - btnPressStart) * 12 / 3000;
        if (dots > 12) dots = 12;
        if (dots != lastBtnDots) {
          lastBtnDots = dots;
          drawLongPressRing(SCREEN_W / 2, SCREEN_H - 22, dots / 12.0f);
        }
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
    } else if (curBtn == HIGH && lastBtnState == LOW) {
      if ((now - btnPressStart) < 3000) {
        fillArea(SCREEN_W / 2 - 16, SCREEN_H - 36, 32, 32, COLOR_BG);
      }
      if (state.showingWarning) {
        if (warningCount > 1 && state.warningIndex < warningCount - 1) {
          state.warningIndex++;
          drawWarningPage();
        } else {
          state.showingWarning = false;
          state.showingMinutely = true;
          bool needsFetch = !minutely.valid || (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS;
          drawMinutelyPage();
          if (needsFetch) {
            fetchMinutelyPrecipitation();
            if (state.showingMinutely) {
              drawMinutelyPage();
            }
          }
        }
      } else if (state.showingMinutely) {
        state.showingMinutely = false;
        state.showingSystemInfo = true;
        state.systemInfoDirty = true;
        drawSystemInfo();
      } else if (state.showingSystemInfo) {
        state.showingSystemInfo = false;
        weatherUpdated = false;
        drawFullUI();
        lastFullDraw = millis();
      } else {
        state.warningIndex = 0;
        state.showingWarning = true;
        drawWarningPage();
      }
    }
    lastBtnState = curBtn;

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
      updateWarningScroll();
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (state.showingMinutely) {
      if (minutely.valid && (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS) {
        fetchMinutelyPrecipitation();
        drawMinutelyPage();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

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

    if (minutely.valid && (millis() - state.lastMinutelyFetch) > MINUTELY_INTERVAL_MS) {
      fetchMinutelyPrecipitation();
    }

    if (networkBusy) {
      drawLoadingFrame(loadingFrame);
      loadingFrame = (loadingFrame + 1) % 8;
    }

    if (weatherUpdated && (now - lastFullDraw > 2000)) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        weatherUpdated = false;
        xSemaphoreGive(dataMutex);
      }
      uiReady = true;
      drawFullUI();
      lastFullDraw = millis();
    }

    int curSec = -1, curMin = -1, curHour = -1;
    if (state.timeSynced) {
      time_t t = timeClient->getEpochTime();
      struct tm *ti = localtime(&t);
      curSec = ti->tm_sec;
      curMin = ti->tm_min;
      curHour = ti->tm_hour;
    } else {
      curSec = (now / 1000) % 60;
      curMin = (now / 60000) % 60;
      curHour = (now / 3600000) % 24;
    }

    if (curSec != lastUiSec) {
      lastUiSec = curSec;
      updateStatusTime();
      updateClockTime(curHour, curMin, curSec);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  state.wifiConnected = false;
  state.timeSynced = false;
  state.ntpTried = false;
  state.weatherLoaded = false;
  state.locationResolved = false;
  state.showingSystemInfo = false;
  state.systemInfoDirty = true;
  state.showingWarning = false;
  state.showingMinutely = false;
  state.hasActiveWarnings = false;
  state.warningIndex = 0;
  state.provisioningMode = false;
  state.lastWeatherFetch = 0;
  state.lastNtpAttempt = 0;
  state.lastWarningFetch = 0;
  state.lastMinutelyFetch = 0;
  strcpy(state.ntpFailReason, "");
  strcpy(state.ntpServer, "");
  memset(state.apName, 0, sizeof(state.apName));
  memset(state.apIP, 0, sizeof(state.apIP));
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  dataMutex = xSemaphoreCreateMutex();
  networkBusy = false;
  weatherUpdated = false;

  initDisplay();

  pinMode(BTN_PIN, INPUT_PULLUP);

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
