#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp32/rom/miniz.h>

#define WIFI_SSID       "2400MHz"
#define WIFI_PASS       "20020512"

#define WEATHER_API_KEY "de84bd69b473418abe80c05ed7ff49fc"
#define WEATHER_HOST    "n96yw2f4cy.re.qweatherapi.com"
#define WEATHER_LOC     "101230101"
#define WEATHER_NAME    "Fuzhou"
#define WEATHER_INTERVAL_MS (30 * 60 * 1000)

#define HANZI_W 16
#define HANZI_H 16

struct HanziDef {
  const char bytes[4];
  const uint16_t data[HANZI_H];
};

static const HanziDef HANZI_DATA[] = {
  { "一", { 0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 }},
  { "七", { 0x0000,0x0200,0x0200,0x0200,0x0200,0x03FE,0xFE00,0x0200,0x0200,0x0200,0x0200,0x0202,0x0202,0x0206,0x01FC,0x0000 }},
  { "万", { 0x0000,0xFFFE,0x0400,0x0400,0x0400,0x07F8,0x0408,0x0408,0x0808,0x0808,0x0808,0x1008,0x2010,0x61F0,0xC000,0x0000 }},
  { "三", { 0x0000,0x7FFE,0x0000,0x0000,0x0000,0x0000,0x0000,0x3FFC,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFF,0x0000,0x0000 }},
  { "上", { 0x0000,0x0100,0x0100,0x0100,0x0100,0x01FE,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0xFFFF,0x0000,0x0000 }},
  { "下", { 0x0000,0xFFFF,0x0100,0x0100,0x0100,0x01C0,0x0160,0x0118,0x010C,0x0106,0x0100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "小", { 0x0080,0x0080,0x0080,0x0890,0x1898,0x1088,0x108C,0x2084,0x2086,0x4082,0xC083,0x8081,0x0080,0x0080,0x0F00,0x0000 }},
  { "不", { 0x0000,0xFFFE,0x0180,0x0100,0x0300,0x0500,0x0960,0x1930,0x310C,0x6106,0xC100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "东", { 0x0400,0x0800,0xFFFF,0x1800,0x1080,0x1080,0x2080,0x3FFE,0x0090,0x0888,0x108C,0x2086,0x4082,0x8081,0x0780,0x0000 }},
  { "个", { 0x0300,0x0780,0x0580,0x0CC0,0x1860,0x3130,0x610C,0x8106,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "中", { 0x0100,0x0100,0x0100,0x7FFC,0x4104,0x4104,0x4104,0x4104,0x7FFC,0x4104,0x0100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "为", { 0x2100,0x1100,0x0900,0x0100,0x7FFC,0x0104,0x0104,0x0184,0x0244,0x0224,0x0414,0x0C04,0x1804,0x3078,0x6000,0x0000 }},
  { "主", { 0x0000,0x0100,0x0180,0x0080,0x7FFE,0x0080,0x0080,0x0080,0x0080,0x7FFE,0x0080,0x0080,0x0080,0x0080,0xFFFF,0x0000 }},
  { "九", { 0x0000,0x0400,0x0400,0x0400,0xFFE0,0x0420,0x0420,0x0420,0x0420,0x0820,0x0822,0x1822,0x3022,0x6022,0xC03C,0x0000 }},
  { "了", { 0x0000,0x7FFE,0x000E,0x0018,0x0060,0x00C0,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0F00,0x0000,0x0000 }},
  { "二", { 0x0000,0x0000,0x7FFC,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xFFFE,0x0000,0x0000,0x0000 }},
  { "于", { 0x0000,0x7FFE,0x0080,0x0080,0x0080,0x0080,0xFFFF,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0F80,0x0000,0x0000 }},
  { "云", { 0x0000,0x3FFE,0x0000,0x0000,0x0000,0x0000,0x7FFF,0x0300,0x0600,0x0620,0x0C10,0x0818,0x100C,0x3FFE,0x0002,0x0000 }},
  { "五", { 0x0000,0x7FFE,0x0400,0x0400,0x0400,0x0400,0x7FF8,0x0C08,0x0808,0x0808,0x0808,0x0808,0x0808,0xFFFF,0x0000,0x0000 }},
  { "产", { 0x0100,0x0080,0x7FFF,0x0800,0x0430,0x0040,0x3FFF,0x2000,0x2000,0x2000,0x2000,0x2000,0x4000,0x4000,0x8000,0x0000 }},
  { "人", { 0x0100,0x0100,0x0100,0x0100,0x0300,0x0300,0x0280,0x0280,0x04C0,0x0440,0x0860,0x1820,0x3010,0x600C,0xC006,0x0000 }},
  { "亿", { 0x0800,0x0BFE,0x1006,0x100C,0x1018,0x3030,0x3060,0x7040,0x50C0,0x1181,0x1301,0x1201,0x1203,0x11FE,0x1000,0x0000 }},
  { "今", { 0x0300,0x0780,0x04C0,0x0860,0x1230,0x610C,0xC083,0x0000,0x3FF8,0x0018,0x0030,0x0060,0x00C0,0x0080,0x0100,0x0000 }},
  { "他", { 0x0000,0x0840,0x1040,0x1440,0x247C,0x27C4,0x6C44,0xE444,0xA444,0x247C,0x2440,0x2442,0x2402,0x2406,0x23FC,0x0000 }},
  { "以", { 0x0204,0x2304,0x2104,0x2084,0x2084,0x2004,0x2004,0x2004,0x2008,0x2108,0x261C,0x2C34,0x3062,0x40C3,0x0301,0x0000 }},
  { "们", { 0x1200,0x113E,0x2082,0x2202,0x2202,0x6202,0x6202,0xE202,0xA202,0x2202,0x2202,0x2202,0x2202,0x2202,0x223C,0x0000 }},
  { "会", { 0x0180,0x03C0,0x0660,0x0830,0x3018,0xE00C,0x9FF3,0x0000,0x0000,0xFFFE,0x0600,0x0840,0x11E0,0x3F10,0x4008,0x0000 }},
  { "体", { 0x1040,0x1040,0x2040,0x2FFE,0x60E0,0x6160,0xE150,0xA350,0x2248,0x2444,0x29FE,0x3043,0x2040,0x2040,0x2040,0x0000 }},
  { "作", { 0x1080,0x1100,0x21FF,0x2240,0x2440,0x6440,0x687E,0xA040,0xA040,0x2040,0x207F,0x2040,0x2040,0x2040,0x2040,0x0000 }},
  { "信", { 0x0000,0x1080,0x10C0,0x2FFE,0x2000,0x67FC,0x6000,0xE7FC,0xA000,0x2000,0x27FC,0x2404,0x2404,0x2404,0x27FC,0x0000 }},
  { "八", { 0x0440,0x0440,0x0440,0x0440,0x0440,0x0820,0x0820,0x0820,0x1830,0x1010,0x3010,0x2008,0x400C,0x8006,0x0000,0x0000 }},
  { "六", { 0x0200,0x0100,0x0080,0x0000,0xFFFE,0x0000,0x0000,0x0440,0x0C20,0x0830,0x1010,0x3008,0x6004,0x4006,0x8002,0x0000 }},
  { "凌", { 0x0000,0x0040,0x87FE,0x4040,0x4040,0x3FFF,0x0010,0x070E,0x3883,0x23FC,0x470C,0x4C98,0x4070,0x81D8,0x8E07,0x0000 }},
  { "出", { 0x0100,0x4104,0x4104,0x4104,0x4104,0x4104,0x7FFC,0x0104,0x0100,0x4104,0x4104,0x4104,0x4104,0x7FFC,0x0004,0x0000 }},
  { "分", { 0x0440,0x0C60,0x0820,0x1010,0x2008,0x6006,0x8003,0x3FF0,0x0410,0x0410,0x0810,0x1810,0x3010,0xC3E0,0x0000,0x0000 }},
  { "到", { 0x0000,0x0002,0x7FC2,0x0812,0x1112,0x1092,0x3F92,0x0052,0x0412,0x7FD2,0x0412,0x0412,0x0402,0x07C2,0x781E,0x0000 }},
  { "力", { 0x0200,0x0200,0x0200,0x7FFC,0x0204,0x0204,0x0204,0x0604,0x0404,0x0404,0x0804,0x1004,0x6004,0xC0F8,0x0000,0x0000 }},
  { "功", { 0x0040,0x0040,0xFE40,0x13FC,0x1044,0x1044,0x1044,0x1044,0x10C4,0x1684,0x3984,0xC104,0x0604,0x0C3C,0x0000,0x0000 }},
  { "加", { 0x1000,0x1000,0x107E,0x7E42,0x1242,0x1242,0x1242,0x1242,0x1242,0x1242,0x2242,0x2242,0x627E,0x5C42,0x8000,0x0000 }},
  { "动", { 0x0000,0x0020,0x3F20,0x0020,0x00FE,0x0022,0x7F22,0x1022,0x1022,0x2422,0x2242,0x2642,0x79C2,0x0082,0x011E,0x0000 }},
  { "北", { 0x0000,0x0440,0x0440,0x0440,0x0446,0xFC4C,0x0470,0x0440,0x0440,0x0440,0x0442,0x1C42,0x6442,0x8442,0x043C,0x0000 }},
  { "十", { 0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0xFFFF,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "千", { 0x007C,0x3F80,0x0100,0x0100,0x0100,0x0100,0x0100,0x7FFE,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "午", { 0x0800,0x0800,0x1FFE,0x1080,0x2080,0x2080,0x4080,0x0080,0x7FFF,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0000 }},
  { "南", { 0x0100,0x0100,0xFFFE,0x0100,0x0100,0x7FFC,0x4C24,0x46C4,0x5FF4,0x4104,0x4104,0x5FF4,0x4104,0x4104,0x417C,0x0000 }},
  { "发", { 0x2130,0x2118,0x420C,0x7FFF,0x0200,0x0400,0x07FC,0x0604,0x0A08,0x1908,0x1090,0x2060,0x40E0,0x8718,0x1807,0x0000 }},
  { "可", { 0x0000,0xFFFF,0x0008,0x0008,0x3F88,0x2088,0x2088,0x2088,0x2088,0x2088,0x3F88,0x2008,0x0008,0x00F0,0x0000,0x0000 }},
  { "号", { 0x0000,0x1FFC,0x1004,0x1004,0x1FFC,0x1004,0x0000,0x7FFF,0x0800,0x1000,0x1FFC,0x0004,0x0004,0x03F8,0x0000,0x0000 }},
  { "后", { 0x003E,0x3FC0,0x2000,0x2000,0x3FFE,0x2000,0x2000,0x2000,0x27FC,0x2404,0x2404,0x4404,0x4404,0x47FC,0x8404,0x0000 }},
  { "和", { 0x0000,0x0E00,0xF000,0x10FE,0x1082,0xFE82,0x1082,0x3082,0x3882,0x7C82,0x5682,0xD082,0x9082,0x10FE,0x1082,0x0000 }},
  { "四", { 0x0000,0x7FFE,0x4442,0x4442,0x4442,0x4442,0x4442,0x4442,0x4842,0x583E,0x7002,0x4002,0x4002,0x7FFE,0x4002,0x0000 }},
  { "国", { 0x0000,0x7FFE,0x4002,0x4002,0x5FFA,0x4102,0x4102,0x5FFA,0x4162,0x4112,0x5FFA,0x4002,0x4002,0x7FFE,0x4002,0x0000 }},
  { "地", { 0x0000,0x2020,0x2020,0x2220,0x2226,0xFA3E,0x23E2,0x2622,0x2222,0x223C,0x2221,0x2A21,0x7221,0xC202,0x01FE,0x0000 }},
  { "多", { 0x0180,0x0300,0x07FC,0x0818,0x3430,0x62C0,0x0300,0x1C60,0x61FE,0x0306,0x0D18,0x31B0,0x00C0,0x0700,0x7800,0x0000 }},
  { "大", { 0x0100,0x0100,0x0100,0x0100,0xFFFE,0x0100,0x0100,0x0380,0x0280,0x0280,0x0440,0x0820,0x1030,0x6018,0xC006,0x0000 }},
  { "天", { 0x0000,0x7FFC,0x0100,0x0100,0x0100,0x0100,0xFFFE,0x0300,0x0300,0x0280,0x0440,0x0C40,0x1830,0x6018,0xC006,0x0000 }},
  { "失", { 0x0080,0x0880,0x1080,0x1FFE,0x2080,0x4080,0x0080,0x7FFF,0x0180,0x0140,0x0340,0x0620,0x0C10,0x300C,0xC003,0x0000 }},
  { "子", { 0x0000,0x3FFC,0x0018,0x0030,0x0040,0x0080,0x0080,0xFFFF,0x0080,0x0080,0x0080,0x0080,0x0080,0x0F00,0x0000,0x0000 }},
  { "对", { 0x0000,0x0004,0x0004,0x7E04,0x0204,0x42FF,0x2604,0x3484,0x1C44,0x0C44,0x0C24,0x1604,0x3204,0x6104,0x8078,0x0000 }},
  { "就", { 0x1020,0x082C,0xFF26,0x0020,0x7EFF,0x4228,0x4228,0x7E28,0x0828,0x2C48,0x4A49,0x4949,0x8889,0x788E,0x0100,0x0000 }},
  { "州", { 0x1084,0x1084,0x1084,0x1084,0x54A4,0x54A4,0x5294,0x9294,0x9084,0x1084,0x3084,0x2084,0x2084,0x4084,0x8084,0x0000 }},
  { "年", { 0x1000,0x3000,0x3FFC,0x2080,0x4080,0xC080,0xBFFC,0x2080,0x2080,0x2080,0xFFFE,0x0080,0x0080,0x0080,0x0080,0x0000 }},
  { "度", { 0x0000,0x0080,0x0040,0x3FFF,0x2000,0x2208,0x3FFF,0x2208,0x23F8,0x2000,0x2FFC,0x2418,0x43E0,0x43F0,0x9C0F,0x0000 }},
  { "得", { 0x0000,0x0800,0x11FC,0x1104,0x21FC,0x4904,0x19FC,0x1000,0x33FE,0x7008,0x57FE,0x1108,0x1088,0x1008,0x1078,0x0000 }},
  { "息", { 0x0000,0x0200,0x3FFC,0x2004,0x3FFC,0x2004,0x3FFC,0x2004,0x3FFC,0x2004,0x0100,0x4984,0xC88A,0x8809,0x0FF0,0x0000 }},
  { "感", { 0x0000,0x002C,0x0026,0x3FFF,0x2020,0x2FA2,0x2024,0x2F9C,0x6899,0x4FBB,0x8066,0x2900,0x4896,0x4811,0x8FE0,0x0000 }},
  { "成", { 0x0050,0x004C,0x0040,0x3FFF,0x2040,0x2040,0x3E42,0x2244,0x226C,0x2238,0x2231,0x5E71,0x40D9,0x818E,0x0000,0x0000 }},
  { "我", { 0x0758,0x784C,0x0846,0x0840,0x7FFF,0x0840,0x0842,0x0F24,0x7828,0x0830,0x0861,0x08D1,0x0B19,0x380E,0x0000,0x0000 }},
  { "报", { 0x1000,0x11FE,0x1102,0x7D02,0x113C,0x1100,0x1DFE,0x3142,0x5144,0x1124,0x1138,0x1118,0x116C,0x71C3,0x0000,0x0000 }},
  { "接", { 0x0000,0x2040,0x2020,0x27FF,0xF904,0x2098,0x27FF,0x3840,0xE0C0,0x2FFF,0x2108,0x23D0,0x2078,0xEF87,0x0000,0x0000 }},
  { "新", { 0x0000,0x1000,0x0806,0x7F78,0x4240,0x2440,0xFF40,0x087E,0x0848,0xFE48,0x0848,0x4C48,0x8A88,0x0888,0x7908,0x0000 }},
  { "无", { 0x0000,0x7FFC,0x0100,0x0100,0x0100,0x0200,0xFFFE,0x0200,0x0480,0x0480,0x0882,0x1082,0x6082,0xC0FC,0x0000,0x0000 }},
  { "日", { 0x0000,0x3FF8,0x2008,0x2008,0x2008,0x2008,0x2008,0x3FF8,0x2008,0x2008,0x2008,0x2008,0x2008,0x3FF8,0x2008,0x0000 }},
  { "早", { 0x0000,0x3FFC,0x2004,0x2004,0x3FFC,0x2004,0x2004,0x3FFC,0x2104,0x0100,0xFFFF,0x0100,0x0100,0x0100,0x0100,0x0000 }},
  { "时", { 0x0000,0x0008,0x7C08,0x4408,0x47FE,0x4408,0x7D08,0x4488,0x4488,0x4448,0x4448,0x4408,0x7C08,0x4408,0x00F0,0x0000 }},
  { "明", { 0x0000,0x7CFE,0x4482,0x4482,0x4482,0x44FE,0x7C82,0x4482,0x4482,0x44FE,0x4482,0x7D02,0x4302,0x0202,0x043E,0x0000 }},
  { "星", { 0x0000,0x3FFC,0x2004,0x3FFC,0x2004,0x3FFC,0x1004,0x3080,0x7FFE,0x4080,0xBFFE,0x0080,0x0080,0xFFFF,0x0000,0x0000 }},
  { "昨", { 0x0040,0x00C0,0x7880,0x49FF,0x4B40,0x4A40,0x4E40,0x787F,0x4840,0x4840,0x4840,0x487F,0x4840,0x7840,0x4840,0x0040 }},
  { "是", { 0x0000,0x1FF8,0x1008,0x1FF8,0x1008,0x1FF8,0x1008,0x0000,0xFFFF,0x0880,0x18FE,0x1880,0x2C80,0x63FF,0x8000,0x0000 }},
  { "晚", { 0x0040,0x0080,0x79FC,0x4A08,0x4C10,0x4FFE,0x4A22,0x7A22,0x4A22,0x4BFE,0x4A32,0x4831,0x7851,0x40D1,0x419E,0x0300 }},
  { "晨", { 0x0000,0x1FF8,0x1008,0x1FF8,0x1008,0x1FF8,0x0000,0x3FFC,0x2000,0x3FFC,0x2000,0x3FFE,0x68CC,0x4B70,0x8C0E,0x0000 }},
  { "晴", { 0x0020,0x7BFF,0x4820,0x4BFE,0x4820,0x4BFF,0x7800,0x49FE,0x4902,0x49FE,0x4902,0x49FE,0x7902,0x4902,0x011E,0x0000 }},
  { "更", { 0x0000,0x7FFE,0x0100,0x3FFC,0x2104,0x2104,0x3FFC,0x2104,0x2104,0x3FFC,0x2104,0x1100,0x0E00,0x1F80,0xE07F,0x0000 }},
  { "月", { 0x0000,0x1FF8,0x1008,0x1008,0x1008,0x1FF8,0x1008,0x1008,0x1008,0x1FF8,0x1008,0x3008,0x2008,0x6008,0x4070,0x0000 }},
  { "有", { 0x0000,0x0200,0x0200,0xFFFE,0x0400,0x0800,0x1FF8,0x3008,0x5FF8,0x9008,0x1008,0x1FF8,0x1008,0x1008,0x1078,0x0000 }},
  { "来", { 0x0080,0x0080,0x3FFC,0x1088,0x0890,0x04A0,0x0080,0x7FFE,0x01C0,0x02C0,0x04A0,0x0890,0x308C,0x6086,0x0080,0x0000 }},
  { "样", { 0x1104,0x1088,0x1050,0x7DFF,0x1020,0x1020,0x39FE,0x3420,0x3220,0x5020,0x53FF,0x1020,0x1020,0x1020,0x1020,0x0000 }},
  { "桌", { 0x0100,0x01FE,0x0100,0x3FFC,0x2004,0x3FFC,0x2004,0x3FFC,0x0100,0x0100,0xFFFF,0x0540,0x1930,0xE10F,0x0100,0x0000 }},
  { "气", { 0x0000,0x0800,0x0FFE,0x1000,0x1000,0x2FFC,0x2000,0x4000,0x5FF8,0x0008,0x0008,0x0009,0x0009,0x0005,0x0006,0x0000 }},
  { "水", { 0x0000,0x0080,0x0080,0x0082,0x00C4,0xFCCC,0x04D0,0x08A0,0x08B0,0x1890,0x1088,0x208C,0x4086,0x8083,0x0F00,0x0000 }},
  { "温", { 0x6000,0x33FC,0x1A04,0x03FC,0x4204,0x63FC,0x2204,0x0000,0x27FE,0x2492,0x2492,0x2492,0x4492,0x5FFF,0x4000,0x0000 }},
  { "湿", { 0x4000,0x23FE,0x1202,0x03FE,0x4202,0x23FE,0x1000,0x0489,0x2489,0x268A,0x228E,0x228C,0x4088,0x4FFF,0x4000,0x0000 }},
  { "点", { 0x0100,0x01FF,0x0100,0x0100,0x3FFC,0x2004,0x2004,0x2004,0x3FFC,0x2004,0x0000,0x2444,0x4442,0x4222,0x8221,0x0000 }},
  { "生", { 0x0000,0x0080,0x0880,0x1880,0x1FFE,0x3080,0x2080,0x4080,0x0080,0x1FFE,0x0080,0x0080,0x0080,0x0080,0x7FFF,0x0000 }},
  { "用", { 0x0000,0x3FFE,0x2082,0x2082,0x2082,0x3FFE,0x2082,0x2082,0x2082,0x3FFE,0x2082,0x6082,0x4082,0x809E,0x0000,0x0000 }},
  { "电", { 0x0000,0x0100,0x0100,0x7FFC,0x4104,0x4104,0x4104,0x7FFC,0x4104,0x4104,0x4104,0x7FFC,0x4102,0x0102,0x01FC,0x0000 }},
  { "百", { 0x0000,0xFFFF,0x0100,0x0100,0x0100,0x3FFC,0x2004,0x2004,0x2004,0x3FFC,0x2004,0x2004,0x2004,0x3FFC,0x2004,0x0000 }},
  { "的", { 0x0000,0x0820,0x1040,0x7E7E,0x4282,0x4282,0x4342,0x4262,0x7E22,0x4212,0x4212,0x4202,0x4202,0x7E02,0x427C,0x0000 }},
  { "福", { 0x2000,0x13FE,0x1000,0x01FC,0x7D04,0x05FC,0x0800,0x3BFE,0x7622,0x9222,0x13FE,0x1222,0x1222,0x13FE,0x1202,0x0000 }},
  { "秒", { 0x1C10,0xF010,0x1094,0x1092,0xFD12,0x1111,0x3311,0x3A10,0x7412,0x5214,0xD00C,0x9018,0x1020,0x10C0,0x1700,0x0000 }},
  { "级", { 0x0000,0x0800,0x13FC,0x1084,0x2488,0x4890,0x78DE,0x10C2,0x30E6,0x3EA4,0x01BC,0x1D18,0x631C,0x0266,0x04C3,0x0000 }},
  { "络", { 0x0000,0x1080,0x21FC,0x458C,0x4B98,0xF270,0x3070,0x219C,0x4606,0xF9FC,0x0104,0x1D04,0xE104,0x01FC,0x0104,0x0000 }},
  { "网", { 0x0000,0x7FFC,0x4004,0x6214,0x5314,0x54A4,0x4CE4,0x4C44,0x4C64,0x5EA4,0x52B4,0x7114,0x6004,0x403C,0x0000,0x0000 }},
  { "置", { 0x0000,0x3FFC,0x2224,0x3FFC,0x0080,0x7FFE,0x0080,0x1FFC,0x1004,0x1FFC,0x1004,0x1FFC,0x1004,0x7FFE,0x0000,0x0000 }},
  { "而", { 0x0000,0xFFFE,0x0100,0x0100,0x0200,0x7FFC,0x4444,0x4444,0x4444,0x4444,0x4444,0x4444,0x4444,0x4444,0x403C,0x0000 }},
  { "能", { 0x1040,0x2046,0x644C,0xC272,0xFF42,0x0142,0x7E7C,0x4200,0x4246,0x7E4C,0x4270,0x7E42,0x4242,0x4242,0x4E7C,0x0000 }},
  { "行", { 0x0800,0x11FE,0x2000,0x4000,0x8400,0x0800,0x13FF,0x1008,0x3008,0x5008,0x1008,0x1008,0x1008,0x1008,0x10F8,0x0000 }},
  { "西", { 0x0000,0xFFFF,0x0440,0x0440,0x7FFE,0x4442,0x4442,0x4842,0x4842,0x507E,0x7002,0x4002,0x4002,0x7FFE,0x4002,0x0000 }},
  { "雾", { 0x0010,0x07FF,0x0410,0x0C91,0x0010,0x0019,0x0080,0x00BD,0x0143,0x0624,0x007F,0x0790,0x09DF,0x0020,0x0020,0x00C5 }},
  { "要", { 0x0000,0x7FFE,0x0220,0x3FFC,0x2224,0x2224,0x3FFC,0x2204,0x0600,0x7FFE,0x0810,0x1E20,0x03C0,0x0770,0x780C,0x0000 }},
  { "视", { 0x2000,0x11FC,0x1104,0xF924,0x1924,0x1924,0x3124,0x7924,0xFD24,0x9534,0x1070,0x1071,0x10D1,0x1091,0x131E,0x1600 }},
  { "设", { 0x0000,0x21F8,0x1108,0x0908,0x0208,0x760F,0x1000,0x13FC,0x1104,0x1188,0x1098,0x1470,0x1CF0,0x338C,0x0C03,0x0000 }},
  { "说", { 0x4208,0x2310,0x1120,0x07FC,0x0404,0xE404,0x2404,0x2404,0x27FC,0x2524,0x2120,0x2922,0x3222,0x6C22,0x183C,0x0000 }},
  { "败", { 0x0020,0x7E60,0x4240,0x527F,0x52C4,0x53C4,0x5344,0x5244,0x5228,0x5228,0x1838,0x2C10,0x2668,0x42C6,0x8303,0x0000 }},
  { "载", { 0x0858,0x7F46,0x0840,0xFFFF,0x0840,0x1042,0xFFC4,0x242C,0x6428,0x7F38,0x0430,0x0731,0xFC51,0x0489,0x0506,0x0000 }},
  { "这", { 0x4040,0x6020,0x2020,0x1FFF,0x1000,0x0004,0xE60C,0x2308,0x21D0,0x2060,0x2070,0x2198,0x270C,0x6C06,0xD800,0x8FFF }},
  { "进", { 0x4110,0x2110,0x3110,0x17FC,0x0110,0x0110,0xE110,0x2FFE,0x2110,0x2110,0x2310,0x2210,0x6410,0xD000,0x8FFE,0x0000 }},
  { "连", { 0x0000,0x4080,0x2180,0x37FE,0x1100,0x0220,0xE620,0x27FE,0x2020,0x2020,0x2FFF,0x2020,0x2020,0xD020,0x8FFF,0x0000 }},
  { "迷", { 0x4020,0x2422,0x3224,0x1128,0x0020,0x6FFF,0x2060,0x20F0,0x21A8,0x2324,0x2622,0x2C21,0x2020,0x5800,0x8FFF,0x0000 }},
  { "阴", { 0x0000,0x7DFE,0x4502,0x4902,0x4902,0x51FE,0x5102,0x5102,0x4902,0x49FE,0x4902,0x4B02,0x7A02,0x4602,0x443C,0x0000 }},
  { "雨", { 0x0000,0xFFFE,0x0100,0x0100,0x7FFC,0x4104,0x5144,0x4924,0x4514,0x5144,0x4924,0x4514,0x4104,0x413C,0x0000,0x0000 }},
  { "雪", { 0x0000,0x3FFC,0x0080,0x7FFE,0x4082,0x5EBA,0x0080,0x1EB8,0x0000,0x3FFC,0x0004,0x3FFC,0x0004,0x3FFC,0x0004,0x0000 }},
  { "零", { 0x0000,0xFFFC,0x0200,0xFFFC,0x8204,0xBA74,0x0200,0x3970,0x06C0,0x3A38,0xC107,0x3FE0,0x0040,0x1F80,0x01C0,0x0000 }},
  { "面", { 0x0000,0xFFFE,0x0100,0x0200,0x7FFC,0x4444,0x4444,0x47C4,0x4444,0x4444,0x47C4,0x4444,0x4444,0x7FFC,0x4004,0x0000 }},
  { "预", { 0x0000,0xFDFF,0x0C20,0x5820,0x31FE,0x1102,0xFD12,0x1512,0x1512,0x1512,0x1112,0x1132,0x104C,0x1186,0xF603,0x0000 }},
  { "风", { 0x0000,0x3FF8,0x2008,0x2428,0x2648,0x2248,0x2188,0x2188,0x2188,0x22C8,0x2668,0x6429,0x4819,0x4009,0x8006,0x0000 }},
  { "严", { 0x07DD,0x0022,0x0422,0x0322,0x0122,0x0123,0x07FF,0x0400,0x0400,0x0400,0x0400,0x0400,0x0400,0x0800,0x0800,0x1000 }},
  { "伴", { 0x0186,0x0146,0x0126,0x0236,0x0216,0x0606,0x0A3F,0x0A06,0x1206,0x0206,0x027F,0x0206,0x0206,0x0206,0x0206,0x0206 }},
  { "冷", { 0x0004,0x080E,0x0C0A,0x0499,0x0490,0x0124,0x0126,0x0242,0x0382,0x023F,0x1C00,0x0400,0x0411,0x0C0E,0x0C06,0x0403 }},
  { "冰", { 0x0804,0x0484,0x0484,0x0506,0x01FE,0x0216,0x0215,0x0225,0x0424,0x0424,0x0844,0x0844,0x0884,0x0D04,0x0E04,0x001C }},
  { "冻", { 0x1004,0x0C88,0x04AF,0x0518,0x0110,0x0233,0x0222,0x0262,0x047F,0x1C42,0x0412,0x0432,0x0C22,0x0C42,0x0C82,0x011E }},
  { "降", { 0x0FCF,0x0888,0x0894,0x0912,0x0923,0x0903,0x090D,0x08B1,0x084F,0x0841,0x0851,0x0F91,0x093F,0x0801,0x0801,0x0801 }},
  { "雹", { 0x0010,0x07FF,0x0810,0x0991,0x0010,0x0099,0x0110,0x01FF,0x0200,0x05FC,0x0908,0x1108,0x01F8,0x010B,0x0101,0x0100 }},
  { "雷", { 0x0010,0x0010,0x07FF,0x0810,0x1991,0x0010,0x0191,0x0010,0x03EF,0x0210,0x0210,0x03FF,0x0210,0x0210,0x03FF,0x0200 }},
  { "霾", { 0x0010,0x07FF,0x0810,0x0991,0x0010,0x0093,0x0180,0x025D,0x0DF2,0x151F,0x0692,0x0B5F,0x04D2,0x014F,0x0642,0x0BFF }},
  { "沙", { 0x0402,0x0202,0x0202,0x0082,0x1092,0x0922,0x0D22,0x0142,0x0242,0x0282,0x0202,0x1C00,0x0401,0x0403,0x0404,0x0C38 }},
  { "尘", { 0x0018,0x0018,0x0199,0x0118,0x0218,0x0418,0x0818,0x1010,0x0018,0x0018,0x03FF,0x0018,0x0018,0x0018,0x0018,0x0FFF }},
  { "浮", { 0x061F,0x0224,0x0224,0x0092,0x1092,0x0910,0x0D3F,0x0100,0x0201,0x0202,0x04FF,0x1C02,0x0402,0x0402,0x0C02,0x0C1E }},
  { "扬", { 0x031F,0x0100,0x0101,0x0FC3,0x0106,0x010C,0x0179,0x0186,0x0704,0x190C,0x0109,0x0113,0x0122,0x01C4,0x0108,0x0F30 }},
  { "薄", { 0x0042,0x07FF,0x0042,0x0402,0x025F,0x00BF,0x1122,0x0D3F,0x0122,0x023F,0x0222,0x0CDF,0x0420,0x0410,0x0410,0x0403 }},
  { "浓", { 0x0404,0x0204,0x0237,0x004C,0x11CC,0x090C,0x0D14,0x0212,0x0232,0x0271,0x0451,0x0490,0x0D10,0x0610,0x0C16,0x0C18 }},
  { "毛", { 0x000F,0x07F0,0x0030,0x0030,0x0030,0x01FF,0x0630,0x0030,0x0030,0x03FF,0x1C30,0x0030,0x0030,0x0030,0x0030,0x001F }},
  { "细", { 0x0200,0x021E,0x0411,0x08D1,0x0891,0x1F11,0x0111,0x021F,0x0411,0x09D1,0x0E11,0x0011,0x0051,0x0391,0x1C1F,0x0010 }},
  { "特", { 0x0102,0x0902,0x091F,0x0902,0x0FE2,0x113F,0x1100,0x0100,0x01C0,0x033F,0x1D00,0x0110,0x0108,0x0108,0x0100,0x0103 }},
  { "极", { 0x0217,0x0208,0x0208,0x0FC8,0x0208,0x0209,0x0309,0x06CC,0x064C,0x0A4A,0x0A12,0x1211,0x0211,0x0223,0x0244,0x0248 }},
  { "端", { 0x0412,0x0212,0x0212,0x0FFF,0x0000,0x0180,0x093D,0x0902,0x053F,0x0524,0x0224,0x03E4,0x0E24,0x1024,0x0024,0x0020 }},
  { "强", { 0x0FC8,0x0088,0x0088,0x008E,0x0F89,0x0881,0x081F,0x0811,0x0F91,0x0891,0x009F,0x0091,0x0081,0x0101,0x0101,0x077F }},
  { "阵", { 0x0FC4,0x0884,0x0897,0x0908,0x0909,0x0A09,0x0911,0x089F,0x0881,0x0881,0x0C81,0x0BFF,0x0901,0x0801,0x0801,0x0801 }},
  { "暴", { 0x0100,0x01FF,0x0100,0x01FF,0x0100,0x0043,0x07FF,0x0042,0x0F67,0x00DA,0x0189,0x024B,0x0C4C,0x106E,0x0389,0x0278 }},
  { "未", { 0x0018,0x0018,0x03FF,0x0018,0x0018,0x0018,0x1FFB,0x003C,0x003C,0x005A,0x00D9,0x0199,0x0318,0x0418,0x0818,0x0018 }},
  { "知", { 0x0200,0x0200,0x0203,0x0772,0x0482,0x0482,0x0882,0x0FFA,0x0082,0x0082,0x0142,0x0122,0x0132,0x021B,0x0402,0x0802 }},
  { "热", { 0x0104,0x0104,0x07FF,0x0104,0x0104,0x0174,0x038C,0x1D06,0x010B,0x0111,0x0320,0x0040,0x0044,0x0446,0x0422,0x0C22 }},
  { "间", { 0x013F,0x0900,0x0D00,0x0C00,0x0C7E,0x0C42,0x0C42,0x0C7E,0x0C42,0x0C42,0x0C42,0x0C7E,0x0C42,0x0C00,0x0C00,0x0C00 }},
  { "夹", { 0x0010,0x0010,0x03FF,0x0010,0x0011,0x0111,0x0191,0x0092,0x0FD7,0x0028,0x0024,0x0064,0x0042,0x0081,0x0100,0x0600 }},
  { "少", { 0x0018,0x0018,0x0018,0x0099,0x0198,0x0118,0x0218,0x0218,0x0418,0x0818,0x1019,0x0003,0x0006,0x0018,0x0060,0x0380 }},
};

static const HanziDef* findHanzi(const char* utf8) {
  for (int i = 0; i < sizeof(HANZI_DATA) / sizeof(HANZI_DATA[0]); i++) {
    if (HANZI_DATA[i].bytes[0] == utf8[0] &&
        HANZI_DATA[i].bytes[1] == utf8[1] &&
        HANZI_DATA[i].bytes[2] == utf8[2]) {
      return &HANZI_DATA[i];
    }
  }
  return NULL;
}

void drawHanzi(Adafruit_ST7789 *display, int x, int y, const HanziDef *ch, uint16_t color) {
  for (int row = 0; row < HANZI_H; row++) {
    uint16_t bits = ch->data[row];
    for (int col = 0; col < HANZI_W; col++) {
      if (bits & (0x8000 >> col)) {
        display->drawPixel(x + col, y + row, color);
      }
    }
  }
}

int drawHanziText(Adafruit_ST7789 *display, int x, int y, const char *text, uint16_t color) {
  int cx = x;
  int charW = HANZI_W + 2;
  while (*text) {
    if ((*text & 0x80) && (*(text + 1) & 0x80)) {
      const HanziDef *ch = findHanzi(text);
      if (ch) {
        drawHanzi(display, cx, y, ch, color);
        cx += charW;
        text += 3;
        continue;
      }
    }
    text++;
  }
  return cx;
}

const char* iconToCN(int code) {
  if (code == 100 || code == 150) return "晴";
  if (code >= 101 && code <= 103) return "多云";
  if (code == 104) return "阴";
  if (code >= 300 && code <= 399) return "雨";
  if (code >= 400 && code <= 499) return "雪";
  if (code >= 500 && code <= 599) return "雾";
  return "";
}

#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_MOSI 23
#define TFT_SCK  18

#define BTN_PIN   0

#define COLOR_BG       0x0000
#define COLOR_CLOCK    0x07FF
#define COLOR_DATE     0xAEDC
#define COLOR_WHITE    0xFFFF
#define COLOR_LABEL    0x8410
#define COLOR_ACCENT   0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_YELLOW   0xFFE0
#define COLOR_CLOUD    0xBDD7
#define COLOR_RAIN     0x4AEF

SPIClass *vspi = nullptr;
Adafruit_ST7789 *tft = nullptr;
WiFiUDP ntpUDP;
NTPClient *timeClient = nullptr;

struct {
  String temp;
  String feelsLike;
  String humidity;
  String windDir;
  String windScale;
  String weatherText;
  String weatherIcon;
  String updateTime;
  String tempMax;
  String tempMin;
  bool valid;
} weather;

#define HOUR_COUNT 6

struct {
  String hourLabel[HOUR_COUNT];
  String temp[HOUR_COUNT];
  String icon[HOUR_COUNT];
  int tempInt[HOUR_COUNT];
  bool valid;
} hourly;

struct {
  bool wifiConnected;
  bool timeSynced;
  bool ntpTried;
  bool weatherLoaded;
  bool showingSystemInfo;
  unsigned long lastWeatherFetch;
  int lastSecond;
  int lastMinute;
  int lastBtnState;
  unsigned long bootTime;
} state;

void initDisplay() {
  vspi = new SPIClass(VSPI);
  vspi->begin(TFT_SCK, -1, TFT_MOSI, -1);
  tft = new Adafruit_ST7789(vspi, TFT_CS, TFT_DC, TFT_RST);
  tft->init(240, 240);
  tft->setRotation(1);
  tft->fillScreen(COLOR_BG);
  tft->setTextWrap(false);
}

void initWiFi() {
  tft->fillScreen(COLOR_BG);

  tft->fillRect(2, 2, 236, 16, COLOR_ACCENT);
  tft->setTextColor(COLOR_WHITE);
  tft->setTextSize(1);
  tft->setCursor(8, 5);
  tft->print("NETWORK STARTUP");

  tft->drawRect(2, 20, 236, 180, COLOR_LABEL);

  int lineY = 28, lineH = 12;

  tft->setTextSize(1);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(12, lineY);
  tft->print("WiFi connecting to [2400MHz]...");
  lineY += lineH;

  tft->fillRect(12, lineY, 220, lineH, COLOR_BG);
  lineY += lineH;

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setAutoReconnect(true);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(300);

    tft->fillRect(12, lineY, 220, lineH, COLOR_BG);
    tft->setTextColor(COLOR_YELLOW);
    tft->setCursor(12, lineY);
    tft->print("Connecting... ");
    tft->print(attempts + 1);
    tft->print("/60");

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    state.wifiConnected = true;

    tft->fillRect(12, lineY - lineH, 220, lineH * 2, COLOR_BG);
    tft->fillRect(12, lineY, 220, lineH, COLOR_BG);
    tft->setTextColor(COLOR_GREEN);
    tft->setCursor(12, lineY);
    tft->print("Connected!");
    lineY += lineH;

    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(12, lineY);
    tft->print("IP: ");
    tft->setTextColor(COLOR_CLOCK);
    tft->print(WiFi.localIP().toString());
    lineY += lineH;
  } else {
    state.wifiConnected = false;

    tft->fillRect(12, lineY - lineH, 220, lineH, COLOR_BG);
    tft->setTextColor(COLOR_ACCENT);
    tft->setCursor(12, lineY - lineH);
    tft->print("Connection failed!");
  }

  tft->drawFastHLine(2, 204, 236, COLOR_LABEL);
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, 210);
  tft->print("Status: ");
  tft->setTextColor(state.wifiConnected ? COLOR_GREEN : COLOR_ACCENT);
  tft->print(state.wifiConnected ? "Connected" : "Failed");

  tft->setCursor(160, 210);
  tft->setTextColor(COLOR_LABEL);
  tft->print("RSSI: ");
  tft->setTextColor(COLOR_WHITE);
  tft->print(WiFi.RSSI());
  tft->print("dBm");

  delay(1500);
}

void initNTP() {
  if (!state.wifiConnected) {
    tft->setTextColor(COLOR_ACCENT);
    tft->setCursor(12, 186);
    tft->print("NTP skipped (no WiFi)");
    return;
  }

  tft->setTextColor(COLOR_YELLOW);
  tft->setCursor(12, 186);
  tft->print("NTP syncing...");

  if (timeClient == nullptr) {
    timeClient = new NTPClient(ntpUDP, "pool.ntp.org", 28800, 3600000);
  }

  state.timeSynced = false;
  for (int i = 0; i < 10 && !state.timeSynced; i++) {
    tft->fillRect(12, 186, 220, 12, COLOR_BG);
    tft->setTextColor(COLOR_LABEL);
    tft->setCursor(12, 186);
    tft->print("NTP attempt ");
    tft->print(i + 1);
    tft->print("/10");

    if (timeClient->forceUpdate()) {
      state.timeSynced = true;
      tft->fillRect(12, 186, 220, 12, COLOR_BG);
      tft->setTextColor(COLOR_GREEN);
      tft->setCursor(12, 186);
      tft->print("NTP synced!");
      break;
    }
    delay(500);
  }

  if (!state.timeSynced) {
    tft->fillRect(12, 186, 220, 12, COLOR_BG);
    tft->setTextColor(COLOR_ACCENT);
    tft->setCursor(12, 186);
    tft->print("NTP failed - local time");
  }
  state.ntpTried = true;
  delay(500);
}

String urlEncode(String str) {
  String encoded = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

static const uint8_t *skipGzipHeader(const uint8_t *data, size_t len, size_t *deflateLen) {
  if (len < 10 || data[0] != 0x1f || data[1] != 0x8b) {
    *deflateLen = 0;
    return NULL;
  }
  size_t offset = 10;
  uint8_t flags = data[3];
  if (flags & 4) {
    if (offset + 2 > len) { *deflateLen = 0; return NULL; }
    uint16_t xlen = data[offset] | (data[offset + 1] << 8);
    offset += 2 + xlen;
  }
  if (flags & 8) {
    while (offset < len && data[offset]) offset++;
    offset++;
  }
  if (flags & 16) {
    while (offset < len && data[offset]) offset++;
    offset++;
  }
  if (flags & 2) offset += 2;
  if (offset > len) { *deflateLen = 0; return NULL; }
  *deflateLen = len - offset;
  return data + offset;
}

bool fetchWeather() {
  if (!state.wifiConnected) return false;

  HTTPClient http;
  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/now?location="
               + urlEncode(String(WEATHER_LOC));

  Serial.println("Fetching: " + url);

  http.begin(url);
  http.addHeader("X-QW-Api-Key", String(WEATHER_API_KEY));
  http.setTimeout(10000);
  int httpCode = http.GET();

  Serial.println("HTTP response code: " + String(httpCode));

  if (httpCode == HTTP_CODE_OK) {
    int totalLen = http.getSize();
    if (totalLen <= 0) totalLen = 4096;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t *buf = (uint8_t *)malloc(totalLen + 4);
    if (!buf) { http.end(); return false; }

    int readLen = 0;
    unsigned long timeout = millis() + 5000;
    while ((stream->available() || readLen == 0) && millis() < timeout) {
      while (stream->available() && readLen < totalLen) {
        buf[readLen++] = stream->read();
        timeout = millis() + 5000;
      }
      if (!stream->available()) delay(10);
    }
    buf[readLen] = 0;
    http.end();

    size_t deflateLen = 0;
    const uint8_t *deflateData = skipGzipHeader(buf, readLen, &deflateLen);
    if (!deflateData) {
      Serial.println("Not valid gzip data");
      free(buf);
      return false;
    }

    tinfl_decompressor *decomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!decomp) { free(buf); return false; }
    tinfl_init(decomp);

    size_t outLen = 16384;
    uint8_t *decompressed = (uint8_t *)malloc(outLen);
    if (!decompressed) { free(decomp); free(buf); return false; }

    size_t inPos = 0;
    size_t outPos = 0;
    int status;
    do {
      size_t inBytes = deflateLen - inPos;
      size_t outBytes = outLen - outPos;
      int flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
      if (inPos + inBytes < deflateLen) flags |= TINFL_FLAG_HAS_MORE_INPUT;

      status = tinfl_decompress(decomp,
                 deflateData + inPos, &inBytes,
                 decompressed, decompressed + outPos, &outBytes,
                 flags);
      inPos += inBytes;
      outPos += outBytes;

      if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
        outLen *= 2;
        uint8_t *newBuf = (uint8_t *)realloc(decompressed, outLen);
        if (!newBuf) { free(decomp); free(decompressed); free(buf); return false; }
        decompressed = newBuf;
      }
    } while (status != TINFL_STATUS_DONE && status > 0);

    free(decomp);
    free(buf);

    if (status != TINFL_STATUS_DONE) {
      Serial.println("gzip decompress failed, status: " + String(status));
      free(decompressed);
      return false;
    }

    decompressed[outPos] = 0;
    String payload = String((char *)decompressed);
    free(decompressed);

    Serial.println("Response: " + payload);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.println("JSON parse failed: " + String(err.c_str()));
      return false;
    }

    const char *code = doc["code"];
    Serial.println("API code: " + String(code));
    if (strcmp(code, "200") != 0) return false;

    JsonObject now = doc["now"];
    if (now.isNull()) return false;

    weather.temp        = now["temp"].as<String>();
    weather.feelsLike   = now["feelsLike"].as<String>();
    weather.humidity    = now["humidity"].as<String>();
    weather.windDir     = now["windDir"].as<String>();
    weather.windScale   = now["windScale"].as<String>();
    weather.weatherText = now["text"].as<String>();
    weather.weatherIcon = now["icon"].as<String>();
    weather.updateTime  = doc["updateTime"].as<String>();
    weather.valid       = true;
    state.weatherLoaded = true;

    Serial.println("Weather OK: " + weather.temp + "C " + weather.weatherText);
    return true;
  }

  String errPayload = http.getString();
  Serial.println("Error response: " + errPayload);
  http.end();
  return false;
}

void fetchHourly() {
  if (!state.wifiConnected) return;

  HTTPClient http;
  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/24h?location="
               + urlEncode(String(WEATHER_LOC));

  http.begin(url);
  http.addHeader("X-QW-Api-Key", String(WEATHER_API_KEY));
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int totalLen = http.getSize();
    if (totalLen <= 0) totalLen = 4096;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t *buf = (uint8_t *)malloc(totalLen + 4);
    if (!buf) { http.end(); return; }

    int readLen = 0;
    unsigned long timeout = millis() + 5000;
    while ((stream->available() || readLen == 0) && millis() < timeout) {
      while (stream->available() && readLen < totalLen) {
        buf[readLen++] = stream->read();
        timeout = millis() + 5000;
      }
      if (!stream->available()) delay(10);
    }
    buf[readLen] = 0;
    http.end();

    size_t deflateLen = 0;
    const uint8_t *deflateData = skipGzipHeader(buf, readLen, &deflateLen);
    if (!deflateData) { free(buf); return; }

    tinfl_decompressor *decomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!decomp) { free(buf); return; }
    tinfl_init(decomp);

    size_t outLen = 16384;
    uint8_t *decompressed = (uint8_t *)malloc(outLen);
    if (!decompressed) { free(decomp); free(buf); return; }

    size_t inPos = 0, outPos = 0;
    int status;
    do {
      size_t inBytes = deflateLen - inPos;
      size_t outBytes = outLen - outPos;
      int flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
      if (inPos + inBytes < deflateLen) flags |= TINFL_FLAG_HAS_MORE_INPUT;
      status = tinfl_decompress(decomp, deflateData + inPos, &inBytes,
                 decompressed, decompressed + outPos, &outBytes, flags);
      inPos += inBytes;
      outPos += outBytes;
      if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
        outLen *= 2;
        uint8_t *newBuf = (uint8_t *)realloc(decompressed, outLen);
        if (!newBuf) { free(decomp); free(decompressed); free(buf); return; }
        decompressed = newBuf;
      }
    } while (status != TINFL_STATUS_DONE && status > 0);

    free(decomp);
    free(buf);

    if (status != TINFL_STATUS_DONE) { free(decompressed); return; }
    decompressed[outPos] = 0;

    String payload = String((char *)decompressed);
    free(decompressed);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) { hourly.valid = false; return; }
    if (strcmp(doc["code"], "200") != 0) { hourly.valid = false; return; }

    JsonArray arr = doc["hourly"].as<JsonArray>();
    int count = min((int)arr.size(), HOUR_COUNT);

    for (int i = 0; i < count; i++) {
      JsonObject h = arr[i];
      String ft = h["fxTime"].as<String>();
      if (ft.length() >= 16) hourly.hourLabel[i] = ft.substring(11, 13) + "h";
      else hourly.hourLabel[i] = "--";
      hourly.temp[i] = h["temp"].as<String>();
      hourly.icon[i] = h["icon"].as<String>();
      hourly.tempInt[i] = hourly.temp[i].toInt();
    }
    hourly.valid = (count > 0);
  } else {
    http.end();
    hourly.valid = false;
  }
}

