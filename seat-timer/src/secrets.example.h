// Copy to src/secrets.h and fill in. src/secrets.h is in .gitignore: it holds
// your Wi-Fi password and home location, so keep it out of the repository.
#pragma once

// Required for date and weather (the timer itself works without Wi-Fi).
// ESP32 supports 2.4 GHz Wi-Fi only.
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// Optional: weather location, decimal degrees as strings. Default: central Taipei.
// #define WEATHER_LAT "25.0330"
// #define WEATHER_LON "121.5654"

// Optional: time zone (POSIX TZ string) and NTP servers. Defaults suit Taiwan.
// #define TIME_ZONE    "CST-8"
// #define NTP_SERVER_1 "time.stdtime.gov.tw"
// #define NTP_SERVER_2 "pool.ntp.org"
// #define NTP_SERVER_3 "time.google.com"
