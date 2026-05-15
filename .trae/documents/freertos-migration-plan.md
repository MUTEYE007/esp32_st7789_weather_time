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
- 在 `initNTP()` 和所有的 `fetch*()` 函数的写入路径上增加