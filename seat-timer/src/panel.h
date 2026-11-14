// E-paper panel selection. Pick one with a build flag (see platformio.ini):
//   -DPANEL_COLOR : Waveshare 2.66inch e-Paper Module (G), 360x184,
//                   black/white/yellow/red, full refresh only (~20 s)
//   -DPANEL_MONO  : Waveshare 2.66inch e-Paper Module (black/white), 296x152,
//                   full refresh ~6 s, partial refresh ~0.3 s
// Both use the same ESP32 pins (SCK 13, MOSI 14, CS 15, DC 27, RST 26, BUSY 25):
// the colour panel through the driver board's FPC connector, the mono module
// through its 8-pin cable onto the same header pins.
#pragma once
#include <GxEPD2_BW.h>
#include <GxEPD2_4C.h>

#if defined(PANEL_COLOR) == defined(PANEL_MONO)
#error "Define exactly one of PANEL_COLOR or PANEL_MONO"
#endif

#if defined(PANEL_COLOR)
using PanelDriver = GxEPD2_266c_GDEY0266F51H;
using PanelDisplay = GxEPD2_4C<PanelDriver, PanelDriver::HEIGHT>;
#define PANEL_NAME "2.66in 4-colour (G) 360x184"
#define PANEL_PALETTE {GxEPD_WHITE, GxEPD_BLACK, GxEPD_RED, GxEPD_YELLOW}
#else
using PanelDriver = GxEPD2_266_BN;
using PanelDisplay = GxEPD2_BW<PanelDriver, PanelDriver::HEIGHT>;
#define PANEL_NAME "2.66in black/white 296x152"
// No red or yellow: accents fall back to black, alert background to black.
#define PANEL_PALETTE {GxEPD_WHITE, GxEPD_BLACK, GxEPD_BLACK, GxEPD_BLACK}
#endif
