#include "clock.h"
#include "display.h"
#include "fonts.h"
#include "config.h"
#include "config_nvs.h"
#include "debug.h"
#include "web.h"

// ── Globals ───────────────────────────────────────────────────────────────────
byte    rtc[7];
byte    clock_mode = 0;
bool    ampm       = AMPM_MODE;
Timezone myTZ;

// ── Button state ──────────────────────────────────────────────────────────────
static uint32_t btnDownMs      = 0;
static bool     btnWasDown     = false;
static bool     longFired      = false;
static uint32_t lastPressMs    = 0;
static uint8_t  brightnessStep = 0;
static byte     activeMode     = 255;

static void beginModeLoop() {
  activeMode = clock_mode;
}

void initButton() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  DBG_INFO("Button initialised on GPIO%d", PIN_BUTTON);
}

// Called from every mode loop.
// Long-press (≥ BUTTON_LONG_PRESS_MS) while held: cycles brightness, fires once.
// Returns false when the active mode has been changed externally.
bool run_mode() {
  if (activeMode != 255 && clock_mode != activeMode) return false;

  bool pressed = (digitalRead(PIN_BUTTON) == LOW);
  uint32_t now = millis();

  if (pressed) {
    if (!btnWasDown) {
      btnWasDown = true;
      longFired  = false;
      btnDownMs  = now;
    } else if (!longFired && (now - btnDownMs) >= BUTTON_LONG_PRESS_MS) {
      longFired = true;
      const uint8_t levels[BRIGHTNESS_STEPS] = {60, 120, 180, 255};
      brightnessStep = (brightnessStep + 1) % BRIGHTNESS_STEPS;
      setBrightness(levels[brightnessStep]);
      rtCfg.brightness = levels[brightnessStep];
      saveConfig();
      DBG_INFO("Long press: brightness step=%d pwm=%d", brightnessStep, levels[brightnessStep]);
    }
  } else {
    // Release: let checkButton() detect the short tap via btnWasDown flag
    if (btnWasDown && longFired) {
      // Long press already consumed — reset without triggering short tap
      btnWasDown = false;
    }
  }
  return true;
}

// Call once per mode loop iteration after run_mode().
// Returns +1 (next mode) on short tap, 0 on no event.
int8_t checkButton() {
  if (!btnWasDown) return 0;           // nothing in progress
  if (digitalRead(PIN_BUTTON) == LOW)  return 0;  // still held

  // Button released
  btnWasDown = false;

  if (longFired) return 0;  // long press was already consumed

  // Debounce: ignore presses closer than BUTTON_DEBOUNCE_MS
  uint32_t now = millis();
  if ((now - lastPressMs) < BUTTON_DEBOUNCE_MS) return 0;
  lastPressMs = now;

  DBG_INFO("Short tap: next mode");
  return 1;  // advance to next mode
}

// ── Date auto-display ─────────────────────────────────────────────────────────
static byte next_display_date = 255;  // sentinel: not yet set

// ── Time ──────────────────────────────────────────────────────────────────────
void get_time() {
  rtc[0] = myTZ.second();
  rtc[1] = myTZ.minute();
  rtc[2] = myTZ.hour();
  rtc[3] = myTZ.weekday() - 1;  // ezTime: 1=Sun → 0=Sun (matches tm_wday)
  rtc[4] = myTZ.day();
  rtc[5] = myTZ.month();
  rtc[6] = (byte)(myTZ.year() % 100);
}

static void serviceNetworkDelay(uint32_t delayMs) {
  uint32_t start = millis();
  while (millis() - start < delayMs) {
    events();
    webLoop();
    delay(5);
  }
}

static bool keepRunningMode(byte expectedMode) {
  return run_mode() && clock_mode == expectedMode;
}

// ── Font helpers (index mapping) ──────────────────────────────────────────────
static byte charIndex5x7(char c) {
  if (c >= 'A' && c <= 'Z') return (byte)(c & 0x1F);
  if (c >= 'a' && c <= 'z') return (byte)((c - 'a') + 41);
  if (c >= '0' && c <= '9') return (byte)((c - '0') + 31);
  if (c == ' ')  return 0;
  if (c == '.')  return 27;
  if (c == '\'') return 28;
  if (c == ':')  return 29;
  if (c == '>')  return 30;
  return 0;
}

static byte charIndex3x5(char c) {
  if (c >= 'A' && c <= 'Z') return (byte)(c & 0x1F);
  if (c >= 'a' && c <= 'z') return (byte)(c & 0x1F);  // lowercase maps same as uppercase
  if (c >= '0' && c <= '9') return (byte)((c - '0') + 31);
  if (c == ' ')  return 0;
  if (c == '.')  return 27;
  if (c == '\'') return 28;
  if (c == '!')  return 29;
  if (c == '?')  return 30;
  return 0;
}