void fetchDaily() {
  if (!state.wifiConnected) return;

  HTTPClient http;
  String url = "https://" + String(WEATHER_HOST) + "/v7/weather/3d?location="
               + urlEncode(String(WEATHER_LOC));

  http.begin(url);
  http.addHeader("X-QW-Api-Key", String(WEATHER_API_KEY));
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int totalLen = http.getSize();
    if (totalLen <= 0) totalLen = 4096;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t *buf = (uint8_t *)malloc(totalLen + 4);
    if (!buf) { http.end(); return; }

    int readLen = 0;
    unsigned long timeout = millis() + 5000;
    while ((stream->available() || readLen == 0) && millis() < timeout) {
      while (stream->available() && readLen < totalLen) {
        buf[readLen++] = stream->read();
        timeout = millis() + 5000;
      }
      if (!stream->available()) delay(10);
    }
    buf[readLen] = 0;
    http.end();

    size_t deflateLen = 0;
    const uint8_t *deflateData = skipGzipHeader(buf, readLen, &deflateLen);
    if (!deflateData) { free(buf); return; }

    tinfl_decompressor *decomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!decomp) { free(buf); return; }
    tinfl_init(decomp);

    size_t outLen = 8192;
    uint8_t *decompressed = (uint8_t *)malloc(outLen);
    if (!decompressed) { free(decomp); free(buf); return; }

    size_t inPos = 0, outPos = 0;
    int status;
    do {
      size_t inBytes = deflateLen - inPos;
      size_t outBytes = outLen - outPos;
      int flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
      if (inPos + inBytes < deflateLen) flags |= TINFL_FLAG_HAS_MORE_INPUT;
      status = tinfl_decompress(decomp, deflateData + inPos, &inBytes,
                 decompressed, decompressed + outPos, &outBytes, flags);
      inPos += inBytes;
      outPos += outBytes;
      if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
        outLen *= 2;
        uint8_t *newBuf = (uint8_t *)realloc(decompressed, outLen);
        if (!newBuf) { free(decomp); free(decompressed); free(buf); return; }
        decompressed = newBuf;
      }
    } while (status != TINFL_STATUS_DONE && status > 0);

    free(decomp);
    free(buf);

    if (status != TINFL_STATUS_DONE) { free(decompressed); return; }
    decompressed[outPos] = 0;

    String payload = String((char *)decompressed);
    free(decompressed);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;
    if (strcmp(doc["code"], "200") != 0) return;

    weather.tempMax = doc["daily"][0]["tempMax"].as<String>();
    weather.tempMin = doc["daily"][0]["tempMin"].as<String>();
  } else {
    http.end();
  }
}

