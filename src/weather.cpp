#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp32/rom/miniz.h>

WeatherData weather;
HourlyData hourly;
AppState state;
WiFiUDP ntpUDP;
NTPClient *timeClient = nullptr;

void initWiFi() {
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setAutoReconnect(true);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(300);
    attempts++;
  }
  state.wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void initNTP() {
  if (!state.wifiConnected) return;

  if (timeClient == nullptr) {
    timeClient = new NTPClient(ntpUDP, "pool.ntp.org", 28800, 3600000);
  }

  state.timeSynced = false;
  for (int i = 0; i < 10 && !state.timeSynced; i++) {
    if (timeClient->forceUpdate()) {
      state.timeSynced = true;
      break;
    }
    delay(500);
  }
  state.ntpTried = true;
}

static String urlEncode(String str) {
  String encoded = "";
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

static String httpGetDecompressed(const String &url) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("X-QW-Api-Key", String(WEATHER_API_KEY));
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return "";
  }

  int totalLen = http.getSize();
  if (totalLen <= 0) totalLen = 4096;

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
  if (!deflateData) { free(buf); return ""; }

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

bool fetchWeather() {
  if (!state.wifiConnected) return false;

  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/now?location="
               + urlEncode(String(WEATHER_LOC));
  String payload = httpGetDecompressed(url);
  if (payload.length() == 0) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return false;

  const char *code = doc["code"];
  if (strcmp(code, "200") != 0) return false;

  JsonObject now = doc["now"];
  if (now.isNull()) return false;

  weather.temp        = now["temp"].as<String>();
  weather.feelsLike   = now["feelsLike"].as<String>();
  weather.humidity    = now["humidity"].as<String>();
  weather.windDir     = now["windDir"].as<String>();
  weather.windScale   = now["windScale"].as<String>();
  weather.weatherText = now["text"].as<String>();
  weather.weatherIcon = now["icon"].as<String>();
  weather.updateTime  = doc["updateTime"].as<String>();
  weather.valid       = true;
  state.weatherLoaded = true;
  return true;
}

void fetchHourly() {
  if (!state.wifiConnected) return;

  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/24h?location="
               + urlEncode(String(WEATHER_LOC));
  String payload = httpGetDecompressed(url);
  if (payload.length() == 0) { hourly.valid = false; return; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) { hourly.valid = false; return; }
  if (strcmp(doc["code"], "200") != 0) { hourly.valid = false; return; }

  JsonArray arr = doc["hourly"].as<JsonArray>();
  int count = min((int)arr.size(), HOUR_COUNT);

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
}

void fetchDaily() {
  if (!state.wifiConnected) return;

  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/3d?location="
               + urlEncode(String(WEATHER_LOC));
  String payload = httpGetDecompressed(url);
  if (payload.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return;
  if (strcmp(doc["code"], "200") != 0) return;

  weather.tempMax = doc["daily"][0]["tempMax"].as<String>();
  weather.tempMin = doc["daily"][0]["tempMin"].as<String>();
}

const char* iconToCN(int code) {
  if (code == 100) return "晴";
  if (code == 101) return "多云";
  if (code == 102) return "少云";
  if (code == 103) return "晴间多云";
  if (code == 104) return "阴";
  if (code >= 300 && code <= 399) return "雨";
  if (code >= 400 && code <= 499) return "雪";
  if (code >= 500 && code <= 599) return "雾";
  return "";
}

String windDirToEn(String cn) {
  if (cn.indexOf("旋转") >= 0) return "Rot";
  if (cn.indexOf("无持续") >= 0) return "Calm";
  if (cn.indexOf("北") >= 0 && cn.indexOf("东") >= 0) return "NE";
  if (cn.indexOf("北") >= 0 && cn.indexOf("西") >= 0) return "NW";
  if (cn.indexOf("南") >= 0 && cn.indexOf("东") >= 0) return "SE";
  if (cn.indexOf("南") >= 0 && cn.indexOf("西") >= 0) return "SW";
  if (cn.indexOf("北") >= 0) return "N";
  if (cn.indexOf("南") >= 0) return "S";
  if (cn.indexOf("东") >= 0) return "E";
  if (cn.indexOf("西") >= 0) return "W";
  return "--";
}
