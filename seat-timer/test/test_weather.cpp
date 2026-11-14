// Host test for Open-Meteo parsing, using a real response captured 2026-11-09.
// Run (ArduinoJson comes from PlatformIO's libdeps):
//   g++ -std=c++17 -I .pio/libdeps/color/ArduinoJson/src test/test_weather.cpp -o /tmp/w && /tmp/w
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "../src/weather.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main() {
  std::ifstream f("test/fixtures/open_meteo.json");
  std::stringstream ss; ss << f.rdbuf();
  std::string json = ss.str();
  CHECK(!json.empty());

  Weather w;
  CHECK(parseWeather(json.c_str(), w));
  CHECK(w.valid);
  CHECK(w.temp == 17);     // 17.2
  CHECK(w.code == 3);
  CHECK(w.tMax == 23);     // 22.7
  CHECK(w.tMin == 16);     // 16.3
  CHECK(w.rainPct == 99);
  CHECK(std::string(wmoText(w.code)) == "陰");

  Weather keep; keep.valid = true; keep.temp = 5;
  CHECK(!parseWeather("{\"error\":true,\"reason\":\"bad\"}", keep));
  CHECK(!parseWeather("not json", keep));
  CHECK(keep.temp == 5);   // failed parse leaves previous data untouched

  CHECK(std::string(wmoText(0)) == "晴");
  CHECK(std::string(wmoText(95)) == "雷雨");
  CHECK(std::string(wmoText(200)) == "雷雨");

  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("weather tests passed\n");
  return 0;
}
