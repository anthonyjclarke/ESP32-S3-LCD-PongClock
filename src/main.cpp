#include <Arduino.h>
#include <WiFiManager.h>
#include <ezTime.h>
#include "waveshare_display.h"
#include "config.h"
#include "config_nvs.h"
#include "debug.h"
#include "display.h"
#include "clock.h"
#include "web.h"

// ── Global display instance ───────────────────────────────────────────────────
// Defined here; declared extern in waveshare_display.h.
WaveshareDisplay gfx;

// ── Splash screen ─────────────────────────────────────────────────────────────
static void drawLedBorder() {
  for (byte x = 0; x < LED_WIDTH; x++) {
    plot(x, 0,              true);
    plot(x, LED_HEIGHT - 1, true);
  }
  for (byte y = 1; y < LED_HEIGHT - 1; y++) {
    plot(0,             y, true);
    plot(LED_WIDTH - 1, y, true);
  }
}

static void showSplash() {
  cls();
  drawLedBorder();
  pushMatrix();

  // "PONG" centred in 80 cols — span = 4×6-1 = 23 px → x_start = (80-23)/2 = 28
  const byte xP[4] = {28, 34, 40, 46};
  const char sP[4] = {'P', 'O', 'N', 'G'};
  for (byte i = 0; i < 4; i++) { putChar(xP[i], 2, sP[i]); pushMatrix(); delay(60); }

  // "CLOCK" centred in 80 cols — span = 5×6-1 = 29 px → x_start = (80-29)/2 = 25
  const byte xC[5] = {25, 31, 37, 43, 49};
  const char sC[5] = {'C', 'L', 'O', 'C', 'K'};
  for (byte i = 0; i < 5; i++) { putChar(xC[i], 12, sC[i]); pushMatrix(); delay(60); }

  // Version string — self-centring formula
  char verBuf[8];
  snprintf(verBuf, sizeof(verBuf), "v%s", FW_VERSION);
  byte vlen = (byte)strlen(verBuf);
  byte vx   = (byte)((LED_WIDTH - (vlen * 6 - 1)) / 2);
  delay(100);
  for (byte i = 0; i < vlen; i++) { putChar(vx + i * 6, 22, verBuf[i]); pushMatrix(); delay(60); }

  delay(900);

  // CRT-collapse: rows fold inward from top and bottom simultaneously
  for (byte i = 0; i < LED_HEIGHT / 2; i++) {
    for (byte x = 0; x < LED_WIDTH; x++) {
      plot(x, i,                  false);
      plot(x, LED_HEIGHT - 1 - i, false);
    }
    pushMatrix();
    delay(15);
  }

  gfx.fillScreen(gfx.color565(5, 2, 0));
  cls(); pushMatrix();
}

// ── Boot status lines (drawn directly on the display, overlaying the sprite) ───
// The sprite exactly fills the 820×320 landscape display (800×320 at x+10),
// so there is no physical footer margin. These are temporary — overwritten
// the moment the clock mode loop calls cls()/pushMatrix().
// Font2 char dimensions: ~6 px wide, ~14 px tall at textSize 1.
static constexpr int16_t kStatusLineY  = 290;
static constexpr int16_t kIpLineY      = 305;
static constexpr uint16_t kBootIpHoldMs = 2500;

static void showFooterLine(int16_t y, const char* msg) {
  gfx.setFont(&lgfx::fonts::Font2);
  gfx.setTextColor(gfx.color565(180, 100, 0), gfx.color565(5, 2, 0));
  gfx.setTextSize(1);
  gfx.fillRect(0, y, gfx.width(), 16, gfx.color565(5, 2, 0));
  int16_t x = (gfx.width() - (int16_t)(strlen(msg) * 6)) / 2;
  gfx.drawString(msg, x > 0 ? x : 0, y);
}

static void showStatus(const char* msg)  { showFooterLine(kStatusLineY, msg); }
static void showIpLine(const char* msg)  { showFooterLine(kIpLineY,     msg); }

// ── Boot IP display on the LED matrix canvas ───────────────────────────────────
// Shows "IP" in 5×7 font (centred) and the IP string in 3×5 tiny font below.
// On an 80-wide grid any IPv4 address fits without scrolling (max 15 chars ×
// 4 px = 60 px < 80 px), so this is always a static hold. The matrix content
// is temporary — cleared when the clock mode loop renders its first frame.
static void drawTinyText(int16_t x, byte y, const char* msg) {
  for (size_t i = 0; msg[i] != '\0'; i++) {
    int16_t cx = x + (int16_t)(i * 4);
    if (cx > (LED_WIDTH - 1) || cx + 2 < 0) continue;
    putTinyChar((byte)cx, y, msg[i]);
  }
}

