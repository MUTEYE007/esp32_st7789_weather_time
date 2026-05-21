#pragma once

void drawStatusHeader();
void updateStatusTime();
void drawWeatherSection();
void drawWeatherIcon(int cx, int cy, int code);
void drawClockSection();
void updateClockTime(int h, int m, int s);
void drawDetailSection();
void drawHourlyChart();
void drawSystemInfo();
void drawWarningPage();
void updateWarningScroll();
void drawMinutelyPage();
void drawLoadingFrame(int frame);
void drawFullUI();