void drawCRTFrame() {
  tft->drawRect(0, 0, 240, 240, COLOR_WHITE);
  tft->drawRect(1, 1, 238, 238, COLOR_WHITE);

  tft->fillRect(4, 4, 232, 16, COLOR_ACCENT);
  tft->setTextColor(COLOR_WHITE);
  tft->setTextSize(1);
  tft->setCursor(10, 7);
  tft->print("DESKTOP MINI TV");

  tft->drawFastHLine(4, 222, 232, COLOR_ACCENT);
  tft->drawFastHLine(4, 223, 232, COLOR_ACCENT);
}

void drawWeatherIcon(int cx, int cy, int code) {
  int x0 = cx, y0 = cy;
  if (code == 100 || code == 150) {
    tft->fillCircle(x0, y0, 10, COLOR_YELLOW);
    tft->drawLine(x0, y0 - 14, x0, y0 - 18, COLOR_YELLOW);
    tft->drawLine(x0, y0 + 14, x0, y0 + 18, COLOR_YELLOW);
    tft->drawLine(x0 - 14, y0, x0 - 18, y0, COLOR_YELLOW);
    tft->drawLine(x0 + 14, y0, x0 + 18, y0, COLOR_YELLOW);
    tft->drawLine(x0 - 10, y0 - 10, x0 - 13, y0 - 13, COLOR_YELLOW);
    tft->drawLine(x0 + 10, y0 + 10, x0 + 13, y0 + 13, COLOR_YELLOW);
    tft->drawLine(x0 + 10, y0 - 10, x0 + 13, y0 - 13, COLOR_YELLOW);
    tft->drawLine(x0 - 10, y0 + 10, x0 - 13, y0 + 13, COLOR_YELLOW);
  } else if (code >= 101 && code <= 104) {
    tft->fillCircle(x0 - 10, y0 + 2, 8, COLOR_CLOUD);
    tft->fillCircle(x0, y0 - 2, 10, COLOR_CLOUD);
    tft->fillCircle(x0 + 10, y0 + 2, 8, COLOR_CLOUD);
    tft->fillRect(x0 - 10, y0 - 2, 20, 14, COLOR_CLOUD);
  } else if ((code >= 300 && code <= 399) || (code >= 400 && code <= 499)) {
    tft->fillCircle(x0 - 8, y0 - 2, 7, COLOR_CLOUD);
    tft->fillCircle(x0, y0 - 5, 9, COLOR_CLOUD);
    tft->fillCircle(x0 + 8, y0 - 2, 7, COLOR_CLOUD);
    tft->fillRect(x0 - 8, y0 - 5, 16, 10, COLOR_CLOUD);
    tft->drawLine(x0 - 8, y0 + 8, x0 - 10, y0 + 14, COLOR_RAIN);
    tft->drawLine(x0, y0 + 8, x0 - 2, y0 + 14, COLOR_RAIN);
    tft->drawLine(x0 + 8, y0 + 8, x0 + 6, y0 + 14, COLOR_RAIN);
  } else if (code >= 500 && code <= 599) {
    tft->fillCircle(x0 - 8, y0 - 2, 7, COLOR_CLOUD);
    tft->fillCircle(x0, y0 - 5, 9, COLOR_CLOUD);
    tft->fillCircle(x0 + 8, y0 - 2, 7, COLOR_CLOUD);
    tft->fillRect(x0 - 8, y0 - 5, 16, 10, COLOR_CLOUD);
    tft->drawLine(x0 - 8, y0 + 8, x0 - 8, y0 + 12, COLOR_WHITE);
    tft->drawLine(x0, y0 + 8, x0, y0 + 12, COLOR_WHITE);
    tft->drawLine(x0 + 8, y0 + 8, x0 + 8, y0 + 12, COLOR_WHITE);
  } else {
    tft->fillCircle(x0 - 8, y0, 8, COLOR_CLOUD);
    tft->fillCircle(x0, y0 - 3, 10, COLOR_CLOUD);
    tft->fillCircle(x0 + 8, y0, 8, COLOR_CLOUD);
    tft->fillRect(x0 - 8, y0 - 3, 16, 11, COLOR_CLOUD);
  }
}

