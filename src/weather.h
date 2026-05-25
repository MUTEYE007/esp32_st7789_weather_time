#pragma once

#include <Arduino.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

struct WeatherData {
  String temp;
  String feelsLike;
  String humidity;
  String windDir;
  String windScale;
  String weatherText;
  String weatherIcon;
  String updateTime;
  String tempMax;
  String tempMin;
  bool valid;
};

struct HourlyData {
  String hourLabel[HOUR_COUNT];
  String temp[HOUR_COUNT];
  String icon[HOUR_COUNT];
  int tempInt[HOUR_COUNT];
  bool valid;
};

struct AppState {
  bool wifiConnected;
  bool timeSynced;
  bool ntpTried;
  bool weatherLoaded;
  bool locationResolved;
  bool showingSystemInfo;
  bool systemInfoDirty;
  bool provisioningMode;
  bool showingWarning;
  bool showingMinutely;
  bool showingHelp;
  bool dimmingActive;
  bool hasActiveWarnings;
  int warningIndex;
  unsigned long lastWeatherFetch;
  unsigned long lastNtpAttempt;
  unsigned long lastWarningFetch;
  unsigned long lastMinutelyFetch;
  char ntpFailReason[24];
  char ntpServer[32];
  char apName[24];
  char apIP[16];
  unsigned long bootTime;

  // Forced alert popup state
  bool forceWarnActive;
  unsigned long forceWarnStartMs;
  bool savedShowingMinutely;
  bool savedShowingSystemInfo;
  int forceWarnShownCount;

  // Remote control from web
  int8_t remotePage;          // -1 = none, 0-4 = page to navigate to
  int8_t pendingRotation;     // -1 = none, 0-3 = screen rotation
  uint32_t weatherIntervalMs; // weather fetch interval (ms)
};

struct WarningData {
  String eventName;
  String eventCode;
  String severity;
  String headline;
  String description;
  String senderName;
  bool valid;
};

#define WARNING_MAX 5
#define MINUTELY_SLOTS 24

struct MinutelyData {
  String summary;
  struct {
    String fxTime;
    float precip;
  } slots[MINUTELY_SLOTS];
  bool valid;
};

extern WeatherData weather;
extern HourlyData hourly;
extern WarningData warnings[WARNING_MAX];
extern int warningCount;
extern MinutelyData minutely;
extern AppState state;
extern String weatherLoc;
extern String weatherName;
extern String weatherLat;
extern String weatherLon;
extern String weatherApiKey;
extern String weatherHost;
extern NTPClient *timeClient;
extern WiFiManager wifiManager;
extern SemaphoreHandle_t dataMutex;
extern volatile bool networkBusy;
extern volatile bool weatherUpdated;

void initWiFiWithProvisioning();
void loadConfig();
void saveConfig(const String &apiKey, const String &host);
void initNTP();
void processNTP();
bool setCityByName(const String &cityName);
bool resolveLocation();
bool fetchWeather();
void fetchHourly();
void fetchDaily();
void fetchWeatherWarnings();
void fetchMinutelyPrecipitation();

// Format the next update time as "HH:MM" given last fetch ms and interval ms.
// Returns "--:--" if time not synced or fetch hasn't happened yet.
String nextTimeStr(unsigned long lastMs, unsigned long intervalMs);

// Get the color of the most severe active warning.
uint16_t getWarningSeverityColor();
