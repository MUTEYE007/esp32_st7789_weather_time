# 任务列表

## Task 1: 项目基础配置
- [ ] 更新 platformio.ini，添加所有依赖库
  - Adafruit ST7735 and ST7789 Library
  - Adafruit GFX Library
  - ArduinoJson
  - NTPClient
- [ ] 定义 VSPI 引脚 (CS=5, DC=16, RST=17, MOSI=23, SCK=18)
- [ ] 添加 WiFi SSID/密码 和 和风天气 API Key 的宏定义

## Task 2: 屏幕驱动初始化
- [ ] 实现 ST7789 初始化 (240×240, VSPI)
- [ ] 验证屏幕正常点亮、清屏、显示基本图形

## Task 3: WiFi 联网与 NTP 时间同步
- [ ] 实现 WiFi 连接，含重试机制
- [ ] 实现 NTP 时间获取 (pool.ntp.org, UTC+8)
- [ ] 将时间存储在全局变量中，每秒更新

## Task 4: 和风天气 API 接入
- [ ] 实现 HTTP 请求获取实时天气数据
- [ ] 解析 JSON 响应 (温度、湿度、天气代码、风向风速、体感温度)
- [ ] 实现 30 分钟自动刷新
- [ ] 实现错误处理（网络失败、JSON 解析失败）

## Task 5: 核心 UI 绘制 - 时钟与日期
- [ ] 使用 Adafruit GFX 绘制大号数字时钟 (时:分)
- [ ] 绘制日期 (年-月-日 星期X)
- [ ] 时钟每秒刷新，日期每分钟刷新

## Task 6: 核心 UI 绘制 - 天气信息
- [ ] 根据天气代码绘制对应的天气图标（使用简单图形或预定义位图）
- [ ] 绘制温度、湿度、风速、体感温度
- [ ] 显示数据更新时间

## Task 7: CRT 电视边框装饰效果
- [ ] 绘制屏幕圆角/方角边框装饰
- [ ] 添加顶部/底部装饰条，模拟 CRT 电视外观

## Task 8: 容错与状态提示
- [ ] WiFi 连接失败时屏幕提示
- [ ] 天气数据加载中/加载失败提示
- [ ] NTP 同步中/失败提示

## Task 9: 集成测试与调试
- [ ] 完整的系统流程测试（上电→联网→NTP→天气→显示）
- [ ] 网络异常恢复测试
- [ ] 长时间运行稳定性测试

# 任务依赖关系
- Task 2 依赖 Task 1
- Task 3 依赖 Task 1
- Task 4 依赖 Task 3
- Task 5 依赖 Task 2, Task 3
- Task 6 依赖 Task 2, Task 4
- Task 7 依赖 Task 2
- Task 8 依赖 Task 3, Task 4
- Task 9 依赖 Task 5, Task 6, Task 7, Task 8
