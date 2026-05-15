# FreeRTOS 多任务重构计划

## 概述

将当前 Arduino `loop()` 单线程架构重构为 FreeRTOS 双核多任务架构，使网络请求不再阻塞 UI 刷新。

## 当前问题

- `httpGetDecompressed()` 单个请求最多阻塞 10s，3 次请求 = 30s 卡死 UI
- 加载动画是靠 `delay(80)` + `for` 循环伪装的，不是真正的并发
- WiFi 连接（最长 18s）和 NTP 同步（最长 ~9s）同样阻塞屏幕更新
- Arduino loop 中所有逻辑顺序执行，每秒的 clock 刷新被网络延迟干扰

## 目标架构

```
Core 0 (PRO_CPU)               Core 1 (APP_CPU)
┌─────────────────┐            ┌─────────────────┐
│  networkTask     │            │  uiTask          │
│  ─────────────   │            │  ─────────────   │
│  WiFi 连接维护    │   Mutex    │  按钮检测         │
│  NTP 同步/重试    │◄─────────►│  时钟每秒刷新     │
│  天气 API 请求    │  共享数据   │  天气区域重绘     │
│  (30 分钟周期)    │            │  加载动画绘制     │
└─────────────────┘            └─────────────────┘
```

## 改动文件

### 1. [`main.cpp`](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/main.cpp) — 完全重写

**改动点：**
- 删除 `bootWiFi()`, `bootNTP()`, `refreshWeatherWithUI()` 等顺序启动函数
- 新增 `networkTask(void*)` — WiFi/NTP/天气，固定在 core 0
- 新增 `uiTask(void*)` — 按钮/时钟刷新/天气区域/加载动画，固定在 core 1
- `setup()` 精简为：initDisplay → pinMode → xTaskCreatePinnedToCore × 2 → vTaskDelete(NULL)
- `loop()` 删除

**networkTask 伪代码：**
```cpp
void networkTask(void*) {
    // 立即尝试连接 WiFi（不等人）
    initWiFiAsync();  // 非阻塞启动
    
    while (1) {
        // --- WiFi 保活 ---
        if (WiFi.status() != WL_CONNECTED) {
            if (millis() - lastWifiTry > 5000) WiFi.reconnect();
            vTaskDelay(500);
            continue;
        }
        
        // --- NTP 同步（未成功时每 30s 重试）---
        if (!timeSynced && millis() - lastNtpTry > 30000) {
            mutexTake, update ntpFailReason/ntpTried, mutexGive
            initNTP()  // 阻塞但只阻塞 networkTask
            成功则 mutex 写 timeSynced=true, ntpServer
        }
        
        // --- NTP 定期心跳（已同步时）---
        if (timeSynced) timeClient->update();
        
        // --- 天气刷新（30 分钟周期）---
        if (timeSynced && now - lastWeatherFetch > WEATHER_INTERVAL) {
            networkBusy = true     // volatile flag，通知 uiTask 显示动画
            fetchWeather()         // 阻塞当前任务
            fetchDaily()
            fetchHourly()
            mutexTake, 写入 weather/hourly structs, mutexGive
            weatherUpdated = true  // volatile flag，通知 uiTask 重绘
            networkBusy = false
            lastWeatherFetch = millis()
        }
        
        vTaskDelay(500);  // 500ms 循环
    }
}
```

**uiTask 伪代码：**
```cpp
void uiTask(void*) {
    drawBootScreen();  // 初始启动画面（无网络时也立刻显示）
    int loadingFrame = 0;
    
    while (1) {
        // --- 按钮检测 ---
        btn = digitalRead(BTN_PIN);
        if (falling edge) toggle systemInfo / mainUI
        
        // --- 加载动画（天气获取中）---
        if (networkBusy && !showingSystemInfo) {
            drawLoadingFrame(loadingFrame++ % 8);
        }
        
        // --- 天气区域更新（数据就绪时）---
        if (weatherUpdated && !showingSystemInfo) {
            mutexTake, 读 weather/hourly, mutexGive
            drawWeatherSection()
            drawDetailSection()
            drawHourlyChart()
            weatherUpdated = false
        }
        
        // --- 时间刷新（每秒）---
        curSec = getCurrentSecond()  // 从 timeClient 或 millis 取
        if (curSec != lastSec) {
            drawClockSection()
            drawStatusHeader()
            lastSec = curSec
        }
        
        // --- 系统信息页刷新（打开时每秒更新数据）---
        if (showingSystemInfo) {
            // 每 1 秒重绘一次系统信息页（显示实时数据）
            if (curSec != infoLastSec) {
                drawSystemInfo()
                infoLastSec = curSec
            }
        }
        
        vTaskDelay(50);  // 50ms tick
    }
}
```

