#pragma once

// ===== Screen dimensions =====
#define SCREEN_W 240
#define SCREEN_H 240

// ===== Layout padding =====
#define PAD_LEFT     8
#define PAD_RIGHT    8
#define CONTENT_W    (SCREEN_W - PAD_LEFT - PAD_RIGHT)

// ===== Status header =====
#define STATUS_Y     2
#define STATUS_H     14
#define LINE1_Y      16

// ===== Weather section =====
#define WEATHER_Y    20
#define WEATHER_H    36
#define ICON_CX      28
#define ICON_CY      35
#define TEMP_X       56
#define TEMP_Y       23
#define HILO_X       180
#define HILO_Y1      23
#define HILO_Y2      39
#define CITY_X       98
#define CITY_Y       23

// ===== Clock section =====
#define CLOCK_Y      56
#define CLOCK_H      62        // was 58, expanded for card
#define CLOCK_TEXT_Y 60
#define SEC_X_OFFSET 6
#define SEC_Y        79
#define DATE_Y       100

// ===== Clock card (macaron redesign) =====
#define CLOCK_CARD_X    4
#define CLOCK_CARD_Y    56
#define CLOCK_CARD_W    232
#define CLOCK_CARD_H    62
#define CLOCK_CARD_R    8
#define CLOCK_HM_Y      60        // HH:MM baseline (size 5, 40px tall)
#define CLOCK_SS_Y      72        // SS baseline (size 2, centered on HH:MM)
#define CLOCK_DATE_Y    100       // Date baseline (drawGB16, 16px tall)

// ===== Detail section =====
#define LINE3_Y      120
#define DETAIL_Y     124
#define DETAIL_H     34
#define DETAIL_ROW1_Y 126
#define DETAIL_ROW2_Y 142

// ===== Hourly chart =====
#define LINE4_Y      164
#define CHART_Y      168
#define CHART_H      66
#define CHART_LABEL_Y 172
#define CHART_LINE1_Y 188
#define CHART_LINE2_Y 204
#define CHART_LINE3_Y 220
#define CHART_DATA_TOP 184
#define CHART_DATA_BOT 220
#define CHART_TEMP_Y  228
#define CHART_START_X  20
#define CHART_COL_W    34
#define CHART_POINTS   7
#define CHART_PT_R     4

// ===== Bottom bar =====
#define BAR_Y        236
#define BAR_H        2

// ===== Hourly chart data =====
#define HOUR_COUNT   7

// ===== Macaron clock card colors =====
#define COL_CLOCK_CARD   0xFEDA  // 暖奶油蜜桃     卡片背景
#define COL_CLOCK_HM     0xC9CC  // 蔷薇粉         HH:MM 主色（更饱和鲜明）
#define COL_CLOCK_SS     0xCB8C  // 珊瑚粉         秒数
#define COL_CLOCK_DATE   0x9288  // 暖可可棕       日期
#define COL_CLOCK_COLON  0xC9CC  // 同 HM          冒号

// ===== Colors =====
#define COLOR_BG       0x08A5
#define COLOR_BG_ALT   0x08E6
#define COLOR_PRIMARY  0xD79F
#define COLOR_CLOCK    0x073F
#define COLOR_LABEL    0x5C53
#define COLOR_MUTED    0x3B4F
#define COLOR_ACCENT   0xE21F
#define COLOR_GREEN    0x6F95
#define COLOR_AMBER    0xFD48
#define COLOR_LINE     0x19AA
#define COLOR_GRID     0x1148
#define COLOR_YELLOW   0xFF2F
#define COLOR_CLOUD    0x1948
#define COLOR_RAIN     0x445F
#define COLOR_GOLD     0xFEA8
#define COLOR_PRECIP_SMALL 0x1ACF

#define COLOR_RED      0xF800
#define COLOR_WHITE    0xFFFF

// ===== Warning colors =====
#define WARN_COLOR_RED    0xF8A8
#define WARN_COLOR_ORANGE 0xFB60
#define WARN_COLOR_YELLOW 0xFF2F
#define WARN_COLOR_BLUE   0x445F

// ===== Warning page layout =====
#define WARN_HEADLINE_Y   50
#define WARN_SCROLL_DELAY 3000
#define WARN_LINE_H       20
#define WARN_DOT_Y        228
#define MAX_WARN_LINES    30
