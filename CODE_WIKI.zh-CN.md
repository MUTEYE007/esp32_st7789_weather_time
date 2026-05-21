# ESP32 ST7789 天气时钟 — 代码百科

## 目录

1. [项目概述](#1-项目概述)
2. [项目结构](#2-项目结构)
3. [硬件配置](#3-硬件配置)
4. [软件架构](#4-软件架构)
5. [模块参考](#5-模块参考)
   - [5.1 配置常量 (config.h)](#51-配置常量-configh)
   - [5.2 显示驱动 (display.h/cpp)](#52-显示驱动-displayhcpp)
   - [5.3 数据与网络 (weather.h/cpp)](#53-数据与网络-weatherhcpp)
   - [5.4 UI 渲染 (ui.h/cpp)](#54-ui-渲染-uihcpp)
   - [5.5 配置服务器 (config_server.h/cpp)](#55-配置服务器-config_serverhcpp)
   - [5.6 中文字库 (gb2312_font.h)](#56-中文字库-gb2312_fonth)
   - [5.7 入口点 (main.cpp)](#57-入口点-maincpp)
6. [数据结构](#6-数据结构)
7. [页面系统与交互](#7-页面系统与交互)
8. [依赖关系](#8-依赖关系)
9. [构建与运行](#9-构建与运行)

---

## 1. 项目概述

**ESP32 ST7789 天气时钟** 是一款基于 ESP32 微控制器、搭配 1.3 寸 240×240 ST7789 TFT LCD 的桌面天气站。它从和风天气（QWeather）API 获取实时天气数据，通过 NTP 同步时间，并支持气象预警、逐分钟级降水预报以及 Web 配置面板。

### 功能特性

- **WiFi 配网** — 首次开机通过 WiFiManager 强制门户自动配置
- **当前天气** — 温度、体感温度、湿度、风向风力（QWeather API）
- **NTP 时间同步** — 非阻塞状态机，支持 15 台备用 NTP 服务器
- **逐时温度图** — 7 点折线图，含插值渲染
- **气象预警** — 多页预警查看器，长文本自动滚动
- **逐分钟降水** — 未来 2 小时降水强度图（24 个 5 分钟时段）
- **系统诊断页** — 芯片信息、WiFi/NTP/API 状态、内存与运行时间
- **Web 配置** — 设备内置 HTTP 服务器，可修改 API Key 和城市
- **中文显示** — 自生成 16×16 点阵 GB2312 字库（7445 个字形，完整标准）
- **FreeRTOS 双核架构** — Core 0 负责网络，Core 1 负责 UI

### 技术栈

| 组件 | 选型 |
|-----------|-----------|
| 主控 | ESP32 (Xtensa LX6 双核) |
| 显示屏 | ST7789 240×240 SPI TFT |
| 框架 | Arduino (espressif32 平台) |
| RTOS | FreeRTOS (ESP-IDF) |
| 构建系统 | PlatformIO |
| 天气 API | 和风天气 (QWeather) |
| 中文字库 | 自生成 GB2312 16×16 位图 (PROGMEM) |

---

## 2. 项目结构

```
esp32_st7789_weather_time/
├── src/                          # 核心源代码
│   ├── main.cpp                  # 入口点，FreeRTOS 任务创建
│   ├── config.h                  # (已 gitignore) 本地配置，含 API Key
│   ├── config.h.template         # 配置模板（可安全提交）
│   ├── display.h / display.cpp   # 显示驱动与绘图原语
│   ├── weather.h / weather.cpp   # 数据结构、WiFi、NTP、API 请求
│   ├── ui.h / ui.cpp             # UI 渲染（所有页面）
│   ├── config_server.h / config_server.cpp  # HTTP 配置服务器
│   └── gb2312_font.h             # GB2312-80 完整字库位图（~233 KB，自动生成）
├── tools/                        # 字库生成与验证工具
│   ├── gen_gb2312_font.py        # GB2312 字库生成器（7445 个字形）
│   └── verify_gb2312_font.py     # 字库正确性校验工具
├── platformio.ini                # PlatformIO 项目配置
├── CODE_WIKI.md                  # 本文档 — 代码百科（英文版）
├── CODE_WIKI.zh-CN.md            # 本文档 — 代码百科（中文版）
└── README.md                     # 项目 README（用户友好）
```

> **注意：** `config.h` 已通过 `.gitignore`（模式：`src/config.h`）排除在版本控制之外。请参照 `config.h.template` 创建你自己的配置。

---

## 3. 硬件配置

### 3.1 物料清单

| 部件 | 型号 |
|------|-------|
| 主控 | ESP32（Adafruit ESP32 Feather 或兼容板） |
| 显示屏 | 1.3 寸 240×240 ST7789 TFT（SPI 接口） |
| 按键 | BOOT 按键（GPIO 0，内部上拉） |

### 3.2 引脚映射

| 功能 | GPIO | 说明 |
|----------|------|-------|
| TFT_CS | 5 | SPI 片选 |
| TFT_DC | 16 | 数据/命令选择 |
| TFT_RST | 17 | 复位 |
| TFT_MOSI | 23 | SPI 主出从入 |
| TFT_SCK | 18 | SPI 时钟 |
| BTN_PIN | 0 | 按键输入（INPUT_PULLUP） |

### 3.3 SPI 初始化

使用 VSPI 硬件 SPI 接口。屏幕旋转设置为 `1`（240×240 横屏模式）。

```cpp
vspi = new SPIClass(VSPI);
vspi->begin(TFT_SCK, -1, TFT_MOSI, -1);
tft = new Adafruit_ST7789(vspi, TFT_CS, TFT_DC, TFT_RST);
tft->init(SCREEN_W, SCREEN_H);
tft->setRotation(1);
tft->fillScreen(COLOR_BG);
tft->setTextWrap(false);
```

### 3.4 分区表

`board_build.partitions = huge_app.csv` — 使用自定义分区表，为应用程序分配更大的空间（约 3 MB），以容纳约 233 KB 的字库。

---

## 4. 软件架构

### 4.1 架构分层

```
┌───────────────────────────────────────────────────────────┐
│  main.cpp — FreeRTOS 双核调度器                            │
│  ├─ Core 0: networkTask — WiFi、NTP、API 数据获取         │
│  └─ Core 1: uiTask — 显示、按键、UI 刷新                   │
├───────────────────────────────────────────────────────────┤
│  ui.cpp — UI 渲染层                                        │
│  ├─ 主页: 状态栏 / 天气 / 时钟 / 详情                      │
│  ├─ 预警页: 多预警翻页 + 滚动                              │
│  ├─ 降水页: 强度柱状图                                      │
│  └─ 系统信息页: 芯片/WiFi/NTP/API 诊断                    │
├───────────────────────────────────────────────────────────┤
│  weather.cpp — 数据与网络层                                 │
│  ├─ WiFi 管理 (WiFiManager 配网)                           │
│  ├─ NTP 时间同步（15 台服务器轮询状态机）                   │
│  ├─ 地理位置解析（3 级降级）                                │
│  └─ 天气 API 请求 (now/24h/3d/alerts/precipitation)       │
├───────────────────────────────────────────────────────────┤
│  display.cpp — 硬件抽象层                                   │
│  ├─ SPI / ST7789 初始化                                    │
│  ├─ 绘图原语: 矩形、标签、WiFi 信号条、动画                │
│  └─ 配网引导屏幕渲染                                       │
├───────────────────────────────────────────────────────────┤
│  config_server.cpp — Web 配置层                             │
│  └─ 端口 80 的 HTTP 服务器（API Key / 城市修改）           │
├───────────────────────────────────────────────────────────┤
│  gb2312_font.h — 字库层                                    │
│  └─ GB2312 16×16 位图字体 + UTF-8 解析器 + 渲染器          │
└───────────────────────────────────────────────────────────┘
```

### 4.2 FreeRTOS 任务布局

| 任务 | 核心 | 栈大小 | 优先级 | 职责 |
|------|------|-------|----------|----------------|
| networkTask | Core 0（协议） | 8 KB | 1 | WiFi 维护、NTP 轮询、所有 API 数据获取 |
| uiTask | Core 1（应用） | 8 KB | 2 | 屏幕渲染、按键处理、定时时钟更新 |

**任务间通信**：

- **互斥锁**（`dataMutex`）— 保护所有共享数据结构（`weather`、`hourly`、`state` 等全局变量）
- **volatile 标志** — `networkBusy`（触发加载动画）、`weatherUpdated`（触发 UI 重绘）
- **轮询** — `uiTask` 每 50 ms 检查一次 `state` 字段

### 4.3 数据流

```
上电
  │
  ▼
setup()
  ├─ 初始化: 串口 / 显示 / 按键 / 互斥锁
  ├─ 创建 networkTask → Core 0
  └─ 创建 uiTask → Core 1

networkTask (Core 0) 循环:
  ├─ WiFiManager::autoConnect()
  │    ├─ 首次启动 → AP 模式 → 手机连接 → WiFi 配置完成
  │    └─ 已配置 → 自动连接
  ├─ initNTP() → processNTP() 轮询（最多 15 台服务器）
  ├─ resolveLocation()          ← 3 级地理定位降级 (ipip → B站 → 乐视)
  ├─ fetchWeather()             ← /v7/weather/now
  ├─ fetchDaily()               ← /v7/weather/3d
  ├─ fetchHourly()              ← /v7/weather/24h
  ├─ fetchWeatherWarnings()     ← /weatheralert/v1/current
  ├─ fetchMinutelyPrecipitation() ← /v7/minutely/5m
  └─ vTaskDelay(500ms)

uiTask (Core 1) 循环:
  ├─ 按键检测
  │    ├─ 短按 (<3s) → 页面切换
  │    └─ 长按 (≥3s) → 重置 WiFi 并重启
  ├─ 配网模式 → AP 引导屏幕
  ├─ 预警页 → 滚动更新
  ├─ 降水页 → 定时刷新
  ├─ 系统信息页 → 秒级更新
  ├─ 主页 → 天气重绘 + 时钟秒级更新
  └─ vTaskDelay(50ms)
```

### 4.4 屏幕布局

```
┌─ 状态栏 ───────────────── (y=2, h=14) ──┐
│ 福州 09:41                      [WiFi]  │
├─ 分割线 1 ────────────────── (y=16) ──┤
│ ⛅ 26°C          H:28°C   ← 天气区    │
│ 福州 晴          L:20°C   (y=20, h=36)│
├─ 时钟区 ───────────────── (y=56, h=58) │
│          12:34  56                      │
│       2026-05-21 Thu                   │
├─ 分割线 3 ───────────────── (y=120) ──┤
│ 体感:24°C  湿度:65%    ← 详情区       │
│ 更新:12:00  风:东北风2级 (y=124, h=34)│
├─ 分割线 4 ───────────────── (y=164) ──┤
│ 12h  ●━━━━━━━━━━━ 18h   ← 逐时图表   │
│ 26°C  26°C  25°C  24°C... (y=168, h=66)│
├─ 底部装饰条 ──────────────── (y=236) ──┤
└─────────────────────────────────────────┘
```

---

## 5. 模块参考

### 5.1 配置常量 ([config.h.template](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config.h.template))

所有硬件引脚、屏幕布局坐标、定时器间隔和 RGB565 颜色常量的集中定义文件。

> **重要：** `config.h` 已通过 `.gitignore` 排除。将 `config.h.template` 复制为 `config.h`，然后填入你的敏感信息。

**网络设置**：

| 宏 | 默认值 | 说明 |
|-------|---------|-------------|
| `WEATHER_API_KEY` | `"your_qweather_api_key"` | 和风天气 API 密钥（用户必须填入） |
| `WEATHER_HOST` | `"devapi.qweather.com"` | API 主机地址 |
| `WEATHER_LOC` | `"101230101"` | 默认城市 Location ID |
| `WEATHER_NAME` | `"Fuzhou"` | 默认城市显示名 |
| `WEATHER_INTERVAL_MS` | `1800000` (30 分钟) | 天气数据获取间隔 |

**硬件引脚**：

| 宏 | 值 |
|-------|-------|
| `TFT_CS` | 5 |
| `TFT_DC` | 16 |
| `TFT_RST` | 17 |
| `TFT_MOSI` | 23 |
| `TFT_SCK` | 18 |
| `BTN_PIN` | 0 |

**屏幕尺寸与布局**：`config.h` 中的所有 `#define` 常量映射了各区域（状态栏、天气、时钟、详情、图表）的固定像素位置。详见[屏幕布局](#44-屏幕布局)。

| 宏 | 值 | 说明 |
|-------|-------|-------------|
| `SCREEN_W` | 240 | 屏幕宽度 |
| `SCREEN_H` | 240 | 屏幕高度 |
| `PAD_LEFT` | 8 | 内容区左内边距 |
| `PAD_RIGHT` | 8 | 内容区右内边距 |
| `CONTENT_W` | 224 | 内容区域宽度 |
| `STATUS_Y` | 2 | 状态栏 Y 起始 |
| `STATUS_H` | 14 | 状态栏高度 |
| `WEATHER_Y` | 20 | 天气区 Y 起始 |
| `WEATHER_H` | 36 | 天气区高度 |
| `CLOCK_Y` | 56 | 时钟区 Y 起始 |
| `CLOCK_H` | 58 | 时钟区高度 |
| `DETAIL_Y` | 124 | 详情区 Y 起始 |
| `DETAIL_H` | 34 | 详情区高度 |
| `CHART_Y` | 168 | 图表区 Y 起始 |
| `CHART_H` | 66 | 图表区高度 |
| `CHART_POINTS` | 7 | 逐时数据点数 |
| `CHART_COL_W` | 34 | 图表点间列宽 |
| `CHART_PT_R` | 4 | 图表数据点圆半径 |
| `HOUR_COUNT` | 7 | 逐时数据数量（与 CHART_POINTS 一致） |

**天气预警和分钟级降水常量（定义在 [weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) 中）**：

| 宏 | 值 | 说明 |
|-------|-------|-------------|
| `WARNING_MAX` | 5 | 最大气象预警数 |
| `MINUTELY_SLOTS` | 24 | 5 分钟降水时段数（2 小时） |
| `WARN_INTERVAL_MS` | 600000（10 分钟） | 预警获取间隔 |
| `MINUTELY_INTERVAL_MS` | 600000（10 分钟） | 分钟级降水获取间隔 |

**RGB565 颜色常量**：

| 常量 | 值 | 用途 |
|----------|-------|-------|
| `COLOR_BG` | `0x0824` | 深蓝色背景 |
| `COLOR_PRIMARY` | `0xFFFF` | 白色文字 |
| `COLOR_CLOCK` | `0x5D9F` | 青色时钟数字 |
| `COLOR_LABEL` | `0x8C14` | 灰色标签 |
| `COLOR_MUTED` | `0x39C7` | 暗色文字 |
| `COLOR_ACCENT` | `0x3C16` | 粉紫色强调色 |
| `COLOR_GREEN` | `0x25E3` | 绿色（WiFi 正常） |
| `COLOR_AMBER` | `0xE526` | 琥珀色（高温） |
| `COLOR_LINE` | `0x10A4` | 区域分割线 |
| `COLOR_GRID` | `0x0843` | 图表网格点 |
| `COLOR_YELLOW` | `0xFFE0` | 太阳图标 |
| `COLOR_CLOUD` | `0xBDD7` | 云图标 |
| `COLOR_RAIN` | `0x4AEF` | 雨图标 |
| `COLOR_GOLD` | `0xFEA0` | 城市名 |
| `WARN_COLOR_RED` | `0xF800` | 极端预警 |
| `WARN_COLOR_ORANGE` | `0xFB00` | 严重预警 |
| `WARN_COLOR_YELLOW` | `0xFFE0` | 中度预警 |
| `WARN_COLOR_BLUE` | `0x001F` | 轻度预警 |

### 5.2 显示驱动 ([display.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.h) / [display.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.cpp))

底层屏幕初始化和绘图原语。

**全局对象**：

| 变量 | 类型 | 说明 |
|----------|------|-------------|
| `vspi` | `SPIClass*` | VSPI 总线对象 |
| `tft` | `Adafruit_ST7789*` | TFT 驱动实例（所有绘图操作都通过此对象） |

**API**：

| 函数 | 签名 | 说明 |
|----------|-----------|-------------|
| `initDisplay()` | `void` | 初始化 SPI + TFT，设置旋转方向和背景色 |
| `drawSectionLine()` | `void(int y)` | 用 `COLOR_LINE` 绘制水平分割线 |
| `drawLabel()` | `void(int x, int y, const char *text)` | 用 `COLOR_LABEL`、textSize=1 绘制标签 |
| `fillArea()` | `void(int x, int y, int w, int h, uint16_t color)` | 填充矩形（封装 `fillRect`） |
| `drawWiFiBars()` | `void(int x, int y, bool connected)` | 绘制 4 级 RSSI 信号条，断连时显示 "X" |
| `drawProvisioningScreen()` | `void(const char *apName, const char *apIP)` | 完整的配网引导屏幕 |
| `updateProvisioningFrame()` | `void(int frame)` | 配网等待时的点阵动画 |
| `drawLongPressRing()` | `void(int cx, int cy, float progress)` | 长按进度环（12 个点，顺时针填充） |
| `animateWipe()` | `void()` | 页面切换动画（6 个块，从上到下清除） |

**WiFi RSSI 映射**（在 `drawWiFiBars` 中）：

| RSSI | 信号条数 |
|------|------|
| > -50 dBm | 4 |
| > -65 dBm | 3 |
| > -80 dBm | 2 |
| ≤ -80 dBm | 1 |

### 5.3 数据与网络 ([weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) / [weather.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.cpp))

核心模块，包含所有数据结构定义、全局变量、WiFi 管理、NTP 同步、地理定位解析和天气 API 请求。

#### 5.3.1 结构体定义

**`WeatherData`** — 当前天气状况：

| 字段 | 类型 | 来源 |
|-------|------|--------|
| `temp` | `String` | `now.temp` |
| `feelsLike` | `String` | `now.feelsLike` |
| `humidity` | `String` | `now.humidity` |
| `windDir` | `String` | `now.windDir` |
| `windScale` | `String` | `now.windScale` |
| `weatherText` | `String` | `now.text`（截断至 21 字节） |
| `weatherIcon` | `String` | `now.icon`（和风天气图标代码） |
| `updateTime` | `String` | `updateTime`（ISO 格式） |
| `tempMax` / `tempMin` | `String` | `daily[0].tempMax/Min`（来自 3d API） |
| `valid` | `bool` | 数据有效性标志 |

**`HourlyData`** — 逐时预报（7 个点）：

| 字段 | 类型 | 说明 |
|-------|------|-------------|
| `hourLabel[7]` | `String[]` | 时间标签（如 "12h"） |
| `temp[7]` | `String[]` | 温度字符串 |
| `icon[7]` | `String[]` | 天气图标代码 |
| `tempInt[7]` | `int[]` | 整数温度（用于图表） |
| `valid` | `bool` | 数据有效性标志 |

**`WarningData`** — 气象预警（最多 5 条）：

| 字段 | 类型 | 说明 |
|-------|------|-------------|
| `eventName` | `String` | 事件名称（如 "台风"） |
| `eventCode` | `String` | 事件代码 |
| `severity` | `String` | 严重程度：extreme/severe/moderate/minor |
| `headline` | `String` | 预警标题 |
| `description` | `String` | 详细描述 |
| `senderName` | `String` | 发布机构 |
| `valid` | `bool` | 数据有效性标志 |

**`MinutelyData`** — 分钟级降水：

| 字段 | 类型 | 说明 |
|-------|------|-------------|
| `summary` | `String` | 降水概要文本 |
| `slots[24]` | `struct {fxTime, precip}` | 24 个 5 分钟预报时段 |
| `valid` | `bool` | 数据有效性标志 |

**`AppState`** — 全局应用状态（互斥锁保护）：

| 字段 | 类型 | 说明 |
|-------|------|-------------|
| `wifiConnected` | `bool` | WiFi 连接状态 |
| `timeSynced` | `bool` | NTP 同步状态 |
| `ntpTried` | `bool` | 是否已尝试过 NTP |
| `weatherLoaded` | `bool` | 天气数据已加载 |
| `locationResolved` | `bool` | 地理位置已解析 |
| `showingSystemInfo` | `bool` | 系统信息页标志 |
| `systemInfoDirty` | `bool` | 需要完全重绘 |
| `provisioningMode` | `bool` | AP 配网模式 |
| `showingWarning` | `bool` | 预警页活动 |
| `showingMinutely` | `bool` | 分钟级降水页活动 |
| `hasActiveWarnings` | `bool` | 存在活动预警 |
| `warningIndex` | `int` | 当前预警索引 |
| `lastWeatherFetch` | `unsigned long` | 上次天气获取时间戳 |
| `lastNtpAttempt` | `unsigned long` | 上次 NTP 尝试时间戳 |
| `lastWarningFetch` | `unsigned long` | 上次预警获取时间戳 |
| `lastMinutelyFetch` | `unsigned long` | 上次分钟级降水获取时间戳 |
| `ntpFailReason[24]` | `char[]` | NTP 失败原因字符串 |
| `ntpServer[32]` | `char[]` | 当前 NTP 服务器名 |
| `apName[24]` | `char[]` | AP 热点名称 |
| `apIP[16]` | `char[]` | AP IP 地址 |
| `bootTime` | `unsigned long` | 启动时间戳（`millis()`） |

#### 5.3.2 全局变量

```cpp
extern WeatherData weather;          // 当前天气
extern HourlyData hourly;            // 逐时预报
extern WarningData warnings[WARNING_MAX]; // 预警数组（最多 5 条）
extern int warningCount;             // 实际预警数量
extern MinutelyData minutely;        // 分钟级降水
extern AppState state;               // 运行时状态
extern String weatherLoc;            // 城市 Location ID
extern String weatherName;           // 城市显示名
extern String weatherLat;            // 城市纬度
extern String weatherLon;            // 城市经度
extern String weatherApiKey;         // 和风天气 API 密钥
extern String weatherHost;           // API 主机地址
extern NTPClient *timeClient;        // NTP 客户端
extern WiFiManager wifiManager;      // WiFi 管理器
extern SemaphoreHandle_t dataMutex;  // 数据互斥锁
extern volatile bool networkBusy;    // 网络繁忙标志
extern volatile bool weatherUpdated; // 天气更新标志
```

此外，weather.cpp 中的静态 NTP 状态机变量：

| 变量 | 类型 | 说明 |
|----------|------|-------------|
| `ntpServers[]` | `const char*[]` | 15 个 NTP 服务器字符串 |
| `ntpServerIdx` | `static int` | 当前服务器索引（-1 = 未活动） |
| `ntpCurServer` | `static const char*` | 当前服务器名指针 |
| `ntpSendMs` | `static unsigned long` | 上次 NTP 请求的时间戳 |

#### 5.3.3 API 函数

**WiFi 与持久化存储**：

| 函数 | 说明 |
|----------|-------------|
| `initWiFiWithProvisioning()` | 从 NVS 加载配置，启动 WiFiManager 强制门户 |
| `loadConfig()` | 从 NVS Preferences 命名空间 `"weather"` 读取 API Key / Host |
| `saveConfig(apiKey, host)` | 将 API Key / Host 写入 NVS Preferences |

**NTP 时间同步** — 非阻塞状态机，支持 15 台服务器：

| 函数 | 说明 |
|----------|-------------|
| `initNTP()` | 初始化 NTP 同步（重置服务器索引，从第一台开始） |
| `processNTP()` | 轮询 NTP 状态机：检查响应 / 超时 / 切换服务器 |
| `advanceNtpServer()` | 切换到列表中的下一台 NTP 服务器（内部函数） |

NTP 服务器列表（按尝试顺序）：
1. `ntp.ntsc.ac.cn`（国家授时中心）
2. `ntp1-7.aliyun.com`（阿里云，7 台服务器）
3. `ntp1-5.tencent.com`（腾讯云，5 台服务器）
4. `pool.ntp.org`（全球池，最后手段）

每台服务器分配 3 秒（`NTP_PER_SERVER_MS`）等待响应。DNS 解析失败也会触发切换到下一台。如果全部 15 台都失败，`ntpTried` 设为 `true`，理由为 `"all servers failed"`。

**地理位置解析** — 3 级降级：

| 函数 | 说明 |
|----------|-------------|
| `resolveLocation()` | 通过 IP 地理定位自动检测城市 |
| `setCityByName(cityName)` | 手动按名称设置城市（由配置服务器调用） |
| `lookupLocationId(searchCity)` | 查询和风天气地理 API，将城市名解析为 Location ID（内部函数） |

解析顺序：
1. **ipip.net** — `myip.ipip.net/json`（首选，解析 `data.location[2]` 为城市）
2. **Bilibili** — `api.live.bilibili.com/xlive/web-room-v1/index/getIpInfo`（降级 1）
3. **LeTV** — `g3.letv.com/r?format=1`（降级 2，解析连字符分隔的位置字符串）

全都失败时，回退到 `WEATHER_LOC` / `WEATHER_NAME` 默认值。

**天气 API 请求** — 全部使用 `httpGetJson()` 并支持 gzip 解压：

| 函数 | 端点 | 说明 |
|----------|----------|-------------|
| `fetchWeather()` | `/v7/weather/now` | 当前天气状况 |
| `fetchHourly()` | `/v7/weather/24h` | 24 小时预报（使用 7 个点） |
| `fetchDaily()` | `/v7/weather/3d` | 3 天预报（高温/低温） |
| `fetchWeatherWarnings()` | `/weatheralert/v1/current` | 活动中的气象预警 |
| `fetchMinutelyPrecipitation()` | `/v7/minutely/5m` | 2 小时降水预报 |

所有请求都包含 `X-QW-Api-Key` 头用于认证。`fetchWeather()` 额外将 `weatherText` 截断为 21 字节（7 个中文字符 × 3 字节）以适应屏幕布局。

#### 5.3.4 HTTP 与 gzip 解压缩

`httpGetJson()` 实现了自定义 gzip 处理：

```
httpGetJson(url, withApiKey)
  │
  ├─ HTTP GET（10 秒超时，User-Agent 和 Accept 头，X-QW-Api-Key 头）
  ├─ 将原始响应读入动态缓冲区（5 秒流超时）
  ├─ 检测 gzip 魔数（0x1F 0x8B）
  │    ├─ 非 gzip → 直接返回明文字符串
  │    └─ Gzip → 跳过头部字段（FEXTRA/FNAME/FCOMMENT/FHCRC）
  │                  └─ tinfl_decompress() deflate 解压缩
  │                     └─ 动态缓冲区扩展（realloc，每次翻倍）
  └─ 返回解压后的 JSON 字符串
```

解压缩使用 ESP32 ROM 的 miniz 库（`esp32/rom/miniz.h`，`tinfl_decompress` 配合 `TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF` 标志）。

`skipGzipHeader()` 函数解析 gzip 标志（`FEXTRA`、`FNAME`、`FCOMMENT`、`FHCRC`）以定位原始 deflate 流，然后将其传递给 `tinfl_decompress`。

#### 5.3.5 URL 编码

`urlEncode(str)` — 自定义 URL 编码器，对非字母数字字符进行百分号编码。用于地理定位查询参数。

---

### 5.4 UI 渲染 ([ui.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.h) / [ui.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.cpp))

所有页面渲染逻辑，按页面组织。

#### 5.4.1 主页面

由 `drawFullUI()` 组装：

| 函数 | 说明 |
|----------|-------------|
| `drawFullUI()` | 编排完整主页：擦除 → 图表 → 状态栏 → 天气 → 时钟 → 详情 → 底部装饰条 |
| `drawStatusHeader()` | 城市名（16px 中文字体）+ 时间（`hh:mm`）+ WiFi 信号条 |
| `updateStatusTime()` | 增量更新时间（不完整重绘） |
| `drawWeatherSection()` | 天气图标 + 大号温度 + 城市名 + 高温/低温 |
| `drawWeatherIcon(cx, cy, code)` | 按和风天气代码绘制天气图标：晴（100/150）、多云（101-104）、雨（300-499）、雪（500-599）、阴（默认） |
| `drawClockSection()` | 大号 HH:MM（textSize=5，青色）+ 秒数（右上角）+ 日期行 |
| `updateClockTime(h, m, s)` | 增量时钟更新（使用背景色传递，不完整重绘） |
| `drawDetailSection()` | 两行详情网格：体感温度 + 湿度（第 1 行），更新时间 + 风向/风力（第 2 行） |
| `drawHourlyChart()` | 7 点折线图，含虚线网格、插值填充线、温度折线、数据点、时间标签 |

**`drawHourlyChart()` 渲染步骤**：
1. 绘制水平网格（3 条虚线，间隔 4px 点阵）
2. 根据温度范围计算 Y 坐标（最小=最大时各加减 1 度）
3. 绘制垂直填充条（网格色到图表底部，线性插值）
4. 绘制温度折线（强调色，点间线性插值）
5. 绘制数据点圆圈（半径 `CHART_PT_R` = 4）
6. 绘制时间标签（底部边缘）和温度值

#### 5.4.2 预警页

| 函数 | 说明 |
|----------|-------------|
| `drawWarningPage()` | 完整预警页：顶部栏、严重度色条、标题、可滚动描述、圆点导航 |
| `updateWarningScroll()` | 每 3 秒将描述文本向上滚动一行 |
| `isWarningScrollNeeded()` | 判断预警描述是否超出视口高度 |

**内部辅助函数**：

| 函数 | 说明 |
|----------|-------------|
| `severityColor(sev)` | 将严重度字符串（`"extreme"` / `"severe"` / `"moderate"`）映射为预警颜色 |
| `contrastColor(bgColor)` | 计算给定 RGB565 背景色的对比色（黑或白），使用亮度加权（R×299 + G×587 + B×114） |
| `wrapText16(text, maxW)` | 将 UTF-8 文本换行以适应 `maxW` 像素。每个 CJK 字符 = 16px，ASCII = 8px。超过宽度时插入换行符（`\n`） |
| `truncateText16(text, maxPx)` | 截断 UTF-8 文本并添加省略号（`...`）以适应 `maxPx` 像素。为 `...` 后缀保留 24px |
| `countLines(text)` | 通过统计 `\n` 字符数（+1）返回行数 |
| `splitLines(text, out[], &count)` | 将 `\n` 分隔的字符串分割为行字符串数组（最大 `MAX_WARN_LINES` = 30） |
| `redrawDots(wi)` | 重绘描述下方的圆点导航指示器。活动圆点 = 实心圆（半径 3），其余 = 较小（半径 2） |
| `redrawHeadline()` | 重绘换行后的标题文本区域 |
| `drawDescLines(offsetY)` | 绘制可滚动的描述中可见的行，裁剪到视口区域 |

**滚动机制**：
- 由静态状态变量驱动（`warnScrollY`、`warnDescLineCount`、`warnScrollActive`、`warnLastScrollMs`）
- 文本换行使用 `wrapText16()`，处理 CJK 字符宽度
- 滚动方向：向上，每次滚动步长 = `WARN_LINE_H`（20px）
- 间隔：`WARN_SCROLL_DELAY` = 3000ms 步进间隔
- 滚动到底部时回到顶部（循环）
- 最大描述长度：400 字符（在 `drawWarningPage()` 中截断）
- `MAX_WARN_LINES` = 30，`WARN_HEADLINE_Y` = 50，`WARN_DOT_Y` = 228

`severityColor()` 映射：
- `"extreme"` → `WARN_COLOR_RED`（0xF800）
- `"severe"` → `WARN_COLOR_ORANGE`（0xFB00）
- `"moderate"` → `WARN_COLOR_YELLOW`（0xFFE0）
- 默认 → `WARN_COLOR_BLUE`（0x001F）

#### 5.4.3 分钟级降水页

| 函数 | 说明 |
|----------|-------------|
| `drawMinutelyPage()` | 完整降水页：含城市名 + 预警指示的页头、概要文本、自定义柱状图、统计信息 |

**图表渲染**：
- X 轴：横跨 2 小时的 24 个时段（每个时段 = 5 分钟）
- Y 轴：降水强度，按最大值缩放（最小底限 0.5 mm）
- 3 级颜色填充：
  - 浅绿色（`0x04B0`）— < 0.5 mm
  - 青色（`0x06BF`）— 0.5–2.0 mm
  - 蓝色（`0x001F`）— > 2.0 mm
- 叠加白色折线连接数据点
- 网格线：3 条水平带，带中文标签（"小" / "中" / "大"）
- X 轴标签："现在"、"1小时后"、"2小时后"
- 底部统计摘要：总降水量和峰值

#### 5.4.4 系统信息页

| 函数 | 说明 |
|----------|-------------|
| `drawSystemInfo()` | 全屏诊断页，4 个区域，双模式渲染 |

**区域**：
1. **ESP32** — 芯片型号、版本、闪存大小、空闲堆、运行时间
2. **WiFi** — SSID、IP、网关、RSSI
3. **NTP** — 同步状态、当前服务器、当前时间
4. **天气 API** — now/3d/24h 状态指示器 + 空闲内存

**双模式渲染**：
- 首次进入（`systemInfoDirty == true`）：完整 `animateWipe()` + 绘制所有区域（静态内容绘制一次）
- 后续更新（`systemInfoDirty == false`）：部分刷新——仅每秒更新运行时间（y=298）和 NTP/API 状态区域（y=350+）

#### 5.4.5 加载动画

| 函数 | 说明 |
|----------|-------------|
| `drawLoadingFrame(frame)` | 8 点旋转加载指示器，当 `networkBusy` 为 true 时显示在详情区域 |

每帧更新一个旋转图案：一个亮色点（`COLOR_CLOCK`）、一个暗色点（`COLOR_ACCENT`）和六个不可见点（`COLOR_LINE`）。下方显示 "Loading" 标签。

#### 5.4.6 文本辅助函数

`textWidth16()` — 计算使用 16px 中文字体渲染 UTF-8 字符串时的像素宽度（CJK = 16px，ASCII = 8px）。用于整个 UI 的布局定位决策。

---

### 5.5 配置服务器 ([config_server.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.h) / [config_server.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.cpp))

基于 `WebServer` 的轻量级 HTTP 配置服务器，监听端口 80。

**路由**：

| 路由 | 方法 | 函数 |
|-------|--------|----------|
| `/` | GET | 配置页面（HTML 表单，内联深色主题 CSS） |
| `/save` | POST | 保存 API Key 和 Host 到 NVS |
| `/setcity` | POST | 设置城市名（通过和风天气地理 API 解析） |

**关键实现细节**：

- `handleRoot()` — 通过拼接 PROGMEM 字符串片段与当前值（`weatherApiKey`、`weatherHost`、`weatherName`、`WiFi.localIP()`）构建页面
- `handleSave()` — 验证非空的 `apiKey` 和 `host`，调用 `saveConfig()`，重置 `lastWeatherFetch = 0` 以触发立即刷新
- `handleSetCity()` — 验证非空的城市名，调用 `setCityByName()`
- 页面预填当前 `weatherApiKey` 和 `weatherHost` 值
- JavaScript `fetch()` API 实现异步提交，带实时状态反馈
- HTML 片段存储在 PROGMEM（`PAGE_HEAD`、`PAGE_MID`、`PAGE_TAIL`、`PAGE_END`）以节省 RAM
- 服务器由 `networkTask` 在 WiFi 连接时一次性启动；`handleConfigClient()` 在每次循环迭代中调用

---

### 5.6 中文字库 ([gb2312_font.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/gb2312_font.h))

自动生成的**完整** GB2312-80 位图字库（01–09 符号区 + 16–55 一级汉字 + 56–87 级汉字），提供 16×16 像素 CJK 字符渲染。

**规格说明**：

| 指标 | 值 |
|--------|-------|
| 字符数 | 7,445（682 个符号 + 3,755 个一级汉字 + 3,008 个二级汉字） |
| 字形数据 | 238,240 字节（每个字形 32 字节：16 行 × uint16_t） |
| 映射表 | 29,780 字节（7,445 × 4 字节条目） |
| 存储 | Flash (PROGMEM) |
| 字体来源 | Windows `simfang.ttf`（仿宋） |
| 验证 | **100% 匹配** vs Python `gb2312` 编码器（GB 2312-80 参考实现） |

**核心内联函数**（定义在头文件中）：

| 函数 | 说明 |
|----------|-------------|
| `findGB2312Glyph(unicode)` | 按 Unicode 码点二分查找字形数据（使用 `pgm_read_word` 安全访问 PROGMEM） |
| `utf8ToUnicode(&p)` | 解码一个 UTF-8 字符（1-3 字节序列），并前移指针 |
| `drawGB16(x, y, text, fg, bg)` | 主渲染器：ASCII（<0x80）使用 TFT 内置字体（8px），CJK 使用位图字体（16px），支持 `\n` 换行 |

**字形数据格式**：

每个字符占用 32 字节（16 行 × 2 字节/行）：
```
行 0: [bit15 bit14 ... bit0]  ← 像素数据
行 1: [bit15 bit14 ... bit0]
...
行 15:[bit15 bit14 ... bit0]
```
每个位代表一个像素：`1` = 前景色，`0` = 透明/背景色。

**码点处理**：
- ASCII（< 0x80）：使用 TFT 内置字体渲染（8px 宽度）
- CJK 统一表意文字（U+4E00–U+9FFF）：通过二分查找在字形表中查找
- CJK 符号（U+3000–303F）：允许通过
- 全角形式（U+FF00–FFEF）：允许通过
- 通用标点（U+2000–206F）：允许通过
- 其他所有非 CJK 码点：跳过，前进 16px（空格）

**字库生成**（参见 [gen_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/gen_gb2312_font.py)）：
1. 遍历完整的 GB2312-80 编码空间（0xA1A1–0xF7FE），跳过未使用区域（0xAA–0xAF = 10–15 区）
2. 使用 Pillow 以 4 倍大小（64×64）渲染每个字符
3. LANCZOS 下采样至 16×16
4. 阈值二值化（80/255）
5. 输出为 PROGMEM 数组 + 二分查找映射表（按 Unicode 排序）
6. **验证** 与 Python `gb2312` 编码器对照（参见 [verify_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/verify_gb2312_font.py)）

**运行**：
```bash
cd tools
python gen_gb2312_font.py         # 重新生成字库头文件
python verify_gb2312_font.py      # 对照标准验证
```

---

### 5.7 入口点 ([main.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/main.cpp))

系统入口点，负责初始化和任务创建。

**`setup()` 执行顺序**：

1. 串口初始化（`Serial.begin(115200)`）
2. 状态字段置零（所有 `state` 字段、`weather.valid`、`hourly.valid`）
3. 创建互斥锁（`xSemaphoreCreateMutex()`）
4. 显示初始化（`initDisplay()`）
5. 按键配置（GPIO 0，`INPUT_PULLUP`）
6. 禁用 Core 0 看门狗（`disableCore0WDT()` — 网络任务可能在 WiFi 上阻塞）
7. 在 Core 0 上创建 `networkTask`（栈 8 KB，优先级 1）
8. 100ms 延迟等待网络任务初始化
9. 在 Core 0 上创建 `uiTask`（栈 8 KB，优先级 2）
10. 如有需要绘制配网屏幕
11. 自删除（`vTaskDelete(NULL)`）

**`networkTask()` 循环**（Core 0）：

```
1. 如果处于配网模式:
   ├─ 处理 WiFiManager (wifiManager.process())
   ├─ WiFi 连接后 → 退出配网模式（设为 FALSE）
   └─ 超时（185s）后 → 退出配网模式，回退到 WIFI_STA
2. 如果 WiFi 断开 → 每 5 秒重新连接 (WiFi.reconnect())
3. 启动配置服务器（一次性，WiFi 连接时）
4. 处理 HTTP 客户端请求 (handleConfigClient())
5. 轮询 NTP 状态机 (processNTP())
6. 如果 NTP 未同步且距离上次尝试超过 30 秒 → 重试 (initNTP())
7. 解析地理位置（一次性，resolveLocation()）
8. 如果 NTP 已同步 → 每日 timeClient->update()
9. 每 WEATHER_INTERVAL_MS（30 分钟）→ 获取所有天气数据
   (fetchWeather → fetchDaily → fetchHourly → fetchWeatherWarnings)
10. vTaskDelay(500ms)
```

**`uiTask()` 循环**（Core 1）：

```
1. 读取 GPIO 0 按键状态 (INPUT_PULLUP)
   ├─ 按下 → 开始计时
   │   └─ 保持 ≥3s → 绘制进度环 → 重置 WiFi → ESP.restart()
   └─ 松开 → 短按 (<3s)，页面切换：
       主页 → 预警(→ 下一个预警) → 降水 → 系统 → 主页
2. 如果配网模式 → 显示 AP 引导屏幕 + 点阵动画
3. 如果预警页 → 每 100ms 更新描述滚动
4. 如果降水页 → 数据过期时刷新 (MINUTELY_INTERVAL_MS 到期)
5. 如果系统信息页 → 每秒更新运行时间/NTP/API 区域
6. 如果 networkBusy → 显示旋转加载动画
7. 如果 weatherUpdated + 2 秒冷却 → 完整重绘 (drawFullUI())
8. 每秒 → 更新状态栏时间 + 时钟 (HH:MM + 秒)
9. vTaskDelay(50ms)
```

**main.cpp 中的静态辅助函数**：

| 函数 | 说明 |
|----------|-------------|
| `showBootScreen(title)` | 绘制强调色顶栏及标题文本 |
| `showBootLine(y, text, color)` | 在指定 Y 位置绘制一行启动消息 |

---

## 6. 数据结构

### 6.1 关系图

```
AppState（全局运行时状态）
  ├── 连接: wifiConnected, timeSynced, ntpTried
  ├── 页面状态: showingSystemInfo, showingWarning, showingMinutely
  ├── 数据状态: weatherLoaded, locationResolved
  ├── 时间戳: lastWeatherFetch, lastNtpAttempt, lastWarningFetch, lastMinutelyFetch
  ├── NTP 信息: ntpFailReason[24], ntpServer[32]
  └── AP 信息: apName[24], apIP[16]

全局数据（互斥锁保护）
  ├── WeatherData weather      ← fetchWeather()
  ├── HourlyData hourly        ← fetchHourly()
  ├── WarningData[WARNING_MAX] + int warningCount ← fetchWeatherWarnings()
  └── MinutelyData minutely    ← fetchMinutelyPrecipitation()

volatile 标志（无锁，原子访问）
  ├── volatile bool networkBusy     ← 网络 I/O 期间设为 true
  └── volatile bool weatherUpdated  ← 数据更新后设为 true
```

### 6.2 设计决策

- **互斥锁保护**：所有对共享结构的读写都必须获取 `dataMutex`。networkTask 写入，uiTask 读取。两者都使用 `portMAX_DELAY` 阻塞式获取。
- **NTP 状态机**：非阻塞轮询，每台服务器 3 秒超时。4 个域共 15 台服务器。DNS 解析失败触发立即切换到下一台。全部失败的状态通过 `state.ntpTried` 明确跟踪。
- **gzip 解压缩**：通过 ESP32 ROM miniz 的 `tinfl_decompress()` 手动处理。如果解压后数据超过当前容量，输出缓冲区每次翻倍（`realloc`）。自定义头部解析逻辑（`skipGzipHeader()`）处理所有 gzip 标志变体。
- **增量 UI 更新**：时钟和状态栏每秒仅重绘变化像素，使用背景色传递（`setTextColor(fg, bg)`），避免全屏闪烁。
- **天气文本截断**：`weatherText` 在 `fetchWeather()` 中截断为 21 字节（UTF-8 安全——7 个中文汉字 × 3 字节）以适应屏幕布局约束。
- **系统信息双模式**：首次进入静态内容绘制一次；后续滴答仅重绘运行时间计数器、NTP 状态和 API 状态——由 `state.systemInfoDirty` 标记。
- **按键去抖**：无显式去抖——依赖 50ms 任务延迟和 GPIO 0 内部上拉。

---

## 7. 页面系统与交互

### 7.1 页面模式

| 模式 | 状态标志 | 进入方式 | 内容 |
|------|-----------|-------|---------|
| 主页 | `!showing*` | 默认 / 从系统页短按返回 | 天气 + 时钟 + 详情 + 图表 |
| 预警 | `showingWarning` | 主页短按 | 多预警翻页 + 滚动描述 |
| 降水 | `showingMinutely` | 预警页短按 | 降水强度图表 + 统计 |
| 系统 | `showingSystemInfo` | 降水页短按 | 芯片/WiFi/NTP/API 诊断 |
| 配网 | `provisioningMode` | 首次开机自动进入 | AP 引导屏幕 + 等待动画 |

### 7.2 页面切换循环

```
主页
  │ 短按 ──→ 预警页
  │           │ 短按 ──→ 降水页
  │           │           │ 短按 ──→ 系统信息页
  │           │           │           │ 短按 ──→ 返回主页
  │ (多条预警) │
  └───────────┘ 如果有多条预警，连续短按会在各条预警之间
                切换（warningIndex++），然后才进入降水页。
```

### 7.3 按键交互

| 操作 | 结果 |
|--------|--------|
| 短按 BOOT（<3s） | 页面切换（如上所述） |
| 长按 BOOT（≥3s） | 重置 WiFi 凭据（`wifiManager.resetSettings()`）并重启（`ESP.restart()`） |

长按会通过屏幕底部的 12 点进度环（`drawLongPressRing()`，cx=120, cy=218）进行可视化。每个点按顺序在 3 秒内填充（12 点 ÷ 3s = 4 点/秒）。

### 7.4 Web 配置

设备连接 WiFi 后，在其 IP 地址（端口 80）打开浏览器即可打开配置页面：

- 修改和风天气 API Key 和 Host
- 更改城市名（通过和风天气地理 API 解析）
- AJAX 异步表单提交，带实时状态反馈
- 深色主题卡片式 UI，内联 CSS
- 预填当前配置值的输入字段

---

## 8. 依赖关系

### 8.1 Arduino 库（PlatformIO 管理）

| 库 | 版本 | 用途 |
|---------|---------|---------|
| `Adafruit ST7735 and ST7789 Library` | latest | ST7789 TFT 显示驱动 |
| `Adafruit GFX Library` | latest | 核心图形原语（形状、文本、颜色） |
| `ArduinoJson` | ^7.0.0 | JSON 响应解析（7.x API：`JsonDocument`、`deserializeJson`） |
| `NTPClient` | latest | NTP 时间同步 |
| `Adafruit BusIO` | latest | SPI 通信支持 |
| `WiFiManager` | latest | WiFi 强制门户配网 |

### 8.2 ESP-IDF / 内部依赖

| 组件 | 用途 |
|-----------|-------|
| FreeRTOS | 任务调度（`xTaskCreatePinnedToCore`）、互斥锁（`xSemaphoreCreateMutex`） |
| `esp32/rom/miniz.h` | gzip 解压缩（`tinfl_decompress`、`tinfl_init`、`TINFL_FLAG_*`） |
| `esp_task_wdt.h` | 任务看门狗管理（`disableCore0WDT()`） |
| `SPI.h` | 硬件 SPI 接口（`SPIClass`、`VSPI`） |
| `WiFi.h` | WiFi 连接（`WiFi.begin`、`WiFi.RSSI`、`WiFi.localIP()` 等） |
| `HTTPClient.h` | HTTP 请求（`HTTPClient`、`GET`、`getStreamPtr`） |
| `WebServer.h` | HTTP 服务器（配置面板） |
| `Preferences.h` | NVS 键值存储（命名空间 `"weather"`） |
| `time.h` | 系统时间函数（`localtime`、`time_t`、`struct tm`） |

### 8.3 外部服务

| 服务 | 用途 | 端点 |
|---------|-------|----------|
| **和风天气 (QWeather)** | 天气数据 | `devapi.qweather.com` |
| **国家授时中心 (NTSC)** | NTP（首选） | `ntp.ntsc.ac.cn` |
| **阿里云 NTP** | NTP（7 台服务器） | `ntp1-7.aliyun.com` |
| **腾讯云 NTP** | NTP（5 台服务器） | `ntp1-5.tencent.com` |
| **pool.ntp.org** | NTP（最终手段） | `pool.ntp.org` |
| **ipip.net** | IP 地理定位（首选） | `myip.ipip.net/json` |
| **Bilibili** | IP 地理定位（降级 1） | `api.live.bilibili.com` |
| **乐视 (LeTV)** | IP 地理定位（降级 2） | `g3.letv.com` |

---

## 9. 构建与运行

### 9.1 开发环境

| 项目 | 要求 |
|------|-------------|
| IDE | VS Code + PlatformIO 扩展 |
| 框架 | Arduino（espressif32 平台） |
| 开发板 | `featheresp32`（Adafruit ESP32 Feather） |

### 9.2 命令

| 操作 | 命令 |
|--------|---------|
| 编译 | `pio run` |
| 上传 | `pio run -t upload` |
| 串口监视器 | `pio device monitor -b 115200` |

### 9.3 首次设置

1. 将 `src/config.h.template` 复制为 `src/config.h`
2. 编辑 `src/config.h`：填入 `WEATHER_API_KEY`（在 [dev.qweather.com](https://dev.qweather.com) 注册）
3. 编译并上传（`pio run -t upload`）
4. 设备启动 → 屏幕显示 AP 名称（`ESP32-Weather-XXXX`）
5. 手机连接 `ESP32-Weather-XXXX` WiFi
6. 手机浏览器打开配置门户（或访问 `192.168.4.1`）
7. 输入 API Key，保存
8. 设备连接互联网，同步时间，获取天气

### 9.4 配置说明

**必需**（在 `config.h` 中）：
- `WEATHER_API_KEY` — 和风天气 API 密钥

**可选**：
- `WEATHER_HOST` — 生产环境用 `api.qweather.com`（开发环境用 `devapi.qweather.com`）
- `WEATHER_LOC` / `WEATHER_NAME` — 默认城市（地理定位失败时的回退）

> `config.h` 已通过 `.gitignore`（模式：`src/config.h`）排除在版本控制之外。请参照 `config.h.template`。

### 9.5 构建标志

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=0    # 禁用 ESP-IDF 调试日志
    -Os                     # 优化体积
```

`-Os` 至关重要——没有体积优化，固件可能超出 4 MB 闪存分区，尤其是在包含约 233 KB 字库的情况下。

### 9.6 分区表

```ini
board_build.partitions = huge_app.csv
```

使用 ESP32 `huge_app` 分区方案，为应用程序固件分配最大可用空间（约 3 MB），为容纳中文字库数据所必需。
