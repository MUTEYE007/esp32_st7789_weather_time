#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp32/rom/miniz.h>

WeatherData weather;
HourlyData hourly;
WarningData warnings[WARNING_MAX];
int warningCount = 0;
MinutelyData minutely;
AppState state;
String weatherLoc;
String weatherName;
String weatherLat;
String weatherLon;
String weatherApiKey;
String weatherHost;
WiFiUDP ntpUDP;
NTPClient *timeClient = nullptr;
SemaphoreHandle_t dataMutex = NULL;
volatile bool networkBusy = false;
volatile bool weatherUpdated = false;

static String httpGetJson(const String &url, bool withApiKey);
WiFiManager wifiManager;
Preferences preferences;

void loadConfig() {
  preferences.begin("weather", false);
  weatherApiKey = preferences.getString("apiKey", WEATHER_API_KEY);
  weatherHost   = preferences.getString("host", WEATHER_HOST);
  preferences.end();
}

void saveConfig(const String &apiKey, const String &host) {
  preferences.begin("weather", false);
  preferences.putString("apiKey", apiKey);
  preferences.putString("host", host);
  preferences.end();
  weatherApiKey = apiKey;
  weatherHost = host;
}

void initWiFiWithProvisioning() {
  loadConfig();
  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  char apName[24];
  snprintf(apName, sizeof(apName), "ESP32-Weather-%04X", chipId);

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(10);
  wifiManager.setConnectRetries(2);

  bool connected = wifiManager.autoConnect(apName);

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    strcpy(state.apName, apName);
    if (connected) {
      state.wifiConnected = true;
      state.provisioningMode = false;
      strcpy(state.apIP, WiFi.localIP().toString().c_str());
    } else {
      state.provisioningMode = true;
      strcpy(state.apIP, WiFi.softAPIP().toString().c_str());
    }
    xSemaphoreGive(dataMutex);
  }
}

void fetchMinutelyPrecipitation() {
  if (!state.wifiConnected || weatherLon.length() == 0 || weatherLat.length() == 0) return;

  String url = "https://" + weatherHost + "/v7/minutely/5m?location="
               + weatherLon + "," + weatherLat;
  String payload = httpGetJson(url, true);
  if (payload.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return;

  JsonArray minutelyArr = doc["minutely"].as<JsonArray>();

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    minutely.summary = doc["summary"].as<String>();
    minutely.valid = false;
    for (int i = 0; i < MINUTELY_SLOTS && i < (int)minutelyArr.size(); i++) {
      minutely.slots[i].fxTime = minutelyArr[i]["fxTime"].as<String>();
      minutely.slots[i].precip = minutelyArr[i]["precip"].as<float>();
    }
    if (minutelyArr.size() > 0) minutely.valid = true;
    state.lastMinutelyFetch = millis();
    xSemaphoreGive(dataMutex);
  }
}

// ---- Async NTP state machine ----
static const char *ntpServers[] = {
  "ntp.ntsc.ac.cn",
  "ntp1.aliyun.com", "ntp2.aliyun.com", "ntp3.aliyun.com",
  "ntp4.aliyun.com", "ntp5.aliyun.com", "ntp6.aliyun.com", "ntp7.aliyun.com",
  "ntp.tencent.com", "ntp1.tencent.com", "ntp2.tencent.com",
  "ntp3.tencent.com", "ntp4.tencent.com", "ntp5.tencent.com",
  "pool.ntp.org"
};
#define NTP_SERVER_COUNT 15
#define NTP_PER_SERVER_MS 3000

static int ntpServerIdx = -1;
static const char *ntpCurServer = NULL;
static unsigned long ntpSendMs = 0;

static void advanceNtpServer() {
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    if (state.timeSynced) { xSemaphoreGive(dataMutex); return; }
    xSemaphoreGive(dataMutex);
  }

  ntpServerIdx++;
  if (ntpServerIdx >= NTP_SERVER_COUNT) {
    ntpServerIdx = -1;
    ntpCurServer = NULL;
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      strcpy(state.ntpFailReason, "all servers failed");
      state.ntpTried = true;
      xSemaphoreGive(dataMutex);
    }
    return;
  }

  ntpCurServer = ntpServers[ntpServerIdx];
  IPAddress ip;
  if (!WiFi.hostByName(ntpCurServer, ip)) {
    advanceNtpServer();
    return;
  }

  delete timeClient;
  timeClient = new NTPClient(ntpUDP, ntpCurServer, 28800, 60000);
  timeClient->begin();
  ntpSendMs = millis();
}

