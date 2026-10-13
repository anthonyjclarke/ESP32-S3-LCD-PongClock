#pragma once

#include <Arduino.h>

// ── Firmware ──────────────────────────────────────────────────────────────────
#define FW_VERSION        "2.1.0"
#define FIRMWARE_VERSION  FW_VERSION   // alias for badge/display use

// ── Board selection ───────────────────────────────────────────────────────────
// Set by the PlatformIO env (-DBOARD_xxx). Everything board-specific lives in
// the two blocks below; clock modes only ever see the 80×32 LED grid.
#if defined(BOARD_WAVESHARE_LCD316) == defined(BOARD_TDISPLAY_S3)
#error "Define exactly one of BOARD_WAVESHARE_LCD316 or BOARD_TDISPLAY_S3"
#endif

#if defined(BOARD_WAVESHARE_LCD316)
// ── Waveshare ESP32-S3-LCD-3.16 — ST7701 RGB parallel, 320×820 ───────────────
#define BOARD_NAME "Waveshare ESP32-S3-LCD-3.16"
#define PANEL_DESC "ST7701 820×320"

namespace cfg {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 820;
constexpr uint8_t kRotation = 1;   // landscape: logical 820×320

constexpr int kPinBacklight = 6;

constexpr int kPinLcdCs   = 0;
constexpr int kPinLcdSck  = 2;
constexpr int kPinLcdMosi = 1;
constexpr int kPinLcdReset = 16;

constexpr int kPinHsync = 38;
constexpr int kPinVsync = 39;
constexpr int kPinDe    = 40;
constexpr int kPinPclk  = 41;

constexpr int kPinR0 = 17;
constexpr int kPinR1 = 46;
constexpr int kPinR2 = 3;
constexpr int kPinR3 = 8;
constexpr int kPinR4 = 18;

constexpr int kPinG0 = 14;
constexpr int kPinG1 = 13;
constexpr int kPinG2 = 12;
constexpr int kPinG3 = 11;
constexpr int kPinG4 = 10;
constexpr int kPinG5 = 9;

constexpr int kPinB0 = 21;
constexpr int kPinB1 = 5;
constexpr int kPinB2 = 45;
constexpr int kPinB3 = 48;
constexpr int kPinB4 = 47;

constexpr uint32_t kRgbClockHz = 14000000;

constexpr uint32_t kBacklightPwmHz             = 50000;
constexpr uint8_t  kBacklightPwmResolution     = 8;
constexpr uint8_t  kBacklightPwmChannel        = 1;

inline uint32_t backlightDutyFromBrightness(uint8_t brightness) {
  return 0xFFu - brightness;  // inverted: duty 0 = full brightness
}

// Boot status lines — no physical margin, so these overlay the bottom of the
// LED grid until the first clock frame repaints it.
constexpr int16_t kStatusLineY = 290;
constexpr int16_t kIpLineY     = 305;

}  // namespace cfg

// No user button and no touch panel on this board: control is web UI only.
// GPIO4 is the battery ADC (ADC1 CH3, per Waveshare's ADC demo) — never use
// it as a button. BOOT is GPIO0, which is also the LCD command CS.
#define PIN_BUTTON              -1

// 80×32 grid at 10 px/LED = 800×320 sprite; 10 px margins left/right
#define PIXEL_SIZE              10
#define PIXEL_INNER             8     // filled inner rect (1 px gap each side in 10 px cell)
#define PIXEL_MARGIN            1
#define PIXEL_RADIUS            1     // rounded LED corners
#define LED_X_OFFSET            10    // (820 - 800) / 2 = 10
#define LED_Y_OFFSET            0     // 320 - 320 = 0 (exact vertical fill)

#elif defined(BOARD_TDISPLAY_S3)
// ── LilyGO T-Display-S3 — ST7789V 8-bit i8080 parallel, 170×320 ──────────────
#define BOARD_NAME "LilyGO T-Display-S3"
#define PANEL_DESC "ST7789 320×170"

