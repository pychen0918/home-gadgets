// E-paper screen layout (landscape; 360x184 colour or 296x152 mono), Traditional Chinese.
// Templated on the canvas so the same code renders on the device (GxEPD2)
// and on the host for previews (test/preview_screens.cpp).
//
// The panel takes ~20 s of flashing per refresh, so screens only show things
// that stay true until the next state change: no live clock, no live counter
// (except Overdue, which is an alert and refreshes on purpose).
//
// Every literal string here must also be in TEXTS of tools/gen_zh_font.py;
// test/preview_screens.cpp fails if any drawn glyph is missing.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include "zh_font.h"
#include "weather_view.h"

enum class Screen : uint8_t { Boot, Ready, Sitting, Overdue, RadarFault };

struct DateView {
  bool    valid = false;
  uint8_t month = 0, day = 0, wday = 0;  // wday: 0 = Sunday
};

struct HM {
  bool    valid = false;
  uint8_t hour = 0, minute = 0;
};

struct Snapshot {
  Screen   screen;
  uint32_t minutes;   // shown only on Overdue
  uint16_t breaks;
  uint32_t limitMin;
  bool     forced;
  DateView date;
  HM       sitStart;  // when this sitting session started
  HM       remindAt;  // sitStart + limit
  Weather  wx;
  bool     online;    // Wi-Fi connected
};

struct Palette { uint16_t white, black, red, yellow; };

static const char* const ZH_WDAY[7] = {"日", "一", "二", "三", "四", "五", "六"};

inline void footerText(char* buf, size_t n, const Snapshot& s) {
  snprintf(buf, n, "今天起身 %u 次%s", s.breaks, s.forced ? "（手動模式）" : "");
}

// "11月9日 星期日"
inline void longDateText(char* buf, size_t n, const DateView& d) {
  if (d.valid) snprintf(buf, n, "%u月%u日 星期%s", d.month, d.day, ZH_WDAY[d.wday % 7]);
  else snprintf(buf, n, "等待網路校時");
}

// "11/9（日）"
inline void shortDateText(char* buf, size_t n, const DateView& d) {
  if (d.valid) snprintf(buf, n, "%u/%u（%s）", d.month, d.day, ZH_WDAY[d.wday % 7]);
  else snprintf(buf, n, "等待網路校時");
}

// "陰 17°C"
inline void weatherNowText(char* buf, size_t n, const Snapshot& s) {
  if (s.wx.valid) snprintf(buf, n, "%s %d°C", wmoText(s.wx.code), s.wx.temp);
  else snprintf(buf, n, "%s", s.online ? "天氣讀取中" : "未連網");
}

// "今天 16–23°C　降雨 99%"
inline void weatherDayText(char* buf, size_t n, const Snapshot& s) {
  if (s.wx.valid) snprintf(buf, n, "今天 %d–%d°C　降雨 %u%%", s.wx.tMin, s.wx.tMax, s.wx.rainPct);
  else buf[0] = 0;
}

// Wraps a canvas and offsets every draw call by (dx, dy). fillScreen is not
// shifted. Used to move the whole layout a pixel or two each hour so static
// lines and text do not sit on the same pixels all day.
template <class C>
struct ShiftedCanvas {
  C& c; int dx, dy;
  int width() const { return c.width(); }
  int height() const { return c.height(); }
  void fillScreen(uint16_t col) { c.fillScreen(col); }
  void fillRect(int x, int y, int w, int h, uint16_t col) { c.fillRect(x + dx, y + dy, w, h, col); }
  void drawRect(int x, int y, int w, int h, uint16_t col) { c.drawRect(x + dx, y + dy, w, h, col); }
  void drawBitmap(int x, int y, const uint8_t* b, int w, int h, uint16_t col) {
    c.drawBitmap(x + dx, y + dy, b, w, h, col);
  }
};

// Pixel offset for a given hour: cycles 0,1,2,1,0,-1,-2,-1 horizontally,
// and 0,1,0,-1 vertically, so nothing static stays put for more than an hour.
inline void burnInShift(uint8_t hour, int& dx, int& dy) {
  static const int8_t X[8] = {0, 1, 2, 1, 0, -1, -2, -1};
  static const int8_t Y[4] = {0, 1, 0, -1};
  dx = X[hour % 8];
  dy = Y[(hour / 2) % 4];
}

template <class C>
void drawCentered(C& c, const ZhFont& f, int baseline, const char* s, uint16_t color) {
  zh::draw(c, f, (c.width() - zh::width(f, s)) / 2, baseline, s, color);
}