// ── putChar — 5×7 ────────────────────────────────────────────────────────────
void putChar(byte x, byte y, char c) {
  byte idx = charIndex5x7(c);
  for (byte col = 0; col < 5; col++) {
    byte dots = pgm_read_byte_near(&myfont[idx][col]);
    for (byte row = 0; row < 7; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (1 << row)) != 0);
      }
    }
  }
}

// ── putTinyChar — 3×5 ────────────────────────────────────────────────────────
void putTinyChar(byte x, byte y, char c) {
  byte idx = charIndex3x5(c);
  for (byte col = 0; col < 3; col++) {
    byte dots = pgm_read_byte_near(&mytinyfont[idx][col]);
    for (byte row = 0; row < 5; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (1 << row)) != 0);
      }
    }
  }
}

// ── putBigChar — 10×14 ───────────────────────────────────────────────────────
// Digits '0'-'9' only.
void putBigChar(byte x, byte y, char c) {
  if (c < '0' || c > '9') return;
  byte idx = (byte)(c - '0');

  for (byte col = 0; col < 10; col++) {
    // Top half: 8 rows
    byte dots = pgm_read_byte_near(&mybigfont[idx][col]);
    for (byte row = 0; row < 8; row++) {
      if (x + col < LED_WIDTH && y + row < LED_HEIGHT) {
        plot(x + col, y + row, (dots & (128 >> row)) != 0);
      }
    }
    // Bottom half: 6 rows (bytes 10-19)
    dots = pgm_read_byte_near(&mybigfont[idx][col + 10]);
    for (byte row = 0; row < 6; row++) {
      if (x + col < LED_WIDTH && y + 8 + row < LED_HEIGHT) {
        plot(x + col, y + 8 + row, (dots & (128 >> row)) != 0);
      }
    }
  }
}

// ── drawDateRow ───────────────────────────────────────────────────────────────
// Renders "WED 18 MAR" (10 chars) using tinyfont at LED row y.
// Centred in 80 cols: span = 9×4+3 = 39 px → x_start = (80-39)/2 = 20.
static void drawDateRow(byte y) {
  static const char *dayAbbr[7]  = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  static const char *monAbbr[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                     "JUL","AUG","SEP","OCT","NOV","DEC"};

  if (rtc[3] > 6 || rtc[5] < 1 || rtc[5] > 12 || rtc[4] < 1 || rtc[4] > 31) return;

  const char *d = dayAbbr[rtc[3]];
  const char *m = monAbbr[rtc[5] - 1];

  char str[11];
  str[0] = d[0]; str[1] = d[1]; str[2] = d[2];
  str[3] = ' ';
  str[4] = (rtc[4] >= 10) ? ('0' + rtc[4] / 10) : ' ';
  str[5] = '0' + rtc[4] % 10;
  str[6] = ' ';
  str[7] = m[0]; str[8] = m[1]; str[9] = m[2];
  str[10] = '\0';

  // Centre in 80 px: x_start = (80-39)/2 = 20
  for (byte i = 0; i < 10; i++) {
    putTinyChar(20 + i * 4, y, str[i]);
  }
}

// ── slideanim ─────────────────────────────────────────────────────────────────
// One frame (0-8) of the slide digit transition — unchanged from CYD.
void slideanim(byte x, byte y, byte sequence, char old_c, char new_c) {
  byte old_idx = charIndex5x7(old_c);
  byte new_idx = charIndex5x7(new_c);

  if (sequence < 7) {
    byte max_row = 6 - sequence;
    byte draw_y  = y + sequence;
    for (byte col = 0; col < 5; col++) {
      byte dots = pgm_read_byte_near(&myfont[old_idx][col]);
      for (byte row = 0; row <= max_row; row++) {
        if (x + col < LED_WIDTH && draw_y + row < LED_HEIGHT) {
          plot(x + col, draw_y + row, (dots & (1 << row)) != 0);
        }
      }
    }
  }

  if (sequence >= 1 && sequence <= 7) {
    byte blank_y = y + sequence - 1;
    for (byte col = 0; col < 5; col++) {
      if (x + col < LED_WIDTH && blank_y < LED_HEIGHT) {
        plot(x + col, blank_y, false);
      }
    }
  }

  if (sequence >= 2) {
    byte min_row = 6 - (sequence - 2);
    for (byte col = 0; col < 5; col++) {
      byte dots = pgm_read_byte_near(&myfont[new_idx][col]);
      byte sy = 0;
      for (byte row = min_row; row <= 6; row++) {
        if (x + col < LED_WIDTH && y + sy < LED_HEIGHT) {
          plot(x + col, y + sy, (dots & (1 << row)) != 0);
        }
        sy++;
      }
    }
  }
}

// ── Date helpers ──────────────────────────────────────────────────────────────
void set_next_date() {
  get_time();
  next_display_date = (rtc[1] + DATE_DISPLAY_MINS) % 60;
}