String windDirToEn(String cn) {
  if (cn.indexOf("旋转") >= 0) return "Rot";
  if (cn.indexOf("无持续") >= 0) return "Calm";
  if (cn.indexOf("北") >= 0 && cn.indexOf("东") >= 0) return "NE";
  if (cn.indexOf("北") >= 0 && cn.indexOf("西") >= 0) return "NW";
  if (cn.indexOf("南") >= 0 && cn.indexOf("东") >= 0) return "SE";
  if (cn.indexOf("南") >= 0 && cn.indexOf("西") >= 0) return "SW";
  if (cn.indexOf("北") >= 0) return "N";
  if (cn.indexOf("南") >= 0) return "S";
  if (cn.indexOf("东") >= 0) return "E";
  if (cn.indexOf("西") >= 0) return "W";
  return "--";
}

void drawWeatherInfo() {
  tft->fillRect(4, 22, 232, 44, COLOR_BG);

  if (!weather.valid) {
    drawHanziText(tft, 50, 36, "不可用", COLOR_LABEL);
    return;
  }

  int iconCode = weather.weatherIcon.toInt();
  drawWeatherIcon(24, 36, iconCode);

  tft->setTextSize(3);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(56, 24);
  tft->print(weather.temp);
  tft->setTextSize(1);
  tft->setCursor(56 + weather.temp.length() * 18, 30);
  tft->print("C");

  drawHanziText(tft, 115, 28, "福州", COLOR_DATE);

  if (weather.tempMax.length() > 0) {
    tft->setCursor(170, 28);
    tft->setTextColor(COLOR_WHITE);
    tft->print("H:");
    tft->setTextColor(COLOR_CLOCK);
    tft->print(weather.tempMax);

    tft->setCursor(170, 42);
    tft->setTextColor(COLOR_WHITE);
    tft->print("L:");
    tft->setTextColor(COLOR_DATE);
    tft->print(weather.tempMin);
  }

  drawHanziText(tft, 8, 52, weather.weatherText.c_str(), COLOR_WHITE);

  tft->setCursor(115, 54);
  tft->setTextColor(COLOR_LABEL);
  tft->print("Hum:");
  tft->setTextColor(COLOR_WHITE);
  tft->print(weather.humidity);
  tft->print("%");

  tft->drawFastHLine(8, 68, 224, COLOR_LABEL);
}

