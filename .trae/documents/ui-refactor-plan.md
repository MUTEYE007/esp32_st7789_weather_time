# ESP32 桌面小电视 - 全面重构计划

## Summary

将 1323 行单文件 `main.cpp` 拆分为 8 个模块文件，重新设计为现代扁平 UI 风格，提取布局常量消除魔法数字，优化折线图并添加加载动画。所有现有显示内容保留不减。

## 当前状态分析

- **单文件**: `src/main.cpp` 1323 行，包含全部逻辑
- **魔法数字**: 所有像素坐标直接硬编码（如 `setCursor(56, 24)`）
- **代码重复**: `fetchWeather/fetchHourly/fetchDaily` 三个函数包含几乎相同的 HTTP+gzip 解压逻辑（各约 60 行）
- **UI 风格**: CRT 仿真（双线边框 + 红色标题栏），将改为现代扁平风格
- **全局状态**: 三个匿名结构体，无封装
- **内存管理**: 多个手动 `malloc/free` 路径，有泄漏风险

## 文件结构设计

```
src/
├── main.cpp              (~80 行)  主入口 setup()/loop()
├── config.h              (~60 行)  WiFi/API 引脚/颜色/布局常量
├── display.h / display.cpp (~60 行)  屏幕初始化 + 绘制工具函数
├── hanzi_font.h          (~200 行) 汉字点阵数据 + 渲染函数
├── ui.h / ui.cpp         (~350 行) 主界面 + 系统信息界面绘制
└── weather.h / weather.cpp (~200 行) 天气 API 请求 + NTP 同步
```

## UI 重新设计 - 现代扁平风格

### 设计原则
- 去掉 CRT 双线边框和红色标题栏
- 深色背景 + 清晰的视觉层次
- 简洁分隔线替代厚重边框
- 更好的间距和对齐
- 保留所有现有显示内容

### 新布局 (240x240)

```
┌──────────────────────────────────────────┐
│  Fuzhou · 12:34             [WiFi bars]  │  y=2~14   状态栏
│──────────────────────────────────────────│  y=16
│                                          │
│  [☀️icon]   25°C           H:28° L:19°  │  y=20~52  天气主区
│             Sunny                        │
│                                          │
│──────────────────────────────────────────│  y=58
│                                          │
│            12 : 30                       │  y=64~116 时钟区
│              25                          │
│         2025-05-15 Thu                   │
│                                          │
│──────────────────────────────────────────│  y=120
│                                          │
│  体感 26°C    Hum 65%                    │  y=124~156 详情区
│  Wind NE-3    更新 12:30                  │
│                                          │
│──────────────────────────────────────────│  y=160
│  [15h] [16h] [17h] [18h] [19h] [20h]    │  y=164~218 小时预报
│  ─ ─ ─ ─ ─ ─ ─ ─ (网格线)              │
│    ╲    ╱╲                               │
│     ╲╱    ╲╱  (折线图 + 渐变填充)        │
│  26°  25°  24°  23°  22°  22°           │
│                                          │
│━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━│  y=236~238 底部强调线
└──────────────────────────────────────────┘
```

### 新配色方案

| 用途 | 颜色 | 16-bit 值 |
|------|------|-----------|
| 背景 | 深黑 #0D1117 | 0x0824 |
| 主文字/温度 | 纯白 #FFFFFF | 0xFFFF |
| 时钟数字 | 亮青 #58D5E3 | 0x5D9F |
| 日期/标签 | 浅灰蓝 #8B95A5 | 0x8C14 |
| 次要文字 | 中灰 #6E7681 | 0x39C7 |
| 强调色 | 蓝色 #388BFD | 0x3C16 |
| 成功状态 | 绿色 #3FB950 | 0x25E3 |
| 警告状态 | 琥珀 #E3B341 | 0xE526 |
| 分隔线 | 暗灰 #21262D | 0x10A4 |
| 图表网格 | 深蓝灰 #161B22 | 0x0843 |
| 图表线 | 蓝色 #388BFD | 0x3C16 |
| 图表填充 | 深蓝 #1A3A5C | 0x1190 |

