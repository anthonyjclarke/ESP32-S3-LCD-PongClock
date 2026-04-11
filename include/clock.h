#pragma once
#include <Arduino.h>
#include <ezTime.h>

// ── Shared globals ────────────────────────────────────────────────────────────
extern byte    rtc[7];      // [0]=sec [1]=min [2]=hr [3]=dow [4]=day [5]=mon [6]=yr
extern byte    clock_mode;
extern bool    ampm;
extern Timezone myTZ;

// ── Init ─────────────────────────────────────────────────────────────────────
void initButton();

// ── Time ─────────────────────────────────────────────────────────────────────
void get_time();

// ── Mode control ──────────────────────────────────────────────────────────────
bool    run_mode();          // false when mode changed externally, true otherwise
int8_t  checkButton();       // returns +1 (short press), 0 (none), -1 (long → brightness)

// ── Date helpers ──────────────────────────────────────────────────────────────
void set_next_date();
bool check_show_date();

// ── Font rendering ────────────────────────────────────────────────────────────
void putChar(byte x, byte y, char c);
void putTinyChar(byte x, byte y, char c);
void putBigChar(byte x, byte y, char c);

// ── Helpers ───────────────────────────────────────────────────────────────────
void flashing_cursor(byte xpos, byte ypos, byte w, byte h, byte repeats);
void levelbar(byte xpos, byte ypos, byte xbar, byte ybar);
void display_date();
void slideanim(byte x, byte y, byte sequence, char old_c, char new_c);

// ── Clock modes ───────────────────────────────────────────────────────────────
void slide();
void pong();
void digits();
void word_clock();
void invaders();