void drawClock() {
  tft->fillRect(8, 70, 224, 70, COLOR_BG);

  int h, m, s;
  char dateBuf[24] = "";

  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    h = ti->tm_hour;
    m = ti->tm_min;
    s = ti->tm_sec;
    static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    sprintf(dateBuf, "%04d-%02d-%02d %s",
      ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
      days[ti->tm_wday]);
  } else if (state.ntpTried) {
    unsigned long ms = millis() - state.bootTime;
    s = (ms / 1000) % 60;
    m = (ms / 60000) % 60;
    h = (ms / 3600000) % 24;
    strcpy(dateBuf, "NTP failed, local time");
  } else {
    tft->setTextColor(COLOR_ACCENT);
    tft->setTextSize(2);
    tft->setCursor(50, 98);
    tft->print("Syncing...");
    return;
  }

  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);

  int strW = strlen(buf) * 30;
  int cx = (240 - strW) / 2;

  tft->setTextSize(5);
  tft->setTextColor(COLOR_CLOCK);
  tft->setCursor(cx, 72);
  tft->print(buf);

  char secBuf[3];
  sprintf(secBuf, "%02d", s);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_DATE);
  tft->setCursor(cx + strW + 6, 85);
  tft->print(secBuf);

  int dateW = strlen(dateBuf) * 12;
  int dcx = (240 - dateW) / 2;
  tft->setTextSize(2);
  tft->setTextColor(COLOR_DATE);
  tft->setCursor(dcx, 118);
  tft->print(dateBuf);

  tft->drawFastHLine(8, 142, 224, COLOR_LABEL);
}