template <class C>
void drawScreen(C& c, const Snapshot& s, const Palette& p) {
  const int W = c.width(), H = c.height();
  // Layout was designed for 184 px height; scale baselines for shorter panels (152 px mono).
  auto Y = [H](int v) { return v * H / 184; };
  const bool alert = s.screen == Screen::Overdue;
  const uint16_t bg = alert ? p.red : p.white;
  const uint16_t fg = alert ? p.white : p.black;
  char buf[96];
  c.fillScreen(bg);

  switch (s.screen) {
    case Screen::Boot:
      drawCentered(c, ZH_L, Y(104), "啟動中…", fg);
      return;  // no footer while booting

    case Screen::Ready: {
      // The short mono panel drops the footer hint so the weather has room.
      const bool compact = H < 184;
      const int yDate = compact ? 34 : 46, yTemp = compact ? 100 : 116, yDay = compact ? 136 : 144;
      longDateText(buf, sizeof buf, s.date);
      drawCentered(c, ZH_L, yDate, buf, fg);
      if (s.wx.valid) {
        // "<word> <temp>°C": word and unit in L, temperature in XL
        char num[8];
        snprintf(num, sizeof num, "%d", s.wx.temp);
        const char* word = wmoText(s.wx.code);
        const int gap = 10;
        int w = zh::width(ZH_L, word) + gap + zh::width(ZH_XL, num) + zh::width(ZH_L, "°C");
        int x = (W - w) / 2;
        x = zh::draw(c, ZH_L, x, yTemp, word, fg) + gap;
        x = zh::draw(c, ZH_XL, x, yTemp, num, fg);
        zh::draw(c, ZH_L, x, yTemp, "°C", fg);
        weatherDayText(buf, sizeof buf, s);
        drawCentered(c, ZH_M, yDay, buf, fg);
      } else {
        weatherNowText(buf, sizeof buf, s);
        drawCentered(c, ZH_M, Y(106), buf, fg);
      }
      if (!compact) {
        snprintf(buf, sizeof buf, "坐下自動開始計時%s", s.forced ? "（手動模式）" : "");
        drawCentered(c, ZH_M, H - 12, buf, fg);
      }
      return;
    }

    case Screen::Sitting: {
      // top strip: date left, weather right
      shortDateText(buf, sizeof buf, s.date);
      zh::draw(c, ZH_M, 12, Y(23), buf, fg);
      weatherNowText(buf, sizeof buf, s);
      zh::draw(c, ZH_M, W - 12 - zh::width(ZH_M, buf), Y(23), buf, fg);
      c.fillRect(4, Y(33), W - 8, 2, fg);  // inset so burn-in shift stays on-screen

      if (s.sitStart.valid && s.remindAt.valid) {
        snprintf(buf, sizeof buf, "%02u:%02u 坐下", s.sitStart.hour, s.sitStart.minute);
        drawCentered(c, ZH_L, Y(82), buf, fg);
        snprintf(buf, sizeof buf, "%02u:%02u 提醒你起身", s.remindAt.hour, s.remindAt.minute);
        drawCentered(c, ZH_L, Y(126), buf, fg);
      } else {
        drawCentered(c, ZH_L, Y(82), "計時中", fg);
        snprintf(buf, sizeof buf, "%lu 分鐘後提醒你起身", (unsigned long)s.limitMin);
        drawCentered(c, ZH_L, Y(126), buf, fg);
      }
      break;
    }

    case Screen::Overdue: {
      drawCentered(c, ZH_L, Y(46), "該站起來了！", fg);
      snprintf(buf, sizeof buf, "已坐 %lu 分鐘", (unsigned long)s.minutes);
      drawCentered(c, ZH_L, Y(100), buf, fg);
      drawCentered(c, ZH_M, Y(134), "起身離開座位 1 分鐘就會重新計時", fg);
      break;
    }

    case Screen::RadarFault:
      drawCentered(c, ZH_L, Y(46), "找不到雷達", p.red);
      drawCentered(c, ZH_M, Y(80), "請檢查 LD2410C 接線", fg);
      drawCentered(c, ZH_M, Y(106), "TX 接 GPIO16，RX 接 GPIO17", fg);
      drawCentered(c, ZH_M, Y(132), "VCC 接 5V，GND 接 GND", fg);
      break;
  }

  footerText(buf, sizeof buf, s);
  drawCentered(c, ZH_M, H - 12, buf, fg);
}
