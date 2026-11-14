// Host-side preview of every e-paper screen, using the same layout code as
// the firmware. Writes one PPM per screen and fails if any glyph is missing.
//
// Run: g++ -std=c++17 test/preview_screens.cpp -o /tmp/p && /tmp/p <out_dir>
// Renders both panel sizes: <name>_color.ppm (360x184) and <name>_mono.ppm (296x152).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/screen.h"

enum : uint16_t { WHITE = 0, BLACK = 1, RED = 2, YELLOW = 3 };

struct Canvas {
  int w, h;
  std::vector<uint16_t> px;
  Canvas(int w_ = 360, int h_ = 184) : w(w_), h(h_), px(w_ * h_, WHITE) {}
  int width() const { return w; }
  int height() const { return h; }
  int outOfBounds = 0;
  void set(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= w || y >= h) { outOfBounds++; return; }
    px[y * w + x] = c;
  }
  void fillScreen(uint16_t c) { std::fill(px.begin(), px.end(), c); }
  void fillRect(int x, int y, int rw, int rh, uint16_t c) {
    for (int j = 0; j < rh; j++) for (int i = 0; i < rw; i++) set(x + i, y + j, c);
  }
  void drawRect(int x, int y, int rw, int rh, uint16_t c) {
    for (int i = 0; i < rw; i++) { set(x + i, y, c); set(x + i, y + rh - 1, c); }
    for (int j = 0; j < rh; j++) { set(x, y + j, c); set(x + rw - 1, y + j, c); }
  }
  void drawBitmap(int x, int y, const uint8_t* bmp, int bw, int bh, uint16_t c) {
    int stride = (bw + 7) / 8;
    for (int j = 0; j < bh; j++)
      for (int i = 0; i < bw; i++)
        if (bmp[j * stride + (i >> 3)] & (0x80 >> (i & 7))) set(x + i, y + j, c);
  }
  void writePPM(const std::string& path) const {
    static const uint8_t rgb[4][3] = {{255, 255, 255}, {0, 0, 0}, {200, 30, 30}, {235, 200, 0}};
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint16_t c : px) fwrite(rgb[c], 1, 3, f);
    fclose(f);
  }
};

static Snapshot base(Screen sc, uint32_t min) {
  Snapshot s{};
  s.screen = sc; s.minutes = min; s.breaks = 3; s.limitMin = 30; s.forced = false;
  s.date = {true, 11, 9, 0};
  s.sitStart = {true, 23, 30};
  s.remindAt = {true, 0, 0};
  s.wx.valid = true; s.wx.temp = 17; s.wx.code = 3; s.wx.tMin = 16; s.wx.tMax = 23; s.wx.rainPct = 99;
  s.online = true;
  return s;
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  struct Panel { const char* tag; int w, h; Palette pal; };
  const Panel panels[] = {{"color", 360, 184, {WHITE, BLACK, RED, YELLOW}},
                          {"mono", 296, 152, {WHITE, BLACK, BLACK, BLACK}}};
  int failures = 0;
  for (const Panel& pn : panels) {
  const Palette& pal = pn.pal;

  Snapshot offline = base(Screen::Sitting, 7);
  offline.date.valid = false; offline.sitStart.valid = false; offline.remindAt.valid = false;
  offline.wx.valid = false; offline.online = false;
  Snapshot loading = base(Screen::Ready, 0);
  loading.wx.valid = false;
  Snapshot noNet = base(Screen::Ready, 0);
  noNet.wx.valid = false; noNet.date.valid = false; noNet.online = false;
  Snapshot manual = base(Screen::Overdue, 135);
  manual.breaks = 12; manual.forced = true;
  Snapshot cold = base(Screen::Ready, 0);
  cold.wx.temp = -13; cold.wx.code = 82; cold.wx.tMin = -18; cold.wx.tMax = -2; cold.wx.rainPct = 100;
  cold.date = {true, 12, 31, 3};
  Snapshot longWord = base(Screen::Sitting, 0);
  longWord.wx.code = 1; longWord.wx.temp = -13; longWord.date = {true, 12, 31, 3};

  struct Case { const char* name; Snapshot s; } cases[] = {
      {"ready",           base(Screen::Ready, 0)},
      {"sitting",         base(Screen::Sitting, 0)},
      {"ready_loading",   loading},
      {"ready_offline",   noNet},
      {"ready_cold",      cold},
      {"sitting_long",    longWord},
      {"sitting_offline", offline},
      {"overdue",         manual},
      {"fault",           base(Screen::RadarFault, 0)},
      {"boot",            base(Screen::Boot, 0)},
  };

  for (auto& c : cases) {
    // Draw at every burn-in offset; nothing may fall off-screen or miss a glyph.
    for (int hour = 0; hour < 8; hour++) {
      Canvas cv(pn.w, pn.h);
      int dx, dy;
      burnInShift(hour, dx, dy);
      ShiftedCanvas<Canvas> sc{cv, dx, dy};
      zh::missCount() = 0;
      drawScreen(sc, c.s, pal);
      if (zh::missCount()) { printf("FAIL %s/%s: %u missing glyph(s)\n", pn.tag, c.name, zh::missCount()); failures++; }
      if (cv.outOfBounds) { printf("FAIL %s/%s shift(%d,%d): %d px off-screen\n", pn.tag, c.name, dx, dy, cv.outOfBounds); failures++; }
      if (hour == 0) cv.writePPM(dir + "/" + c.name + "_" + pn.tag + ".ppm");
    }
  }

  // Every weather word must render.
  for (int code = 0; code < 256; code++) {
    for (Screen scr : {Screen::Ready, Screen::Sitting}) {
    Snapshot s = base(scr, 0);
    s.wx.code = (uint8_t)code;
    Canvas cv(pn.w, pn.h);
    zh::missCount() = 0;
    drawScreen(cv, s, pal);
    if (cv.outOfBounds) { printf("FAIL %s weather code %d off-screen\n", pn.tag, code); failures++; }
    if (zh::missCount()) { printf("FAIL weather code %d (%s) missing glyphs\n", code, wmoText(code)); failures++; }
    }
  }

  }  // panels

  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("all screens rendered at all shifts, no missing glyphs, nothing off-screen\n");
  return 0;
}
