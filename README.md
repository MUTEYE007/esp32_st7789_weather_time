# ESP32 ST7789 天气时钟

基于 ESP32 微控制器和 **1.3 寸 240×240 ST7789 TFT 液晶屏** 的桌面天气站项目。通过和风天气（QWeather）API 获取实时天气数据，支持 NTP 时间同步、气象预警、分钟级降水预测，具备 Web 配置页面和 FreeRTOS 双核架构。

---

## 目录

1. [项目概述](#1-项目概述)
2. [项目结构](#2-项目结构)
3. [硬件配置](#3-硬件配置)
4. [软件架构](#4-软件架构)
5. [主要模块说明](#5-主要模块说明)
6. [关键数据结构](#6-关键数据结构)
7. [页面与交互](#7-页面与交互)
8. [依赖关系](#8-依赖关系)
9. [运行方式](#9-运行方式)
10. [辅助工具](#10-辅助工具)

---

## 1. 项目概述

### 核心功能

- **WiFi 自动配网** — 首次使用通过 WiFiManager 开启配置门户，手机连接后配置 Wi-Fi
- **实时天气** — 通过和风天气 API 获取当前温度、体感温度、湿度、风向风力
- **时钟显示** — 通过 NTP 自动同步时间（内置 15 台 NTP 服务器轮询），断网时使用运行计时
- **未来 24 小时温度曲线** — 折线图展示 7 个时间点的预报温度
- **天气预警** — 获取并展示气象灾害预警信息（支持滚动长文本、多预警分页）
- **分钟级降水** — 未来 2 小时分钟级降水强度图表（24 个 5 分钟时段）
- **系统状态页** — 显示芯片信息、WiFi 状态、NTP 状态、API 状态
- **Web 配置** — 设备接入网络后可浏览器访问配置页面，修改 API Key 和城市
- **GB2312 中文显示** — 通过自生成 16×16 位图字库支持中文渲染（4437 个字符）
- **FreeRTOS 双核架构** — 网络任务运行在 Core 0，UI 任务运行在 Core 1

### 技术栈

| 组件 | 技术选型 |
|------|---------|
| 微控制器 | ESP32 (Xtensa LX6 双核) |
| 显示驱动 | ST7789 (SPI 接口, 240×240) |
| 开发框架 | Arduino (espressif32 Platform) |
| 实时系统 | FreeRTOS (ESP-IDF) |
| 构建工具 | PlatformIO |
| 天气数据源 | 和风天气 QWeather API |
| 中文字库 | 自生成 GB2312 16×16 点阵字库 (PROGMEM) |

---

## 2. 项目结构

```
esp32_st7789_weather_time/
├── .vscode/
│   └── extensions.json              # VS Code 扩展推荐配置
├── Lib/                              # PlatformIO Python 工具链依赖包
├── tools/
│   ├── gen_gb2312_font.py            # GB2312 字库生成脚本（主字库）
│   ├── gen_font.py                   # 备用字库生成脚本（从 cities.json）
│   ├── test_weather_text_sim.py      # 天气文本截断/布局 PC 模拟测试
│   └── cities.json                   # 城市名称数据文件
├── src/                              # 核心源代码
│   ├── main.cpp                      # 程序入口，FreeRTOS 任务创建
│   ├── config.h                      # 硬件引脚、颜色、布局常量定义
│   ├── config.h.template             # 配置模板文件
│   ├── display.h / display.cpp       # 显示驱动初始化与基础绘图 API
│   ├── weather.h / weather.cpp       # 数据结构、WiFi/NTP、API 请求
│   ├── ui.h / ui.cpp                 # UI 渲染（所有页面绘制）
│   ├── config_server.h / config_server.cpp  # Web 配置服务器
│   └── gb2312_font.h                 # GB2312 一级字库位图（自动生成，~142KB）
├── platformio.ini                    # PlatformIO 项目配置
├── ui_layout.html                    # 屏幕布局参考 HTML
├── compile_commands.json             # 编译命令数据库
└── README.md                         # 本文档
```

---

## 3. 硬件配置

### 3.1 核心组件

| 组件 | 型号 |
|------|------|
| 主控 | ESP32 (Adafruit ESP32 Feather 或兼容板) |
| 屏幕 | 1.3 寸 240×240 ST7789 TFT (SPI 接口) |
| 按键 | BOOT 按钮 (GPIO 0，内部上拉) |

### 3.2 引脚连接

| 功能 | GPIO | 描述 |
|------|------|------|
| TFT_CS | 5 | SPI 片选 |
| TFT_DC | 16 | 数据/命令选择 |
| TFT_RST | 17 | 复位 |
| TFT_MOSI | 23 | SPI 主出从入 |
| TFT_SCK | 18 | SPI 时钟 |
| BTN_PIN | 0 | 按键输入 (INPUT_PULLUP) |

### 3.3 SPI 配置

使用 VSPI 硬件 SPI 接口，屏幕旋转 `setRotation(1)` 实现 240×240 横屏模式。

```cpp
vspi = new SPIClass(VSPI);
vspi->begin(TFT_SCK, -1, TFT_MOSI, -1);
tft = new Adafruit_ST7789(vspi, TFT_CS, TFT_DC, TFT_RST);
tft->init(SCREEN_W, SCREEN_H);
tft->setRotation(1);
```

---

## 4. 软件架构

### 4.1 架构层次

```
┌─────────────────────────────────────────────────────┐
│  main.cpp — FreeRTOS 双核调度层                      │
│  ├─ Core 0: networkTask — WiFi/NTP/天气API          │
│  └─ Core 1: uiTask — 显示/按键/UI 刷新              │
├─────────────────────────────────────────────────────┤
│  ui.cpp — UI 渲染层                                  │
│  ├─ 主页面: 状态栏/天气/时钟/详情/温度折线图          │
│  ├─ 预警页面: 多预警分页 + 滚动长描述                 │
│  ├─ 降水页面: 分钟降水强度柱状图                     │
│  └─ 系统信息页面: 芯片/WiFi/NTP/API 诊断信息         │
├─────────────────────────────────────────────────────┤
│  weather.cpp — 数据/网络层                           │
│  ├─ WiFi 管理 (WiFiManager 配网)                    │
│  ├─ NTP 时间同步 (15 台服务器轮询状态机)              │
│  ├─ 地理位置解析 (三源降级: ipip→B站→乐视)           │
│  └─ 天气 API 请求 (实时/24h/3d/预警/分钟降水)         │
├─────────────────────────────────────────────────────┤
│  display.cpp — 硬件抽象层                            │
│  ├─ SPI/ST7789 初始化                               │
│  ├─ 基础绘图: 矩形/标签/WiFi 图标/动画               │
│  └─ 配网提示屏幕渲染                                │
├─────────────────────────────────────────────────────┤
│  config_server.cpp — Web 配置层                      │
│  └─ HTTP 服务器 (API Key/城市修改)                   │
├─────────────────────────────────────────────────────┤
│  gb2312_font.h — 字库层                             │
│  └─ GB2312 16×16 位图字库 + UTF-8 解析渲染函数       │
└─────────────────────────────────────────────────────┘
```

### 4.2 FreeRTOS 双核任务

| 任务 | 核心 | 栈大小 | 优先级 | 职责 |
|------|------|--------|--------|------|
| networkTask | Core 0 (协议栈) | 8KB | 1 | WiFi 连接维护、NTP 同步、所有 API 数据拉取 |
| uiTask | Core 1 (应用) | 8KB | 2 | 屏幕渲染、按键响应、时钟定时更新 |

**任务间通信**：

- **互斥锁** (`dataMutex`)：保护所有共享数据（`weather`、`hourly`、`state` 等全局变量）
- **volatile 标志**：`networkBusy`（触发加载动画）、`weatherUpdated`（触发 UI 重绘）
- **轮询读取**：uiTask 每 50ms 检查 `state` 状态字段

### 4.3 数据流

```
开机
  │
  ▼
setup()
  ├─ 初始化: 串口 / 显示 / 按键 / 互斥锁
  ├─ 创建 networkTask → Core 0
  └─ 创建 uiTask → Core 1

networkTask (Core 0):
  ├─ WiFiManager::autoConnect()
  │    ├─ 首次启动 → 开 AP → 配网 → 连接
  │    └─ 已配网 → 自动连接
  ├─ initNTP() → processNTP() 轮询
  ├─ resolveLocation()          ← 三级降级定位
  ├─ fetchWeather()             ← /v7/weather/now
  ├─ fetchDaily()               ← /v7/weather/3d
  ├─ fetchHourly()              ← /v7/weather/24h
  ├─ fetchWeatherWarnings()     ← /weatheralert/v1/current
  ├─ fetchMinutelyPrecipitation() ← /v7/minutely/5m
  └─ loop (500ms 间隔)

uiTask (Core 1):
  ├─ 开机画面显示
  ├─ WiFi 配网界面 (如需)
  ├─ 主页面 drawFullUI()
  │    ├─ drawStatusHeader()    — 城市/时间/WiFi
  │    ├─ drawWeatherSection()  — 图标/温度/描述/高低温
  │    ├─ drawClockSection()    — 大数字时钟
  │    ├─ drawDetailSection()   — 体感/湿度/更新/风
  │    └─ drawHourlyChart()     — 温度折线图
  ├─ 页面切换: 主页→预警→降水→系统→主页
  └─ 每秒更新时钟显示
```

### 4.4 屏幕布局

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

## 5. 主要模块说明

### 5.1 配置模块 ([src/config.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config.h))

所有硬件引脚、屏幕布局坐标、颜色常量的集中定义。

**网络配置**：
- `WEATHER_API_KEY` — 和风天气 API Key（需自行填入）
- `WEATHER_HOST` — API 服务器地址（默认 `devapi.qweather.com`）
- `WEATHER_LOC` / `WEATHER_NAME` — 默认城市 Location ID 和名称
- `WEATHER_INTERVAL_MS` — 天气刷新间隔（30 分钟）

**颜色定义**（RGB565 格式）：

| 常量 | 值 | 用途 |
|------|-----|------|
| `COLOR_BG` | `0x0824` | 深蓝背景 |
| `COLOR_PRIMARY` | `0xFFFF` | 白色文字 |
| `COLOR_CLOCK` | `0x5D9F` | 青色时钟 |
| `COLOR_ACCENT` | `0x3C16` | 粉紫强调色 |
| `COLOR_GOLD` | `0xFEA0` | 金色城市名 |
| `COLOR_GREEN` | `0x25E3` | 绿色（WiFi） |
| `WARN_COLOR_RED` | `0xF800` | 预警红色 |
| `WARN_COLOR_ORANGE` | `0xFB00` | 预警橙色 |
| `WARN_COLOR_YELLOW` | `0xFFE0` | 预警黄色 |
| `WARN_COLOR_BLUE` | `0x001F` | 预警蓝色 |

### 5.2 显示模块 ([src/display.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.h) / [display.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/display.cpp))

负责 ST7789 屏幕的底层初始化和基础绘图原语。

**全局对象**：

| 变量 | 类型 | 说明 |
|------|------|------|
| `vspi` | `SPIClass*` | VSPI 总线对象 |
| `tft` | `Adafruit_ST7789*` | TFT 驱动对象（所有绘制均通过此指针） |

**API 函数**：

| 函数 | 说明 |
|------|------|
| `initDisplay()` | 初始化 SPI 和 TFT，设置旋转/背景色 |
| `drawSectionLine(y)` | 绘制分割横线 |
| `drawLabel(x, y, text)` | 绘制标签文字 (COLOR_LABEL, textSize=1) |
| `fillArea(x, y, w, h, color)` | 填充矩形区域 |
| `drawWiFiBars(x, y, connected)` | 绘制 WiFi 信号格 (4 级 RSSI 映射) 或断开"X" |
| `drawProvisioningScreen(apName, apIP)` | 配网引导界面 |
| `updateProvisioningFrame(frame)` | 配网等待动画 (4 点循环) |
| `drawLongPressRing(cx, cy, progress)` | 长按进度环 (12 个点顺时针填充) |
| `animateWipe()` | 页面切换擦除动画 (6 块从上到下逐块清除) |

### 5.3 数据与网络模块 ([src/weather.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) / [weather.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.cpp))

核心模块，包含所有数据结构定义、全局变量、WiFi/NTP 管理、地理位置解析和天气 API 请求。

#### 5.3.1 数据结构

**WeatherData** — 当前天气数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `temp` | `String` | 当前温度 |
| `feelsLike` | `String` | 体感温度 |
| `humidity` | `String` | 湿度百分比 |
| `windDir` | `String` | 风向 |
| `windScale` | `String` | 风力等级 |
| `weatherText` | `String` | 天气现象文字 |
| `weatherIcon` | `String` | 天气图标代码 |
| `updateTime` | `String` | 数据更新时间 |
| `tempMax` / `tempMin` | `String` | 当日最高/最低温度 |
| `valid` | `bool` | 数据是否有效 |

**HourlyData** — 逐小时预报（7 个时间点）

| 字段 | 类型 | 说明 |
|------|------|------|
| `hourLabel[7]` | `String[]` | 时间标签 (如 "12h") |
| `temp[7]` | `String[]` | 温度字符串 |
| `icon[7]` | `String[]` | 天气图标代码 |
| `tempInt[7]` | `int[]` | 温度整数值（用于折线图） |
| `valid` | `bool` | 数据是否有效 |

**WarningData** — 气象预警（最多 5 条）

| 字段 | 类型 | 说明 |
|------|------|------|
| `eventName` | `String` | 预警事件名称 |
| `eventCode` | `String` | 事件代码 |
| `severity` | `String` | 严重程度 (extreme/severe/moderate/minor) |
| `headline` | `String` | 预警标题 |
| `description` | `String` | 详细描述 |
| `senderName` | `String` | 发布单位 |
| `valid` | `bool` | 数据是否有效 |

**MinutelyData** — 分钟级降水

| 字段 | 类型 | 说明 |
|------|------|------|
| `summary` | `String` | 降水概要文字 |
| `slots[24]` | `struct {fxTime, precip}` | 24 个 5 分钟预测时段 |
| `valid` | `bool` | 数据是否有效 |

**AppState** — 全局应用状态

集中管理运行时状态标志和时间戳：

| 字段 | 类型 | 说明 |
|------|------|------|
| `wifiConnected` | `bool` | WiFi 连接状态 |
| `timeSynced` | `bool` | NTP 同步状态 |
| `weatherLoaded` | `bool` | 天气数据加载状态 |
| `locationResolved` | `bool` | 地理位置解析状态 |
| `showingSystemInfo` | `bool` | 系统信息页面标志 |
| `showingWarning` | `bool` | 预警页面标志 |
| `showingMinutely` | `bool` | 分钟降水页面标志 |
| `provisioningMode` | `bool` | 配网模式标志 |
| `hasActiveWarnings` | `bool` | 是否有活跃预警 |
| `warningIndex` | `int` | 当前查看的预警索引 |
| `lastWeatherFetch` / `lastNtpAttempt` 等 | `ulong` | 各类操作的时间戳 |
| `ntpFailReason[24]` | `char[]` | NTP 失败原因 |
| `ntpServer[32]` | `char[]` | 当前 NTP 服务器名 |
| `apName[24]` / `apIP[16]` | `char[]` | 配网 AP 信息 |
| `bootTime` | `ulong` | 启动时间戳 |

**全局变量**：

```cpp
extern WeatherData weather;        // 当前天气
extern HourlyData hourly;          // 逐时预报
extern WarningData warnings[5];    // 预警数组
extern int warningCount;           // 实际预警数量
extern MinutelyData minutely;      // 分钟降水数据
extern AppState state;             // 运行状态
extern String weatherLoc;          // 城市 Location ID
extern String weatherName;         // 城市显示名
extern String weatherLat/Lon;      // 城市经纬度
extern String weatherApiKey/Host;  // API 配置
extern NTPClient *timeClient;      // NTP 客户端
extern WiFiManager wifiManager;    // WiFi 管理器
extern SemaphoreHandle_t dataMutex;// 数据互斥锁
extern volatile bool networkBusy;  // 网络忙标志
extern volatile bool weatherUpdated;// 天气更新标志
```

#### 5.3.2 核心 API 函数

**WiFi 与存储**：

| 函数 | 说明 |
|------|------|
| `initWiFiWithProvisioning()` | 从 NVS 加载配置，启动 WiFiManager 配网 |
| `loadConfig()` | 从 NVS Preferences 读取 API Key / Host |
| `saveConfig(apiKey, host)` | 写入 NVS Preferences |

**NTP 时间同步**：

采用多服务器轮询状态机（15 台服务器，每台超时 3 秒），服务器列表包括：国家授时中心、阿里云 ×7、腾讯云 ×5、pool.ntp.org。

| 函数 | 说明 |
|------|------|
| `initNTP()` | 初始化 NTP 同步（重置服务器索引） |
| `processNTP()` | NTP 状态机轮询，检查响应/超时/切换服务器 |
| `advanceNtpServer()` | 切换至下一台 NTP 服务器 |

**地理位置解析**：

三级降级策略获取城市名：ipip.net → B站 API → 乐视 API，获取城市后通过和风地理 API 解析为 Location ID 和经纬度。

| 函数 | 说明 |
|------|------|
| `resolveLocation()` | 自动获取城市位置（三级降级） |
| `setCityByName(cityName)` | 手动设置城市 |

**天气 API 请求**：

所有请求通过 `httpGetJson()` 发送，支持 gzip 解压缩（使用 `tinfl_decompress`）。

| 函数 | API 端点 | 说明 |
|------|----------|------|
| `fetchWeather()` | `/v7/weather/now` | 实时天气 |
| `fetchHourly()` | `/v7/weather/24h` | 24 小时逐时预报 |
| `fetchDaily()` | `/v7/weather/3d` | 3 天预报（高低温） |
| `fetchWeatherWarnings()` | `/weatheralert/v1/current` | 气象灾害预警 |
| `fetchMinutelyPrecipitation()` | `/v7/minutely/5m` | 分钟级降水预测 |

#### 5.3.3 HTTP 与 gzip 解压流程

```
httpGetJson(url, withApiKey)
  │
  ├─ HTTP GET 请求 (超时 10s, 含 X-QW-Api-Key 头)
  ├─ 读取原始响应到缓冲区
  ├─ 检测 gzip 头部 (0x1F 0x8B)
  │    ├─ 非 gzip → 直接返回字符串
  │    └─ 是 gzip → 跳过头部字段 (FEXTRA/FNAME/FCOMMENT/FHCRC)
  │                  └─ tinfl_decompress() deflate 解压
  │                     └─ 动态扩展输出缓冲区 (realloc 翻倍)
  └─ 返回解压后的 JSON 字符串
```

### 5.4 界面模块 ([src/ui.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.h) / [ui.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.cpp))

所有 UI 页面的渲染逻辑。

**主页面渲染** (`drawFullUI()`)：

| 函数 | 说明 |
|------|------|
| `drawStatusHeader()` | 状态栏：城市名 + 时间 + WiFi 信号 |
| `updateStatusTime()` | 增量更新状态栏时间 |
| `drawWeatherSection()` | 天气区域：图标 + 温度 + 城市名 + 高低温 |
| `drawWeatherIcon(cx, cy, code)` | 绘制天气图标（太阳/云/雨/雪） |
| `drawClockSection()` | 时钟区域：大字号 HH:MM + 秒 + 日期 |
| `updateClockTime(h, m, s)` | 增量更新时钟显示 |
| `drawDetailSection()` | 详情区域：体感/湿度/更新时间/风 |
| `drawHourlyChart()` | 温度折线图：7 个时间点的连线图 + 数据标注 |

**预警页面** (`drawWarningPage()`)：

| 函数 | 说明 |
|------|------|
| `drawWarningPage()` | 绘制完整预警页（标题栏+严重程度色条+滚动描述） |
| `updateWarningScroll()` | 预警描述滚动更新（每 3 秒一行） |
| `isWarningScrollNeeded()` | 判断是否需要滚动 |

**分钟降水页面** (`drawMinutelyPage()`)：

| 函数 | 说明 |
|------|------|
| `drawMinutelyPage()` | 绘制分钟降水页（柱状图 + 三段色阶 + 统计） |

**系统信息页面** (`drawSystemInfo()`)：

| 函数 | 说明 |
|------|------|
| `drawSystemInfo()` | 系统诊断信息（芯片/WiFi/NTP/API 四个区块） |

**辅助函数**：

| 函数 | 说明 |
|------|------|
| `drawLoadingFrame(frame)` | 网络加载旋转动画 |
| `drawFullUI()` | 完整主页面组装（擦除动画后依次绘制各区域） |

### 5.5 配置服务器 ([src/config_server.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.h) / [config_server.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/config_server.cpp))

基于 `WebServer` 库的轻量 HTTP 配置界面，监听端口 80。

| 路由 | 方法 | 功能 |
|------|------|------|
| `/` | GET | 配置页面（HTML 表单，内联 CSS 深色主题） |
| `/save` | POST | 保存 API Key 和 Host |
| `/setcity` | POST | 设置城市名（通过和风地理 API 解析） |

配置页面支持 AJAX 异步提交和状态反馈，预填当前值。

### 5.6 中文字库 ([src/gb2312_font.h](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/gb2312_font.h))

自动生成的 GB2312 一级字库（含符号区），提供 CJK 字符的 16×16 像素点阵渲染。

| 指标 | 值 |
|------|-----|
| 字符数 | 4,437 个（GB2312 Level 1 + 01-09 区符号） |
| 字模数据 | 141,984 字节（每个字符 32 字节） |
| 映射表 | 17,748 字节（每个条目 4 字节） |
| 存储 | Flash (PROGMEM) |
| 字体来源 | Windows simfang.ttf (方正仿宋) |

**核心内联函数**（定义在头文件中）：

| 函数 | 说明 |
|------|------|
| `findGB2312Glyph(unicode)` | 二分查找字符位图（PROGMEM-safe） |
| `utf8ToUnicode(&p)` | UTF-8 解码（支持 1-3 字节序列） |
| `drawGB16(x, y, text, fg, bg)` | 主渲染函数（ASCII 用内置字体，CJK 用位图字库） |

### 5.7 主程序 ([src/main.cpp](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/main.cpp))

系统入口，负责初始化和任务创建。

**`setup()` 执行流程**：

1. 串口初始化 (`Serial.begin(115200)`)
2. 状态字段清零
3. 创建互斥锁 (`xSemaphoreCreateMutex`)
4. 显示初始化 (`initDisplay()`)
5. 按键配置 (GPIO 0, `INPUT_PULLUP`)
6. 禁用 Core 0 看门狗 (`disableCore0WDT()`)
7. 创建 `networkTask` pinned to Core 0
8. 创建 `uiTask` pinned to Core 1
9. 删除 setup 任务 (`vTaskDelete(NULL)`)

**`networkTask()` 循环**：

```
1. 配网模式 → 处理 WiFiManager，185s 超时退出
2. WiFi 断开 → 每 5 秒重连
3. 启动 Web 配置服务器（一次性）
4. 处理 HTTP 客户端请求
5. NTP 同步轮询（15 台服务器）
6. 地理位置解析（一次性）
7. 定时拉取天气/逐时/3天/预警/分钟降水
8. vTaskDelay(500ms)
```

**`uiTask()` 循环**：

```
1. 按键检测
   ├─ 短按 (<3s) → 页面切换
   └─ 长按 (≥3s) → 重置 WiFi 并重启
2. 配网模式 → 显示引导界面 + 等待动画
3. 预警页 → 滚动更新
4. 降水页 → 定时刷新数据
5. 系统信息页 → 每秒更新状态
6. 主页面 → 天气重绘 + 每秒时钟刷新
7. vTaskDelay(50ms)
```

---

## 6. 关键数据结构

### 6.1 数据结构关系图

```
AppState (全局运行状态)
  ├── 连接状态: wifiConnected, timeSynced, ntpTried
  ├── 页面状态: showingSystemInfo, showingWarning, showingMinutely
  ├── 数据状态: weatherLoaded, locationResolved
  ├── 时间戳: lastWeatherFetch, lastNtpAttempt, lastWarningFetch, lastMinutelyFetch
  ├── NTP 信息: ntpFailReason[24], ntpServer[32]
  └── AP 信息: apName[24], apIP[16]

全局数据 (Mutex 保护)
  ├── WeatherData weather      ← fetchWeather()
  ├── HourlyData hourly        ← fetchHourly()
  ├── WarningData[5] + int     ← fetchWeatherWarnings()
  └── MinutelyData minutely    ← fetchMinutelyPrecipitation()

Volatile 标志 (无锁原子读写)
  ├── volatile bool networkBusy     ← 网络存取期间置 true
  └── volatile bool weatherUpdated  ← 数据更新后置 true
```

### 6.2 设计要点

- **Mutex 保护**：所有共享数据读写必须通过 `xSemaphoreTake/give` 包裹，networkTask 写入，uiTask 读取
- **NTP 状态机**：非阻塞方式，每 3 秒检查响应，超时自动切换，15 台全部失败后标记失败
- **gzip 解压**：手动处理 HTTP gzip 响应，支持动态缓冲区扩容
- **增量更新**：时钟和状态栏每秒仅重绘变化部分，避免全屏闪烁

---

## 7. 页面与交互

### 7.1 页面模式

| 模式 | 标志 | 进入方式 | 内容 |
|------|------|----------|------|
| 主页面 | `!showing*` | 默认 / 从系统页短按 | 天气 + 时钟 + 详情 + 折线图 |
| 预警页 | `showingWarning` | 主页面短按 | 逐条预警 + 滚动描述 |
| 降水页 | `showingMinutely` | 预警页短按 | 降水强度图 + 统计 |
| 系统页 | `showingSystemInfo` | 降水页短按 | 系统诊断信息 |
| 配网页 | `provisioningMode` | 自动 | 配网引导界面 |

### 7.2 按键交互

| 操作 | 动作 |
|------|------|
| 短按 BOOT (<3s) | 页面切换（主→预警→降水→系统→主） |
| 长按 BOOT (≥3s) | 清除 WiFi 配置并重启 |

### 7.3 Web 配置

设备连接 WiFi 后，浏览器访问设备 IP（端口 80）即可打开配置页面：

- 修改和风天气 API Key 和 Host
- 设置城市名（通过和风地理 API 解析）
- 异步 AJAX 提交，实时状态反馈

---

## 8. 依赖关系

### 8.1 Arduino 库（PlatformIO 自动管理）

| 库 | 用途 | 版本 |
|----|------|------|
| `Adafruit ST7735 and ST7789 Library` | ST7789 TFT 驱动 | latest |
| `Adafruit GFX Library` | 基础图形绘制 | latest |
| `ArduinoJson` | JSON 解析 | ^7.0.0 |
| `NTPClient` | NTP 时间同步 | latest |
| `Adafruit BusIO` | SPI 通信支持 | latest |
| `WiFiManager` | WiFi 配网管理 | latest |

### 8.2 ESP-IDF / 内部依赖

| 组件 | 用途 |
|------|------|
| FreeRTOS | 多任务调度 |
| `esp32/rom/miniz.h` | gzip 解压缩 (tinfl_decompress) |
| `esp_task_wdt.h` | 任务看门狗 |
| `SPI.h` | SPI 硬件接口 |
| `WiFi.h` | WiFi 连接 |
| `HTTPClient.h` | HTTP 请求 |
| `WebServer.h` | HTTP 服务器 |
| `Preferences.h` | NVS 存储 |
| `time.h` | 系统时间 |

### 8.3 外部 API 与服务

| 服务 | 用途 | 端点 |
|------|------|------|
| **和风天气** | 天气数据 | `devapi.qweather.com` |
| **国家授时中心** | NTP | `ntp.ntsc.ac.cn` |
| **阿里云 NTP** | NTP (备用) | `ntp1-7.aliyun.com` |
| **腾讯云 NTP** | NTP (备用) | `ntp1-5.tencent.com` |
| **pool.ntp.org** | NTP (最终) | `pool.ntp.org` |
| **ipip.net** | IP 定位 (首选) | `myip.ipip.net/json` |
| **B站** | IP 定位 (降级1) | `api.live.bilibili.com` |
| **乐视** | IP 定位 (降级2) | `g3.letv.com` |

---

## 9. 运行方式

### 9.1 开发环境

| 项目 | 要求 |
|------|------|
| IDE | VS Code + PlatformIO 扩展 |
| 框架 | Arduino (espressif32 platform) |
| 板型 | `featheresp32` (Adafruit ESP32 Feather) |

### 9.2 常用命令

| 操作 | 命令 |
|------|------|
| 编译 | `pio run` |
| 上传 | `pio run -t upload` |
| 串口监控 | `pio device monitor -b 115200` |

### 9.3 首次使用流程

1. **烧录固件**：将 `config.h.template` 复制为 `config.h`，填入和风天气 API Key，编译上传
2. **配网**：设备启动→屏幕显示 AP 名称和 IP→手机连接 ESP32 Wi-Fi 热点
3. **配置**：手机浏览器自动打开配置页面（或访问 192.168.4.1）
4. **保存**：填入和风天气 API Key 并保存
5. **完成**：设备自动连接互联网→同步时间→拉取天气数据

### 9.4 配置说明

**必须配置**（在 `config.h` 中）：

- `WEATHER_API_KEY` — 和风天气 API Key（[免费注册](https://dev.qweather.com/)获取）

**可选配置**：

- `WEATHER_HOST` — API 主机地址（生产环境建议 `api.qweather.com`）
- `WEATHER_LOC` / `WEATHER_NAME` — 默认城市 Location ID 和名称
- `WEATHER_INTERVAL_MS` — 天气刷新间隔（默认 30 分钟）

> `config.h` 中的敏感信息不应提交到版本控制。项目提供 `config.h.template` 作为模板，使用时复制为 `config.h` 并填入真实值。

---

## 10. 辅助工具

### 10.1 字库生成 — [tools/gen_gb2312_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/gen_gb2312_font.py)

将 Windows TTF 字体转换为嵌入式 C 头文件位图字库。

**工作原理**：
1. 遍历 GB2312-80 编码空间 (0xA1A1-0xD7FE)
2. 跳过空白区域 (0xAA-0xAF)
3. 每个字符 4× 放大渲染到 64×64 位图
4. LANCZOS 缩放到 16×16，按阈值二值化
5. 输出为 PROGMEM 数组 + 二分查找映射表

**运行**：
```bash
cd tools
python gen_gb2312_font.py
```
输出：`src/gb2312_font.h`

### 10.2 字体生成（备用） — [tools/gen_font.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/gen_font.py)

从 `cities.json` 中的城市名提取中文字符生成字库，基于 msyh.ttc（微软雅黑）。

### 10.3 布局模拟测试 — [tools/test_weather_text_sim.py](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/test_weather_text_sim.py)

在 PC 端模拟 ESP32 上的天气文字截断和布局逻辑，测试 UTF-8 字节截断边界、中文字符宽度计数、城市名+天气描述同行/换行判断等 17 个边界案例。

**运行**：
```bash
cd tools
python test_weather_text_sim.py
```

### 10.4 城市数据 — [tools/cities.json](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/tools/cities.json)

中国主要城市名称数据文件，包含各省市名称。