void initNTP() {
  if (!state.wifiConnected) return;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    if (state.timeSynced) { xSemaphoreGive(dataMutex); return; }
    state.lastNtpAttempt = millis();
    xSemaphoreGive(dataMutex);
  }
  ntpServerIdx = -1;
  ntpCurServer = NULL;
  advanceNtpServer();
}

void processNTP() {
  if (ntpServerIdx < 0) return;

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    if (state.timeSynced) {
      xSemaphoreGive(dataMutex);
      ntpServerIdx = -1;
      ntpCurServer = NULL;
      return;
    }
    xSemaphoreGive(dataMutex);
  }

  if (timeClient->update()) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      state.timeSynced = true;
      strcpy(state.ntpServer, ntpCurServer);
      xSemaphoreGive(dataMutex);
    }
    ntpServerIdx = -1;
    ntpCurServer = NULL;
    return;
  }

  if (millis() - ntpSendMs > NTP_PER_SERVER_MS) {
    advanceNtpServer();
  }
}

static String urlEncode(String str) {
  String encoded;
  encoded.reserve(str.length() * 3);
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

static const uint8_t *skipGzipHeader(const uint8_t *data, size_t len, size_t *deflateLen) {
  if (len < 10 || data[0] != 0x1f || data[1] != 0x8b) {
    *deflateLen = 0;
    return NULL;
  }
  size_t offset = 10;
  uint8_t flags = data[3];
  if (flags & 4) {
    if (offset + 2 > len) { *deflateLen = 0; return NULL; }
    uint16_t xlen = data[offset] | (data[offset + 1] << 8);
    offset += 2 + xlen;
  }
  if (flags & 8) {
    while (offset < len && data[offset]) offset++;
    offset++;
  }
  if (flags & 16) {
    while (offset < len && data[offset]) offset++;
    offset++;
  }
  if (flags & 2) offset += 2;
  if (offset > len) { *deflateLen = 0; return NULL; }
  *deflateLen = len - offset;
  return data + offset;
}

static String httpGetJson(const String &url, bool withApiKey) {
  HTTPClient http;
  http.begin(url);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
  http.addHeader("Accept", "application/json, */*");
  http.addHeader("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8");
  http.addHeader("Connection", "close");
  if (withApiKey) {
    http.addHeader("X-QW-Api-Key", weatherApiKey);
  }
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return "";
  }

  int totalLen = http.getSize();
  if (totalLen <= 0) totalLen = 4096;
  if (totalLen > 65536) totalLen = 65536;

  WiFiClient *stream = http.getStreamPtr();
  uint8_t *buf = (uint8_t *)malloc(totalLen + 4);
  if (!buf) { http.end(); return ""; }

  int readLen = 0;
  unsigned long timeout = millis() + 5000;
  while ((stream->available() || readLen == 0) && millis() < timeout) {
    while (stream->available() && readLen < totalLen) {
      buf[readLen++] = stream->read();
      timeout = millis() + 5000;
    }
    if (!stream->available()) delay(10);
  }
  buf[readLen] = 0;
  http.end();

  size_t deflateLen = 0;
  const uint8_t *deflateData = skipGzipHeader(buf, readLen, &deflateLen);
  if (!deflateData || deflateLen == 0) {
    String payload = String((char *)buf);
    free(buf);
    return payload;
  }

  tinfl_decompressor *decomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!decomp) { free(buf); return ""; }
  tinfl_init(decomp);

  size_t outLen = 16384;
  uint8_t *decompressed = (uint8_t *)malloc(outLen);
  if (!decompressed) { free(decomp); free(buf); return ""; }

  size_t inPos = 0, outPos = 0;
  int status;
  do {
    size_t inBytes = deflateLen - inPos;
    size_t outBytes = outLen - outPos;
    int flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    if (inPos + inBytes < deflateLen) flags |= TINFL_FLAG_HAS_MORE_INPUT;
    status = tinfl_decompress(decomp, deflateData + inPos, &inBytes,
               decompressed, decompressed + outPos, &outBytes, flags);
    inPos += inBytes;
    outPos += outBytes;
    if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
      outLen *= 2;
      uint8_t *newBuf = (uint8_t *)realloc(decompressed, outLen);
      if (!newBuf) { free(decomp); free(decompressed); free(buf); return ""; }
      decompressed = newBuf;
    }
  } while (status != TINFL_STATUS_DONE && status > 0);

  free(decomp);
  free(buf);

  if (status != TINFL_STATUS_DONE) {
    free(decompressed);
    return "";
  }

  decompressed[outPos] = 0;
  String payload = String((char *)decompressed);
  free(decompressed);
  return payload;
}

