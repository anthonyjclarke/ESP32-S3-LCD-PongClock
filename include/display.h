#pragma once
#include <Arduino.h>
#include "waveshare_display.h"

// ── Sprite ────────────────────────────────────────────────────────────────────
extern lgfx::LGFX_Sprite matrixSprite;

// ── State ─────────────────────────────────────────────────────────────────────
extern uint8_t  currentBrightness;
extern bool     ledColourChanged;

// ── Init ─────────────────────────────────────────────────────────────────────
void initColours();   // call after gfx.init() and gfx.setRotation()

// ── Drawing ───────────────────────────────────────────────────────────────────
void plot(int x, int y, bool on);
void cls();
void clsNow();
void pushMatrix();

// ── Brightness ────────────────────────────────────────────────────────────────
void setBrightness(uint8_t pwm);

// ── Colour ───────────────────────────────────────────────────────────────────
void setLedColours(uint8_t onR, uint8_t onG, uint8_t onB,
                   uint8_t offR, uint8_t offG, uint8_t offB);

// ── Animations ───────────────────────────────────────────────────────────────
void fade_down();
void fade_up();
