// Open-Meteo forecast parsing and WMO weather-code -> Traditional Chinese text.
// Pure C++ + ArduinoJson so it can be unit-tested on the host.
#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include "weather_view.h"

// Location for the forecast. Override in src/secrets.h to keep your own
// location out of the repository; the default is central Taipei.
#ifndef WEATHER_LAT
#define WEATHER_LAT "25.0330"
#define WEATHER_LON "121.5654"
#endif

#define WEATHER_URL                                                                     \
  "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LAT "&longitude=" WEATHER_LON \
  "&current=temperature_2m,weather_code"                                                \
  "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" \
  "&timezone=auto&forecast_days=1"  /* "today" in the location's own time zone */

inline bool parseWeather(const char* json, Weather& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariantConst cur = doc["current"], day = doc["daily"];
  if (!cur["temperature_2m"].is<float>() || !cur["weather_code"].is<int>()) return false;
  if (!day["temperature_2m_max"][0].is<float>() || !day["temperature_2m_min"][0].is<float>())
    return false;
  Weather w;
  w.temp    = (int8_t)lroundf(cur["temperature_2m"].as<float>());
  w.code    = (uint8_t)cur["weather_code"].as<int>();
  w.tMax    = (int8_t)lroundf(day["temperature_2m_max"][0].as<float>());
  w.tMin    = (int8_t)lroundf(day["temperature_2m_min"][0].as<float>());
  w.rainPct = (uint8_t)(day["precipitation_probability_max"][0] | 0);
  w.valid   = true;
  out = w;
  return true;
}