void drawBottomInfo() {
  tft->fillRect(8, 146, 224, 72, COLOR_BG);

  if (weather.valid) {
    drawHanziText(tft, 16, 150, "体感", COLOR_LABEL);
    tft->setTextSize(2);
    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(52, 150);
    tft->print(weather.feelsLike);
    tft->print("C");

    tft->setTextColor(COLOR_LABEL);
    tft->setCursor(120, 150);
    tft->print("Wind:");
    tft->setTextColor(COLOR_WHITE);
    tft->print(windDirToEn(weather.windDir));
    tft->print("-");
    tft->print(weather.windScale);

    tft->drawFastHLine(12, 168, 216, COLOR_LABEL);

    drawHanziText(tft, 16, 174, "更新", COLOR_LABEL);
    tft->setTextSize(2);
    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(52, 174);
    if (weather.updateTime.length() >= 16) {
      tft->print(weather.updateTime.substring(11, 16));
    }

    tft->setCursor(180, 178);
    tft->setTextSize(1);
    if (state.wifiConnected) {
      int rssi = WiFi.RSSI();
      int bars = rssi > -50 ? 4 : rssi > -65 ? 3 : rssi > -80 ? 2 : 1;
      for (int i = 0; i < 4; i++) {
        int x = 156 + i * 6;
        int h = 3 + i * 3;
        tft->fillRect(x, 186 - h, 4, h, i < bars ? COLOR_GREEN : COLOR_LABEL);
      }
      tft->setTextColor(COLOR_GREEN);
      tft->print("WiFi");
    } else {
      drawHanziText(tft, 156, 174, "无信号", COLOR_ACCENT);
    }
  }

  if (hourly.valid) {
    tft->drawFastHLine(12, 190, 216, COLOR_LABEL);

    int minT = hourly.tempInt[0], maxT = hourly.tempInt[0];
    for (int i = 1; i < HOUR_COUNT; i++) {
      if (hourly.tempInt[i] < minT) minT = hourly.tempInt[i];
      if (hourly.tempInt[i] > maxT) maxT = hourly.tempInt[i];
    }
    if (maxT == minT) { maxT = minT + 1; minT = minT - 1; }

    int ptsX[HOUR_COUNT], ptsY[HOUR_COUNT];
    for (int i = 0; i < HOUR_COUNT; i++) {
      ptsX[i] = 20 + i * 34;
      ptsY[i] = 213 - ((hourly.tempInt[i] - minT) * 12 / (maxT - minT));
    }

    for (int i = 1; i < HOUR_COUNT; i++) {
      tft->drawLine(ptsX[i-1], ptsY[i-1], ptsX[i], ptsY[i], COLOR_DATE);
    }

    for (int i = 0; i < HOUR_COUNT; i++) {
      tft->fillCircle(ptsX[i], ptsY[i], 3, COLOR_CLOCK);

      tft->setTextSize(1);
      tft->setTextColor(COLOR_LABEL);
      tft->setCursor(ptsX[i] - 6, 192);
      tft->print(hourly.hourLabel[i]);

      tft->setTextColor(COLOR_WHITE);
      tft->setCursor(ptsX[i] - 6, 215);
      tft->print(hourly.temp[i]);
      tft->print("C");
    }
  }
}