### 2. [`weather.h`](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.h) — 增量修改

**改动点：**
- 添加 `extern SemaphoreHandle_t dataMutex;`
- 添加 `extern volatile bool networkBusy;`
- 添加 `extern volatile bool weatherUpdated;`

### 3. [`weather.cpp`](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/weather.cpp) — 增量修改

**改动点：**
- 定义 `SemaphoreHandle_t dataMutex = NULL;`
- 定义 `volatile bool networkBusy = false;`
- 定义 `volatile bool weatherUpdated = false;`
- 在 `initNTP()` 和所有的 `fetch*()` 函数的写入路径上增加 `xSemaphoreTake/Give` 包裹
- 修改 `initWiFi()` 使其支持在 networkTask 中被调用时不阻塞 UI

### 4. [`ui.cpp`](file:///c:/Users/muteh/Documents/PlatformIO/Projects/esp32_st7789_weather_time/src/ui.cpp) — 不改

UI 绘制函数本身不需要改动。`drawClockSection()` 中读取 `state.timeSynced` 的方式保持不变。所有 UI 函数仅在 uiTask 中调用，天然单线程。

`drawSystemInfo()` 中读取所有共享数据（weather/hourly/state）时，由 uiTask 在调用前通过 mutex 保护。

## 数据保护策略

| 数据 | 写入者 | 读取者 | 保护方式 |
|------|--------|--------|----------|
| `weather` (struct) | networkTask | uiTask | `dataMutex` |
| `hourly` (struct) | networkTask | uiTask | `dataMutex` |
| `state.timeSynced/ntpTried` | networkTask | uiTask | `dataMutex` |
| `state.wifiConnected` | networkTask | uiTask | `dataMutex` |
| `networkBusy` | networkTask | uiTask | `volatile bool`（单字节原子） |
| `weatherUpdated` | networkTask | uiTask | `volatile bool`（单字节原子） |
| `timeClient`（NTPClient*） | networkTask | uiTask | 写入后在 `timeSynced=true` 时才可读取，由该 flag + mutex 保护 |

## 启动流程

```
上电 → setup()
        ├─ initDisplay()          ← 1. SPI/屏幕初始化
        ├─ pinMode(BTN)           ← 2. 按钮
        ├─ xTaskCreate(networkTask, core 0)  ← 3. 网络任务
        └─ xTaskCreate(uiTask, core 1)       ← 4. UI 任务
     → vTaskDelete(NULL)          ← 删除 setup/loop 任务

networkTask → 异步启动 WiFi → NTP → 天气...
uiTask      → 实时显示 ["Starting..." / "WiFi..." / 时钟 / 天气]
```

## 缓存 / 保留

注意：以下与当前实现不同，因采用了不同的处理方法

- `WiFi.onEvent()` 回调也可用于保活，但本方案保持原有 polling 方式，不引入新事件模型
- 任务间不引入 xQueue/xEventGroup，仅使用 volatile flags + 单 mutex，保持极简

## 实施步骤（共 3 步）

### Step 1: 修改 weather.h — 添加 FreeRTOS 共享声明
添加 `SemaphoreHandle_t dataMutex`, `volatile networkBusy`, `volatile weatherUpdated` 的 extern 声明。

### Step 2: 修改 weather.cpp — 添加 mutex 保护 + volatile 标志
- 定义上述 3 个变量
- 在 `initNTP()`, `fetchWeather()`, `fetchDaily()`, `fetchHourly()` 的写入路径加 `xSemaphoreTake/Give`
- `initWiFi()` 改为可被 task 调用（添加 `WiFi.mode(WIFI_STA)` 等初始化）

### Step 3: 重写 main.cpp — FreeRTOS 任务入口
- 新增 `networkTask()`, `uiTask()`
- `setup()` 精简
- `loop()` 删除

## 验证标准

- [ ] `pio run` 编译通过，无错误
- [ ] RAM ≤ 18%, Flash ≤ 80%
- [ ] 上电后 100ms 内屏幕亮起（不再等待 WiFi）
- [ ] WiFi 连接过程中时钟走秒正常
- [ ] 天气获取期间加载动画正常旋转（不卡顿）
- [ ] 时钟每秒刷新不抖动
- [ ] 按钮切换系统信息页功能正常
- [ ] 系统信息页数据实时更新
- [ ] NTP 失败后每 30s 自动重试
- [ ] 天气 30 分钟自动刷新
