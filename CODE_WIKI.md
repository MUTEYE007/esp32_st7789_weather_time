# ESP32 ST7789 Weather Clock — Code Wiki

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Project Structure](#2-project-structure)
3. [Hardware Configuration](#3-hardware-configuration)
4. [Software Architecture](#4-software-architecture)
5. [Module Reference](#5-module-reference)
   - [5.1 Configuration Constants (config.h)](#51-configuration-constants-configh)
   - [5.2 Display Driver (display.h/cpp)](#52-display-driver-displayhcpp)
   - [5.3 Data & Network (weather.h/cpp)](#53-data--network-weatherhcpp)
   - [5.4 UI Rendering (ui.h/cpp)](#54-ui-rendering-uihcpp)
   - [5.5 Config Server (config_server.h/cpp)](#55-config-server-config_serverhcpp)
   - [5.6 Chinese Font Library (gb2312_font.h)](#56-chinese-font-library-gb2312_fonth)
   - [5.7 Entry Point (main.cpp)](#57-entry-point-maincpp)
6. [Data Structures](#6-data-structures)
7. [Page System & Interaction](#7-page-system--interaction)
8. [Dependencies](#8-dependencies)
9. [Build & Run](#9-build--run)

---

## 1. Project Overview

**ESP32 ST7789 Weather Clock** is a desktop weather station built on the ESP32 microcontroller with a 1.3-inch 240×240 ST7789 TFT LCD. It fetches real-time weather data from the QWeather API, synchronises time via NTP, and supports weather alerts, minute-level precipitation forecasts, and a web-based configuration panel.

### Feature Summary

- **WiFi Provisioning** — First-boot setup via WiFiManager captive portal
- **Current Weather** — Temperature, feels-like, humidity, wind direction/scale via QWeather API
- **NTP Time Sync** — Non-blocking state machine with 15 fallback NTP servers
- **Hourly Temperature Chart** — 7-point line chart with interpolated rendering
- **Weather Alerts** — Multi-page alert viewer with auto-scrolling long descriptions
- **Minute Precipitation** — 2-hour precipitation intensity chart (24 × 5-minute slots)
- **System Diagnostic Page** — Chip info, WiFi/NTP/API status, memory & uptime
- **Web Configuration** — On-device HTTP server for API Key and city settings
- **Chinese Text Rendering** — Self-generated 16×16 dot-matrix GB2312 font (7,445 glyphs, full standard)
- **FreeRTOS Dual-Core Architecture** — Network on Core 0, UI on Core 1

### Technology Stack

| Component | Selection |
|-----------|-----------|
| MCU | ESP32 (Xtensa LX6 dual-core) |
| Display | ST7789 240×240 SPI TFT |
| Framework | Arduino (espressif32 Platform) |
| RTOS | FreeRTOS (ESP-IDF) |
| Build | PlatformIO |
| Weather API | QWeather (和风天气) |
| Chinese Font | Self-generated GB2312 16×16 bitmap (PROGMEM) |

---

## 2. Project Structure

```
esp32_st7789_weather_time/
├── src/                          # Core source code
│   ├── main.cpp                  # Entry point, FreeRTOS task creation
│   ├── config.h                  # (gitignored) Local configuration with API Key
│   ├── config.h.template         # Configuration template (safe to commit)
│   ├── display.h / display.cpp   # Display driver & drawing primitives
│   ├── weather.h / weather.cpp   # Data structures, WiFi, NTP, API requests
│   ├── ui.h / ui.cpp             # UI rendering (all pages)
│   ├── config_server.h / config_server.cpp  # HTTP configuration server
│   └── gb2312_font.h             # GB2312-80 full bitmap font (~233 KB, auto-generated)
├── tools/                        # Font generation tools
│   └── gen_gb2312_font.py        # GB2312 font generator (7445 glyphs)
├── platformio.ini                # PlatformIO project configuration
├── CODE_WIKI.md                  # This document — Code Wiki (EN)
├── CODE_WIKI.zh-CN.md            # Code Wiki — 中文版
└── README.md                     # Project README (user-friendly)
```

> **Note:** `config.h` is excluded from version control via `.gitignore` (pattern: `src/config.h`). Use `config.h.template` as a reference to create your own.

---

## 3. Hardware Configuration

### 3.1 Bill of Materials

| Part | Model |
|------|-------|
| MCU | ESP32 (Adafruit ESP32 Feather or compatible) |
| Display | 1.3" 240×240 ST7789 TFT (SPI) |
| Button | BOOT button (GPIO 0, internal pull-up) |

### 3.2 Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| TFT_CS | 5 | SPI chip select |
| TFT_DC | 16 | Data/command select |
| TFT_RST | 17 | Reset |
| TFT_MOSI | 23 | SPI master-out-slave-in |
| TFT_SCK | 18 | SPI clock |
| BTN_PIN | 0 | Button input (INPUT_PULLUP) |

### 3.3 SPI Initialisation

Uses the VSPI hardware SPI interface. Screen rotation is set to `1` for 240×240 landscape mode.

```cpp
vspi = new SPIClass(VSPI);
vspi->begin(TFT_SCK, -1, TFT_MOSI, -1);
tft = new Adafruit_ST7789(vspi, TFT_CS, TFT_DC, TFT_RST);
tft->init(SCREEN_W, SCREEN_H);
tft->setRotation(1);
tft->fillScreen(COLOR_BG);
tft->setTextWrap(false);
```

### 3.4 Partition Table

`board_build.partitions = huge_app.csv` — uses a custom partition table with a larger application slot to accommodate the ~233 KB font library.

---

## 4. Software Architecture

### 4.1 Architecture Layers

```
┌───────────────────────────────────────────────────────────┐
│  main.cpp — FreeRTOS Dual-Core Scheduler                  │
│  ├─ Core 0: networkTask — WiFi, NTP, API fetching         │
│  └─ Core 1: uiTask — Display, button, UI refresh          │
├───────────────────────────────────────────────────────────┤
│  ui.cpp — UI Rendering Layer                               │
│  ├─ Main page: status bar / weather / clock / details     │
│  ├─ Alert page: multi-alert paging + scrolling            │
│  ├─ Precipitation page: intensity bar chart                │
│  └─ System info page: chip/WiFi/NTP/API diagnostics       │
├───────────────────────────────────────────────────────────┤
│  weather.cpp — Data & Network Layer                        │
│  ├─ WiFi management (WiFiManager provisioning)            │
│  ├─ NTP time sync (15-server round-robin state machine)   │
│  ├─ Geo-location resolution (3-tier fallback)             │
│  └─ Weather API requests (now/24h/3d/alerts/precipitation)│
├───────────────────────────────────────────────────────────┤
│  display.cpp — Hardware Abstraction Layer                  │
│  ├─ SPI / ST7789 initialisation                           │
│  ├─ Drawing primitives: rects, labels, WiFi bars, anims   │
│  └─ Provisioning screen rendering                         │
├───────────────────────────────────────────────────────────┤
│  config_server.cpp — Web Configuration Layer               │
│  └─ HTTP server on port 80 (API Key / city update)        │
├───────────────────────────────────────────────────────────┤
│  gb2312_font.h — Font Layer                                │
│  └─ GB2312 16×16 bitmap font + UTF-8 parser + renderer    │
└───────────────────────────────────────────────────────────┘
```

### 4.2 FreeRTOS Task Layout

| Task | Core | Stack | Priority | Responsibility |
|------|------|-------|----------|----------------|
| networkTask | Core 0 (protocol) | 8 KB | 1 | WiFi maintenance, NTP polling, all API data fetching |
| uiTask | Core 1 (application) | 8 KB | 2 | Screen rendering, button handling, periodic clock updates |

**Inter-Task Communication**:

- **Mutex** (`dataMutex`) — Guards all shared data structures (`weather`, `hourly`, `state` globals)
- **volatile flags** — `networkBusy` (triggers loading animation), `weatherUpdated` (triggers UI redraw)
- **Polling** — `uiTask` checks `state` fields every 50 ms

### 4.3 Data Flow

```
Power On
  │
  ▼
setup()
  ├─ Init: Serial / Display / Button / Mutex
  ├─ Create networkTask → Core 0
  └─ Create uiTask → Core 1

networkTask (Core 0) loop:
  ├─ WiFiManager::autoConnect()
  │    ├─ First boot → AP mode → phone connects → Wi-Fi configured
  │    └─ Already configured → auto-connect
  ├─ initNTP() → processNTP() round-robin (max 15 servers)
  ├─ resolveLocation()          ← 3-tier geo lookup (ipip → B站 → 乐视)
  ├─ fetchWeather()             ← /v7/weather/now
  ├─ fetchDaily()               ← /v7/weather/3d
  ├─ fetchHourly()              ← /v7/weather/24h
  ├─ fetchWeatherWarnings()     ← /weatheralert/v1/current
  ├─ fetchMinutelyPrecipitation() ← /v7/minutely/5m
  └─ vTaskDelay(500ms)

uiTask (Core 1) loop:
  ├─ Button detection
  │    ├─ Short press (<3s) → page cycle
  │    └─ Long press (≥3s) → reset WiFi & restart
  ├─ Provisioning mode → AP guide screen
  ├─ Alert page → scroll update
  ├─ Precipitation page → periodic refresh
  ├─ System info page → second-level updates
  ├─ Main page → weather redraw + clock second-level updates
  └─ vTaskDelay(50ms)
```

### 4.4 Screen Layout

```
┌─ Status Header ─────────────── (y=2, h=14) ─┐
│ 福州 09:41                          [WiFi]  │
├─ Line 1 ───────────────────────── (y=16) ──┤
│ ⛅ 26°C          H:28°C   ← Weather Section│
│ 福州 晴          L:20°C   (y=20, h=36)     │
├─ Clock Section ──────────────── (y=56, h=58)│
│          12:34  56                          │
│       2026-05-21 Thu                       │
├─ Line 3 ──────────────────────── (y=120) ──┤
│ 体感:24°C  湿度:65%       ← Detail Section │
│ 更新:12:00  风:东北风2级  (y=124, h=34)    │
├─ Line 4 ──────────────────────── (y=164) ──┤
│ 12h  ●━━━━━━━━━━━ 18h     ← Hourly Chart  │
│ 26°C  26°C  25°C  24°C... (y=168, h=66)   │
├─ Bottom Bar ──────────────────── (y=236) ──┤
└─────────────────────────────────────────────┘
```

---

## 5. Module Reference

### 5.1 Configuration Constants ([config.h.template](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config.h.template))

Central definition file for all hardware pins, screen layout coordinates, interval timers, and RGB565 colour constants.

> **Important:** `config.h` is gitignored. Copy `config.h.template` → `config.h` and fill in your sensitive values.

**Network settings**:

| Macro | Default | Description |
|-------|---------|-------------|
| `WEATHER_API_KEY` | `"your_qweather_api_key"` | QWeather API key (user must fill) |
| `WEATHER_HOST` | `"devapi.qweather.com"` | API host |
| `WEATHER_LOC` | `"101230101"` | Default city Location ID |
| `WEATHER_NAME` | `"Fuzhou"` | Default city display name |
| `WEATHER_INTERVAL_MS` | `1800000` (30 min) | Weather fetch interval |

**Hardware pins**:

| Macro | Value |
|-------|-------|
| `TFT_CS` | 5 |
| `TFT_DC` | 16 |
| `TFT_RST` | 17 |
| `TFT_MOSI` | 23 |
| `TFT_SCK` | 18 |
| `BTN_PIN` | 0 |

**Screen dimensions & layout**: All `#define` constants in config.h map fixed pixel positions for each section (status bar, weather, clock, details, chart). See [Screen Layout](#44-screen-layout) for visual reference.

| Macro | Value | Description |
|-------|-------|-------------|
| `SCREEN_W` | 240 | Screen width |
| `SCREEN_H` | 240 | Screen height |
| `PAD_LEFT` | 8 | Left padding for content |
| `PAD_RIGHT` | 8 | Right padding for content |
| `CONTENT_W` | 224 | Content area width |
| `STATUS_Y` | 2 | Status bar Y start |
| `STATUS_H` | 14 | Status bar height |
| `WEATHER_Y` | 20 | Weather section Y start |
| `WEATHER_H` | 36 | Weather section height |
| `CLOCK_Y` | 56 | Clock section Y start |
| `CLOCK_H` | 58 | Clock section height |
| `DETAIL_Y` | 124 | Detail section Y start |
| `DETAIL_H` | 34 | Detail section height |
| `CHART_Y` | 168 | Chart section Y start |
| `CHART_H` | 66 | Chart section height |
| `CHART_POINTS` | 7 | Number of hourly data points |
| `CHART_COL_W` | 34 | Column width between chart points |
| `CHART_PT_R` | 4 | Chart data point circle radius |
| `HOUR_COUNT` | 7 | Hourly data count (aligns with CHART_POINTS) |

**Weather alert and minutely constants (defined in [weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h))**:

| Macro | Value | Description |
|-------|-------|-------------|
| `WARNING_MAX` | 5 | Maximum number of weather alerts |
| `MINUTELY_SLOTS` | 24 | Number of 5-minute precipitation slots (2 hours) |
| `WARN_INTERVAL_MS` | 600000 (10 min) | Warning fetch interval |
| `MINUTELY_INTERVAL_MS` | 600000 (10 min) | Minutely precip fetch interval |

**RGB565 colour constants**:

| Constant | Value | Usage |
|----------|-------|-------|
| `COLOR_BG` | `0x0824` | Deep blue background |
| `COLOR_PRIMARY` | `0xFFFF` | White text |
| `COLOR_CLOCK` | `0x5D9F` | Cyan clock digits |
| `COLOR_LABEL` | `0x8C14` | Grey labels |
| `COLOR_MUTED` | `0x39C7` | Dimmed text |
| `COLOR_ACCENT` | `0x3C16` | Pink-purple accent |
| `COLOR_GREEN` | `0x25E3` | Green (WiFi OK) |
| `COLOR_AMBER` | `0xE526` | Amber (high temp) |
| `COLOR_LINE` | `0x10A4` | Section divider line |
| `COLOR_GRID` | `0x0843` | Chart grid dots |
| `COLOR_YELLOW` | `0xFFE0` | Sun icon |
| `COLOR_CLOUD` | `0xBDD7` | Cloud icon |
| `COLOR_RAIN` | `0x4AEF` | Rain icon |
| `COLOR_GOLD` | `0xFEA0` | City name |
| `WARN_COLOR_RED` | `0xF800` | Extreme alert |
| `WARN_COLOR_ORANGE` | `0xFB00` | Severe alert |
| `WARN_COLOR_YELLOW` | `0xFFE0` | Moderate alert |
| `WARN_COLOR_BLUE` | `0x001F` | Minor alert |

### 5.2 Display Driver ([display.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.h) / [display.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.cpp))

Low-level screen initialisation and drawing primitives.

**Global objects**:

| Variable | Type | Description |
|----------|------|-------------|
| `vspi` | `SPIClass*` | VSPI bus object |
| `tft` | `Adafruit_ST7789*` | TFT driver instance (all drawing goes through this) |

**API**:

| Function | Signature | Description |
|----------|-----------|-------------|
| `initDisplay()` | `void` | Initialise SPI + TFT, set rotation and background colour |
| `drawSectionLine()` | `void(int y)` | Draw horizontal divider line in `COLOR_LINE` |
| `drawLabel()` | `void(int x, int y, const char *text)` | Draw label in `COLOR_LABEL`, textSize=1 |
| `fillArea()` | `void(int x, int y, int w, int h, uint16_t color)` | Fill rectangle (wraps `fillRect`) |
| `drawWiFiBars()` | `void(int x, int y, bool connected)` | Draw 4-level RSSI signal bars, or "X" when disconnected |
| `drawProvisioningScreen()` | `void(const char *apName, const char *apIP)` | Full provisioning guide screen with instructions |
| `updateProvisioningFrame()` | `void(int frame)` | Animated dot cycle during provisioning wait |
| `drawLongPressRing()` | `void(int cx, int cy, float progress)` | Long-press progress ring (12 dots, clockwise fill) |
| `animateWipe()` | `void()` | Page transition animation (6 blocks, top-to-bottom clear) |

**WiFi RSSI mapping** (in `drawWiFiBars`):

| RSSI | Bars |
|------|------|
| > -50 dBm | 4 |
| > -65 dBm | 3 |
| > -80 dBm | 2 |
| ≤ -80 dBm | 1 |

### 5.3 Data & Network ([weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) / [weather.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.cpp))

Core module containing all data structure definitions, global variables, WiFi management, NTP synchronisation, geo-location resolution, and weather API requests.

#### 5.3.1 Struct Definitions

**`WeatherData`** — Current weather condition:

| Field | Type | Source |
|-------|------|--------|
| `temp` | `String` | `now.temp` |
| `feelsLike` | `String` | `now.feelsLike` |
| `humidity` | `String` | `now.humidity` |
| `windDir` | `String` | `now.windDir` |
| `windScale` | `String` | `now.windScale` |
| `weatherText` | `String` | `now.text` (truncated to 21 bytes) |
| `weatherIcon` | `String` | `now.icon` (QWeather icon code) |
| `updateTime` | `String` | `updateTime` (ISO format) |
| `tempMax` / `tempMin` | `String` | `daily[0].tempMax/Min` (from 3d API) |
| `valid` | `bool` | Data validity flag |

**`HourlyData`** — Hourly forecast (7 points):

| Field | Type | Description |
|-------|------|-------------|
| `hourLabel[7]` | `String[]` | Time labels (e.g. "12h") |
| `temp[7]` | `String[]` | Temperature strings |
| `icon[7]` | `String[]` | Weather icon codes |
| `tempInt[7]` | `int[]` | Integer temps (for chart) |
| `valid` | `bool` | Data validity flag |

**`WarningData`** — Weather alert (max 5):

| Field | Type | Description |
|-------|------|-------------|
| `eventName` | `String` | Event name (e.g. "Typhoon") |
| `eventCode` | `String` | Event code |
| `severity` | `String` | Severity: extreme/severe/moderate/minor |
| `headline` | `String` | Alert headline |
| `description` | `String` | Detailed description |
| `senderName` | `String` | Issuing authority |
| `valid` | `bool` | Data validity flag |

**`MinutelyData`** — Minute-level precipitation:

| Field | Type | Description |
|-------|------|-------------|
| `summary` | `String` | Precipitation summary text |
| `slots[24]` | `struct {fxTime, precip}` | 24 × 5-minute forecast slots |
| `valid` | `bool` | Data validity flag |

**`AppState`** — Global application state (mutex-protected):

| Field | Type | Description |
|-------|------|-------------|
| `wifiConnected` | `bool` | WiFi connectivity |
| `timeSynced` | `bool` | NTP sync status |
| `ntpTried` | `bool` | Whether NTP has been attempted |
| `weatherLoaded` | `bool` | Weather data loaded |
| `locationResolved` | `bool` | Geo-location resolved |
| `showingSystemInfo` | `bool` | System info page flag |
| `systemInfoDirty` | `bool` | Full redraw needed |
| `provisioningMode` | `bool` | AP provisioning mode |
| `showingWarning` | `bool` | Warning page active |
| `showingMinutely` | `bool` | Minutely page active |
| `hasActiveWarnings` | `bool` | Active alerts exist |
| `warningIndex` | `int` | Current alert index |
| `lastWeatherFetch` | `unsigned long` | Last weather fetch timestamp |
| `lastNtpAttempt` | `unsigned long` | Last NTP attempt timestamp |
| `lastWarningFetch` | `unsigned long` | Last warning fetch timestamp |
| `lastMinutelyFetch` | `unsigned long` | Last minutely fetch timestamp |
| `ntpFailReason[24]` | `char[]` | NTP failure reason string |
| `ntpServer[32]` | `char[]` | Current NTP server name |
| `apName[24]` | `char[]` | AP hotspot name |
| `apIP[16]` | `char[]` | AP IP address |
| `bootTime` | `unsigned long` | Boot timestamp (`millis()`) |

#### 5.3.2 Global Variables

```cpp
extern WeatherData weather;          // Current weather
extern HourlyData hourly;            // Hourly forecast
extern WarningData warnings[WARNING_MAX]; // Alert array (max 5)
extern int warningCount;             // Actual alert count
extern MinutelyData minutely;        // Minutely precipitation
extern AppState state;               // Runtime state
extern String weatherLoc;            // City Location ID
extern String weatherName;           // City display name
extern String weatherLat;            // City latitude
extern String weatherLon;            // City longitude
extern String weatherApiKey;         // QWeather API key
extern String weatherHost;           // API host
extern NTPClient *timeClient;        // NTP client
extern WiFiManager wifiManager;      // WiFi manager
extern SemaphoreHandle_t dataMutex;  // Data mutex
extern volatile bool networkBusy;    // Network busy flag
extern volatile bool weatherUpdated; // Weather update flag
```

Additionally, static (file-scope) NTP state machine variables in weather.cpp:

| Variable | Type | Description |
|----------|------|-------------|
| `ntpServers[]` | `const char*[]` | 15 NTP server strings |
| `ntpServerIdx` | `static int` | Current server index (-1 = inactive) |
| `ntpCurServer` | `static const char*` | Current server name pointer |
| `ntpSendMs` | `static unsigned long` | Timestamp of last NTP request |

#### 5.3.3 API Functions

**WiFi & persistent storage**:

| Function | Description |
|----------|-------------|
| `initWiFiWithProvisioning()` | Load config from NVS, start WiFiManager captive portal |
| `loadConfig()` | Read API Key / Host from NVS Preferences namespace `"weather"` |
| `saveConfig(apiKey, host)` | Write API Key / Host to NVS Preferences |

**NTP time synchronisation** — Non-blocking state machine with 15 servers:

| Function | Description |
|----------|-------------|
| `initNTP()` | Initialise NTP sync (reset server index, start with first server) |
| `processNTP()` | Poll NTP state machine: check response / timeout / switch |
| `advanceNtpServer()` | Advance to next NTP server in the list (internal) |

NTP server list (in order of attempted use):
1. `ntp.ntsc.ac.cn` (National Time Service Center, China)
2. `ntp1-7.aliyun.com` (Alibaba Cloud, 7 servers)
3. `ntp1-5.tencent.com` (Tencent Cloud, 5 servers)
4. `pool.ntp.org` (global pool, last resort)

Each server is given 3 seconds (`NTP_PER_SERVER_MS`) to respond before advancing. DNS resolution failure also triggers advance. If all 15 fail, `ntpTried` is set to `true` with reason `"all servers failed"`.

**Geo-location resolution** — 3-tier fallback:

| Function | Description |
|----------|-------------|
| `resolveLocation()` | Auto-detect city via IP geolocation |
| `setCityByName(cityName)` | Manually set city by name (used by config server) |
| `lookupLocationId(searchCity)` | Query QWeather geo API to resolve city name → location ID (internal) |

Resolution order:
1. **ipip.net** — `myip.ipip.net/json` (primary, parses `data.location[2]` as city)
2. **Bilibili** — `api.live.bilibili.com/xlive/web-room-v1/index/getIpInfo` (fallback 1)
3. **LeTV** — `g3.letv.com/r?format=1` (fallback 2, parses hyphen-separated location string)

If all fail, falls back to `WEATHER_LOC` / `WEATHER_NAME` defaults.

**Weather API requests** — All use `httpGetJson()` with gzip decompression:

| Function | Endpoint | Description |
|----------|----------|-------------|
| `fetchWeather()` | `/v7/weather/now` | Current conditions |
| `fetchHourly()` | `/v7/weather/24h` | 24-hour forecast (7 points used) |
| `fetchDaily()` | `/v7/weather/3d` | 3-day forecast (high/low temps) |
| `fetchWeatherWarnings()` | `/weatheralert/v1/current` | Active weather alerts |
| `fetchMinutelyPrecipitation()` | `/v7/minutely/5m` | 2-hour precipitation forecast |

All requests include the `X-QW-Api-Key` header for authentication. `fetchWeather()` additionally truncates `weatherText` to 21 bytes (7 Chinese chars × 3 bytes) to fit the screen layout.

#### 5.3.4 HTTP & gzip Decompression

`httpGetJson()` implements custom gzip handling:

```
httpGetJson(url, withApiKey)
  │
  ├─ HTTP GET (10s timeout, User-Agent & Accept headers, X-QW-Api-Key header)
  ├─ Read raw response into dynamic buffer (5s stream timeout)
  ├─ Detect gzip magic bytes (0x1F 0x8B)
  │    ├─ Not gzipped → return as plain String
  │    └─ Gzipped → skip header fields (FEXTRA/FNAME/FCOMMENT/FHCRC)
  │                  └─ tinfl_decompress() deflate decompression
  │                     └─ Dynamic buffer expansion (realloc, double each pass)
  └─ Return decompressed JSON string
```

Decompression uses the ESP32 ROM's miniz library (`esp32/rom/miniz.h`, `tinfl_decompress` with `TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF` flag).

The `skipGzipHeader()` function parses gzip flags (`FEXTRA`, `FNAME`, `FCOMMENT`, `FHCRC`) to locate the raw deflate stream, then passes it to `tinfl_decompress`.

#### 5.3.5 URL Encoding

`urlEncode(str)` — Custom URL encoder that percent-encodes non-alphanumeric characters. Used for geo-location lookup query parameters.

---

### 5.4 UI Rendering ([ui.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.h) / [ui.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.cpp))

All page rendering logic, organised by page.

#### 5.4.1 Main Page

Assembled by `drawFullUI()`:

| Function | Description |
|----------|-------------|
| `drawFullUI()` | Orchestrates full main page: wipe → chart → header → weather → clock → details → bottom bar |
| `drawStatusHeader()` | City name (16px Chinese font) + time (`hh:mm`) + WiFi bars |
| `updateStatusTime()` | Incremental time update in status bar (no full redraw) |
| `drawWeatherSection()` | Weather icon + large temperature + city name + hi/lo temps |
| `drawWeatherIcon(cx, cy, code)` | Draws weather icon by QWeather code: sun (100/150), cloud (101-104), rain (300-499), snow (500-599), overcast (default) |
| `drawClockSection()` | Large HH:MM (textSize=5, cyan) + seconds (top-right) + date line |
| `updateClockTime(h, m, s)` | Incremental clock update with background colour passthrough (no full redraw) |
| `drawDetailSection()` | Two-row detail grid: feels-like + humidity (row 1), update time + wind dir/scale (row 2) |
| `drawHourlyChart()` | 7-point line chart with dashed grid, interpolated fill lines, temperature polyline, data points, time labels |

**`drawHourlyChart()`** rendering steps:
1. Draw horizontal grid (3 dashed lines, dotted at 4px intervals)
2. Calculate Y coordinates from temperature range (with 1-degree padding when min==max)
3. Draw vertical fill bars (grid colour to chart bottom, linear interpolation)
4. Draw temperature polyline (accent colour, linear interpolation between points)
5. Draw data point circles (radius `CHART_PT_R` = 4)
6. Draw time labels (bottom edge) and temperature values

#### 5.4.2 Alert Page

| Function | Description |
|----------|-------------|
| `drawWarningPage()` | Full alert page: header bar, severity colour strip, headline, scrollable description, dot navigation |
| `updateWarningScroll()` | Scroll description text by one line every 3 seconds |
| `isWarningScrollNeeded()` | Returns whether the warning description exceeds viewport height |

**Internal helper functions**:

| Function | Description |
|----------|-------------|
| `severityColor(sev)` | Maps severity string (`"extreme"` / `"severe"` / `"moderate"`) to warning colour |
| `contrastColor(bgColor)` | Computes contrasting text colour (black or white) against a given background RGB565 colour using luminance weighting (R×299 + G×587 + B×114) |
| `wrapText16(text, maxW)` | Wraps UTF-8 text to fit within `maxW` pixels. Each CJK char = 16px, ASCII = 8px. Inserts newline (`\n`) when width would be exceeded |
| `truncateText16(text, maxPx)` | Truncates UTF-8 text with ellipsis (`...`) to fit within `maxPx` pixels. Leaves 24px for the `...` suffix |
| `countLines(text)` | Returns number of lines by counting `\n` characters (+1) |
| `splitLines(text, out[], &count)` | Splits a `\n`-delimited string into an array of line strings (max `MAX_WARN_LINES` = 30) |
| `redrawDots(wi)` | Redraws dot navigation indicators below description. Active dot = filled circle (radius 3), others = smaller (radius 2) |
| `redrawHeadline()` | Redraws the wrapped headline text section |
| `drawDescLines(offsetY)` | Draws visible lines of the scrollable description, clipped to viewport area |

**Scrolling mechanism**:
- Driven by static state variables (`warnScrollY`, `warnDescLineCount`, `warnScrollActive`, `warnLastScrollMs`)
- Text wrapping uses `wrapText16()` which handles CJK character widths
- Scroll direction: upward, each scroll step = `WARN_LINE_H` (20px)
- Interval: `WARN_SCROLL_DELAY` = 3000ms between scroll steps
- When scroll reaches bottom, resets to top (wraparound)
- Max description length: 400 characters (truncation in `drawWarningPage()`)
- `MAX_WARN_LINES` = 30, `WARN_HEADLINE_Y` = 50, `WARN_DOT_Y` = 228

`severityColor()` mapping:
- `"extreme"` → `WARN_COLOR_RED` (0xF800)
- `"severe"` → `WARN_COLOR_ORANGE` (0xFB00)
- `"moderate"` → `WARN_COLOR_YELLOW` (0xFFE0)
- default → `WARN_COLOR_BLUE` (0x001F)

#### 5.4.3 Minutely Precipitation Page

| Function | Description |
|----------|-------------|
| `drawMinutelyPage()` | Full precipitation page: header with city name + warning indicator, summary text, custom bar chart, statistics |

**Chart rendering**:
- X-axis: 24 slots spanning 2 hours (each slot = 5 minutes)
- Y-axis: precipitation intensity, scaled to max value (minimum floor 0.5 mm)
- 3-tier colour fill:
  - Light green (`0x04B0`) — < 0.5 mm
  - Teal (`0x06BF`) — 0.5–2.0 mm
  - Blue (`0x001F`) — > 2.0 mm
- Overlaid white polyline connecting data points
- Grid lines: 3 horizontal bands with Chinese labels ("小" / "中" / "大")
- X-axis labels: "现在", "1小时后", "2小时后"
- Summary statistics at bottom: total precipitation and peak value

#### 5.4.4 System Info Page

| Function | Description |
|----------|-------------|
| `drawSystemInfo()` | Full-screen diagnostic page with 4 sections, dual-mode rendering |

**Sections**:
1. **ESP32** — Chip model, revision, flash size, free heap, uptime
2. **WiFi** — SSID, IP, gateway, RSSI
3. **NTP** — Sync status, server in use, current time
4. **Weather API** — now/3d/24h status indicators + free memory

**Dual-mode rendering**:
- First entry (`systemInfoDirty == true`): Full `animateWipe()` + draw all sections (static content drawn once)
- Subsequent updates (`systemInfoDirty == false`): Partial refresh — only updates uptime (y=298) and NTP/API status sections (y=350+) every second

#### 5.4.5 Loading Animation

| Function | Description |
|----------|-------------|
| `drawLoadingFrame(frame)` | 8-dot rotating loading indicator, displayed in the detail section area when `networkBusy` is true |

Each frame updates a rotating pattern: one bright dot (`COLOR_CLOCK`), one dim dot (`COLOR_ACCENT`), and six invisible dots (`COLOR_LINE`). A "Loading" label appears below.

#### 5.4.6 Text Helper

`textWidth16()` — Calculates pixel width of a UTF-8 string when rendered in 16px Chinese font (CJK = 16px, ASCII = 8px). Used for layout positioning decisions throughout the UI.

---

### 5.5 Config Server ([config_server.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.h) / [config_server.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.cpp))

Lightweight HTTP configuration server based on `WebServer`, listening on port 80.

**Routes**:

| Route | Method | Function |
|-------|--------|----------|
| `/` | GET | Configuration page (HTML form, inline dark-theme CSS) |
| `/save` | POST | Save API Key and Host to NVS |
| `/setcity` | POST | Set city name (resolved via QWeather geo API) |

**Key implementation details**:

- `handleRoot()` — Builds the page by concatenating PROGMEM string fragments with current values (`weatherApiKey`, `weatherHost`, `weatherName`, `WiFi.localIP()`)
- `handleSave()` — Validates non-empty `apiKey` and `host`, calls `saveConfig()`, resets `lastWeatherFetch = 0` to trigger immediate refresh
- `handleSetCity()` — Validates non-empty city name, calls `setCityByName()`
- Page pre-fills input fields with current `weatherApiKey` and `weatherHost` values
- JavaScript `fetch()` API for async form submission with real-time status feedback
- HTML fragments stored in PROGMEM (`PAGE_HEAD`, `PAGE_MID`, `PAGE_TAIL`, `PAGE_END`) to conserve RAM
- Server started once by `networkTask` when WiFi connects; `handleConfigClient()` called every loop iteration

---

### 5.6 Chinese Font Library ([gb2312_font.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/gb2312_font.h))

Auto-generated **complete** GB2312-80 bitmap font (01–09 symbol region + 16–55 Level 1 hanzi + 56–87 Level 2 hanzi), providing 16×16 pixel CJK character rendering.

**Specifications**:

| Metric | Value |
|--------|-------|
| Characters | 7,445 (682 symbols + 3,755 L1 hanzi + 3,008 L2 hanzi) |
| Glyph data | 238,240 bytes (32 bytes per glyph: 16 rows × uint16_t) |
| Mapping table | 29,780 bytes (7,445 × 4-byte entries) |
| Storage | Flash (PROGMEM) |
| Font source | Windows `simfang.ttf` (FangSong) |
| Verification | **100% match** vs Python `gb2312` codec (GB 2312-80 reference impl.) |

**Core inline functions** (defined in the header):

| Function | Description |
|----------|-------------|
| `findGB2312Glyph(unicode)` | Binary search for glyph data by Unicode codepoint (PROGMEM-safe via `pgm_read_word`) |
| `utf8ToUnicode(&p)` | Decode one UTF-8 character (1-3 byte sequences), advance pointer |
| `drawGB16(x, y, text, fg, bg)` | Main renderer: ASCII (<0x80) uses TFT built-in font (8px), CJK uses bitmap font (16px), supports `\n` line breaks |

**Glyph data format**:

Each character occupies 32 bytes (16 rows × 2 bytes/row):
```
Row 0: [bit15 bit14 ... bit0]  ← pixel data
Row 1: [bit15 bit14 ... bit0]
...
Row 15:[bit15 bit14 ... bit0]
```
Each bit represents one pixel: `1` = foreground colour, `0` = transparent/background.

**Codepoint handling**:
- ASCII (< 0x80): Rendered with TFT's built-in font (8px width)
- CJK Unified Ideographs (U+4E00–U+9FFF): Looked up in glyph table via binary search
- CJK Symbols (U+3000–303F): Allowed through
- Fullwidth Forms (U+FF00–FFEF): Allowed through
- General Punctuation (U+2000–206F): Allowed through
- All other non-CJK codepoints: Skipped with 16px advance (space)

**Font generation** (see [gen_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/gen_gb2312_font.py)):
1. Iterate GB2312-80 full encoding space (0xA1A1–0xF7FE), skip unused region (0xAA–0xAF = zones 10–15)
2. Render each character at 4× size (64×64) using Pillow
3. LANCZOS downscale to 16×16
4. Threshold binarise (80/255)
5. Output as PROGMEM array + binary-search mapping table (sorted by Unicode)
6. **Verify** against Python `gb2312` codec (see [verify_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/verify_gb2312_font.py))

**Running**:
```bash
cd tools
python gen_gb2312_font.py         # regenerate the font header
python verify_gb2312_font.py      # verify against the standard
```

---

### 5.7 Entry Point ([main.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/main.cpp))

System entry point responsible for initialisation and task creation.

**`setup()` execution order**:

1. Serial init (`Serial.begin(115200)`)
2. State field zeroing (all `state` fields, `weather.valid`, `hourly.valid`)
3. Mutex creation (`xSemaphoreCreateMutex()`)
4. Display init (`initDisplay()`)
5. Button config (GPIO 0, `INPUT_PULLUP`)
6. Core 0 WDT disabled (`disableCore0WDT()` — network task may block on WiFi)
7. `networkTask` created on Core 0 (stack 8 KB, priority 1)
8. 100ms delay for network task to initialise
9. Provisioning screen drawn if needed
10. `uiTask` created on Core 1 (stack 8 KB, priority 2)
11. Self-deletion (`vTaskDelete(NULL)`)

**`networkTask()` loop** (Core 0):

```
1. If provisioning mode:
   ├─ Process WiFiManager (wifiManager.process())
   ├─ On WiFi connected → exit provisioning mode (set FALSE)
   └─ On timeout (185s) → exit provisioning mode, fall back to WIFI_STA
2. If WiFi disconnected → reconnect every 5 seconds (WiFi.reconnect())
3. Start config server (one-time, when WiFi connects)
4. Handle HTTP client requests (handleConfigClient())
5. Poll NTP state machine (processNTP())
6. If NTP not synced and 30s elapsed from last attempt → retry (initNTP())
7. Resolve location (one-time, resolveLocation())
8. If NTP synced → daily timeClient->update()
9. Every WEATHER_INTERVAL_MS (30 min) → fetch all weather data
   (fetchWeather → fetchDaily → fetchHourly → fetchWeatherWarnings)
10. vTaskDelay(500ms)
```

**`uiTask()` loop** (Core 1):

```
1. Read button state from GPIO 0 (INPUT_PULLUP)
   ├─ Pressed → start timing
   │   └─ If held ≥3s → draw progress ring → reset WiFi → ESP.restart()
   └─ Released → on short press (<3s), page cycle:
       Main → Alert(→ next alert) → Minutely → System → Main
2. If provisioning mode → show AP guide screen + dot animation
3. If alert page → update description scrolling (every 100ms)
4. If minutely page → refresh data if stale (MINUTELY_INTERVAL_MS expired)
5. If system info page → update uptime/NTP/API sections once per second
6. If networkBusy → show rotating loading animation
7. If weatherUpdated + 2s cooldown → full redraw (drawFullUI())
8. Every second → update status bar time + clock (HH:MM + seconds)
9. vTaskDelay(50ms)
```

**Static helper functions in main.cpp**:

| Function | Description |
|----------|-------------|
| `showBootScreen(title)` | Draw accent-coloured header bar with title text |
| `showBootLine(y, text, color)` | Draw a single boot message line at given Y position |

---

## 6. Data Structures

### 6.1 Relationship Diagram

```
AppState (global runtime state)
  ├── Connection: wifiConnected, timeSynced, ntpTried
  ├── Page state: showingSystemInfo, showingWarning, showingMinutely
  ├── Data state: weatherLoaded, locationResolved
  ├── Timestamps: lastWeatherFetch, lastNtpAttempt, lastWarningFetch, lastMinutelyFetch
  ├── NTP info: ntpFailReason[24], ntpServer[32]
  └── AP info: apName[24], apIP[16]

Global data (mutex-protected)
  ├── WeatherData weather      ← fetchWeather()
  ├── HourlyData hourly        ← fetchHourly()
  ├── WarningData[WARNING_MAX] + int warningCount ← fetchWeatherWarnings()
  └── MinutelyData minutely    ← fetchMinutelyPrecipitation()

Volatile flags (lock-free, atomic access)
  ├── volatile bool networkBusy     ← set true during network I/O
  └── volatile bool weatherUpdated  ← set true after data update
```

### 6.2 Design Decisions

- **Mutex protection**: All reads/writes to shared structures must acquire `dataMutex`. networkTask writes, uiTask reads. Both use `portMAX_DELAY` for blocking acquisition.
- **NTP state machine**: Non-blocking round-robin with 3-second per-server timeout. 15 servers across 4 domains. DNS resolution failure triggers immediate advance. All-fail state explicitly tracked in `state.ntpTried`.
- **gzip decompression**: Manual handling via `tinfl_decompress()` from ESP32 ROM miniz. Output buffer doubles on each pass (`realloc`) if decompressed data exceeds current capacity. Custom header-parsing logic (`skipGzipHeader()`) handles all gzip flag variants.
- **Incremental UI updates**: Clock and status bar redraw only changed pixels every second using background-colour passthrough (`setTextColor(fg, bg)`), avoiding full-screen flicker.
- **Weather text truncation**: `weatherText` truncated to 21 bytes (UTF-8 safe — 7 Chinese chars × 3 bytes) in `fetchWeather()` to fit screen layout constraints.
- **System info dual-mode**: Static content drawn once on first entry; only uptime counter, NTP status, and API status are redrawn on subsequent ticks — marked by `state.systemInfoDirty`.
- **Button debounce**: No explicit debounce — relies on 50ms task delay and GPIO 0 internal pull-up.

---

## 7. Page System & Interaction

### 7.1 Page Modes

| Mode | State Flag | Entry | Content |
|------|-----------|-------|---------|
| Main | `!showing*` | Default / from system page short press | Weather + clock + details + chart |
| Alert | `showingWarning` | Main page short press | Multi-alert paging + scrolling description |
| Minutely | `showingMinutely` | Alert page short press | Precipitation intensity chart + stats |
| System | `showingSystemInfo` | Minutely page short press | Chip/WiFi/NTP/API diagnostics |
| Provisioning | `provisioningMode` | Automatic on first boot | AP guide screen + waiting animation |

### 7.2 Page Transition Cycle

```
Main Page
  │ short press ──→ Alert Page
  │                   │ short press ──→ Minutely Page
  │                   │                   │ short press ──→ System Info Page
  │                   │                   │                   │ short press ──→ back to Main
  │ (multiple alerts) │
  └───────────────────┘ If multiple alerts, successive short presses cycle
                        through individual alerts (warningIndex++) before
                        advancing to Minutely page.
```

### 7.3 Button Interaction

| Action | Result |
|--------|--------|
| Short press BOOT (<3s) | Page cycle (as above) |
| Long press BOOT (≥3s) | Reset WiFi credentials (`wifiManager.resetSettings()`) and restart (`ESP.restart()`) |

Long press is visualised with a 12-dot progress ring (`drawLongPressRing()`) at the bottom of the screen (cx=120, cy=218). Each dot fills sequentially over 3 seconds (12 dots ÷ 3s = 4 dots/s).

### 7.4 Web Configuration

When the device is connected to WiFi, visiting its IP address (port 80) opens the configuration page:

- Modify QWeather API Key and Host
- Change city name (resolved via QWeather geo API)
- AJAX async form submission with real-time status feedback
- Dark-theme card-style UI with inline CSS
- Pre-filled input fields with current configuration values

---

## 8. Dependencies

### 8.1 Arduino Libraries (PlatformIO-managed)

| Library | Version | Purpose |
|---------|---------|---------|
| `Adafruit ST7735 and ST7789 Library` | latest | ST7789 TFT display driver |
| `Adafruit GFX Library` | latest | Core graphics primitives (shapes, text, colours) |
| `ArduinoJson` | ^7.0.0 | JSON response parsing (7.x API: `JsonDocument`, `deserializeJson`) |
| `NTPClient` | latest | NTP time synchronisation |
| `Adafruit BusIO` | latest | SPI communication support |
| `WiFiManager` | latest | WiFi captive portal provisioning |

### 8.2 ESP-IDF / Internal Dependencies

| Component | Usage |
|-----------|-------|
| FreeRTOS | Task scheduling (`xTaskCreatePinnedToCore`), mutex (`xSemaphoreCreateMutex`) |
| `esp32/rom/miniz.h` | gzip decompression (`tinfl_decompress`, `tinfl_init`, `TINFL_FLAG_*`) |
| `esp_task_wdt.h` | Task watchdog management (`disableCore0WDT()`) |
| `SPI.h` | Hardware SPI interface (`SPIClass`, `VSPI`) |
| `WiFi.h` | WiFi connectivity (`WiFi.begin`, `WiFi.RSSI`, `WiFi.localIP()`, etc.) |
| `HTTPClient.h` | HTTP requests (`HTTPClient`, `GET`, `getStreamPtr`) |
| `WebServer.h` | HTTP server (config panel) |
| `Preferences.h` | NVS key-value storage (namespace `"weather"`) |
| `time.h` | System time functions (`localtime`, `time_t`, `struct tm`) |

### 8.3 External Services

| Service | Usage | Endpoint |
|---------|-------|----------|
| **QWeather** | Weather data | `devapi.qweather.com` |
| **NTSC** | NTP (primary) | `ntp.ntsc.ac.cn` |
| **Aliyun NTP** | NTP (7 servers) | `ntp1-7.aliyun.com` |
| **Tencent NTP** | NTP (5 servers) | `ntp1-5.tencent.com` |
| **pool.ntp.org** | NTP (final resort) | `pool.ntp.org` |
| **ipip.net** | IP geolocation (primary) | `myip.ipip.net/json` |
| **Bilibili** | IP geolocation (fallback 1) | `api.live.bilibili.com` |
| **LeTV** | IP geolocation (fallback 2) | `g3.letv.com` |

---

## 9. Build & Run

### 9.1 Development Environment

| Item | Requirement |
|------|-------------|
| IDE | VS Code + PlatformIO extension |
| Framework | Arduino (espressif32 platform) |
| Board | `featheresp32` (Adafruit ESP32 Feather) |

### 9.2 Commands

| Action | Command |
|--------|---------|
| Compile | `pio run` |
| Upload | `pio run -t upload` |
| Serial monitor | `pio device monitor -b 115200` |

### 9.3 First-Time Setup

1. Copy `src/config.h.template` → `src/config.h`
2. Edit `src/config.h`: fill in `WEATHER_API_KEY` (register at [dev.qweather.com](https://dev.qweather.com))
3. Build and upload (`pio run -t upload`)
4. Device boots → shows AP name (`ESP32-Weather-XXXX`) on screen
5. Connect phone to `ESP32-Weather-XXXX` Wi-Fi
6. Phone browser opens config portal (or visit `192.168.4.1`)
7. Enter API Key, save
8. Device connects to internet, syncs time, fetches weather

### 9.4 Configuration Notes

**Required** (in `config.h`):
- `WEATHER_API_KEY` — QWeather API key

**Optional**:
- `WEATHER_HOST` — `api.qweather.com` for production (vs `devapi.qweather.com` for dev)
- `WEATHER_LOC` / `WEATHER_NAME` — Default city (fallback if geo resolution fails)

> `config.h` is excluded from version control via `.gitignore` (pattern: `src/config.h`). Use `config.h.template` as a reference.

### 9.5 Build Flags

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=0    # Disable ESP-IDF debug logging
    -Os                     # Optimise for size
```

`-Os` is critical — without size optimisation, the firmware may exceed the 4 MB flash partition, especially with the ~233 KB font library.

### 9.6 Partition Table

```ini
board_build.partitions = huge_app.csv
```

Uses the ESP32 `huge_app` partition scheme, which allocates the maximum possible space (~3 MB) for the application binary, necessary to accommodate the Chinese font data.
