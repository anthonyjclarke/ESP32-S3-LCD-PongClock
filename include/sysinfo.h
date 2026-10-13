#pragma once
#include <Arduino.h>

// Device identity — one source for the serial boot banner, the boot line on
// the display, and /api/info.

const char* buildTimestamp();   // "13-10-2026 12:40:01" — see note in sysinfo.cpp
const char* resetReasonStr();   // "power-on", "software", "panic", ...
const char* controlsDesc();     // "button GPIO14 + web UI" or "web UI only"
void        macString(char* out, size_t len);    // "80:B5:4E:DA:61:F0"
void        chipRevString(char* out, size_t len); // "v0.2"

void printBootBanner();
