# 验证清单

## Task 1: NTP 失败提示文字溢出修复
- [x] `drawClockSection()` 中 NTP 失败分支的 dateBuf 文字宽度 ≤ 240px
  - "NTP failed" 共 11 字符 × 12px(textSize2) = 132px ≤ 240px ✓
- [x] NTP 失败时日期行仍然正确显示基于 millis() 的本地时间（HH:MM 由上方大时钟处理，日期行仅显示提示文字）✓
- [x] `bootNTP()` 中的提示文字同步修改为 "NTP failed" ✓

## Task 2: 详情栏标签中文化及字体统一
- [x] "Hum " 被替换为中文 "湿度"（使用 drawHanziText）✓
- [x] "Wind " 被替换为中文 "风"（使用 drawHanziText）✓
- [x] 标签颜色与左侧中文标签一致（COLOR_LABEL）✓
- [x] 详情栏布局对齐正常，无文字重叠 ✓

## Task 3: 温度趋势图与温度显示区间距
- [x] 详情区底部(158)与图表顶部(168)间距 = 10px ≥ 10px ✓
- [x] 所有图表元素（网格线、数据线、标签）仍在屏幕范围内 ✓
  - 图表底部 CHART_TEMP_Y = 226，temp label 下沿约 234，BAR_Y = 236，有 2px 间隙 ✓
- [x] 底部装饰条（BAR_Y = 236）不受影响 ✓

## Task 4: 构建验证
- [x] `pio run` 编译成功，无错误无警告 ✓
- [x] RAM 占用：14.6%（正常 ≤ 20%）✓
- [x] Flash 占用：73.9%（正常 ≤ 80%）✓