## 实施步骤

### Step 1: 创建 `config.h` — 配置与常量
→ verify: 编译通过，所有常量定义完整

创建 `src/config.h`，包含：
- WiFi SSID/密码
- 天气 API 配置（KEY, HOST, LOC, NAME, INTERVAL）
- TFT 引脚定义（CS, DC, RST, MOSI, SCK）
- 按钮引脚（BTN_PIN）
- 所有颜色常量（新配色方案）
- 布局常量：
  - 屏幕尺寸 `SCREEN_W=240, SCREEN_H=240`
  - 内容区边距 `PAD_LEFT=8, PAD_RIGHT=8, CONTENT_W=224`
  - 各区域 Y 坐标和高度：
    - `STATUS_Y=2, STATUS_H=14`
    - `WEATHER_Y=20, WEATHER_H=38`
    - `CLOCK_Y=64, CLOCK_H=52`
    - `DETAIL_Y=124, DETAIL_H=36`
    - `CHART_Y=164, CHART_H=54`
    - `BAR_Y=236, BAR_H=2`
  - 分隔线 Y 坐标：`LINE1_Y=16, LINE2_Y=58, LINE3_Y=120, LINE4_Y=160`
  - 时钟字体尺寸常量
  - 小时预报布局：`CHART_COL_W=34, CHART_START_X=20, CHART_POINTS=6`

### Step 2: 创建 `hanzi_font.h` — 汉字点阵数据与渲染
→ verify: 编译通过，drawHanziText 函数可用

从 `main.cpp` 提取：
- `HANZI_W`, `HANZI_H` 定义
- `HanziDef` 结构体
- `HANZI_DATA[]` 数组（155 个汉字，L29-L186）
- `findHanzi()` 函数
- `drawHanzi()` 函数
- `drawHanziText()` 函数
- 使用 `extern Adafruit_ST7789* tft` 引用全局显示对象

### Step 3: 创建 `display.h / display.cpp` — 屏幕初始化与工具
→ verify: 编译通过

从 `main.cpp` 提取并重构：
- `initDisplay()` — SPI 总线 + ST7789 初始化
- `extern` 声明 `tft` 指针
- 新增辅助函数：
  - `drawSectionLine(int y)` — 绘制分隔线（使用 `LINE_COLOR`）
  - `drawLabel(int x, int y, const char* text)` — 绘制标签文字（灰色小字）
  - `drawValue(int x, int y, const char* text)` — 绘制数值文字（白色）
  - `drawValueInt(int x, int y, int val)` — 绘制整数值
  - `fillBackground()` — 填充背景色
  - `drawWiFiBars(int x, int y)` — 绘制 WiFi 信号条

### Step 4: 创建 `weather.h / weather.cpp` — 网络与数据
→ verify: 编译通过，HTTP 请求逻辑统一

从 `main.cpp` 提取并重构：
- 数据结构（`weather`, `hourly`, `state` 改为命名结构体）
- 公共 HTTP+gzip 解压函数 `httpGetDecompressed(url)` → 返回 `String`
  - 统一处理：HTTP 请求、流读取、gzip 头跳过、tinfl 解压
  - 统一内存管理：所有分配在函数内，单一出口释放
  - 消除三个 fetch 函数中 ~180 行重复代码
- `fetchWeather()` — 调用 httpGetDecompressed，解析实时天气 JSON
- `fetchHourly()` — 调用 httpGetDecompressed，解析 24h 预报 JSON
- `fetchDaily()` — 调用 httpGetDecompressed，解析 3d 预报 JSON
- `iconToCN()` — 天气代码转中文名
- `windDirToEn()` — 风向中文转英文
- `urlEncode()` — URL 编码工具

### Step 5: 创建 `ui.h / ui.cpp` — UI 绘制
→ verify: 编译通过，所有 UI 函数可用

