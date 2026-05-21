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
10. [Development Tools](#10-development-tools)

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
- **Chinese Text Rendering** — Self-generated 16×16 dot-matrix GB2312 font (4,437 glyphs)
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
├── .vscode/
│   └── extensions.json           # VS Code extension recommendations
├── tools/
│   ├── gen_gb2312_font.py        # GB2312 font generator (primary)
│   ├── test_weather_text_sim.py  # Python simulation test for text layout
│   └── cities.json               # City name data for font generation
├── src/                          # Core source code
│   ├── main.cpp                  # Entry point, FreeRTOS task creation
│   ├── config.h                  # (gitignored) Local configuration with API Key
│   ├── config.h.template         # Configuration template (safe to commit)
│   ├── display.h / display.cpp   # Display driver & drawing primitives
│   ├── weather.h / weather.cpp   # Data structures, WiFi, NTP, API requests
│   ├── ui.h / ui.cpp             # UI rendering (all pages)
│   ├── config_server.h / config_server.cpp  # HTTP configuration server
│   └── gb2312_font.h             # GB2312 level-1 bitmap font (~142 KB, auto-generated)
├── platformio.ini                # PlatformIO project configuration
├── ui_layout.html                # Screen layout reference (HTML simulation)
└── README.md                     # Project README
```

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

`board_build.partitions = huge_app.csv` — uses a custom partition table with a larger application slot to accommodate the ~142 KB font library.

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
│  ├─ Geo-location resolution (3-tier fallback: ipip→B站→乐视)│
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
  ├─ initNTP() → processNTP() round-robin
  ├─ resolveLocation()          ← 3-tier geo lookup
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

### 5.1 Configuration Constants ([src/config.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config.h))

Central definition file for all hardware pins, screen layout coordinates, interval timers, and RGB565 colour constants.

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

**Refresh intervals**:

| Macro | Value | Description |
|-------|-------|-------------|
| `WARN_INTERVAL_MS` | 600,000 (10 min) | Warning fetch interval |
| `MINUTELY_INTERVAL_MS` | 600,000 (10 min) | Minutely precipitation fetch interval |

### 5.2 Display Driver ([src/display.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.h) / [display.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.cpp))

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

### 5.3 Data & Network ([src/weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) / [weather.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.cpp))

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
| `bootTime` | `unsigned long` | Boot timestamp |

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

#### 5.3.3 API Functions

**WiFi & persistent storage**:

| Function | Description |
|----------|-------------|
| `initWiFiWithProvisioning()` | Load config from NVS, start WiFiManager |
| `loadConfig()` | Read API Key / Host from NVS Preferences |
| `saveConfig(apiKey, host)` | Write API Key / Host to NVS Preferences |

**NTP time synchronisation** — Non-blocking state machine with 15 servers:

| Function | Description |
|----------|-------------|
| `initNTP()` | Initialise NTP sync (reset server index) |
| `processNTP()` | Poll NTP state machine: check response / timeout / switch |
| `advanceNtpServer()` | Advance to next NTP server in the list |

NTP server list (in order of attempted use):
1. `ntp.ntsc.ac.cn` (National Time Service Center, China)
2. `ntp1-7.aliyun.com` (Alibaba Cloud, 7 servers)
3. `ntp1-5.tencent.com` (Tencent Cloud, 5 servers)
4. `pool.ntp.org` (global pool, last resort)

Each server is given 3 seconds (`NTP_PER_SERVER_MS`) to respond before advancing. If all 15 fail, `ntpTried` is set to `true`.

**Geo-location resolution** — 3-tier fallback:

| Function | Description |
|----------|-------------|
| `resolveLocation()` | Auto-detect city via IP geolocation |
| `setCityByName(cityName)` | Manually set city by name |

Resolution order:
1. **ipip.net** — `myip.ipip.net/json` (primary)
2. **Bilibili** — `api.live.bilibili.com` (fallback)
3. **LeTV** — `g3.letv.com` (fallback)

If all fail, uses hardcoded `WEATHER_LOC` / `WEATHER_NAME`.

**Weather API requests** — All use `httpGetJson()` with gzip decompression:

| Function | Endpoint | Description |
|----------|----------|-------------|
| `fetchWeather()` | `/v7/weather/now` | Current conditions |
| `fetchHourly()` | `/v7/weather/24h` | 24-hour forecast (7 points used) |
| `fetchDaily()` | `/v7/weather/3d` | 3-day forecast (high/low temps) |
| `fetchWeatherWarnings()` | `/weatheralert/v1/current` | Active weather alerts |
| `fetchMinutelyPrecipitation()` | `/v7/minutely/5m` | 2-hour precipitation forecast |

All requests include the `X-QW-Api-Key` header for authentication.

#### 5.3.4 HTTP & gzip Decompression

`httpGetJson()` implements custom gzip handling:

```
httpGetJson(url, withApiKey)
  │
  ├─ HTTP GET (10s timeout, X-QW-Api-Key header)
  ├─ Read raw response into buffer
  ├─ Detect gzip magic bytes (0x1F 0x8B)
  │    ├─ Not gzipped → return as plain string
  │    └─ Gzipped → skip header fields (FEXTRA/FNAME/FCOMMENT/FHCRC)
  │                  └─ tinfl_decompress() deflate decompression
  │                     └─ Dynamic buffer expansion (realloc, double each pass)
  └─ Return decompressed JSON string
```

Decompression uses the ESP32 ROM's miniz library (`esp32/rom/miniz.h`, `tinfl_decompress`).

### 5.4 UI Rendering ([src/ui.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.h) / [ui.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.cpp))

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
| `updateClockTime(h, m, s)` | Incremental clock update (no full redraw) |
| `drawDetailSection()` | Two-row detail: feels-like + humidity (row 1), update time + wind (row 2) |
| `drawHourlyChart()` | 7-point line chart with grid, interpolated lines, data points, labels |

**`drawHourlyChart()`** rendering steps:
1. Draw horizontal grid (3 dashed lines)
2. Calculate Y coordinates from temperature range
3. Draw vertical fill bars (grid colour to chart bottom)
4. Draw temperature polyline (accent colour, linear interpolation)
5. Draw data point circles (radius 4)
6. Draw time labels (bottom) and temperature values

#### 5.4.2 Alert Page

| Function | Description |
|----------|-------------|
| `drawWarningPage()` | Full alert page: header bar, severity colour strip, headline, scrollable description, dot navigation |
| `updateWarningScroll()` | Scroll description text by one line every 3 seconds |

Scrolling is driven by static state variables (`warnScrollY`, `warnDescLineCount`, `warnScrollActive`). Text wrapping uses `wrapText16()` which handles CJK character widths.

`severityColor()` maps severity strings to colours:
- `"extreme"` → `WARN_COLOR_RED` (0xF800)
- `"severe"` → `WARN_COLOR_ORANGE` (0xFB00)
- `"moderate"` → `WARN_COLOR_YELLOW` (0xFFE0)
- default → `WARN_COLOR_BLUE` (0x001F)

#### 5.4.3 Minutely Precipitation Page

| Function | Description |
|----------|-------------|
| `drawMinutelyPage()` | Full precipitation page: summary text + custom bar chart + statistics |

Chart rendering:
- X-axis: 24 slots spanning 2 hours
- Y-axis: precipitation intensity, scaled to max value (minimum floor 0.5 mm)
- 3-tier colour fill: light green (<0.5mm) → teal (0.5-2mm) → blue (>2mm)
- Overlaid white polyline
- Summary text at bottom: total precipitation and peak value

#### 5.4.4 System Info Page

| Function | Description |
|----------|-------------|
| `drawSystemInfo()` | Full-screen diagnostic page with 4 sections |

Sections:
1. **ESP32** — Chip model, revision, flash size, free heap, uptime
2. **WiFi** — SSID, IP, gateway, RSSI
3. **NTP** — Sync status, server in use, current time
4. **Weather API** — now/3d/24h status indicators

First entry does a full `animateWipe()` + draw. Subsequent updates only refresh the uptime and NTP sections every second.

#### 5.4.5 Loading Animation

| Function | Description |
|----------|-------------|
| `drawLoadingFrame(frame)` | 8-dot rotating loading indicator, displayed in the detail section area when `networkBusy` is true |

#### 5.4.6 Text Helper

`textWidth16()` — Calculates pixel width of a UTF-8 string when rendered in 16px Chinese font (CJK = 16px, ASCII = 8px). Used for layout positioning decisions.

### 5.5 Config Server ([src/config_server.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.h) / [config_server.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.cpp))

Lightweight HTTP configuration server based on `WebServer`, listening on port 80.

**Routes**:

| Route | Method | Function |
|-------|--------|----------|
| `/` | GET | Configuration page (HTML form, inline dark-theme CSS) |
| `/save` | POST | Save API Key and Host to NVS |
| `/setcity` | POST | Set city name (resolved via QWeather geo API) |

**Key implementation details**:

- `handleRoot()` — Builds the page by concatenating PROGMEM string fragments with current values
- `handleSave()` — Validates non-empty `apiKey` and `host`, calls `saveConfig()`, resets `lastWeatherFetch = 0` to trigger immediate refresh
- `handleSetCity()` — Validates non-empty city name, calls `setCityByName()`
- Page pre-fills input fields with current `weatherApiKey` and `weatherHost` values
- JavaScript `fetch()` API for async form submission with status feedback

### 5.6 Chinese Font Library ([src/gb2312_font.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/gb2312_font.h))

Auto-generated GB2312-80 Level 1 bitmap font (including 01-09 symbol region), providing 16×16 pixel CJK character rendering.

**Specifications**:

| Metric | Value |
|--------|-------|
| Characters | 4,437 (GB2312 Level 1 + symbols) |
| Glyph data | 141,984 bytes (32 bytes per glyph: 16 rows × uint16_t) |
| Mapping table | 17,748 bytes (4,437 × 4-byte entries) |
| Storage | Flash (PROGMEM) |
| Font source | Windows `simfang.ttf` (FangSong) |

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

Non-CJK codepoints in certain ranges are explicitly allowed through (CJK symbols U+3000-303F, fullwidth forms U+FF00-FFEF, general punctuation U+2000-206F, etc.). All other non-CJK codepoints are skipped with a 16px advance.

### 5.7 Entry Point ([src/main.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/main.cpp))

System entry point responsible for initialisation and task creation.

**`setup()` execution order**:

1. Serial init (`Serial.begin(115200)`)
2. State field zeroing (all `state` fields, `weather.valid`, `hourly.valid`)
3. Mutex creation (`xSemaphoreCreateMutex()`)
4. Display init (`initDisplay()`)
5. Button config (GPIO 0, `INPUT_PULLUP`)
6. Core 0 WDT disabled (`disableCore0WDT()` — network task may block)
7. `networkTask` created on Core 0 (stack 8 KB, priority 1)
8. 100ms delay for network task to start
9. Provisioning screen drawn if needed
10. `uiTask` created on Core 1 (stack 8 KB, priority 2)
11. Self-deletion (`vTaskDelete(NULL)`)

**`networkTask()` loop** (Core 0):

```
1. If provisioning mode:
   ├─ Process WiFiManager
   ├─ On WiFi connected → exit provisioning mode
   └─ On timeout (185s) → exit provisioning mode, fall back to STA
2. If WiFi disconnected → reconnect every 5 seconds
3. Start config server (one-time)
4. Handle HTTP client requests
5. Poll NTP state machine
6. If NTP not synced and 30s elapsed → retry
7. Resolve location (one-time)
8. If NTP synced → daily timeClient update
9. Every WEATHER_INTERVAL_MS → fetch all weather data
10. vTaskDelay(500ms)
```

**`uiTask()` loop** (Core 1):

```
1. Read button state
   ├─ Pressed → start timing
   │   └─ If held ≥3s → reset WiFi & restart
   └─ Released → on short press, page cycle:
       Main → Alert → Minutely → System → Main
2. If provisioning mode → show AP guide screen + animation
3. If alert page → update scrolling
4. If minutely page → refresh data if stale
5. If system info page → update once per second
6. If network busy → show loading animation
7. If weather updated + 2s cooldown → full redraw
8. Every second → update clock (HH:MM + seconds)
9. vTaskDelay(50ms)
```

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

- **Mutex protection**: All reads/writes to shared structures must acquire `dataMutex`. networkTask writes, uiTask reads.
- **NTP state machine**: Non-blocking round-robin with 3-second per-server timeout. 15 servers total. All-fail state is explicitly tracked.
- **gzip decompression**: Manual handling via `tinfl_decompress` from ESP32 ROM miniz. Output buffer doubles on each pass if decompressed data exceeds capacity.
- **Incremental UI updates**: Clock and status bar redraw only changed pixels every second, avoiding full-screen flicker.
- **Weather text truncation**: `weatherText` truncated to 21 bytes (UTF-8 safe — 7 Chinese chars × 3 bytes) in `fetchWeather()` to fit screen layout constraints.

---

## 7. Page System & Interaction

### 7.1 Page Modes

| Mode | State Flag | Entry | Content |
|------|-----------|-------|---------|
| Main | `!showing*` | Default / from system page short press | Weather + clock + details + chart |
| Alert | `showingWarning` | Main page short press | Multi-alert paging + scrolling description |
| Minutely | `showingMinutely` | Alert page short press | Precipitation intensity chart + stats |
| System | `showingSystemInfo` | Minutely page short press | Chip/WiFi/NTP/API diagnostics |
| Provisioning | `provisioningMode` | Automatic on first boot | AP guide screen |

### 7.2 Page Transition Cycle

```
Main Page
  │ short press ──→ Alert Page
  │                   │ short press ──→ Minutely Page
  │                                    │ short press ──→ System Info Page
  │                                                     │ short press ──→ back to Main
```

Within the Alert page, if multiple alerts exist, successive short presses cycle through individual alerts before advancing to the Minutely page.

### 7.3 Button Interaction

| Action | Result |
|--------|--------|
| Short press BOOT (<3s) | Page cycle (as above) |
| Long press BOOT (≥3s) | Reset WiFi credentials and restart |

Long press is visualised with a 12-dot progress ring at the bottom of the screen. Each dot fills sequentially over 3 seconds.

### 7.4 Web Configuration

When the device is connected to WiFi, visiting its IP address (port 80) opens the configuration page:

- Modify QWeather API Key and Host
- Change city name (resolved via QWeather geo API)
- AJAX async submission with real-time status feedback

---

## 8. Dependencies

### 8.1 Arduino Libraries (PlatformIO-managed)

| Library | Version | Purpose |
|---------|---------|---------|
| `Adafruit ST7735 and ST7789 Library` | latest | ST7789 TFT display driver |
| `Adafruit GFX Library` | latest | Core graphics primitives |
| `ArduinoJson` | ^7.0.0 | JSON response parsing |
| `NTPClient` | latest | NTP time synchronisation |
| `Adafruit BusIO` | latest | SPI communication support |
| `WiFiManager` | latest | WiFi captive portal provisioning |

### 8.2 ESP-IDF / Internal Dependencies

| Component | Usage |
|-----------|-------|
| FreeRTOS | Task scheduling and mutex |
| `esp32/rom/miniz.h` | gzip decompression (`tinfl_decompress`) |
| `esp_task_wdt.h` | Task watchdog management |
| `SPI.h` | Hardware SPI interface |
| `WiFi.h` | WiFi connectivity |
| `HTTPClient.h` | HTTP requests |
| `WebServer.h` | HTTP server (config panel) |
| `Preferences.h` | NVS key-value storage |
| `time.h` | System time functions |

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
3. Build and upload
4. Device boots → shows AP name on screen
5. Connect phone to `ESP32-Weather-XXXX` Wi-Fi
6. Phone browser opens config portal (or visit 192.168.4.1)
7. Enter API Key, save
8. Device connects to internet, syncs time, fetches weather

### 9.4 Configuration Notes

**Required** (in `config.h`):
- `WEATHER_API_KEY` — QWeather API key

**Optional**:
- `WEATHER_HOST` — `api.qweather.com` for production
- `WEATHER_LOC` / `WEATHER_NAME` — Default city

> `config.h` is excluded from version control via `.gitignore` (pattern: `src/config.h`). Use `config.h.template` as a reference.

### 9.5 Build Flags

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=0    # Disable ESP-IDF debug logging
    -Os                     # Optimise for size
```

`-Os` is critical — without size optimisation, the firmware may exceed the 4 MB flash partition.

---

## 10. Development Tools

### 10.1 Font Generator — [tools/gen_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/gen_gb2312_font.py)

Generates the GB2312 bitmap font C header from a Windows TTF font.

**Process**:
1. Iterate GB2312-80 encoding space (0xA1A1-0xD7FE), skip unused regions (0xAA-0xAF)
2. Render each character at 4× size (64×64) using Pillow
3. LANCZOS downscale to 16×16
4. Threshold binarise (80/255)
5. Output as PROGMEM array + binary-search mapping table (sorted by Unicode)

**Running**:
```bash
cd tools
pip install Pillow
python gen_gb2312_font.py
```
Output: `src/gb2312_font.h`

Requires `simfang.ttf` (方正仿宋) at `C:/Windows/Fonts/simfang.ttf`.

### 10.2 Layout Simulation Test — [tools/test_weather_text_sim.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/test_weather_text_sim.py)

PC-side simulation of the ESP32 weather text truncation and layout logic. Tests 17 edge cases including:
- UTF-8 byte-level truncation at 21-byte boundary
- Chinese character width calculation vs available space
- City name + weather description same-line/wrap decision
- Edge cases: empty city, Latin city names, very long descriptions

**Running**:
```bash
cd tools
python test_weather_text_sim.py
```

### 10.3 City Data — [tools/cities.json](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/cities.json)

Chinese province and city name data file, used by the font generator scripts as a character source.
