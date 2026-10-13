#pragma once
// tdisplay_s3_display.h — TDisplayS3Display hardware class (ST7789V, 8-bit i8080).
// Do not include directly — include board_display.h, which picks the board.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

// Unlike the Waveshare RGB panel, the ST7789 holds its own frame memory: the
// ESP32 only writes when pushSprite() runs, so there is no continuous PSRAM
// read and none of the RGB pixel-clock / WiFi jitter limits apply here.
class TDisplayS3Display : public lgfx::LGFX_Device {
 public:
  TDisplayS3Display() {
    {
      auto cfg       = bus_.config();
      cfg.freq_write = cfg::kBusWriteHz;
      cfg.pin_wr     = cfg::kPinLcdWr;
      cfg.pin_rd     = cfg::kPinLcdRd;
      cfg.pin_rs     = cfg::kPinLcdDc;
      cfg.pin_d0     = cfg::kPinD0;
      cfg.pin_d1     = cfg::kPinD1;
      cfg.pin_d2     = cfg::kPinD2;
      cfg.pin_d3     = cfg::kPinD3;
      cfg.pin_d4     = cfg::kPinD4;
      cfg.pin_d5     = cfg::kPinD5;
      cfg.pin_d6     = cfg::kPinD6;
      cfg.pin_d7     = cfg::kPinD7;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg            = panel_.config();
      cfg.pin_cs          = cfg::kPinLcdCs;
      cfg.pin_rst         = cfg::kPinLcdReset;
      cfg.pin_busy        = -1;
      cfg.memory_width    = 240;  // ST7789 controller RAM
      cfg.memory_height   = 320;
      cfg.panel_width     = cfg::kScreenWidth;
      cfg.panel_height    = cfg::kScreenHeight;
      cfg.offset_x        = cfg::kPanelOffsetX;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = false;
      cfg.invert          = true;   // LilyGO factory example inverts colour
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      panel_.config(cfg);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_Parallel8 bus_;
  lgfx::Panel_ST7789  panel_;
};