bool check_show_date() {
  if (next_display_date == 255) { set_next_date(); return false; }
  if (rtc[1] == next_display_date) {
    fade_down();
    display_date();
    fade_down();
    set_next_date();
    return true;
  }
  return false;
}

// ── flashing_cursor ──────────────────────────────────────────────────────────
void flashing_cursor(byte xpos, byte ypos, byte w, byte h, byte repeats) {
  for (byte r = 0; r <= repeats; r++) {
    for (byte x = 0; x <= w; x++)
      for (byte y = 0; y <= h; y++)
        plot(x + xpos, y + ypos, true);
    pushMatrix();
    serviceNetworkDelay(repeats > 0 ? 400 : 70);

    for (byte x = 0; x <= w; x++)
      for (byte y = 0; y <= h; y++)
        plot(x + xpos, y + ypos, false);
    pushMatrix();
    if (repeats > 0) serviceNetworkDelay(400);
  }
}

void levelbar(byte xpos, byte ypos, byte xbar, byte ybar) {
  for (byte x = 0; x <= xbar; x++)
    for (byte y = 0; y <= ybar; y++)
      plot(x + xpos, y + ypos, true);
}

// ── display_date ─────────────────────────────────────────────────────────────
// Full-screen typewriter date reveal.
void display_date() {
  const byte y1 = (LED_HEIGHT - 16) / 2;  // = 8 for LED_HEIGHT=32
  const byte y2 = y1 + 8;                  // = 16

  cls(); pushMatrix();

  const char *days_full[7] = {"Sunday","Monday","Tuesday","Wed","Thursday","Friday","Saturday"};
  const char *suffix[4]    = {"st","nd","rd","th"};
  const char *months[12]   = {"January","February","March","April","May","June",
                               "July","August","Sept","October","November","December"};

  byte dow   = rtc[3];
  byte date  = rtc[4];
  byte month = rtc[5] - 1;

  flashing_cursor(0, y1, 5, 7, 1);

  byte i = 0;
  while (days_full[dow][i]) {
    flashing_cursor(i * 6, y1, 5, 7, 0);
    putChar(i * 6, y1, days_full[dow][i]);
    pushMatrix();
    i++;
  }
  if (i * 6 < 80) flashing_cursor(i * 6, y1, 5, 7, 1);
  else serviceNetworkDelay(300);

  flashing_cursor(0, y2, 5, 7, 0);

  char buf[5];
  itoa(date, buf, 10);

  byte s = 3;
  if (date == 1 || date == 21 || date == 31) s = 0;
  else if (date == 2 || date == 22)          s = 1;
  else if (date == 3 || date == 23)          s = 2;

  putChar(0, y2, buf[0]);
  byte sx = 6;
  if (date > 9) { sx = 12; flashing_cursor(6, y2, 5, 7, 0); putChar(6, y2, buf[1]); pushMatrix(); }
  flashing_cursor(sx, y2, 5, 7, 0);
  putChar(sx, y2, suffix[s][0]); pushMatrix();
  flashing_cursor(sx + 6, y2, 5, 7, 0);
  putChar(sx + 6, y2, suffix[s][1]); pushMatrix();
  flashing_cursor(sx + 12, y2, 5, 7, 1);

  cls();
  putChar(0, y1, buf[0]);
  putChar(6, y1, date > 9 ? buf[1] : ' ');
  putChar(sx, y1, suffix[s][0]);
  putChar(sx + 6, y1, suffix[s][1]);
  flashing_cursor(sx + 12, y1, 5, 7, 0);

  i = 0;
  while (months[month][i]) {
    flashing_cursor(i * 6, y2, 5, 7, 0);
    putChar(i * 6, y2, months[month][i]);
    pushMatrix();
    i++;
  }
  if (i * 6 < 80) flashing_cursor(i * 6, y2, 5, 7, 2);
  else serviceNetworkDelay(1000);
  serviceNetworkDelay(3000);
}

// ── Housekeeping (called every mode loop) ─────────────────────────────────────
static void tickHousekeeping() {
  events();   // ezTime NTP re-sync polling
  webLoop();
}

