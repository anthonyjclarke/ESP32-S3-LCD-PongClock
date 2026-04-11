#pragma once

#include <stdint.h>
#include <stdbool.h>

// Persisted runtime configuration — stored in NVS namespace "pclock".
struct RuntimeConfig {
  uint8_t  clockMode;
  uint8_t  brightness;
  bool     ampm;
  uint8_t  ledOnR,  ledOnG,  ledOnB;
  uint8_t  ledOffR, ledOffG, ledOffB;
  char     timezone[40];
  char     ntpServer[64];
  uint8_t  dateInterval;
};

extern RuntimeConfig rtCfg;

void loadConfig();
void saveConfig();
void resetConfigToDefaults();
