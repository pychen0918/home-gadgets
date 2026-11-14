// Seat timer: reminds you to stand up after 30 minutes of continuous sitting.
//
// Hardware
//   Board : Waveshare e-Paper ESP32 Driver Board (ESP32-WROOM-32E)
//   Panel : Waveshare 2.66inch e-Paper, 4-colour (G) or black/white; see src/panel.h
//   Radar : HLK-LD2410C, UART 256000 baud
//   Buzzer: active buzzer module, low-level trigger (3 pins: VCC / GND / I/O)
//   LEDs  : WS2812B 10-LED bar (PH2.0 3P -> Dupont cable)
//
// Serial console (115200) commands, for testing without the radar:
//   1 = force "seated"   0 = force "not seated"   a = back to radar (auto)
//
// Network (optional; timer works without it): Wi-Fi from src/secrets.h,
// time via NTP, weather from Open-Meteo every 30 minutes.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SPI.h>
#include <ld2410.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"  // first: may override WEATHER_LAT / WEATHER_LON
#include "panel.h"
#include "sit_logic.h"
#include "screen.h"
#include "weather.h"

// ---------------------------------------------------------------- config ----
// Pins. E-paper pins are fixed by the driver board's FPC connector (colour panel);
// wire the mono module's cable to the same GPIOs.
constexpr int PIN_EPD_SCK = 13, PIN_EPD_MOSI = 14, PIN_EPD_CS = 15;
constexpr int PIN_EPD_DC = 27, PIN_EPD_RST = 26, PIN_EPD_BUSY = 25;
constexpr int PIN_RADAR_RX = 16;  // ESP32 RX  <-  LD2410C TX
constexpr int PIN_RADAR_TX = 17;  // ESP32 TX  ->  LD2410C RX
constexpr int PIN_BUZZER   = 32;
constexpr int PIN_LED      = 33;
constexpr int LED_COUNT    = 10;  // TIS WS2812B 10-LED bar with PH2.0 connectors

// Many buzzer modules sound when I/O is LOW. If yours beeps nonstop at boot
// and goes quiet on reminders, flip this.
constexpr bool BUZZER_ACTIVE_LOW = true;   // TIS "有源蜂鳴器模組 低電平觸發"

// A detected person counts as "seated" only within this distance of the radar.
// Watch the serial log ("dist=") while sitting / standing beside the chair to tune.
constexpr uint16_t SEATED_MAX_CM = 100;

constexpr uint8_t  LED_BRIGHTNESS = 60;      // 0..255
constexpr uint32_t RADAR_FAULT_MS = 3000;    // no radar frames this long = fault
constexpr uint32_t EPD_MIN_GAP_MS = 180000;  // min time between e-paper refreshes (Waveshare: >= 180 s)
constexpr uint32_t WX_REDRAW_MS   = 60UL * 60 * 1000;  // redraw for changed weather at most this often
constexpr uint32_t WX_EVERY_MS    = 30UL * 60 * 1000;  // weather refresh
constexpr uint32_t WX_RETRY_MS    = 2UL * 60 * 1000;   // after a failed fetch
constexpr uint8_t  CLEAR_HOUR     = 3;       // daily anti-ghosting wipe to white (local hour)

// Time zone (POSIX TZ string) and NTP servers. Override any of them in
// src/secrets.h; the defaults suit Taiwan.
#ifndef TIME_ZONE
#define TIME_ZONE "CST-8"  // UTC+8, no daylight saving
#endif
#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "time.stdtime.gov.tw"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_3
#define NTP_SERVER_3 "time.google.com"
#endif

static sit::Config makeConfig() {
  sit::Config c;
#ifdef DEMO_MODE
  c.sitConfirmMs  = 3000;
  c.breakMs       = 20000;
  c.sitLimitMs    = 60000;
  c.warnBeforeMs  = 15000;
  c.remindEveryMs = 10000;
#endif
  return c;
}

