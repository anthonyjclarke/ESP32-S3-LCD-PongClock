#pragma once

#include <Arduino.h>

// ── Firmware ──────────────────────────────────────────────────────────────────
#define FW_VERSION        "2.0.3"
#define FIRMWARE_VERSION  FW_VERSION   // alias for badge/display use

// ── Hardware pins (Waveshare ESP32-S3-LCD-3.16) ───────────────────────────────
namespace cfg {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 820;

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

constexpr uint32_t kRgbClockHz = 18000000;

constexpr uint8_t  kBacklightDefaultBrightness = 255;
constexpr uint8_t  kBacklightMinBrightness     = 0;
constexpr uint8_t  kBacklightMaxBrightness     = 255;
constexpr uint32_t kBacklightPwmHz             = 50000;
constexpr uint8_t  kBacklightPwmResolution     = 8;
constexpr uint8_t  kBacklightPwmChannel        = 1;

inline uint32_t backlightDutyFromBrightness(uint8_t brightness) {
  return 0xFFu - brightness;  // inverted: duty 0 = full brightness
}

}  // namespace cfg

// ── Button ────────────────────────────────────────────────────────────────────
#define PIN_BUTTON              4     // tactile button, wired to GND (INPUT_PULLUP)
#define BUTTON_LONG_PRESS_MS    600   // hold ≥ 600 ms → brightness cycle
#define BUTTON_DEBOUNCE_MS      300   // minimum ms between registered presses
#define BRIGHTNESS_STEPS        4     // 4-step cycle: 60/120/180/255

// ── LED matrix ────────────────────────────────────────────────────────────────
// Physical display: 320×820 portrait → setRotation(1) → logical 820×320
// 80×32 grid at 10 px/LED = 800×320 sprite; 10 px margins left/right
#define LED_WIDTH               80
#define LED_HEIGHT              32
#define PIXEL_SIZE              10
#define PIXEL_INNER             8     // filled inner rect (1 px gap each side in 10 px cell)
#define PIXEL_MARGIN            1
#define LED_X_OFFSET            10    // (820 - 800) / 2 = 10
#define LED_Y_OFFSET            0     // 320 - 320 = 0 (exact vertical fill)

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
#define WIFI_TIMEOUT_S          60    // captive-portal auto-close timeout

// ── NTP / time ───────────────────────────────────────────────────────────────
#define NTP_TIMEZONE            "Australia/Sydney"
#define NTP_POSIX_FALLBACK      "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SYNC_TIMEOUT_S      20

// ── Web server ────────────────────────────────────────────────────────────────
constexpr uint16_t WEB_SERVER_PORT = 80;