从 `main.cpp` 提取并重新设计：

**现有函数重构（使用布局常量替代魔法数字）：**
- `drawStatusHeader()` — 新：城市名 + 时间 + WiFi 信号条
- `drawWeatherSection()` — 重构自 `drawWeatherInfo()`：图标 + 温度 + 高低温 + 天气文字
- `drawClockSection()` — 重构自 `drawClock()`：大号时钟 + 秒 + 日期
- `drawDetailSection()` — 重构自 `drawBottomInfo()` 前半部分：体感/湿度/风向/更新时间
- `drawHourlyChart()` — 重构自 `drawBottomInfo()` 后半部分：优化折线图
- `drawSystemInfo()` — 重构：使用布局常量，保持功能不变
- `drawError()` — 小幅重构
- `drawFullUI()` — 组合调用新函数

**折线图优化（`drawHourlyChart`）：**
- 添加 2-3 条水平虚线网格（`GRID_COLOR`）
- 数据点圆圈从 radius=3 增大到 4
- 折线下方添加半透明填充效果（用深色矩形逐行填充）
- 改用蓝色调（`CHART_LINE_COLOR`）
- 温度标签居中对齐在数据点下方
- Y 轴留出更多边距

**加载动画（新增 `drawLoadingAnimation`）：**
- 在屏幕中央显示旋转点动画（8 个点，每次绘制 2 个亮点旋转）
- 用于天气刷新期间显示
- 通过 `drawLoadingFrame(int frame)` 实现，frame 0-7 循环
- 仅在内容区绘制，不覆盖标题栏和时钟

### Step 6: 重构 `main.cpp` — 主入口
→ verify: 编译通过，功能等价

重写 `main.cpp` 为简洁的入口文件：
- `#include` 所有模块头文件
- `setup()`: 调用 initDisplay → initWiFi → initNTP → fetchAll → drawFullUI
- `loop()`: 按钮检测 → NTP 更新 → 天气刷新（带加载动画）→ 时钟刷新 → WiFi 检测
- 移除 `drawBottomInfo()` 的重复调用（当前 L1298 和 L1303 重复）
- 天气刷新时调用加载动画

### Step 7: 验证与清理
→ verify: PlatformIO 编译通过，无警告

- 运行 `pio build` 确认编译通过
- 确认无未使用的 include 或变量
- 确认所有函数声明/定义匹配
- 确认 Flash/RAM 占用合理

## 关键假设与决策

1. **不引入新库**: 保持现有依赖不变，不添加 UI 框架
2. **全局 tft 指针**: 通过 `extern` 在模块间共享，不在嵌入式项目中过度抽象
3. **数据结构不变**: `weather`/`hourly`/`state` 改为命名结构体但字段不变
4. **加载动画简单实现**: 使用帧计数器在 loop() 中驱动，不使用 FreeRTOS 任务
5. **折线图填充**: 用简单的逐行深色矩形模拟，不使用 alpha 混合
6. **保持 rotation(1)**: 横屏模式，坐标系不变
7. **中文点阵数据原样保留**: 不增减汉字，后续可独立维护
8. **敏感信息保持在 config.h**: WiFi 密码和 API Key 仍在代码中（与当前行为一致）

## 成功标准

- [ ] `pio build` 编译通过，零错误零警告
- [ ] 所有现有显示内容保留（天气/时钟/日期/体感/风向/湿度/更新时间/WiFi/系统信息/折线图）
- [ ] 零魔法数字：所有像素坐标使用布局常量
- [ ] `fetchWeather/fetchHourly/fetchDaily` 共享公共 HTTP+gzip 函数
- [ ] 8 个模块文件各司其职，main.cpp ≤ 100 行
- [ ] UI 呈现现代扁平风格，无 CRT 仿真边框
- [ ] 折线图包含网格线和填充效果
- [ ] 天气刷新时显示加载动画
