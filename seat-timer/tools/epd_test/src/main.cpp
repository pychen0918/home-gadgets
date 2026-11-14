#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_4C.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// Waveshare e-Paper ESP32 Driver Board: SCK=13, MOSI=14, CS=15, DC=27, RST=26, BUSY=25
GxEPD2_4C<GxEPD2_266c_GDEY0266F51H, GxEPD2_266c_GDEY0266F51H::HEIGHT>
    display(GxEPD2_266c_GDEY0266F51H(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25));

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[epd_test] start");
  SPI.end();
  SPI.begin(/*SCK=*/13, /*MISO=*/12, /*MOSI=*/14, /*SS=*/15);
  display.init(115200, true, 2, false);  // short reset pulse for Waveshare boards
  display.setRotation(1);                 // landscape: 360 x 184
  Serial.printf("[epd_test] size %d x %d\n", display.width(), display.height());

  uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do {
    int w = display.width(), h = display.height();
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(0, 0, w, h, GxEPD_BLACK);
    display.drawRect(3, 3, w - 6, h - 6, GxEPD_RED);

    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(16, 44);
    display.print("BLACK OK");
    display.setTextColor(GxEPD_RED);
    display.setCursor(16, 84);
    display.print("RED OK");

    // color swatches: black / red / yellow
    int sw = (w - 32 - 16) / 3, y = 100, sh = 50;
    display.fillRect(16, y, sw, sh, GxEPD_BLACK);
    display.fillRect(16 + sw + 8, y, sw, sh, GxEPD_RED);
    display.fillRect(16 + 2 * (sw + 8), y, sw, sh, GxEPD_YELLOW);

    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(16, h - 12);
    display.print("black  /  red  /  yellow");
  } while (display.nextPage());
  Serial.printf("[epd_test] refresh done in %lu ms\n", millis() - t0);
  display.hibernate();
  Serial.println("[epd_test] hibernate, done");
}

void loop() {}