// --------------------------------------------------------------- globals ----
PanelDisplay display(PanelDriver(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
ld2410 radar;
Adafruit_NeoPixel leds(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);
sit::Tracker tracker(makeConfig());

enum class Override : uint8_t { Auto, ForceOn, ForceOff };
Override overrideMode = Override::Auto;

// ---------------------------------------------------------------- buzzer ----
// Non-blocking beep pattern: `remaining` short beeps, 120 ms on / 120 ms off.
struct Buzzer {
  uint8_t  remaining = 0;
  bool     on = false;
  uint32_t nextToggle = 0;
  void write(bool sound) { digitalWrite(PIN_BUZZER, (sound != BUZZER_ACTIVE_LOW) ? HIGH : LOW); }
  void begin() { pinMode(PIN_BUZZER, OUTPUT); write(false); }
  // Restart cleanly even mid-beep: silence first, so the new pattern is countable.
  void start(uint8_t beeps, uint32_t now) { write(false); remaining = beeps; on = false; nextToggle = now + 120; }
  void stop() { remaining = 0; on = false; write(false); }
  void tick(uint32_t now) {
    if (remaining == 0 || (int32_t)(now - nextToggle) < 0) return;
    on = !on;
    write(on);
    if (!on) remaining--;
    nextToggle = now + 120;
  }
} buzzer;

// ------------------------------------------------------------------ LEDs ----
// Quiet by design: the bar stays dark except when it is time to stand up
// (red blink), a break has just been counted (3 green blinks), or the radar
// has stopped talking (slow purple blink).
constexpr uint32_t BREAK_BLINK_MS = 250;  // green on/off period after a completed break

static void showLeds(const sit::Output& o, bool radarFault, uint32_t breakDoneAt, uint32_t now) {
  leds.clear();
  uint32_t sinceBreak = now - breakDoneAt;
  if (radarFault) {
    if ((now / 1000) % 2) for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, 120, 0, 160);
  } else if (breakDoneAt && sinceBreak < 6 * BREAK_BLINK_MS) {
    if ((sinceBreak / BREAK_BLINK_MS) % 2 == 0)
      for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, 0, 200, 0);
  } else if (o.state == sit::State::Overdue) {
    if ((now / 500) % 2) for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, 255, 0, 0);
  }
  leds.show();
}

// -------------------------------------------------------------- e-paper -----
QueueHandle_t epdQueue;

// Queue item: a snapshot plus "wipe to white first" (daily anti-ghosting).
struct EpdJob {
  Snapshot s;
  bool     clearFirst;
  int8_t   shiftHour;  // local hour for burn-in shift, -1 if unknown
};

static void drawSnapshot(const EpdJob& j) {
  static const Palette pal PANEL_PALETTE;
  if (j.clearFirst) display.clearScreen();
  int dx = 0, dy = 0;
  if (j.shiftHour >= 0) burnInShift((uint8_t)j.shiftHour, dx, dy);
  ShiftedCanvas<decltype(display)> canvas{display, dx, dy};
  display.setFullWindow();
  display.firstPage();
  do {
    drawScreen(canvas, j.s, pal);
  } while (display.nextPage());
  display.hibernate();
}

// Refreshing this panel blocks for ~20 s, so it runs in its own task and the
// main loop (radar, buzzer, LEDs) never waits on it.
static void epdTask(void*) {
  EpdJob j;
  for (;;) {
    if (xQueueReceive(epdQueue, &j, portMAX_DELAY) == pdTRUE) {
      uint32_t t0 = millis();
      drawSnapshot(j);
      Serial.printf("[epd] screen=%u min=%lu clear=%d refreshed in %lu ms\n", (unsigned)j.s.screen,
                    (unsigned long)j.s.minutes, j.clearFirst, (unsigned long)(millis() - t0));
      vTaskDelay(pdMS_TO_TICKS(EPD_MIN_GAP_MS));  // newer requests overwrite the queue slot
    }
  }
}

// The screen only shows date + weather; sitting state lives on the LEDs and
// buzzer, so sitting down or standing up never costs a 20 s panel refresh.
static Screen screenFor(bool fault) { return fault ? Screen::RadarFault : Screen::Ready; }

// -------------------------------------------------------------- network ----
static portMUX_TYPE wxLock = portMUX_INITIALIZER_UNLOCKED;
static Weather wxShared;  // guarded by wxLock

static bool wifiConfigured() { return WIFI_SSID[0] != 0; }

