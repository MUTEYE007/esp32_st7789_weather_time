#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "ui.h"
#include "page_manager.h"
#include "ui_common.h"
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
        wifiManager.stopConfigPortal();
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          state.provisioningMode = false;
          state.wifiConnected = true;
          state.lastWeatherFetch = 0;
          state.lastMinutelyFetch = 0;
          xSemaphoreGive(dataMutex);
        }
        Serial.println("[BOOT] WiFi configured, restarting for clean state...");
        delay(200);
        ESP.restart();
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
        state.lastWeatherFetch = 0;
        state.lastMinutelyFetch = 0;
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

    if (state.wifiConnected &&
        (now - state.lastWeatherFetch > state.weatherIntervalMs || state.lastWeatherFetch == 0)) {
      networkBusy = true;
      bool ok = fetchWeather();
      fetchDaily();
      fetchHourly();
      networkBusy = false;
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (ok) {
          weatherUpdated = true;
          state.lastWeatherFetch = millis();
        }
        xSemaphoreGive(dataMutex);
      }
    }

    // Independent timer for weather warnings (decoupled from weather data fetch)
    if (state.wifiConnected &&
        (now - state.lastWarningFetch > WARN_INTERVAL_MS || state.lastWarningFetch == 0)) {
      networkBusy = true;
      fetchWeatherWarnings();
      networkBusy = false;
    }

    // Periodic remote OTA check — every 24 hours, start 5 min after boot
    static unsigned long lastOtaCheck = 0;
    unsigned long otaInterval = 86400000UL; // 24h
    if (lastOtaCheck == 0) {
      if (now > 300000) lastOtaCheck = now - otaInterval + 300000; // first check at 5min
      else lastOtaCheck = now;
    }
    if (now - lastOtaCheck > otaInterval) {
      lastOtaCheck = now;
      periodicCheckUpdate();
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void uiTask(void *pvParameters) {
  showBootScreen("ESP32 Mini TV");
  int y = 40;
  showBootLine(y, "Starting...", COLOR_LABEL);
  vTaskDelay(300 / portTICK_PERIOD_MS);

  const unsigned long LONG_PRESS_MS = 500;
  const int RAMP_INTERVAL_MS = 40;
  const uint8_t BRIGHTNESS_MIN = 1;
  const uint8_t BRIGHTNESS_MAX = 255;
  const uint8_t RAMP_STEP = 3;

  int loadingFrame = 0;
  int lastUiSec = -1;
  int infoLastSec = -1;
  int lastBtnStateLong = HIGH;
  int lastBtnStateShort = LOW;
  bool uiReady = false;
  unsigned long lastFullDraw = 0;
  unsigned long btnPressStart = 0;
  bool edgeGlowShown = false;
  bool provScreenDrawn = false;
  unsigned long shortBtnPressStart = 0;
  bool rampActive = false;
  int8_t rampDir = -1;
  unsigned long lastRampTick = 0;
  bool nextDirDown = true;

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
        tft->print("Resetting all...");
        {
          Preferences prefs;
          prefs.begin("display", false);
          prefs.clear();
          prefs.end();
        }
        {
          Preferences prefs;
          prefs.begin("weather", false);
          prefs.clear();
          prefs.end();
        }
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
      shortBtnPressStart = now;
      Serial.println("[BTN] IO13 pressed");
    }

    if (curBtnShort == HIGH) {
      if (!rampActive && (now - shortBtnPressStart >= LONG_PRESS_MS)) {
        if (g_brightness <= BRIGHTNESS_MIN && nextDirDown) {
          rampDir = 1;
          nextDirDown = false;
        } else if (g_brightness >= BRIGHTNESS_MAX && !nextDirDown) {
          rampDir = -1;
          nextDirDown = true;
        } else {
          rampDir = nextDirDown ? -1 : 1;
        }
        rampActive = true;
        lastRampTick = now;
        enterBrightnessPage();
        brightness_page::drawBrightnessPage();
        Serial.printf("[BTN] Long press: ramp %s\n", rampDir < 0 ? "DOWN" : "UP");
      }
      if (rampActive && (now - lastRampTick >= RAMP_INTERVAL_MS)) {
        lastRampTick = now;
        int newB = g_brightness + rampDir * RAMP_STEP;
        if (newB < BRIGHTNESS_MIN) newB = BRIGHTNESS_MIN;
        if (newB > BRIGHTNESS_MAX) newB = BRIGHTNESS_MAX;
        setBrightness((uint8_t)newB);
        brightness_page::updateBrightnessBar(g_brightness, rampDir);
      }
    } else if (curBtnShort == LOW && lastBtnStateShort == HIGH) {
      if (rampActive) {
        rampActive = false;
        nextDirDown = !nextDirDown;
        Serial.printf("[BTN] Released: brightness=%d, next dir %s\n",
          g_brightness, nextDirDown ? "DOWN" : "UP");
      } else {
        unsigned long heldMs = now - shortBtnPressStart;
        Serial.printf("[BTN] Short press: held=%lums\n", heldMs);
        if (state.dimmingActive) {
          exitBrightnessPage();
          PageId cur = getCurrentPage();
          switch (cur) {
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
            case PAGE_HELP:
              help_page::drawHelpPage();
              break;
            default:
              weatherUpdated = false;
              main_page::drawFullUI();
              lastFullDraw = millis();
              break;
          }
        } else {
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
            case PAGE_HELP:
              help_page::drawHelpPage();
              break;
            case PAGE_MAIN:
              weatherUpdated = false;
              main_page::drawFullUI();
              lastFullDraw = millis();
              break;
          }
        }
      }
    }
    lastBtnStateShort = curBtnShort;

    if (state.remotePage >= 0) {
      int8_t rp = state.remotePage;
      state.remotePage = -1;
      PageId target = PAGE_MAIN;
      switch (rp) {
        case 0: target = PAGE_MAIN; break;
        case 1: target = PAGE_WARNING; break;
        case 2: target = PAGE_MINUTELY; break;
        case 3: target = PAGE_SYSTEM_INFO; break;
        case 4: target = PAGE_HELP; break;
      }
      setCurrentPage(target);
      switch (target) {
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
        case PAGE_HELP:
          help_page::drawHelpPage();
          break;
        default:
          weatherUpdated = false;
          main_page::drawFullUI();
          lastFullDraw = millis();
          break;
      }
    }

    if (state.pendingRotation >= 0) {
      tft->setRotation(state.pendingRotation);
      state.pendingRotation = -1;
      PageId cur = getCurrentPage();
      if (cur == PAGE_SYSTEM_INFO) { state.systemInfoDirty = true; system_page::drawSystemInfo(); }
      else if (cur == PAGE_WARNING) warning_page::drawWarningPage();
      else if (cur == PAGE_MINUTELY) minutely_page::drawMinutelyPage();
      else if (cur == PAGE_HELP) help_page::drawHelpPage();
      else { weatherUpdated = false; main_page::drawFullUI(); lastFullDraw = millis(); }
    }

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
            case PAGE_HELP:
               help_page::drawHelpPage();
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

    if (state.showingHelp) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    if (state.dimmingActive) {
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

    if (!state.dimmingActive && tryStartForceWarnings()) {
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

    // Flash status bar inset block (between W/A text and WiFi) when warnings active
    static unsigned long lastWarnFlash = 0;
    static bool warnFlashOn = false;
    uint16_t warnColor = getWarningSeverityColor();
    if (warnColor != COLOR_BG) {
      int nameEndX = PAD_LEFT + textWidth16(weatherName.c_str());
      // textSize=1: " 14:30 W15:00 A15:10" ≈ 20 chars × ~6px
      int textEndX = nameEndX + 20 * 6;
      int wifiStartX = SCREEN_W - PAD_RIGHT - 20;
      static const int FLASH_MARGIN = 6; // px inset on each side
      int flashX = textEndX + FLASH_MARGIN;
      int flashW = (wifiStartX - textEndX) - FLASH_MARGIN * 2;
      if (flashW > 0 && (now - lastWarnFlash >= 500)) {
        lastWarnFlash = now;
        warnFlashOn = !warnFlashOn;
        uint16_t bg = warnFlashOn ? warnColor : COLOR_BG;
        fillArea(flashX, STATUS_Y, flashW, STATUS_H, bg);

        // Show first 2 Chinese chars of current warning name in the block
        int wi = state.warningIndex;
        if (wi >= 0 && wi < warningCount) {
          String name = warnings[wi].eventName;
          String twoChars = name.substring(0, min(6, (int)name.length()));
          if (twoChars.length() > 0) {
            int tw = textWidth16(twoChars.c_str());
            int tx = flashX + (flashW - tw) / 2;
            drawGB16(tx, STATUS_Y, twoChars.c_str(),
                     warnFlashOn ? COLOR_WHITE : COLOR_BG, bg);
          }
        }
      }
    } else if (warnFlashOn) {
      warnFlashOn = false;
      int nameEndX = PAD_LEFT + textWidth16(weatherName.c_str());
      int textEndX = nameEndX + 20 * 6;
      int wifiStartX = SCREEN_W - PAD_RIGHT - 20;
      static const int FLASH_MARGIN = 6;
      int flashX = textEndX + FLASH_MARGIN;
      int flashW = (wifiStartX - textEndX) - FLASH_MARGIN * 2;
      if (flashW > 0) {
        fillArea(flashX, STATUS_Y, flashW, STATUS_H, COLOR_BG);
      }
    }

    // Full-screen OTA progress overlay (remote OTA)
    if (otaPhase >= 0) {
      static int lastPh = -2, lastPct = -1;
      static unsigned long errStart = 0;
      if (otaPhase != lastPh || otaPercent != lastPct) {
        lastPh = otaPhase; lastPct = otaPercent;
        // Full clear
        fillArea(0, 0, SCREEN_W, SCREEN_H, COLOR_BG);
        fillArea(0, 0, SCREEN_W, 18, COLOR_ACCENT);
        drawGB16(8, 1, "固件升级", COLOR_BG, COLOR_ACCENT);

        const char *msg = "";
        switch (otaPhase) {
          case 0: msg = "正在检查更新..."; break;
          case 1: msg = "正在下载固件..."; break;
          case 2: msg = "升级成功，重启中..."; break;
          case 3: msg = "升级失败"; break;
        }
        drawGB16(8, 36, msg, COLOR_PRIMARY, COLOR_BG);

        if (otaPhase == 1) {
          int fw = map(otaPercent, 0, 100, 0, SCREEN_W - 40);
          fillArea(20, 60, SCREEN_W - 40, 14, COLOR_BG_ALT);
          fillArea(20, 60, fw, 14, COLOR_GREEN);
          char buf[8]; sprintf(buf, "%d%%", otaPercent);
          drawGB16((SCREEN_W - (int)strlen(buf) * 16) / 2, 62, buf, COLOR_BG, COLOR_GREEN);
        }
        if (otaPhase == 3) {
          drawGB16(8, 56, "请检查服务器地址和网络", COLOR_MUTED, COLOR_BG);
          errStart = millis();
        }
        if (otaPhase == 2) {
          drawGB16(8, 56, "设备即将重启...", COLOR_MUTED, COLOR_BG);
        }
      }
      // Auto-clear error after 5s
      if (otaPhase == 3 && millis() - errStart > 5000) {
        otaPhase = -1;
        state.systemInfoDirty = true;
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    // OTA progress bar at bottom of screen (browser upload)
    static int lastOtaPct = -2;
    if (otaProgress != lastOtaPct) {
      lastOtaPct = otaProgress;
      if (otaProgress >= 0 && otaProgress <= 100) {
        int bw = map(otaProgress, 0, 100, 0, SCREEN_W);
        fillArea(0, SCREEN_H - 4, SCREEN_W, 4, COLOR_BG);
        fillArea(0, SCREEN_H - 4, bw, 4, COLOR_GREEN);
      } else if (otaProgress < 0 && lastOtaPct != -2) {
        fillArea(0, SCREEN_H - 4, SCREEN_W, 4, COLOR_BG);
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  state = AppState{};
  state.pendingRotation = -1;
  state.weatherIntervalMs = WEATHER_INTERVAL_MS;
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  dataMutex = xSemaphoreCreateMutex();
  networkBusy = false;
  weatherUpdated = false;

  initDisplay();

  pinMode(BTN_LONG_PIN, INPUT_PULLUP);
  pinMode(BTN_SHORT_PIN, INPUT);

  disableCore0WDT();

  xTaskCreatePinnedToCore(
    networkTask, "network", 65536, NULL, 1, NULL, 0
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
