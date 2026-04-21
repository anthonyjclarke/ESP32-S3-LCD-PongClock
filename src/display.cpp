#include "display.h"
#include "config.h"
#include "debug.h"
#include "web.h"
#include <Arduino.h>
#include <driver/ledc.h>

// ── Sprite ────────────────────────────────────────────────────────────────────
lgfx::LGFX_Sprite matrixSprite(&gfx);

uint8_t currentBrightness = BRIGHTNESS_DEFAULT;

static uint16_t colourOn  = 0;
static uint16_t colourOff = 0;

bool ledColourChanged = false;

// ── Backlight (ESP-IDF LEDC, channel 1, timer 3, inverted) ───────────────────
void initBacklight(uint8_t brightness) {
  ledc_timer_config_t timer_conf = {
      .speed_mode       = LEDC_LOW_SPEED_MODE,
      .duty_resolution  = static_cast<ledc_timer_bit_t>(cfg::kBacklightPwmResolution),
      .timer_num        = LEDC_TIMER_3,
      .freq_hz          = cfg::kBacklightPwmHz,
      .clk_cfg          = LEDC_AUTO_CLK,
  };
  ledc_channel_config_t channel_conf = {
      .gpio_num   = cfg::kPinBacklight,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel    = static_cast<ledc_channel_t>(cfg::kBacklightPwmChannel),
      .intr_type  = LEDC_INTR_DISABLE,
      .timer_sel  = LEDC_TIMER_3,
      .duty       = cfg::backlightDutyFromBrightness(brightness),
      .hpoint     = 0,
  };
  ledc_timer_config(&timer_conf);
  ledc_channel_config(&channel_conf);
}

void setBacklightBrightness(uint8_t brightness) {
  const uint32_t duty = cfg::backlightDutyFromBrightness(brightness);
  ledc_set_duty(LEDC_LOW_SPEED_MODE,
                static_cast<ledc_channel_t>(cfg::kBacklightPwmChannel),
                duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE,
                   static_cast<ledc_channel_t>(cfg::kBacklightPwmChannel));
}

// ── Colour + sprite init ──────────────────────────────────────────────────────
void initColours() {
  colourOn  = gfx.color565(COLOUR_LED_ON_R,  COLOUR_LED_ON_G,  COLOUR_LED_ON_B);
  colourOff = gfx.color565(COLOUR_LED_OFF_R, COLOUR_LED_OFF_G, COLOUR_LED_OFF_B);

  // 8-bit depth (RGB332): 800×320 = 256 KB.
  // Try DMA-capable internal RAM first (faster, less PSRAM bus pressure).
  // Fall back to PSRAM if internal heap is fragmented (e.g. after a crash reset).
  matrixSprite.setColorDepth(8);
  void* buf = matrixSprite.createSprite(LED_WIDTH * PIXEL_SIZE, LED_HEIGHT * PIXEL_SIZE);
  if (!buf) {
    matrixSprite.setPsram(true);
    buf = matrixSprite.createSprite(LED_WIDTH * PIXEL_SIZE, LED_HEIGHT * PIXEL_SIZE);
  }
  if (!buf) {
    DBG_ERROR("Sprite alloc FAILED — heap %d bytes, PSRAM %d bytes",
              ESP.getFreeHeap(), ESP.getFreePsram());
  } else {
    DBG_INFO("Sprite OK — buffer @ %p  heap=%d  PSRAM=%d",
             buf, ESP.getFreeHeap(), ESP.getFreePsram());
  }
  matrixSprite.fillSprite(colourOff);

  // Background colour for the 10 px margins (left and right of the LED grid)
  gfx.fillScreen(gfx.color565(5, 2, 0));
}

// ── Virtual LED operations ────────────────────────────────────────────────────
void plot(int x, int y, bool on) {
  if (x < 0 || x >= LED_WIDTH || y < 0 || y >= LED_HEIGHT) return;

  uint16_t colour = on ? colourOn : colourOff;
  int px = x * PIXEL_SIZE + PIXEL_MARGIN;
  int py = y * PIXEL_SIZE + PIXEL_MARGIN;
  matrixSprite.fillRoundRect(px, py, PIXEL_INNER, PIXEL_INNER, 1, colour);
}

void pushMatrix() {
  matrixSprite.pushSprite(LED_X_OFFSET, LED_Y_OFFSET);
}

void cls() {
  // fillSprite already sets every pixel — including all LED cell areas — to
  // colourOff. A redundant 80×32 loop of fillRoundRect(colourOff) over an
  // already-colourOff background produced 2560 unnecessary PSRAM writes per
  // frame, which caused display jitter under PSRAM bus contention with WiFi.
  matrixSprite.fillSprite(colourOff);
}

void clsNow() {
  cls();
  pushMatrix();
}

// ── Brightness ────────────────────────────────────────────────────────────────
void setBrightness(uint8_t pwm) {
  currentBrightness = pwm;
  setBacklightBrightness(pwm);
}

// ── Fades ─────────────────────────────────────────────────────────────────────
void fade_down() {
  for (int i = currentBrightness; i >= 0; i -= 8) {
    setBacklightBrightness((uint8_t)i);
    webLoop();
    delay(FADE_DELAY);
  }
  setBacklightBrightness(0);
  cls();
  pushMatrix();
  setBacklightBrightness(currentBrightness);
}

void fade_up() {
  for (int i = 0; i <= currentBrightness; i += 8) {
    setBacklightBrightness((uint8_t)i);
    webLoop();
    delay(FADE_DELAY);
  }
  setBacklightBrightness(currentBrightness);
}

// ── Colour update ─────────────────────────────────────────────────────────────
void setLedColours(uint8_t onR, uint8_t onG, uint8_t onB,
                   uint8_t offR, uint8_t offG, uint8_t offB) {
  colourOn  = gfx.color565(onR, onG, onB);
  colourOff = gfx.color565(offR, offG, offB);
  cls();
  pushMatrix();
  ledColourChanged = true;
}
