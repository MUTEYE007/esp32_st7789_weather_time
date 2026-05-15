#pragma once

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
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
  bool showingSystemInfo;
  unsigned long lastWeatherFetch;
  unsigned long lastNtpAttempt;
  char ntpFailReason[24];
  char ntpServer[32];
  int lastSecond;
  int lastMinute;
  int lastBtnState;
  unsigned long bootTime;
};

extern WeatherData weather;
extern HourlyData hourly;
extern AppState state;
extern NTPClient *timeClient;

void initWiFi();
void initNTP();
bool fetchWeather();
void fetchHourly();
void fetchDaily();
const char* iconToCN(int code);