static bool lookupLocationId(const String &searchCity) {
  String geoUrl = "https://" + weatherHost + "/geo/v2/city/lookup?location="
                  + urlEncode(searchCity);
  String geoPayload = httpGetJson(geoUrl, true);
  if (geoPayload.length() == 0) return false;

  JsonDocument geoDoc;
  if (deserializeJson(geoDoc, geoPayload)) return false;

  const char *geoCode = geoDoc["code"];
  if (!geoCode || strcmp(geoCode, "200") != 0) return false;

  JsonObject firstLoc = geoDoc["location"][0];
  if (firstLoc.isNull()) return false;

  String locId = firstLoc["id"].as<String>();
  weatherLat = firstLoc["lat"].as<String>();
  weatherLon = firstLoc["lon"].as<String>();

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    weatherLoc = locId;
    weatherName = searchCity;
    state.locationResolved = true;
    xSemaphoreGive(dataMutex);
  }
  return true;
}

bool setCityByName(const String &cityName) {
  if (!state.wifiConnected || cityName.length() == 0) return false;
  if (lookupLocationId(cityName)) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      state.lastWeatherFetch = 0;
      minutely.valid = false;
      state.lastMinutelyFetch = 0;
      xSemaphoreGive(dataMutex);
    }
    return true;
  }
  return false;
}

static void fallbackDefault(const String &cityName) {
  weatherLoc = String(WEATHER_LOC);
  weatherName = cityName.length() > 0 ? cityName : String(WEATHER_NAME);
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    state.locationResolved = true;
    minutely.valid = false;
    state.lastMinutelyFetch = 0;
    xSemaphoreGive(dataMutex);
  }
}

bool resolveLocation() {
  if (!state.wifiConnected) return false;

  String searchCity;

  // --- ipip.net (primary) ---
  String ipPayload = httpGetJson("https://myip.ipip.net/json", false);
  if (ipPayload.length() > 0) {
    JsonDocument ipDoc;
    if (!deserializeJson(ipDoc, ipPayload)) {
      const char *ret = ipDoc["ret"] | "";
      if (strcmp(ret, "ok") == 0) {
        JsonArray locArr = ipDoc["data"]["location"].as<JsonArray>();
        String city = locArr.size() > 2 ? locArr[2].as<String>() : "";
        String prov = locArr.size() > 1 ? locArr[1].as<String>() : "";
        searchCity = city.length() > 0 ? city : prov;
      }
    }
  }

  // --- B站 API (fallback 1) ---
  if (searchCity.length() == 0) {
    String biliPayload = httpGetJson("https://api.live.bilibili.com/xlive/web-room/v1/index/getIpInfo", false);
    if (biliPayload.length() > 0) {
      JsonDocument biliDoc;
      if (!deserializeJson(biliDoc, biliPayload)) {
        int code = biliDoc["code"] | -1;
        if (code == 0) {
          String city = biliDoc["data"]["city"].as<String>();
          String prov = biliDoc["data"]["province"].as<String>();
          searchCity = city.length() > 0 ? city : prov;
        }
      }
    }
  }

  // --- 乐视 API (fallback 2) ---
  if (searchCity.length() == 0) {
    String letvPayload = httpGetJson("https://g3.letv.com/r?format=1", false);
    if (letvPayload.length() > 0) {
      JsonDocument letvDoc;
      if (!deserializeJson(letvDoc, letvPayload)) {
        String desc = letvDoc["desc"].as<String>();
        if (desc.length() > 0) {
          int seg = 0, start = 0;
          String parts[4];
          for (int i = 0; i <= desc.length() && seg < 4; i++) {
            if (i == desc.length() || desc.charAt(i) == '-') {
              parts[seg++] = desc.substring(start, i);
              start = i + 1;
            }
          }
          String city = seg > 2 ? parts[2] : "";
          String prov = seg > 1 ? parts[1] : "";
          if (city.length() > 0 && city != "未知") {
            searchCity = city;
          } else if (prov.length() > 0) {
            searchCity = prov;
          }
        }
      }
    }
  }

  // --- all sources failed ---
  if (searchCity.length() == 0) {
    fallbackDefault("");
    return false;
  }

  if (lookupLocationId(searchCity)) return true;

  fallbackDefault(searchCity);
  return false;
}

static bool fetchWeatherDoc(const char *path, JsonDocument &doc) {
  if (!state.wifiConnected) return false;
  String url = "https://" + weatherHost + path + "?location=" + urlEncode(weatherLoc);
  String payload = httpGetJson(url, true);
  if (payload.length() == 0) return false;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return false;
  const char *code = doc["code"];
  if (!code || strcmp(code, "200") != 0) return false;
  return true;
}