static bool fetchWeather(Weather& out) {
  WiFiClientSecure tls;
  tls.setInsecure();  // public, non-sensitive data; skip CA bundle management
  // Open-Meteo sometimes accepts the TCP connection and then never answers the
  // TLS handshake (seen from a Mac too, 2026-11-14). The default handshake
  // timeout is 120 s; give up after 15 s so the 2-minute retry gets a fresh try.
  tls.setHandshakeTimeout(15);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  if (!http.begin(tls, WEATHER_URL)) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) ok = parseWeather(http.getString().c_str(), out);
  Serial.printf("[wx] HTTP %d (%s) parse=%s rssi=%d heap=%u maxblock=%u\n", code,
                code < 0 ? HTTPClient::errorToString(code).c_str() : "-", ok ? "ok" : "fail",
                WiFi.RSSI(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  http.end();
  return ok;
}

// Runs on core 0 so slow HTTP never delays the radar / buzzer loop.
static void netTask(void*) {
  uint32_t nextFetch = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && (int32_t)(millis() - nextFetch) >= 0) {
      Weather w;
      bool ok = fetchWeather(w);
      if (ok) {
        portENTER_CRITICAL(&wxLock);
        wxShared = w;
        portEXIT_CRITICAL(&wxLock);
      }
      nextFetch = millis() + (ok ? WX_EVERY_MS : WX_RETRY_MS);
    }
    static uint32_t lastNetLog = 0;
    if (millis() - lastNetLog >= 60000) {
      lastNetLog = millis();
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED)
        Serial.printf("[net] wifi=up rssi=%d dBm ip=%s\n", WiFi.RSSI(), WiFi.localIP().toString().c_str());
      else
        Serial.printf("[net] wifi=down status=%d\n", (int)st);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

static Weather currentWeather() {
  portENTER_CRITICAL(&wxLock);
  Weather w = wxShared;
  portEXIT_CRITICAL(&wxLock);
  return w;
}

// Local time, or false if NTP has not synced yet.
static bool localNow(struct tm& t) { return getLocalTime(&t, 0) && t.tm_year > 120; }

static bool sameWeather(const Weather& a, const Weather& b) {
  return a.valid == b.valid && a.temp == b.temp && a.code == b.code && a.tMin == b.tMin &&
         a.tMax == b.tMax && a.rainPct == b.rainPct;
}

// ----------------------------------------------------------------- main ----
static uint32_t lastRadarOk = 0;
static uint16_t lastDist = 0;

static bool readSeated(uint32_t now, bool& fault) {
  radar.read();
  if (radar.isConnected()) lastRadarOk = now;
  fault = now - lastRadarOk > RADAR_FAULT_MS;

  if (overrideMode == Override::ForceOn)  { fault = false; return true; }
  if (overrideMode == Override::ForceOff) { fault = false; return false; }
  if (fault) return false;

  bool seated = false;
  lastDist = 0;
  if (radar.stationaryTargetDetected()) {
    lastDist = radar.stationaryTargetDistance();
    seated |= lastDist <= SEATED_MAX_CM;
  }
  if (radar.movingTargetDetected()) {
    uint16_t d = radar.movingTargetDistance();
    if (!lastDist || d < lastDist) lastDist = d;
    seated |= d <= SEATED_MAX_CM;
  }
  return seated;
}

static void handleSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '1') overrideMode = Override::ForceOn;
    else if (ch == '0') overrideMode = Override::ForceOff;
    else if (ch == 'a') overrideMode = Override::Auto;
    else continue;
    Serial.printf("[cmd] mode=%s\n", overrideMode == Override::Auto ? "auto"
                                     : overrideMode == Override::ForceOn ? "seated" : "not seated");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[seat-timer] boot, panel " PANEL_NAME);
#ifdef DEMO_MODE
  Serial.println("[seat-timer] DEMO_MODE: 60 s limit, 20 s break");
#endif

  buzzer.begin();
  leds.begin();
  leds.setBrightness(LED_BRIGHTNESS);
  leds.clear();
  leds.show();

  Serial2.begin(256000, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
  radar.begin(Serial2, false);

  SPI.end();
  SPI.begin(PIN_EPD_SCK, /*MISO=*/12, PIN_EPD_MOSI, PIN_EPD_CS);
  display.init(0, true, 2, false);  // short reset pulse for Waveshare boards
  display.setRotation(1);            // landscape

  epdQueue = xQueueCreate(1, sizeof(EpdJob));
  xTaskCreatePinnedToCore(epdTask, "epd", 8192, nullptr, 1, nullptr, 0);

  if (wifiConfigured()) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // non-blocking; reconnects on its own
    configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 1, nullptr, 0);
    Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  } else {
    Serial.println("[wifi] no SSID in secrets.h, running offline");
  }

  lastRadarOk = millis();  // give the radar a grace period after boot
}

