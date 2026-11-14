// Weather data as shown on screen. No dependencies, so screen.h can use it on the host.
#pragma once
#include <stdint.h>

struct Weather {
  bool    valid   = false;
  int8_t  temp    = 0;   // current, rounded °C
  uint8_t code    = 0;   // current WMO code
  int8_t  tMin    = 0;   // today
  int8_t  tMax    = 0;
  uint8_t rainPct = 0;   // today's max precipitation probability
};

// Every string returned here must also be in TEXTS["M"] of tools/gen_zh_font.py.
inline const char* wmoText(uint8_t c) {
  if (c == 0) return "晴";
  if (c == 1) return "晴時多雲";
  if (c == 2) return "多雲";
  if (c == 3) return "陰";
  if (c == 45 || c == 48) return "霧";
  if (c >= 51 && c <= 57) return "毛毛雨";
  if (c == 61 || c == 63) return "雨";
  if (c == 65) return "大雨";
  if (c == 66 || c == 67) return "凍雨";
  if (c >= 71 && c <= 77) return "雪";
  if (c >= 80 && c <= 82) return "陣雨";
  if (c == 85 || c == 86) return "陣雪";
  if (c >= 95) return "雷雨";
  return "—";
}
