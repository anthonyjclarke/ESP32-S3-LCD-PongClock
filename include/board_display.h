#pragma once
// board_display.h — selects the display class for the board chosen in
// platformio.ini and declares the global `gfx` plus backlight control.

#include "config.h"

#if defined(BOARD_WAVESHARE_LCD316)
#include "waveshare_display.h"
using BoardDisplay = WaveshareDisplay;
#elif defined(BOARD_TDISPLAY_S3)
#include "tdisplay_s3_display.h"
using BoardDisplay = TDisplayS3Display;
#endif

// Global display instance — defined in main.cpp
extern BoardDisplay gfx;

// ── Backlight control ─────────────────────────────────────────────────────────
// Board-specific implementations in display.cpp. brightness: 0 = off, 255 = full.
void initBacklight(uint8_t brightness);
void setBacklightBrightness(uint8_t brightness);
