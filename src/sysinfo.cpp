#include "sysinfo.h"
#include "config.h"
#include "debug.h"
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_arduino_version.h>

// Build time comes from __DATE__/__TIME__, i.e. when this file was last
// compiled. It includes config.h, so any FW_VERSION bump or config change
// refreshes it; a build that only touches other .cpp files keeps the old time.
const char* buildTimestamp() {
  static char buf[24] = "";
  if (buf[0]) return buf;

  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* d = __DATE__;  // "Mmm dd yyyy" (day space-padded)
  char mon[4] = {d[0], d[1], d[2], '\0'};
  const char* p = strstr(months, mon);
  int month = p ? (int)(p - months) / 3 + 1 : 0;
  snprintf(buf, sizeof(buf), "%02d-%02d-%s %s", atoi(d + 4), month, d + 7, __TIME__);
  return buf;
}

const char* resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_JTAG:      return "JTAG";
    default:                return "unknown";
  }
}

const char* controlsDesc() {
  static char buf[32] = "";
  if (buf[0]) return buf;
  if (PIN_BUTTON >= 0) snprintf(buf, sizeof(buf), "button GPIO%d + web UI", PIN_BUTTON);
  else                 strlcpy(buf, "web UI only", sizeof(buf));
  return buf;
}

void macString(char* out, size_t len) {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(out, len, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void chipRevString(char* out, size_t len) {
  uint16_t rev = ESP.getChipRevision();  // IDF 5.x format MXX: major*100 + minor
  snprintf(out, len, "v%u.%u", rev / 100, rev % 100);
}

void printBootBanner() {
  char mac[18], rev[8];
  macString(mac, sizeof(mac));
  chipRevString(rev, sizeof(rev));

  DBG_INFO("==========================================================");
  DBG_INFO(" ESP32-S3 PongClock v%s   built %s", FW_VERSION, buildTimestamp());
  DBG_INFO(" Board    : %s", BOARD_NAME);
  DBG_INFO(" Display  : %s", PANEL_DESC);
  DBG_INFO(" Chip     : %s %s, %d cores @ %lu MHz",
           ESP.getChipModel(), rev, ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  DBG_INFO(" Memory   : flash %lu MB, PSRAM %lu MB",
           (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
           (unsigned long)(ESP.getPsramSize()     / (1024UL * 1024UL)));
  DBG_INFO(" MAC      : %s", mac);
  DBG_INFO(" Core     : Arduino-ESP32 %s, IDF %s", ESP_ARDUINO_VERSION_STR, ESP.getSdkVersion());
  DBG_INFO(" Reset    : %s", resetReasonStr());
  DBG_INFO(" Controls : %s", controlsDesc());
  DBG_INFO("==========================================================");
}