void drawSystemInfo() {
  tft->fillScreen(COLOR_BG);

  tft->fillRect(2, 2, 236, 16, COLOR_CLOCK);
  tft->setTextColor(COLOR_BG);
  tft->setTextSize(1);
  tft->setCursor(8, 5);
  tft->print("SYSTEM STATUS");

  int y = 24;
  int lh = 12;

  tft->setTextSize(1);
  tft->setTextColor(COLOR_DATE);
  tft->setCursor(8, y);
  tft->print("[ ESP32 ]");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Chip: ");

  tft->setTextColor(COLOR_WHITE);
  tft->print(ESP.getChipModel());
  tft->print(" rev");
  tft->print(ESP.getChipRevision());
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Flash: ");
  tft->setTextColor(COLOR_WHITE);
  tft->print(ESP.getFlashChipSize() / (1024 * 1024));
  tft->print("MB  Free: ");
  tft->print(ESP.getFreeHeap() / 1024);
  tft->print("KB");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Uptime: ");
  tft->setTextColor(COLOR_WHITE);
  unsigned long up = millis() / 1000;
  tft->print(up / 3600);
  tft->print("h ");
  tft->print((up % 3600) / 60);
  tft->print("m ");
  tft->print(up % 60);
  tft->print("s");
  y += lh + 4;

  tft->drawFastHLine(8, y, 224, COLOR_LABEL);
  y += 4;

  tft->setTextColor(COLOR_DATE);
  tft->setCursor(8, y);
  tft->print("[ WiFi ]");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("SSID: ");
  tft->setTextColor(COLOR_WHITE);
  tft->print(WIFI_SSID);
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("IP:   ");
  tft->setTextColor(state.wifiConnected ? COLOR_GREEN : COLOR_ACCENT);
  if (state.wifiConnected) tft->print(WiFi.localIP().toString());
  else tft->print("--.--.--.--");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("GW:   ");
  tft->setTextColor(COLOR_WHITE);
  if (state.wifiConnected) tft->print(WiFi.gatewayIP().toString());
  else tft->print("--.--.--.--");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("RSSI: ");
  tft->setTextColor(COLOR_WHITE);
  if (state.wifiConnected) {
    tft->print(WiFi.RSSI());
    tft->print("dBm");
  } else {
    tft->print("N/A");
  }
  y += lh + 4;

  tft->drawFastHLine(8, y, 224, COLOR_LABEL);
  y += 4;

  tft->setTextColor(COLOR_DATE);
  tft->setCursor(8, y);
  tft->print("[ NTP ]");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Status: ");
  tft->setTextColor(state.timeSynced ? COLOR_GREEN : COLOR_ACCENT);
  tft->print(state.timeSynced ? "Synced" : "Failed");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Time:  ");
  tft->setTextColor(COLOR_WHITE);
  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    char buf[20];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d",
      ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
      ti->tm_hour, ti->tm_min);
    tft->print(buf);
  } else {
    tft->print("Not available");
  }
  y += lh + 4;

  tft->drawFastHLine(8, y, 224, COLOR_LABEL);
  y += 4;

  tft->setTextColor(COLOR_DATE);
  tft->setCursor(8, y);
  tft->print("[ Weather API ]");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("Now: ");
  tft->setTextColor(weather.valid ? COLOR_GREEN : COLOR_ACCENT);
  tft->print(weather.valid ? "OK" : "N/A");
  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(120, y);
  tft->print("3d: ");
  tft->setTextColor(weather.tempMax.length() > 0 ? COLOR_GREEN : COLOR_ACCENT);
  tft->print(weather.tempMax.length() > 0 ? "OK" : "N/A");
  y += lh;

  tft->setTextColor(COLOR_LABEL);
  tft->setCursor(8, y);
  tft->print("24h: ");
  tft->setTextColor(hourly.valid ? COLOR_GREEN : COLOR_ACCENT);
  tft->print(hourly.valid ? "OK" : "N/A");

  tft->setCursor(120, y);
  tft->setTextColor(COLOR_LABEL);
  tft->print("Mem: ");
  tft->setTextColor(COLOR_WHITE);
  tft->print(ESP.getFreeHeap() / 1024);
  tft->print("KB");
  y += lh;

  tft->drawFastHLine(4, y, 232, COLOR_LABEL);
  y += 2;
  tft->setTextColor(COLOR_LABEL);
  tft->setTextSize(1);
  tft->setCursor(8, y);
  tft->print("Press BOOT to return");
}