void loop() {
  static uint32_t lastSample = 0, lastLog = 0, wxDrawnAt = 0, breakDoneAt = 0;
  static Screen shown = Screen::Boot;
  static Weather shownWx;
  static DateView shownDate;
  static bool shownOnline = false, shownForced = false;
  static int lastClearDay = -1;
  static sit::Output o;
  static bool fault = false;

  uint32_t now = millis();
  handleSerial();
  buzzer.tick(now);

  if (now - lastSample >= 200) {
    lastSample = now;
    bool seated = readSeated(now, fault);
    o = tracker.update(now, seated);

    if (o.beep) {
      uint8_t n = o.reminderIndex + 2;  // 3, 4, 5 ... beeps, escalating
      buzzer.start(n > 8 ? 8 : n, now);
      Serial.printf("[remind] #%u\n", o.reminderIndex);
    }
    // Short cues so you know the radar noticed: 1 beep sitting down, 2 getting up.
    // Getting up also cuts off any reminder still sounding.
    if (o.stoodUp) {
      buzzer.start(2, now);
      Serial.println("[cue] stood up");
    } else if (o.satDown && !o.beep) {
      buzzer.start(1, now);
      Serial.println("[cue] sat down");
    }
    if (o.breakCompleted) {  // away long enough: 3 beeps + 3 green blinks, in case you're nearby
      breakDoneAt = now ? now : 1;
      buzzer.start(3, now);
      Serial.printf("[break] done, breaks=%u\n", o.breaks);
    }

    // Decide whether the e-paper needs a redraw.
    Screen sc = screenFor(fault);
    Weather wx = currentWeather();
    bool online = WiFi.status() == WL_CONNECTED;
    bool forced = overrideMode != Override::Auto;
    struct tm lt;
    bool timeOk = localNow(lt);
    DateView date;
    if (timeOk) { date.valid = true; date.month = lt.tm_mon + 1; date.day = lt.tm_mday; date.wday = lt.tm_wday; }

    // Redraw only when what is on screen would become wrong:
    //   date changed, weather first arrived (or changed, at most once per WX_REDRAW_MS),
    //   online status changed while no weather is shown, or radar fault came / went.
    bool wxChanged = !sameWeather(wx, shownWx);
    bool redraw = sc != shown || forced != shownForced;
    if (sc == Screen::Ready)
      redraw |= date.valid != shownDate.valid || date.day != shownDate.day ||
                (wxChanged && (wx.valid != shownWx.valid || now - wxDrawnAt >= WX_REDRAW_MS)) ||
                (!wx.valid && online != shownOnline);

    // Once a day at CLEAR_HOUR, if nobody is seated, wipe to white before redrawing.
    bool clearFirst = false;
    if (timeOk && lt.tm_hour == CLEAR_HOUR && sc == Screen::Ready && lastClearDay != lt.tm_mday) {
      clearFirst = true;
      lastClearDay = lt.tm_mday;
      redraw = true;
    }

    if (redraw) {
      EpdJob j{};
      j.s.screen = sc;
      j.s.limitMin = tracker.config().sitLimitMs / 60000;
      j.s.forced = forced;
      j.s.date = date; j.s.wx = wx; j.s.online = online;
      j.clearFirst = clearFirst;
      j.shiftHour = timeOk ? lt.tm_hour : -1;
      xQueueOverwrite(epdQueue, &j);
      if (wxChanged) wxDrawnAt = now;
      shown = sc; shownForced = forced;
      shownWx = wx; shownDate = date; shownOnline = online;
    }

    if (now - lastLog >= 1000) {
      lastLog = now;
      static const char* names[] = {"Empty", "Sitting", "Warning", "Overdue", "Away"};
      Serial.printf("[state] %-7s seated=%d dist=%ucm sit=%lus away=%lus breaks=%u radar=%s\n",
                    names[(int)o.state], seated, lastDist, (unsigned long)(o.sittingMs / 1000),
                    (unsigned long)(o.awayMs / 1000), o.breaks, fault ? "FAULT" : "ok");
    }
  }

  showLeds(o, fault, breakDoneAt, now);
  delay(10);
}
