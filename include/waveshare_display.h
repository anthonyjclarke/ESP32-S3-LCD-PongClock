#pragma once
// waveshare_display.h — WaveshareDisplay hardware class and global display instance.
// Include this wherever access to `gfx` or backlight control is needed.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include "config.h"

// ── ST7701 RGB parallel display class ────────────────────────────────────────
class WaveshareDisplay : public lgfx::LGFX_Device {
 public:
  class WavesharePanel : public lgfx::Panel_ST7701_Base {
   protected:
    const uint8_t *getInitCommands(uint8_t listno) const override {
      static constexpr const uint8_t list0[] = {
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
          0xEF, 1, 0x08,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
          0xC0, 2, 0xE5, 0x02,
          0xC1, 2, 0x15, 0x0A,
          0xC2, 2, 0x07, 0x02,
          0xCC, 1, 0x10,
          0xB0, 16, 0x00, 0x08, 0x51, 0x0D, 0xCE, 0x06, 0x00, 0x08,
                    0x08, 0x24, 0x05, 0xD0, 0x0F, 0x6F, 0x36, 0x1F,
          0xB1, 16, 0x00, 0x10, 0x4F, 0x0C, 0x11, 0x05, 0x00, 0x07,
                    0x07, 0x18, 0x02, 0xD3, 0x11, 0x6E, 0x34, 0x1F,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
          0xB0, 1, 0x4D,
          0xB1, 1, 0x37,
          0xB2, 1, 0x87,
          0xB3, 1, 0x80,
          0xB5, 1, 0x4A,
          0xB7, 1, 0x85,
          0xB8, 1, 0x21,
          0xB9, 2, 0x00, 0x13,
          0xC0, 1, 0x09,
          0xC1, 1, 0x78,
          0xC2, 1, 0x78,
          0xD0, 1, 0x88,
          0xE0, 3 + CMD_INIT_DELAY, 0x80, 0x00, 0x02, 100,
          0xE1, 11, 0x0F, 0xA0, 0x00, 0x00, 0x10, 0xA0, 0x00, 0x00, 0x00, 0x60, 0x60,
          0xE2, 13, 0x30, 0x30, 0x60, 0x60, 0x45, 0xA0, 0x00, 0x00, 0x46, 0xA0, 0x00, 0x00, 0x00,
          0xE3, 4, 0x00, 0x00, 0x33, 0x33,
          0xE4, 2, 0x44, 0x44,
          0xE5, 16, 0x0F, 0x4A, 0xA0, 0xA0, 0x11, 0x4A, 0xA0, 0xA0,
                    0x13, 0x4A, 0xA0, 0xA0, 0x15, 0x4A, 0xA0, 0xA0,
          0xE6, 4, 0x00, 0x00, 0x33, 0x33,
          0xE7, 2, 0x44, 0x44,
          0xE8, 16, 0x10, 0x4A, 0xA0, 0xA0, 0x12, 0x4A, 0xA0, 0xA0,
                    0x14, 0x4A, 0xA0, 0xA0, 0x16, 0x4A, 0xA0, 0xA0,
          0xEB, 7, 0x02, 0x00, 0x4E, 0x4E, 0xEE, 0x44, 0x00,
          0xED, 16, 0xFF, 0xFF, 0x04, 0x56, 0x72, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0x27, 0x65, 0x40, 0xFF, 0xFF,
          0xEF, 6, 0x08, 0x08, 0x08, 0x40, 0x3F, 0x64,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
          0xE8, 2, 0x00, 0x0E,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
          0x11, 0 + CMD_INIT_DELAY, 120,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
          0xE8, 2 + CMD_INIT_DELAY, 0x00, 0x0C, 10,
          0xE8, 2, 0x00, 0x00,
          0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
          0x3A, 1, 0x55,
          0x36, 1, 0x00,
          0x35, 1, 0x00,
          0x29, 0 + CMD_INIT_DELAY, 20,
          0xFF, 0xFF,
      };

      switch (listno) {
        case 0:  return list0;
        default: return nullptr;
      }
    }
  };

  WaveshareDisplay() {
    {
      auto cfg = panel_.config();
      cfg.pin_cs        = cfg::kPinLcdCs;
      cfg.pin_rst       = cfg::kPinLcdReset;
      cfg.memory_width  = cfg::kScreenWidth;
      cfg.memory_height = cfg::kScreenHeight;
      cfg.panel_width   = cfg::kScreenWidth;
      cfg.panel_height  = cfg::kScreenHeight;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.rgb_order     = true;
      cfg.invert        = false;
      panel_.config(cfg);
    }
    {
      auto cfg       = panel_.config_detail();
      cfg.pin_cs     = cfg::kPinLcdCs;
      cfg.pin_sclk   = cfg::kPinLcdSck;
      cfg.pin_mosi   = cfg::kPinLcdMosi;
      cfg.use_psram  = 1;
      panel_.config_detail(cfg);
    }
    {
      auto cfg               = bus_.config();
      cfg.panel              = &panel_;
      cfg.pin_d0             = cfg::kPinB0;
      cfg.pin_d1             = cfg::kPinB1;
      cfg.pin_d2             = cfg::kPinB2;
      cfg.pin_d3             = cfg::kPinB3;
      cfg.pin_d4             = cfg::kPinB4;
      cfg.pin_d5             = cfg::kPinG0;
      cfg.pin_d6             = cfg::kPinG1;
      cfg.pin_d7             = cfg::kPinG2;
      cfg.pin_d8             = cfg::kPinG3;
      cfg.pin_d9             = cfg::kPinG4;
      cfg.pin_d10            = cfg::kPinG5;
      cfg.pin_d11            = cfg::kPinR0;
      cfg.pin_d12            = cfg::kPinR1;
      cfg.pin_d13            = cfg::kPinR2;
      cfg.pin_d14            = cfg::kPinR3;
      cfg.pin_d15            = cfg::kPinR4;
      cfg.pin_henable        = cfg::kPinDe;
      cfg.pin_vsync          = cfg::kPinVsync;
      cfg.pin_hsync          = cfg::kPinHsync;
      cfg.pin_pclk           = cfg::kPinPclk;
      cfg.freq_write         = cfg::kRgbClockHz;
      cfg.hsync_polarity     = 0;
      cfg.hsync_front_porch  = 30;
      cfg.hsync_pulse_width  = 6;
      cfg.hsync_back_porch   = 30;
      cfg.vsync_polarity     = 0;
      cfg.vsync_front_porch  = 20;
      cfg.vsync_pulse_width  = 40;
      cfg.vsync_back_porch   = 20;
      cfg.pclk_idle_high     = 1;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_RGB    bus_;
  WavesharePanel   panel_;
};

// Global display instance — defined in main.cpp
extern WaveshareDisplay gfx;

// ── Backlight control ─────────────────────────────────────────────────────────
// Uses ESP-IDF LEDC channel 1, timer 3. Inverted: duty 0 = full brightness.
void initBacklight(uint8_t brightness);
void setBacklightBrightness(uint8_t brightness);