void drawError(const char *msg) {
  tft->fillRect(30, 100, 180, 30, COLOR_BG);
  tft->drawRect(30, 100, 180, 30, COLOR_ACCENT);
  tft->setTextColor(COLOR_ACCENT);
  tft->setTextSize(1);
  tft->setCursor(40, 110);
  tft->print(msg);
}

void drawFullUI() {
  tft->fillScreen(COLOR_BG);
  drawCRTFrame();
  drawWeatherInfo();
  drawClock();
  drawBottomInfo();
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Desktop Mini TV Starting...");

  state.wifiConnected = false;
  state.timeSynced = false;
  state.ntpTried = false;
  state.weatherLoaded = false;
  state.showingSystemInfo = false;
  state.lastWeatherFetch = 0;
  state.lastSecond = -1;
  state.lastMinute = -1;
  state.lastBtnState = HIGH;
  state.bootTime = millis();
  weather.valid = false;
  hourly.valid = false;

  initDisplay();
  pinMode(BTN_PIN, INPUT_PULLUP);
  initWiFi();
  initNTP();
  fetchWeather();
  fetchDaily();
  fetchHourly();
  drawSystemInfo();
  drawFullUI();
}

void loop() {
  unsigned long now = millis();

  int curBtn = digitalRead(BTN_PIN);
  if (curBtn == LOW && state.lastBtnState == HIGH) {
    state.showingSystemInfo = !state.showingSystemInfo;
    if (state.showingSystemInfo) drawSystemInfo();
    else drawFullUI();
  }
  state.lastBtnState = curBtn;

  if (state.showingSystemInfo) { delay(50); return; }

  if (state.wifiConnected && state.timeSynced) {
    timeClient->update();
  }

  if (!state.timeSynced && !state.ntpTried && state.wifiConnected) {
    initNTP();
    if (state.timeSynced) {
      drawFullUI();
    } else {
      drawClock();
    }
  }

  int curSec = -1, curMin = -1;
  if (state.timeSynced) {
    time_t t = timeClient->getEpochTime();
    struct tm *ti = localtime(&t);
    curSec = ti->tm_sec;
    curMin = ti->tm_min;
  } else {
    curSec = (now / 1000) % 60;
    curMin = (now / 60000) % 60;
  }

  if (curMin != state.lastMinute || !state.weatherLoaded) {
    state.lastMinute = curMin;

    if (now - state.lastWeatherFetch > WEATHER_INTERVAL_MS || state.lastWeatherFetch == 0) {
      if (state.wifiConnected) {
        fetchWeather();
        fetchDaily();
        fetchHourly();
        drawWeatherInfo();
        drawBottomInfo();
        state.lastWeatherFetch = now;
      }
    }

    drawBottomInfo();
  }

  if (curSec != state.lastSecond) {
    state.lastSecond = curSec;
    drawClock();
  }

  if (WiFi.status() != WL_CONNECTED && state.wifiConnected) {
    state.wifiConnected = false;
    drawError("WiFi Disconnected");
    WiFi.reconnect();
  } else if (WiFi.status() == WL_CONNECTED && !state.wifiConnected) {
    state.wifiConnected = true;
    if (!state.timeSynced) initNTP();
    state.lastWeatherFetch = 0;
    drawWeatherInfo();
  }

  delay(50);
}
