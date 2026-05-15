# UI Display Fixes Spec

## Why

当前 UI 存在三个显示问题：NTP 失败后 "local time" 文字溢出屏幕；详情栏标签（"Hum"、"Wind"）与中文标签字体大小不统一且有英文未中文化；温度趋势图与上方温度显示区域过于紧凑。

## What Changes

### 1. NTP 失败提示文字溢出修复
- "NTP failed, local time"（22字符）在 setTextSize(2) 下宽度约 264px，超出 240px 屏幕
- 改为简短中文提示，改用 setTextSize(1) 显示

### 2. 详情栏标签中文化及字体统一
- "Hum " → 改为 "湿度"（使用 drawHanziText），与左侧中文标签风格统一
- "Wind " → 改为 "风"（使用 drawHanziText），与左侧中文标签风格统一
- 所有标签使用统一字体规格

### 3. 温度趋势图与温度显示区间距调整
- DETAIL_H = 34，DETAIL_Y = 124 结束于 158
- LINE4_Y = 160，CHART_Y = 164 开始 → 间隔仅 6px
- 增大详情区与图表区之间距，让布局更透气

## Impact

- Affected specs: esp32-desktop-mini-tv（UI 布局部分）
- Affected code:
  - `src/config.h` — 布局常量微调（间距调整）
  - `src/ui.cpp` — NTP 失败文字、详情标签中文化、字体大小
  - `src/main.cpp` — bootNTP 中 "NTP failed - local time" 提示同步修改

## ADDED Requirements

### Requirement: NTP 失败显示
The system SHALL display a short non-overflowing message when NTP sync fails.

#### Scenario: NTP 同步失败
- **WHEN** NTP 尝试后失败（state.ntpTried == true）
- **THEN** 时钟区日期行显示缩短的提示文字（不溢出屏幕），并正确显示基于 millis() 的本地时间

### Requirement: 详情标签统一中文化
The system SHALL display detailed weather labels in Chinese with uniform font sizing.

#### Scenario: 详情信息显示
- **WHEN** drawDetailSection 被调用且 weather.valid == true
- **THEN** 第二列标签（原 "Hum " / "Wind "）改用中文显示，并与左侧中文标签字体规格一致

### Requirement: 温度趋势图间距
The system SHALL maintain adequate spacing between the detail section and the hourly chart.

#### Scenario: 图表布局
- **WHEN** drawFullUI 绘制布局
- **THEN** 详情区与图表区之间有足够的垂直间距，不产生拥挤感

## MODIFIED Requirements

### Requirement: NTP 容错显示
The system SHALL handle NTP failure gracefully without text overflow.

**变更**：NTP 失败时日期行文字从 "NTP failed, local time" 缩短为不超过屏幕宽度的中文提示，同时保留基于 millis() 的本地时间显示。

## REMOVED Requirements
None.
