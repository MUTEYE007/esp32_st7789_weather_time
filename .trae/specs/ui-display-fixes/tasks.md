# Tasks

## Task 1: 修复 NTP 失败提示文字溢出
- [x] 修改 `ui.cpp` 中 `drawClockSection()` 的 NTP 失败分支：将 "NTP failed, local time" 缩短为不溢出屏幕的短文本，并同步修改 `main.cpp` 中 `bootNTP()` 的提示文字。

**改动：**
- `src/ui.cpp` L129: `"NTP failed, local time"` → `"NTP failed"`
- `src/main.cpp` L59: `"NTP failed - local time"` → `"NTP failed"`

**验证：** NTP 失败时日期行不溢出 240px 屏幕 ✓

## Task 2: 详情栏标签中文化及字体统一
- [x] 修改 `ui.cpp` 中 `drawDetailSection()`：将 "Hum " 替换为中文 "湿度"，"Wind " 替换为中文 "风"，使用 `drawHanziText`，并与左侧标签字体规格统一。

**改动：**
- `src/ui.cpp` L178-179: `print("Hum ")` → `drawHanziText(..., "湿度")`
- `src/ui.cpp` L194-195: `print("Wind ")` → `drawHanziText(..., "风")`
- 值起始光标位置相应调整（湿度后 120+36=156，风后 120+18=138）

**验证：** 详情栏第二列标签显示为中文 "湿度" 和 "风"，字体风格统一 ✓

## Task 3: 调整温度趋势图与温度显示区间距
- [x] 修改 `config.h` 中的布局常量，增大详情区与图表区之间的垂直间距。

**改动：**
- 方案 A（实施）：保持 CHART_H ≈ 原有覆盖范围，整体下移 4px
- `config.h`: LINE4_Y 160→164, CHART_Y 164→168, CHART_H 70→66, 所有 chart 内部 Y 坐标 +4

**验证：** 详情区底部(158)与图表区顶部(168)间距 = 10px ✓

## Task 4: 构建验证
- [x] 运行 `pio run` 确保编译通过，无错误无警告。

**验证：** `pio run` 返回 SUCCESS（RAM 14.6%, Flash 73.9%） ✓

# Task Dependencies
- Task 4 依赖 Task 1, Task 2, Task 3
- Task 1, Task 2, Task 3 之间无依赖，可并行执行