// ── SLIDE MODE ────────────────────────────────────────────────────────────────
void slide() {
  beginModeLoop();

  byte digits_old[6];
  byte digits_new[6];
  // x positions for: secs-ones, secs-tens, mins-ones, mins-tens, hrs-ones, hrs-tens
  // Centred in 80 cols (+16 vs 48-col original): {42,36,24,18,6,0} + 16
  const byte xpos[6] = {58, 52, 40, 34, 22, 16};

  get_time();
  DBG_INFO("Slide: %02d:%02d:%02d  NTP=%d", rtc[2], rtc[1], rtc[0], (int)timeStatus());

  {
    byte hours = rtc[2];
    if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }
    digits_old[0] = rtc[0] % 10;
    digits_old[1] = rtc[0] / 10;
    digits_old[2] = rtc[1] % 10;
    digits_old[3] = rtc[1] / 10;
    digits_old[4] = hours  % 10;
    digits_old[5] = hours  / 10;
  }

  const byte TIME_Y = 9;
  const byte DATE_Y = 18;

  cls();
  for (byte i = 0; i < 6; i++) {
    char ch[2]; itoa(digits_old[i], ch, 10);
    if (ampm && i == 5 && digits_old[5] == 0) ch[0] = ' ';
    putChar(xpos[i], TIME_Y, ch[0]);
  }
  putChar(28, TIME_Y, ':');  // colon between HH and MM (was 12 in 48-col → +16)
  putChar(46, TIME_Y, ':');  // colon between MM and SS (was 30 in 48-col → +16)
  drawDateRow(DATE_Y);
  pushMatrix();

  byte old_secs = rtc[0];
  byte old_mins = rtc[1];

  while (keepRunningMode(0)) {
    tickHousekeeping();

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down(); return;
    }

    get_time();

    if (ampm) {
      byte h = rtc[2];
      if (h > 12) h -= 12;
      if (h < 1)  h += 12;
      digits_new[4] = h % 10;
      digits_new[5] = h / 10;
    } else {
      digits_new[4] = rtc[2] % 10;
      digits_new[5] = rtc[2] / 10;
    }
    digits_new[0] = rtc[0] % 10;
    digits_new[1] = rtc[0] / 10;
    digits_new[2] = rtc[1] % 10;
    digits_new[3] = rtc[1] / 10;

    if (old_mins != rtc[1]) {
      old_mins = rtc[1];
      check_show_date();
      if (old_mins != rtc[1]) {
        // Date was shown — re-sync
        digits_old[0] = digits_new[0];
        digits_old[1] = digits_new[1];
        digits_old[2] = digits_new[2];
        digits_old[3] = digits_new[3];
        digits_old[4] = digits_new[4];
        digits_old[5] = digits_new[5];
      }
    }

    // Colour changed: immediately redraw current time in new colours, then
    // re-sync old_secs so the next second triggers a normal slide animation.
    if (ledColourChanged) {
      ledColourChanged = false;
      cls();
      for (byte i = 0; i < 6; i++) {
        char ch[2]; itoa(digits_new[i], ch, 10);
        if (ampm && i == 5 && digits_new[5] == 0) ch[0] = ' ';
        putChar(xpos[i], TIME_Y, ch[0]);
      }
      putChar(28, TIME_Y, ':');
      putChar(46, TIME_Y, ':');
      drawDateRow(DATE_Y);
      pushMatrix();
      for (byte i = 0; i < 6; i++) digits_old[i] = digits_new[i];
      old_secs = rtc[0];
    }

    if (old_secs == rtc[0]) { serviceNetworkDelay(SLIDE_DELAY); continue; }
    old_secs = rtc[0];

    for (byte seq = 0; seq < 9; seq++) {
      cls();
      for (byte i = 0; i < 6; i++) {
        char old_c[2], new_c[2];
        itoa(digits_old[i], old_c, 10);
        itoa(digits_new[i], new_c, 10);
        if (ampm && i == 5) {
          if (digits_old[5] == 0) old_c[0] = ' ';
          if (digits_new[5] == 0) new_c[0] = ' ';
        }
        if (digits_old[i] == digits_new[i]) {
          putChar(xpos[i], TIME_Y, old_c[0]);
        } else {
          slideanim(xpos[i], TIME_Y, seq, old_c[0], new_c[0]);
        }
      }
      putChar(28, TIME_Y, ':');
      putChar(46, TIME_Y, ':');
      drawDateRow(DATE_Y);
      pushMatrix();
      serviceNetworkDelay(SLIDE_DELAY);
    }

    for (byte i = 0; i < 6; i++) digits_old[i] = digits_new[i];
  }
  fade_down();
}

// ── PONG MODE ─────────────────────────────────────────────────────────────────
static float pong_get_ball_endpoint(float bx, float by, float vx, float vy) {
  float tx = (vx < 0) ? (float)BAT1_X : (float)BAT2_X;
  float steps = fabsf((tx - bx) / vx);
  float ey = by + vy * steps;

  // Bounce off top and bottom
  while (ey < 0 || ey >= (float)LED_HEIGHT) {
    if (ey < 0)               { ey = -ey; }
    if (ey >= (float)LED_HEIGHT) { ey = 2.0f * (float)LED_HEIGHT - ey - 1; }
  }
  return ey;
}

static bool pong_intro_wait(uint32_t durationMs) {
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    tickHousekeeping();
    if (!run_mode()) return false;

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down();
      return false;
    }
    delay(10);
  }
  return true;
}

