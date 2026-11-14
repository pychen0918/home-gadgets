// Minimal UTF-8 text renderer for pre-rendered glyph bitmaps (see zh_font.h).
// Works with any canvas that has drawBitmap(x, y, bitmap, w, h, color),
// e.g. Adafruit_GFX / GxEPD2, or the host-side preview canvas.
#pragma once
#include <stdint.h>

struct ZhGlyph {
  uint16_t cp;   // Unicode code point
  uint8_t  w, h; // bitmap size
  int8_t   xo;   // x offset from pen position
  int8_t   yo;   // y offset from baseline to top of bitmap (negative = above)
  uint8_t  adv;  // pen advance
  uint32_t off;  // byte offset into bitmap; rows are MSB-first, padded to bytes
};

struct ZhFont {
  const uint8_t* bitmap;
  const ZhGlyph* glyphs;  // sorted by cp
  uint16_t count;
  uint8_t  ascent, descent, space;
};

namespace zh {

// Decode one UTF-8 code point and advance `s`. Returns 0 at end of string.
inline uint32_t next(const char*& s) {
  const uint8_t* p = (const uint8_t*)s;
  if (!*p) return 0;
  uint32_t c; int n;
  if (*p < 0x80) { c = *p; n = 1; }
  else if ((*p >> 5) == 0x6) { c = *p & 0x1F; n = 2; }
  else if ((*p >> 4) == 0xE) { c = *p & 0x0F; n = 3; }
  else { c = *p & 0x07; n = 4; }
  for (int i = 1; i < n; i++) {
    if ((p[i] & 0xC0) != 0x80) { s += i; return 0xFFFD; }
    c = (c << 6) | (p[i] & 0x3F);
  }
  s += n;
  return c;
}

inline const ZhGlyph* find(const ZhFont& f, uint32_t cp) {
  int lo = 0, hi = (int)f.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (f.glyphs[mid].cp == cp) return &f.glyphs[mid];
    if (f.glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}

// Glyphs requested by draw() that the font lacks. The host preview test
// asserts this stays 0; on the device it is only informational.
inline uint32_t& missCount() { static uint32_t n = 0; return n; }

inline int advance(const ZhFont& f, uint32_t cp) {
  if (cp == ' ') return f.space;
  const ZhGlyph* g = find(f, cp);
  return g ? g->adv : f.ascent / 2;  // missing glyph: leave a gap
}

inline int width(const ZhFont& f, const char* s) {
  int w = 0;
  for (uint32_t cp; (cp = next(s));) w += advance(f, cp);
  return w;
}

// Draw `s` with its baseline at y. Returns the x after the last glyph.
template <class Canvas>
int draw(Canvas& c, const ZhFont& f, int x, int y, const char* s, uint16_t color) {
  for (uint32_t cp; (cp = next(s));) {
    const ZhGlyph* g = cp == ' ' ? nullptr : find(f, cp);
    if (!g && cp != ' ') missCount()++;
    if (g && g->w && g->h)
      c.drawBitmap(x + g->xo, y + g->yo, f.bitmap + g->off, g->w, g->h, color);
    x += advance(f, cp);
  }
  return x;
}

// Count glyphs in `s` that the font lacks (for tests).
inline int missing(const ZhFont& f, const char* s) {
  int n = 0;
  for (uint32_t cp; (cp = next(s));) if (cp != ' ' && !find(f, cp)) n++;
  return n;
}

}  // namespace zh
