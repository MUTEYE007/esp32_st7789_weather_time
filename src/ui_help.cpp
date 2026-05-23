#include "ui.h"
#include "ui_common.h"

namespace help_page {

static const int LH = 18;
static int curY;

static void section(const char *title) {
    curY += 2;
    drawGB16(8, curY, title, COLOR_CLOCK, COLOR_BG);
    curY += LH;
}

static void line(const char *text, uint16_t color) {
    drawGB16(10, curY, text, color, COLOR_BG);
    curY += LH;
}

void drawHelpPage() {
    tft->fillScreen(COLOR_BG);

    fillArea(2, 2, SCREEN_W - 4, 16, COLOR_ACCENT);
    drawGB16(8, 2, "操作帮助", COLOR_PRIMARY, COLOR_ACCENT);

    curY = 24;

    section("按键A（BOOT）");
    line("长按3秒重置WiFi配置", COLOR_PRIMARY);

    section("按键B（GPIO13）");
    line("短按切换页面", COLOR_PRIMARY);
    line("长按调节亮度", COLOR_PRIMARY);

    section("页面顺序");
    line("主页>预警>分钟降水", COLOR_LABEL);
    line(">系统信息>帮助>主页", COLOR_LABEL);

    section("亮度调节");
    line("长按时亮度持续变化", COLOR_PRIMARY);
    line("松手后方向自动翻转", COLOR_PRIMARY);

    curY += 2;
    drawSectionLine(curY);
    curY += 2;
    drawGB16(8, curY, "短按此键退出帮助页", COLOR_LABEL, COLOR_BG);
}

} // namespace help_page