static bool pong_setup() {
  const byte INTRO_Y = (LED_HEIGHT - 5) / 2;  // = 13

  cls();
  pushMatrix();

  byte i = 0;
  const char intro1[] = "Ready";
  while (intro1[i]) {
    putTinyChar((i * 4) + 30, INTRO_Y, intro1[i]);
    pushMatrix();
    if (!pong_intro_wait(25)) return false;
    i++;
  }
  for (byte f = 0; f < 2; f++) {
    putTinyChar(50, INTRO_Y, '?');
    pushMatrix();
    if (!pong_intro_wait(200)) return false;
    putTinyChar(50, INTRO_Y, ' ');
    pushMatrix();
    if (!pong_intro_wait(400)) return false;
  }

  cls();
  i = 0;
  const char intro2[] = "Play Pong!";
  while (intro2[i]) {
    putTinyChar((i * 4) + 20, INTRO_Y, intro2[i]);
    pushMatrix();
    i++;
  }
  if (!pong_intro_wait(800)) return false;
  cls();

  get_time();
  byte offset = random(0, 2);
  for (byte j = 0; j < LED_HEIGHT; j++) {
    if (j % 2 == 0) {
      byte x = 40;
      if (j + offset < LED_HEIGHT) plot(x, j + offset, true);
      pushMatrix();
      if (!pong_intro_wait(30)) return false;
    }
  }
  if (!pong_intro_wait(120)) return false;

  return true;
}

static void draw_pong_net(byte offset) {
  const byte x = 40;
  for (byte y = 0; y < LED_HEIGHT; y++) {
    bool on = (y % 2 == 0) && (y + offset < LED_HEIGHT);
    plot(x, y, on);
  }
}