namespace cfg {

constexpr int kScreenWidth  = 170;
constexpr int kScreenHeight = 320;
constexpr int kPanelOffsetX = 35;  // 170-wide glass centred in 240-wide ST7789 RAM
constexpr uint8_t kRotation = 1;   // landscape: logical 320×170

constexpr int kPinPowerOn   = 15;  // must be HIGH or the panel is dark on battery
constexpr int kPinBacklight = 38;  // pulse-count dimming, 16 levels (LilyGO factory example)
constexpr uint8_t kBacklightLevels = 16;

constexpr int kPinLcdCs    = 6;
constexpr int kPinLcdDc    = 7;
constexpr int kPinLcdWr    = 8;
constexpr int kPinLcdRd    = 9;
constexpr int kPinLcdReset = 5;

constexpr int kPinD0 = 39;
constexpr int kPinD1 = 40;
constexpr int kPinD2 = 41;
constexpr int kPinD3 = 42;
constexpr int kPinD4 = 45;
constexpr int kPinD5 = 46;
constexpr int kPinD6 = 47;
constexpr int kPinD7 = 48;

constexpr uint32_t kBusWriteHz = 20000000;

// Boot status lines sit in the 21 px margins above and below the LED grid.
constexpr int16_t kStatusLineY = 2;
constexpr int16_t kIpLineY     = 152;

}  // namespace cfg

#define PIN_BUTTON              14    // on-board KEY button; same role as Waveshare GPIO4.
                                      // GPIO4 is battery sense here — never use it. BOOT (GPIO0) unused.

// 80×32 grid at 4 px/LED = 320×128 sprite; 21 px margins top/bottom
#define PIXEL_SIZE              4
#define PIXEL_INNER             3     // 3 px LED + 1 px gap
#define PIXEL_MARGIN            0
#define PIXEL_RADIUS            0     // square: a 1 px radius on 3 px reads as a blob
#define LED_X_OFFSET            0
#define LED_Y_OFFSET            21    // (170 - 128) / 2

#endif

// ── Button (PIN_BUTTON is per board, above; -1 = no button) ────────────────────
// Short press = next mode, long press = brightness step, on boards that have one.
#define BUTTON_LONG_PRESS_MS    600   // hold ≥ 600 ms → brightness cycle
#define BUTTON_DEBOUNCE_MS      300   // minimum ms between registered presses
#define BRIGHTNESS_STEPS        4     // 4-step cycle: 60/120/180/255

// ── LED matrix ────────────────────────────────────────────────────────────────
// Grid size is fixed for every board — clock-mode layouts are hand-placed on it.
#define LED_WIDTH               80
#define LED_HEIGHT              32

// ── Clock modes ───────────────────────────────────────────────────────────────
#define NUM_MODES               5     // 0=Slide 1=Pong 2=Digits 3=WordClock 4=Invaders
#define DEFAULT_CLOCK_MODE      0
#define AMPM_MODE               0     // 0 = 24-hour, 1 = 12-hour AM/PM

// ── Pong bat and ball constants ───────────────────────────────────────────────
// Scaled from 48-wide (CYD) to 80-wide: factor ≈ 1.67
#define BAT1_X                  3     // left bat x-position (LED column)
#define BAT2_X                  76    // right bat x-position
#define BAT_HEIGHT              8     // bat height in LED rows
#define BAT_MAX_Y               24    // max bat top row (LED_HEIGHT - BAT_HEIGHT)
#define BALL_START_X            39    // initial ball x (midfield for 80-wide)
#define MIDFIELD_LEFT           39    // detect x for left-side predict (vx < 0)
#define MIDFIELD_RIGHT          41    // detect x for right-side predict (vx > 0)

// ── Timing ────────────────────────────────────────────────────────────────────
#define SLIDE_DELAY             20    // ms between slide animation frames
#define PONG_BALL_DELAY         20    // ms between ball movement frames
#define INVADER_SCROLL_DELAY    100   // ms per invader scroll step
#define FADE_DELAY              25    // ms per fade step

// ── Brightness ───────────────────────────────────────────────────────────────
#define BRIGHTNESS_DEFAULT      180
#define BRIGHTNESS_MIN          60
#define BRIGHTNESS_MAX          255

// ── LED colours (default amber) ───────────────────────────────────────────────
#define COLOUR_LED_ON_R         255
#define COLOUR_LED_ON_G         140
#define COLOUR_LED_ON_B         0
#define COLOUR_LED_OFF_R        20
#define COLOUR_LED_OFF_G        8
#define COLOUR_LED_OFF_B        0

// ── Date display ─────────────────────────────────────────────────────────────
#define DATE_DISPLAY_MINS       10    // show date every N minutes in Slide/Digits

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_AP_NAME            "ESP32-S3-PongClock"
#define WIFI_TIMEOUT_S          60    // portal auto-close when saved WiFi is unreachable

// ── NTP / time ───────────────────────────────────────────────────────────────
#define NTP_TIMEZONE            "Australia/Sydney"
#define NTP_POSIX_FALLBACK      "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SYNC_TIMEOUT_S      20

// ── Web server ────────────────────────────────────────────────────────────────
constexpr uint16_t WEB_SERVER_PORT = 80;
