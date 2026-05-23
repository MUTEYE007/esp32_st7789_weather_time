#pragma once

// ===== Main page =====
namespace main_page {
    void drawStatusHeader();
    void updateStatusTime();
    void drawWeatherSection();
    void drawWeatherIcon(int cx, int cy, int code);
    void drawClockSection();
    void updateClockTime(int h, int m, int s);
    void drawDetailSection();
    void drawHourlyChart();
    void drawLoadingFrame(int frame);
    void drawFullUI();
}

// ===== Minutely precipitation page =====
namespace minutely_page {
    void drawMinutelyPage();
}

// ===== System info page =====
namespace system_page {
    void drawSystemInfo();
}

// ===== Warning page =====
namespace warning_page {
    void drawWarningPage();
    void updateWarningScroll();
}
