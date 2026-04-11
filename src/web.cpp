#include "web.h"
#include "display.h"
#include "clock.h"
#include "config.h"
#include "config_nvs.h"
#include "debug.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static WebServer server(WEB_SERVER_PORT);
static bool      webStarted = false;
static bool      fsReady    = false;

// ── LittleFS file serve helper ────────────────────────────────────────────────
// Prefers a pre-gzipped version (<path>.gz) if present in LittleFS.
// clock.js alone is 37 KB; the .gz version is ~9.6 KB — a 4× reduction in
// streamFile() blocking time, which is what causes the TFT animation to freeze
// while the Arduino task is stuck in TCP write.
//
// IMPORTANT: do NOT call sendHeader("Content-Encoding","gzip") here. The Arduino
// ESP32 WebServer::_streamFileCore() adds that header automatically whenever the
// file name passed to streamFile() ends in ".gz" (see WebServer.cpp _streamFileCore).
// Adding it manually as well produces a duplicate header, which browsers interpret
// as "Content-Encoding: gzip, gzip" → they try to decompress twice, the second
// decode fails on plain content, and the response is silently aborted → "nothing
// loads" on the web UI even though the server is responding 200 OK.
static void serveFile(const char* path, const char* mime) {
  if (!fsReady) {
    server.send(404, "text/plain", "Not found");
    return;
  }

  // Try the pre-compressed variant for text assets only (.html/.js/.css).
  // LittleFS.exists() on a missing file triggers [E][vfs_api.cpp] "does not exist",
  // so skip the probe entirely for binary types (PNG, etc.) that are never gzipped.
  String pathStr(path);
  bool canGz = pathStr.endsWith(".html") || pathStr.endsWith(".js") || pathStr.endsWith(".css");
  String gzPath = pathStr + ".gz";
  bool   useGz  = canGz && LittleFS.exists(gzPath.c_str());
  const char* actualPath = useGz ? gzPath.c_str() : path;

  if (!LittleFS.exists(actualPath)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  fs::File f = LittleFS.open(actualPath, "r");
  if (!f) { server.send(500, "text/plain", "Open failed"); return; }
  server.sendHeader("Cache-Control", "max-age=300");
  server.streamFile(f, mime);  // auto-adds Content-Encoding: gzip when path ends in .gz
  f.close();
}

// ── Static routes ─────────────────────────────────────────────────────────────
// Every file referenced directly in index.html needs an explicit server.on() entry.
// In Arduino ESP32 3.x, any URL without a matching route logs [E] "request handler
// not found" before falling back to onNotFound — even when onNotFound serves it fine.
static void handleRoot()     { serveFile("/index.html",   "text/html"); }
static void handleClockJs()  { serveFile("/clock.js",     "application/javascript"); }
static void handleStyleCss() { serveFile("/style.css",    "text/css"); }
static void handleLogo()     { serveFile("/pong-logo.png","image/png"); }
static void handleFavicon()  { server.send(204); }

// ── /screenshot.bmp ───────────────────────────────────────────────────────────
// Streams the matrixSprite buffer (800×320, 8-bit RGB332) as a 24-bit BMP.
// BMP row size: 800 × 3 = 2400 bytes (already 4-byte aligned).
// File size: 54-byte header + 2400 × 320 = 768,054 bytes.
static void handleScreenshot() {
  const int W        = LED_WIDTH  * PIXEL_SIZE;  // 800
  const int H        = LED_HEIGHT * PIXEL_SIZE;  // 320
  const int rowBytes = W * 3;                    // 2400
  const int fileSize = 54 + rowBytes * H;        // 768,054

  uint8_t hdr[54] = {};
  hdr[0] = 'B';  hdr[1] = 'M';
  hdr[2] = (uint8_t)(fileSize);
  hdr[3] = (uint8_t)(fileSize >> 8);
  hdr[4] = (uint8_t)(fileSize >> 16);
  hdr[5] = (uint8_t)(fileSize >> 24);
  hdr[10] = 54;
  hdr[14] = 40;
  hdr[18] = (uint8_t)(W);
  hdr[19] = (uint8_t)(W >> 8);
  int32_t negH = -(int32_t)H;
  memcpy(&hdr[22], &negH, 4);
  hdr[26] = 1;
  hdr[28] = 24;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.setContentLength(fileSize);
  server.send(200, "image/bmp", "");
  server.sendContent((const char*)hdr, 54);

  const uint8_t* px = (const uint8_t*)matrixSprite.getBuffer();
  static uint8_t row[2400];  // static: 2400 bytes on heap not stack
  for (int y = 0; y < H; y++) {
    const uint8_t* src = px + y * W;
    for (int x = 0; x < W; x++) {
      uint8_t p = src[x];
      uint8_t r = (p & 0xE0) | ((p & 0xE0) >> 3) | ((p & 0xC0) >> 6);
      uint8_t g = ((p & 0x1C) << 3) | (p & 0x1C) | ((p & 0x18) >> 3);
      uint8_t b = ((p & 0x03) << 6) | ((p & 0x03) << 4) | ((p & 0x03) << 2) | (p & 0x03);
      row[x * 3 + 0] = b;
      row[x * 3 + 1] = g;
      row[x * 3 + 2] = r;
    }
    server.sendContent((const char*)row, rowBytes);
  }
  DBG_VERBOSE("Screenshot served (%d bytes)", fileSize);
}

// ── /api/info ─────────────────────────────────────────────────────────────────
static const char* const MODE_NAMES[NUM_MODES] = {
  "Slide", "Pong", "Digits", "WordClock", "Invaders"
};

static void handleApiInfo() {
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"firmware\":\"%s\","
    "\"mode\":%d,"
    "\"modeName\":\"%s\","
    "\"brightness\":%d,"
    "\"uptime\":%lu,"
    "\"freeHeap\":%u,"
    "\"freePsram\":%u,"
    "\"ip\":\"%s\"}",
    FW_VERSION,
    (int)clock_mode,
    MODE_NAMES[clock_mode < NUM_MODES ? clock_mode : 0],
    (int)currentBrightness,
    millis() / 1000UL,
    (unsigned)ESP.getFreeHeap(),
    (unsigned)ESP.getFreePsram(),
    WiFi.localIP().toString().c_str()
  );
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buf);
}

