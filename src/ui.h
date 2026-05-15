#pragma once

void drawStatusHeader();
void drawWeatherSection();
void drawWeatherIcon(int cx, int cy, int code);
void drawClockSection();
void drawDetailSection();
void drawHourlyChart();
void drawSystemInfo();
void drawError(const char *msg);
void drawLoadingFrame(int frame);
void drawFullUI();