static void showBootIpOnMatrix(const char* ip) {
  // "IP" in 5×7 font, centred: 2 chars × 6 px - 1 = 11 px
  const byte ipLabelX = (byte)((LED_WIDTH - 11) / 2);  // = 34
  const byte labelY   = 6;
  const byte addrY    = 20;

  cls();
  putChar(ipLabelX,     labelY, 'I');
  putChar(ipLabelX + 6, labelY, 'P');

  // IP address in 3×5 tiny font, centred
  int16_t ipWidth = (int16_t)(strlen(ip) * 4) - 1;
  int16_t addrX   = (LED_WIDTH - ipWidth) / 2;
  drawTinyText(addrX, addrY, ip);
  pushMatrix();
  delay(kBootIpHoldMs);
}

// ── WiFi init ─────────────────────────────────────────────────────────────────
static void initWiFi() {
  showStatus("Connecting WiFi...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);

  if (!wm.autoConnect(WIFI_AP_NAME)) {
    DBG_WARN("WiFi: connect timeout, continuing offline");
    showStatus("WiFi offline");
    showIpLine("");
  } else {
    String ip = WiFi.localIP().toString();
    char ipBuf[24];
    snprintf(ipBuf, sizeof(ipBuf), "IP %s", ip.c_str());
    DBG_INFO("WiFi connected: %s", ip.c_str());
    showStatus("WiFi OK");
    showIpLine(ipBuf);
    showBootIpOnMatrix(ip.c_str());
  }
}

// ── Time init ─────────────────────────────────────────────────────────────────
static void initTime() {
  showStatus("Syncing NTP...");
  waitForSync(NTP_SYNC_TIMEOUT_S);

  if (timeStatus() == timeNotSet) {
    DBG_WARN("Time: NTP sync failed — clock will show 0");
  }

  if (!myTZ.setLocation(F(NTP_TIMEZONE))) {
    DBG_WARN("Time: Olson lookup failed — applying POSIX fallback: %s", NTP_POSIX_FALLBACK);
    myTZ.setPosix(NTP_POSIX_FALLBACK);
  }

  DBG_INFO("Time synced: %s  tz=%s offset=%s",
           myTZ.dateTime("H:i:s d-M-Y").c_str(),
           myTZ.dateTime("T").c_str(),
           myTZ.dateTime("O").c_str());
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);  // wait for USB CDC enumeration

  DBG_INFO("=== ESP32-S3 PongClock %s starting ===", FW_VERSION);

  // Load NVS config first — values used during display init below
  loadConfig();

  // Backlight: start dark so the panel init sequence isn't visible
  initBacklight(0);
  delay(20);

  // Display: landscape rotation (logical 820×320)
  gfx.init();
  gfx.setRotation(1);
  gfx.setColorDepth(16);
  gfx.fillScreen(0);

  // Sprite + colour init — allocates 800×320 8-bit sprite in PSRAM
  initColours();
  clsNow();

  // Apply persisted brightness and LED colours
  setBrightness(rtCfg.brightness);
  setLedColours(rtCfg.ledOnR, rtCfg.ledOnG, rtCfg.ledOnB,
                rtCfg.ledOffR, rtCfg.ledOffG, rtCfg.ledOffB);

  // Apply persisted clock state
  clock_mode = rtCfg.clockMode;
  ampm       = rtCfg.ampm;

  showSplash();

  initButton();
  initWiFi();
  initTime();
  initWeb();

  // Seed RNG — ADC on any floating pin gives good entropy on ESP32-S3
  randomSeed(esp_random());

  DBG_INFO("Setup complete — heap=%u bytes  PSRAM=%u bytes",
           ESP.getFreeHeap(), ESP.getFreePsram());
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  events();  // ezTime NTP re-sync housekeeping

  switch (clock_mode) {
    case 0: slide();      break;
    case 1: pong();       break;
    case 2: digits();     break;
    case 3: word_clock(); break;
    case 4: invaders();   break;
    default: clock_mode = 0; break;
  }
}