void pong() {
  beginModeLoop();

  const byte SCORE_Y = 0;
  const byte DATE_Y  = LED_HEIGHT - 5;  // = 27

  get_time();
  DBG_INFO("Pong: %02d:%02d  NTP=%d", rtc[2], rtc[1], (int)timeStatus());
  if (!pong_setup()) return;

  byte sc_h0 = '0' + (rtc[2] / 10);
  byte sc_h1 = '0' + (rtc[2] % 10);
  byte sc_m0 = '0' + (rtc[1] / 10);
  byte sc_m1 = '0' + (rtc[1] % 10);

  if (ampm) {
    byte h = rtc[2];
    if (h > 12) h -= 12; if (h < 1) h += 12;
    sc_h0 = (h < 10) ? ' ' : ('0' + h / 10);
    sc_h1 = '0' + h % 10;
  }

  char sc_date[11];
  {
    static const char *da[7]  = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    static const char *ma[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                  "JUL","AUG","SEP","OCT","NOV","DEC"};
    if (rtc[3] <= 6 && rtc[5] >= 1 && rtc[5] <= 12) {
      const char *d = da[rtc[3]];
      const char *m = ma[rtc[5] - 1];
      sc_date[0] = d[0]; sc_date[1] = d[1]; sc_date[2] = d[2];
      sc_date[3] = ' ';
      sc_date[4] = (rtc[4] >= 10) ? ('0' + rtc[4] / 10) : ' ';
      sc_date[5] = '0' + rtc[4] % 10;
      sc_date[6] = ' ';
      sc_date[7] = m[0]; sc_date[8] = m[1]; sc_date[9] = m[2];
      sc_date[10] = '\0';
    } else {
      strlcpy(sc_date, "          ", sizeof(sc_date));
    }
  }

  // Initial bat and ball positions
  byte bat1_y = (LED_HEIGHT / 2) - (BAT_HEIGHT / 2);
  byte bat2_y = (LED_HEIGHT / 2) - (BAT_HEIGHT / 2);
  byte bat1_t = bat1_y;  // AI target
  byte bat2_t = bat2_y;

  float bx = (float)BALL_START_X;
  float by = (float)random(8, 24);
  float vx = (random(0, 2) == 0) ? 0.5f : -0.5f;
  float vy = (random(0, 2) == 0) ? 0.5f : -0.5f;

  byte ex = (byte)(bx + 0.5f);
  byte ey = (byte)(by + 0.5f);
  byte netOffset = (byte)random(0, 2);

  bool bat1_upd = true;
  bool bat2_upd = true;
  bool restart  = false;

  cls();
  draw_pong_net(netOffset);

  while (keepRunningMode(1) && !restart) {
    tickHousekeeping();

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down(); return;
    }

    if (ledColourChanged) {
      ledColourChanged = false;
      cls();
      bat1_upd = true;
      bat2_upd = true;
    }

    // Update time (score = HH:MM)
    get_time();
    byte new_h0 = '0' + (rtc[2] / 10);
    byte new_h1 = '0' + (rtc[2] % 10);
    byte new_m0 = '0' + (rtc[1] / 10);
    byte new_m1 = '0' + (rtc[1] % 10);
    if (ampm) {
      byte h = rtc[2]; if (h > 12) h -= 12; if (h < 1) h += 12;
      new_h0 = (h < 10) ? ' ' : ('0' + h / 10);
      new_h1 = '0' + h % 10;
    }
    sc_h0 = new_h0; sc_h1 = new_h1; sc_m0 = new_m0; sc_m1 = new_m1;

    // AI: predict ball endpoint and move bats toward it
    if ((int)bx == MIDFIELD_LEFT && vx < 0) {
      bat1_t = (byte)constrain((int)pong_get_ball_endpoint(bx, by, vx, vy) - (BAT_HEIGHT / 2),
                               0, BAT_MAX_Y);
    }
    if ((int)bx == MIDFIELD_RIGHT && vx > 0) {
      bat2_t = (byte)constrain((int)pong_get_ball_endpoint(bx, by, vx, vy) - (BAT_HEIGHT / 2),
                               0, BAT_MAX_Y);
    }

    // Move bats toward targets
    if (bat1_y < bat1_t) { bat1_y++; bat1_upd = true; }
    else if (bat1_y > bat1_t) { bat1_y--; bat1_upd = true; }

    if (bat2_y < bat2_t) { bat2_y++; bat2_upd = true; }
    else if (bat2_y > bat2_t) { bat2_y--; bat2_upd = true; }

    // Draw bat 1 (cols 1 and 2 — BAT1_X-2 and BAT1_X-1)
    if (bat1_upd) {
      for (byte i = 0; i < LED_HEIGHT; i++) {
        bool on = (i >= bat1_y && i < bat1_y + BAT_HEIGHT);
        plot(BAT1_X - 1, i, on);
        plot(BAT1_X - 2, i, on);
      }
      bat1_upd = false;
    }
    // Draw bat 2 (cols 77 and 78 — BAT2_X+1 and BAT2_X+2)
    if (bat2_upd) {
      for (byte i = 0; i < LED_HEIGHT; i++) {
        bool on = (i >= bat2_y && i < bat2_y + BAT_HEIGHT);
        plot(BAT2_X + 1, i, on);
        plot(BAT2_X + 2, i, on);
      }
      bat2_upd = false;
    }

    bx += vx; by += vy;

    if (by <= 0)                { vy = -vy; by = 0; }
    if (by >= (LED_HEIGHT - 1)) { vy = -vy; by = LED_HEIGHT - 1; }

    // Bat 1 collision
    if ((int)bx == BAT1_X && bat1_y <= (int)by && (int)by <= bat1_y + (BAT_HEIGHT - 1)) {
      vx = -vx;
      if (random(0, 3) != 0) {
        byte fl = (bat1_y <= 2) ? 0 : (bat1_y >= BAT_MAX_Y - 2) ? 1 : (byte)random(0, 2);
        if (fl == 0) { bat1_t += random(1, 3); if (vy < 2.0f) vy += 0.2f; }
        else         { bat1_t -= random(1, 3); if (vy > 0.2f) vy -= 0.2f; }
      }
    }
    // Bat 2 collision
    if ((int)bx == BAT2_X && bat2_y <= (int)by && (int)by <= bat2_y + (BAT_HEIGHT - 1)) {
      vx = -vx;
      if (random(0, 3) != 0) {
        byte fl = (bat2_y <= 2) ? 0 : (bat2_y >= BAT_MAX_Y - 2) ? 1 : (byte)random(0, 2);
        if (fl == 0) { bat2_t += random(1, 3); if (vy < 2.0f) vy += 0.2f; }
        else         { bat2_t -= random(1, 3); if (vy > 0.2f) vy -= 0.2f; }
      }
    }

    // Draw ball
    byte px = (byte)(bx + 0.5f);
    byte py = (byte)(by + 0.5f);
    plot(ex, ey, false);
    draw_pong_net(netOffset);
    plot(px, py, true);
    ex = px; ey = py;

    // Redraw score and date every frame (ball never permanently erases them)
    // Score positions centred in 80 cols (+16 vs 48-col: 14→30, 18→34, 28→44, 32→48)
    putTinyChar(30, SCORE_Y, sc_h0);
    putTinyChar(34, SCORE_Y, sc_h1);
    putTinyChar(44, SCORE_Y, sc_m0);
    putTinyChar(48, SCORE_Y, sc_m1);
    for (byte i = 0; i < 10; i++) putTinyChar(20 + i * 4, DATE_Y, sc_date[i]);
    pushMatrix();

    if ((int)bx == 0 || (int)bx == LED_WIDTH - 1) restart = 1;

    serviceNetworkDelay(PONG_BALL_DELAY);
  }
  fade_down();
}