// ── /api/config GET ───────────────────────────────────────────────────────────
static void handleConfigGet() {
  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"mode\":%d,"
    "\"brightness\":%d,"
    "\"ampm\":%s,"
    "\"ledOnR\":%d,\"ledOnG\":%d,\"ledOnB\":%d,"
    "\"ledOffR\":%d,\"ledOffG\":%d,\"ledOffB\":%d,"
    "\"timezone\":\"%s\","
    "\"ntpServer\":\"%s\","
    "\"dateInterval\":%d}",
    rtCfg.clockMode,
    rtCfg.brightness,
    rtCfg.ampm         ? "true" : "false",
    rtCfg.ledOnR,  rtCfg.ledOnG,  rtCfg.ledOnB,
    rtCfg.ledOffR, rtCfg.ledOffG, rtCfg.ledOffB,
    rtCfg.timezone,
    rtCfg.ntpServer,
    rtCfg.dateInterval
  );
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buf);
}

// ── /api/config POST ──────────────────────────────────────────────────────────
static void handleConfigPost() {
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    DBG_WARN("Config POST: JSON parse error: %s", err.c_str());
    server.send(400, "application/json", "{\"error\":\"JSON parse error\"}");
    return;
  }

  uint8_t prv_mode  = rtCfg.clockMode,  prv_bright = rtCfg.brightness;
  bool    prv_ampm  = rtCfg.ampm;
  uint8_t prv_onR   = rtCfg.ledOnR,     prv_onG    = rtCfg.ledOnG,    prv_onB   = rtCfg.ledOnB;
  uint8_t prv_offR  = rtCfg.ledOffR,    prv_offG   = rtCfg.ledOffG,   prv_offB  = rtCfg.ledOffB;
  uint8_t prv_dint  = rtCfg.dateInterval;
  char    prv_tz[sizeof(rtCfg.timezone)];   strlcpy(prv_tz,  rtCfg.timezone,  sizeof(prv_tz));
  char    prv_ntp[sizeof(rtCfg.ntpServer)]; strlcpy(prv_ntp, rtCfg.ntpServer, sizeof(prv_ntp));

  bool changed = false;

  if (doc.containsKey("mode")) {
    uint8_t m = doc["mode"].as<uint8_t>();
    if (m < NUM_MODES) { rtCfg.clockMode = m; clock_mode = m; changed = true; }
  }
  if (doc.containsKey("brightness")) {
    rtCfg.brightness = doc["brightness"].as<uint8_t>();
    setBrightness(rtCfg.brightness);
    changed = true;
  }
  if (doc.containsKey("ampm")) {
    rtCfg.ampm = doc["ampm"].as<bool>();
    ampm = rtCfg.ampm;
    changed = true;
  }
  if (doc.containsKey("ledOnR"))  { rtCfg.ledOnR  = doc["ledOnR"].as<uint8_t>();  changed = true; }
  if (doc.containsKey("ledOnG"))  { rtCfg.ledOnG  = doc["ledOnG"].as<uint8_t>();  changed = true; }
  if (doc.containsKey("ledOnB"))  { rtCfg.ledOnB  = doc["ledOnB"].as<uint8_t>();  changed = true; }
  if (doc.containsKey("ledOffR")) { rtCfg.ledOffR = doc["ledOffR"].as<uint8_t>(); changed = true; }
  if (doc.containsKey("ledOffG")) { rtCfg.ledOffG = doc["ledOffG"].as<uint8_t>(); changed = true; }
  if (doc.containsKey("ledOffB")) { rtCfg.ledOffB = doc["ledOffB"].as<uint8_t>(); changed = true; }

  if (doc.containsKey("ledOnR") || doc.containsKey("ledOnG") || doc.containsKey("ledOnB") ||
      doc.containsKey("ledOffR") || doc.containsKey("ledOffG") || doc.containsKey("ledOffB")) {
    setLedColours(rtCfg.ledOnR, rtCfg.ledOnG, rtCfg.ledOnB,
                  rtCfg.ledOffR, rtCfg.ledOffG, rtCfg.ledOffB);
  }

  if (doc.containsKey("timezone")) {
    const char* tz = doc["timezone"].as<const char*>();
    if (tz && strlen(tz) < sizeof(rtCfg.timezone)) {
      strlcpy(rtCfg.timezone, tz, sizeof(rtCfg.timezone));
      if (!myTZ.setLocation(rtCfg.timezone)) {
        DBG_WARN("Config: Olson lookup failed for '%s'", rtCfg.timezone);
      }
      changed = true;
    }
  }
  if (doc.containsKey("ntpServer")) {
    const char* ntp = doc["ntpServer"].as<const char*>();
    if (ntp && strlen(ntp) < sizeof(rtCfg.ntpServer)) {
      strlcpy(rtCfg.ntpServer, ntp, sizeof(rtCfg.ntpServer));
      changed = true;
    }
  }
  if (doc.containsKey("dateInterval")) {
    rtCfg.dateInterval = doc["dateInterval"].as<uint8_t>();
    changed = true;
  }

  if (changed) {
    saveConfig();

    char   log[320];
    int    pos = 0;
    #define LOGCAT(...) pos += snprintf(log + pos, sizeof(log) - pos, __VA_ARGS__)
    if (prv_mode  != rtCfg.clockMode)
      LOGCAT("mode %s→%s  ", MODE_NAMES[prv_mode], MODE_NAMES[rtCfg.clockMode < NUM_MODES ? rtCfg.clockMode : 0]);
    if (prv_bright != rtCfg.brightness) LOGCAT("bright %d→%d  ", prv_bright, rtCfg.brightness);
    if (prv_ampm  != rtCfg.ampm)        LOGCAT("ampm %d→%d  ",  prv_ampm,   rtCfg.ampm);
    if (prv_onR != rtCfg.ledOnR || prv_onG != rtCfg.ledOnG || prv_onB != rtCfg.ledOnB)
      LOGCAT("ledOn #%02X%02X%02X→#%02X%02X%02X  ", prv_onR, prv_onG, prv_onB,
             rtCfg.ledOnR, rtCfg.ledOnG, rtCfg.ledOnB);
    if (prv_offR != rtCfg.ledOffR || prv_offG != rtCfg.ledOffG || prv_offB != rtCfg.ledOffB)
      LOGCAT("ledOff #%02X%02X%02X→#%02X%02X%02X  ", prv_offR, prv_offG, prv_offB,
             rtCfg.ledOffR, rtCfg.ledOffG, rtCfg.ledOffB);
    if (strcmp(prv_tz,  rtCfg.timezone)  != 0) LOGCAT("tz %s→%s  ",  prv_tz,  rtCfg.timezone);
    if (strcmp(prv_ntp, rtCfg.ntpServer) != 0) LOGCAT("ntp %s→%s  ", prv_ntp, rtCfg.ntpServer);
    if (prv_dint != rtCfg.dateInterval)         LOGCAT("dateInt %d→%d  ", prv_dint, rtCfg.dateInterval);
    #undef LOGCAT
    while (pos > 0 && log[pos - 1] == ' ') pos--;
    log[pos] = '\0';
    DBG_INFO("Config saved — %s", pos > 0 ? log : "(no changes)");
  }

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ── /api/wifi-reset POST ──────────────────────────────────────────────────────
static void handleWifiReset() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"status\":\"restarting\"}");
  DBG_INFO("WiFi reset requested — erasing credentials and restarting");
  delay(200);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