bool fetchWeather() {
  JsonDocument doc;
  if (!fetchWeatherDoc("/v7/weather/now", doc)) return false;

  JsonObject now = doc["now"];
  if (now.isNull()) return false;

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    weather.temp        = now["temp"].as<String>();
    weather.feelsLike   = now["feelsLike"].as<String>();
    weather.humidity    = now["humidity"].as<String>();
    weather.windDir     = now["windDir"].as<String>();
    weather.windScale   = now["windScale"].as<String>();
    weather.weatherText = now["text"].as<String>();
    if (weather.weatherText.length() > 21) {
      weather.weatherText = weather.weatherText.substring(0, 21);
    }
    weather.weatherIcon = now["icon"].as<String>();
    weather.updateTime  = doc["updateTime"].as<String>();
    weather.valid       = true;
    state.weatherLoaded = true;
    xSemaphoreGive(dataMutex);
  }
  return true;
}

void fetchHourly() {
  JsonDocument doc;
  if (!fetchWeatherDoc("/v7/weather/24h", doc)) { hourly.valid = false; return; }

  JsonArray arr = doc["hourly"].as<JsonArray>();
  int count = min((int)arr.size(), HOUR_COUNT);

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    for (int i = 0; i < count; i++) {
      JsonObject h = arr[i];
      String ft = h["fxTime"].as<String>();
      if (ft.length() >= 16) hourly.hourLabel[i] = ft.substring(11, 13) + "h";
      else hourly.hourLabel[i] = "--";
      hourly.temp[i] = h["temp"].as<String>();
      hourly.icon[i] = h["icon"].as<String>();
      hourly.tempInt[i] = hourly.temp[i].toInt();
    }
    hourly.valid = (count > 0);
    xSemaphoreGive(dataMutex);
  }
}

void fetchDaily() {
  JsonDocument doc;
  if (!fetchWeatherDoc("/v7/weather/3d", doc)) return;

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    weather.tempMax = doc["daily"][0]["tempMax"].as<String>();
    weather.tempMin = doc["daily"][0]["tempMin"].as<String>();
    xSemaphoreGive(dataMutex);
  }
}

void fetchWeatherWarnings() {
  if (!state.wifiConnected || weatherLat.length() == 0 || weatherLon.length() == 0) return;

  String url = "https://" + weatherHost + "/weatheralert/v1/current/"
               + weatherLat + "/" + weatherLon + "?localTime=true";
  String payload = httpGetJson(url, true);

  if (payload.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return;

  JsonArray alerts = doc["alerts"].as<JsonArray>();

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    warningCount = 0;
    for (int i = 0; i < WARNING_MAX && i < (int)alerts.size(); i++) {
      JsonObject a = alerts[i];
      warnings[i].eventName = a["eventType"]["name"].as<String>();
      warnings[i].eventCode = a["eventType"]["code"].as<String>();
      warnings[i].severity = a["severity"].as<String>();
      warnings[i].headline = a["headline"].as<String>();
      warnings[i].description = a["description"].as<String>();
      warnings[i].senderName = a["senderName"].as<String>();
      warnings[i].valid = true;
      warningCount++;
    }
    if (warningCount > 0 && state.warningIndex >= warningCount) {
      state.warningIndex = 0;
    }
    state.hasActiveWarnings = (warningCount > 0);
    state.lastWarningFetch = millis();
    xSemaphoreGive(dataMutex);
  }
}

String nextTimeStr(unsigned long lastMs, unsigned long intervalMs) {
  if (!state.timeSynced || lastMs == 0) return "--:--";
  unsigned long now = millis();
  unsigned long nextMs = lastMs + intervalMs;
  if (nextMs <= now) return "--:--";
  unsigned long remainingSec = (nextMs - now) / 1000;
  if (remainingSec > 24 * 3600) return "--:--"; // sanity
  time_t t = timeClient->getEpochTime() + remainingSec;
  struct tm *ti = localtime(&t);
  char buf[6];
  sprintf(buf, "%02d:%02d", ti->tm_hour, ti->tm_min);
  return String(buf);
}

uint16_t getWarningSeverityColor() {
  if (warningCount == 0 || !state.hasActiveWarnings) return COLOR_BG;

  int highest = -1;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    for (int i = 0; i < warningCount && i < WARNING_MAX; i++) {
      if (!warnings[i].valid) continue;
      const String &s = warnings[i].severity;
      int level = 3;
      if (s == "extreme")   level = 0;
      else if (s == "severe")   level = 1;
      else if (s == "moderate") level = 2;
      if (level < highest || highest < 0) highest = level;
    }
    xSemaphoreGive(dataMutex);
  }
  switch (highest) {
    case 0:  return WARN_COLOR_RED;
    case 1:  return WARN_COLOR_ORANGE;
    case 2:  return WARN_COLOR_YELLOW;
    case 3:  return WARN_COLOR_BLUE;
    default: return COLOR_BG;
  }
}