// ── DIGITS MODE ───────────────────────────────────────────────────────────────
void digits() {
  beginModeLoop();

  get_time();
  DBG_INFO("Digits: %02d:%02d:%02d  NTP=%d", rtc[2], rtc[1], rtc[0], (int)timeStatus());

  // Layout (32-row matrix): same as CYD (Y positions unchanged — only X shifted)
  //   Rows  0- 8  — top margin
  //   Rows  9-22  — big font 10×14
  //   Rows 23-31  — bottom margin
  const byte BIG_Y  = 9;
  const byte COL_Y1 = BIG_Y + 3;   // 12
  const byte COL_Y2 = BIG_Y + 4;   // 13
  const byte COL_Y3 = BIG_Y + 9;   // 18
  const byte COL_Y4 = BIG_Y + 10;  // 19

  byte mins = 100;
  byte secs = 100;
  set_next_date();
  cls(); pushMatrix();

  while (keepRunningMode(2)) {
    tickHousekeeping();

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down(); return;
    }

    if (ledColourChanged) {
      ledColourChanged = false;
      cls();
      mins = 100;
      secs = 100;
    }

    get_time();
    check_show_date();

    byte hours = rtc[2];
    if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }

    // Flash colon on seconds change
    if (secs != rtc[0]) {
      secs = rtc[0];
      bool dot_on = (secs % 2 == 0);
      // Colon centred in 80 cols: original 23,24 → +16 = 39,40
      plot(39, COL_Y1, dot_on); plot(40, COL_Y1, dot_on);
      plot(39, COL_Y2, dot_on); plot(40, COL_Y2, dot_on);
      plot(39, COL_Y3, dot_on); plot(40, COL_Y3, dot_on);
      plot(39, COL_Y4, dot_on); plot(40, COL_Y4, dot_on);
      pushMatrix();
    }

    if (mins == rtc[1]) { serviceNetworkDelay(50); continue; }
    mins = rtc[1];

    char buf[3];
    itoa(hours, buf, 10);
    if (hours < 10) { buf[1] = buf[0]; buf[0] = (ampm && hours < 10) ? ' ' : '0'; }

    // Big chars centred: original 0,12,26,38 → +16 = 16,28,42,54
    byte offset = (ampm && hours < 10) ? 5 : 0;
    if (offset == 0) putBigChar(16, BIG_Y, buf[0]);
    putBigChar(28 - offset, BIG_Y, buf[1]);

    itoa(mins, buf, 10);
    if (mins < 10) { buf[1] = buf[0]; buf[0] = '0'; }
    putBigChar(42 - offset, BIG_Y, buf[0]);
    putBigChar(54 - offset, BIG_Y, buf[1]);
    pushMatrix();
  }
  fade_down();
}

// ── WORD CLOCK ────────────────────────────────────────────────────────────────
void word_clock() {
  beginModeLoop();

  const char *numbers[19] = {
    "one","two","three","four","five","six","seven","eight","nine","ten",
    "eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"
  };
  const char *tens[5] = {"ten","twenty","thirty","forty","fifty"};

  // Layout (32-row, Y positions unchanged from CYD)
  const byte WC_TOP_Y  = 6;
  const byte WC_BOT_Y  = 13;
  const byte WC_DATE_Y = 21;

  byte old_mins = 100;

  while (keepRunningMode(3)) {
    tickHousekeeping();

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down(); return;
    }

    if (ledColourChanged) {
      ledColourChanged = false;
      cls();
      old_mins = 100;
    }

    get_time();

    byte mins  = rtc[1];
    byte hours = rtc[2];
    if (hours > 12) hours -= 12;
    if (hours == 0) hours = 12;

    if (mins == old_mins) { serviceNetworkDelay(50); continue; }
    old_mins = mins;

    byte md  = mins % 10;
    byte mdt = (mins / 10) % 10;

    char top[14], bot[14];

    if (mins < 10 && mins > 0) {
      strcpy(top, numbers[md - 1]); strcat(top, " PAST");
      strcpy(bot, numbers[hours - 1]);
    } else if (mins == 10) {
      strcpy(top, numbers[9]); strcat(top, " PAST");
      strcpy(bot, numbers[hours - 1]);
    } else if (mdt != 0 && md != 0) {
      strcpy(top, numbers[hours - 1]);
      if (mins >= 11 && mins <= 19) strcpy(bot, numbers[mins - 1]);
      else { strcpy(bot, tens[mdt - 1]); strcat(bot, " "); strcat(bot, numbers[md - 1]); }
    } else if (mdt != 0 && md == 0) {
      strcpy(top, numbers[hours - 1]);
      strcpy(bot, tens[mdt - 1]);
    } else {
      strcpy(top, numbers[hours - 1]);
      strcpy(bot, "O'CLOCK");
    }

    // Centre in 80 px: (77 - (len-1)*4) / 2  [was (45 - (len-1)*4) / 2 for 48-col]
    byte len = 0; while (top[len]) len++;
    byte ox = (77 - (len - 1) * 4) / 2;
    byte i = 0;
    fade_down();
    while (top[i]) { putTinyChar(ox + i * 4, WC_TOP_Y, top[i]); i++; }

    len = 0; while (bot[len]) len++;
    ox = (77 - (len - 1) * 4) / 2;
    i = 0;
    while (bot[i]) { putTinyChar(ox + i * 4, WC_BOT_Y, bot[i]); i++; }

    drawDateRow(WC_DATE_Y);
    pushMatrix();
  }
  fade_down();
}