// ── Not-found fallback ────────────────────────────────────────────────────────
static const char* mimeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".ico"))  return "image/x-icon";
  return "application/octet-stream";
}

static void handleNotFound() {
  String path = server.uri();
  if (fsReady && LittleFS.exists(path)) {
    fs::File f = LittleFS.open(path, "r");
    if (f) {
      server.sendHeader("Cache-Control", "max-age=300");
      server.streamFile(f, mimeFor(path));
      f.close();
      DBG_VERBOSE("Served %s", path.c_str());
      return;
    }
  }
  DBG_WARN("404 %s", path.c_str());
  server.send(404, "text/plain", "Not found");
}

// ── Public API ────────────────────────────────────────────────────────────────
void initWeb() {
  if (WiFi.status() != WL_CONNECTED) {
    DBG_WARN("Web server skipped — WiFi not connected");
    return;
  }

  // LittleFS — true = format on first boot if unmountable
  fsReady = LittleFS.begin(true);
  if (!fsReady) {
    DBG_WARN("LittleFS mount failed — web UI files unavailable");
  } else {
    DBG_INFO("LittleFS mounted — %u KB used / %u KB total",
             (unsigned)(LittleFS.usedBytes()  / 1024),
             (unsigned)(LittleFS.totalBytes() / 1024));
  }

  server.on("/",               HTTP_GET,  handleRoot);
  server.on("/clock.js",       HTTP_GET,  handleClockJs);
  server.on("/style.css",      HTTP_GET,  handleStyleCss);
  server.on("/pong-logo.png",  HTTP_GET,  handleLogo);
  server.on("/favicon.ico",    HTTP_GET,  handleFavicon);
  server.on("/screenshot.bmp", HTTP_GET,  handleScreenshot);
  server.on("/api/info",       HTTP_GET,  handleApiInfo);
  server.on("/api/config",     HTTP_GET,  handleConfigGet);
  server.on("/api/config",     HTTP_POST, handleConfigPost);
  server.on("/api/wifi-reset", HTTP_POST, handleWifiReset);
  server.onNotFound(handleNotFound);
  server.begin();
  webStarted = true;
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void webLoop() {
  if (webStarted) server.handleClient();
}
