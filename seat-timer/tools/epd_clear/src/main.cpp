// Clear the e-paper to white and hibernate. Use before storing the panel.
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_4C.h>

GxEPD2_4C<GxEPD2_266c_GDEY0266F51H, GxEPD2_266c_GDEY0266F51H::HEIGHT>
    display(GxEPD2_266c_GDEY0266F51H(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25));

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[epd_clear] start");
  SPI.end();
  SPI.begin(13, 12, 14, 15);
  display.init(0, true, 2, false);
  uint32_t t0 = millis();
  display.clearScreen();  // full refresh to white
  Serial.printf("[epd_clear] cleared in %lu ms\n", millis() - t0);
  display.hibernate();
  Serial.println("[epd_clear] hibernate, done");
}

void loop() {}