// ── INVADERS MODE ─────────────────────────────────────────────────────────────
static void draw_invader(int x, byte y, byte type, bool wiggle) {
  byte frame = wiggle ? 1 : 0;
  for (byte half = 0; half < 2; half++) {
    for (byte col = 0; col < 5; col++) {
      byte dots = pgm_read_byte(&invader_sprites[type][frame][half][col]);
      int  px   = x + (int)(half * 5) + (int)col;
      if (px < 0 || px >= LED_WIDTH) continue;
      for (byte row = 0; row < 7; row++) {
        if (y + row < LED_HEIGHT)
          plot(px, y + row, (dots & (0x40 >> row)) != 0);
      }
    }
  }
}

static bool invader_scroll(byte ypos, int xstart, int xend, byte type) {
  bool wiggle = false;
  int  xstep  = (xstart < xend) ? 1 : -1;

  for (int i = xstart; i != xend; i += xstep) {
    if (clock_mode != 4) { fade_down(); return false; }
    draw_invader(i, ypos, type, wiggle);
    wiggle = !wiggle;
    pushMatrix();

    uint32_t t0 = millis();
    while (millis() - t0 < INVADER_SCROLL_DELAY) {
      if (clock_mode != 4 || !run_mode()) {
        fade_down();
        return false;
      }
      webLoop();
      int8_t btn = checkButton();
      if (btn != 0) {
        clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
        fade_down();
        return false;
      }
      delay(5);
    }

    if (abs(i - xend) > 1) {
      for (byte yr = ypos; yr < ypos + 7; yr++) {
        if (i     >= 0 && i     < LED_WIDTH) plot(i,     yr, false);
        if (i + 9 >= 0 && i + 9 < LED_WIDTH) plot(i + 9, yr, false);
      }
    }
  }
  return true;
}

void invaders() {
  beginModeLoop();

  const byte TIME_Y = 1;          // HH:MM characters (rows 1-7)
  const byte INV_Y  = 11;         // invader sprite (rows 11-17)
  const byte DATE_Y = LED_HEIGHT - 5;  // = 27

  cls();

  // Intro
  byte i = 0;
  const char intro[] = "Invaders!";
  while (intro[i]) {
    // "Invaders!" is 9 chars, span=8×4+3=35 px. Centred in 80: (80-35)/2=22.
    putTinyChar((i * 4) + 22, (LED_HEIGHT - 5) / 2, intro[i]);
    pushMatrix();
    i++;
  }
  serviceNetworkDelay(1200);
  cls();

  get_time();
  byte prev_mins = 255;

  DBG_INFO("Invaders: %02d:%02d  NTP=%d", rtc[2], rtc[1], (int)timeStatus());

  while (keepRunningMode(4)) {
    tickHousekeeping();

    int8_t btn = checkButton();
    if (btn != 0) {
      clock_mode = (clock_mode + NUM_MODES + btn) % NUM_MODES;
      fade_down(); return;
    }

    if (ledColourChanged) {
      ledColourChanged = false;
      cls();
      prev_mins = 255;
    }

    get_time();

    if (rtc[1] != prev_mins) {
      prev_mins = rtc[1];

      byte hours = rtc[2];
      byte mins  = rtc[1];
      if (ampm) { if (hours > 12) hours -= 12; if (hours < 1) hours += 12; }

      char buf[3];
      itoa(hours, buf, 10);
      if (hours < 10) { buf[1] = buf[0]; buf[0] = '0'; }

      // HH:MM centred in 80 cols (+16 vs 48-col: 13→29, 19→35, 25→41, 27→43, 33→49)
      putChar(29, TIME_Y, buf[0]);
      putChar(35, TIME_Y, buf[1]);

      plot(41, TIME_Y + 2, true);   // colon top dot
      plot(41, TIME_Y + 4, true);   // colon bottom dot

      itoa(mins, buf, 10);
      if (mins < 10) { buf[1] = buf[0]; buf[0] = '0'; }
      putChar(43, TIME_Y, buf[0]);
      putChar(49, TIME_Y, buf[1]);
    }

    drawDateRow(DATE_Y);
    pushMatrix();

    // Scroll invader left-to-right then right-to-left
    // Original 48-col bounds: -11 to 50 and 49 to -11
    // 80-col: -11 to 91 and 90 to -11
    byte type = (byte)random(0, 3);
    if (!invader_scroll(INV_Y, -11, 91, type)) return;

    type = (byte)random(0, 3);
    if (!invader_scroll(INV_Y, 90, -11, type)) return;
  }
  fade_down();
}
