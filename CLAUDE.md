# Project: ESP32-S3-LCD-PongClock

Five-mode digital clock (Slide, Pong, Digits, Word Clock, Invaders) rendering an 80×32 virtual LED grid. One firmware, two boards selected at build time: Waveshare ESP32-S3-LCD-3.16 (`-e waveshare_lcd316`) and LilyGO T-Display-S3 (`-e tdisplay_s3`). WiFiManager + ezTime NTP, LittleFS web UI with live canvas preview and Device panel; T-Display-S3 also has a mode/brightness button. v2.1.0.

## Target Hardware

Both boards: ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM (`qio_opi`), native USB CDC (port varies — `ls /dev/cu.usbmodem*`).

| Board          | Panel                        | LED pitch | Sprite                   |
|:---------------|:-----------------------------|:----------|:-------------------------|
| Waveshare 3.16 | ST7701 RGB parallel, 320×820 | 10 px     | 800×320, 256 KB, PSRAM   |
| T-Display-S3   | ST7789V 8-bit i8080, 170×320 | 4 px      | 320×128, 40 KB, internal |

## Board Abstraction

- `-DBOARD_WAVESHARE_LCD316` / `-DBOARD_TDISPLAY_S3` come from the env; `config.h` `#error`s unless exactly one is set.
- Everything board-specific is in the two `#if` blocks in `config.h`, the panel classes, and the backlight functions in `display.cpp`. Include `board_display.h`, never a panel header directly.
- `LED_WIDTH`/`LED_HEIGHT` (80×32) are fixed: clock-mode layouts are hand-placed on that grid. A new board scales `PIXEL_SIZE`, never the grid.
- No touch on either board (T-Display-S3 touch variant's controller unused). Waveshare has no user button at all: `PIN_BUTTON -1` skips button handling, web UI only. T-Display-S3 uses KEY on GPIO14. Button code in `clock.cpp` checks `PIN_BUTTON < 0` — keep that guard.
- GPIO4 is battery ADC on **both** boards (Waveshare: ADC1 CH3). Never configure it as an input/button.
- Device identity lives in `sysinfo.cpp` and feeds the serial boot banner, the on-screen boot line and `/api/info`. Add new identity fields there, not inline. Build time is `__DATE__`/`__TIME__` of `sysinfo.cpp` — refreshes only when it recompiles (it includes `config.h`).

## Pin Assignments (Non-Standard)

| Function   | Waveshare 3.16                 | T-Display-S3                        |
|:-----------|:-------------------------------|:------------------------------------|
| Backlight  | 6 – inverted LEDC, ch 1 tmr 3  | 38 – pulse-count dimming, 16 levels |
| Panel ctrl | CS 0, MOSI 1, SCK 2, RST 16    | CS 6, DC 7, WR 8, RD 9, RST 5       |
| Data bus   | 16-bit RGB, BGR-wired          | D0–D7 = 39–42, 45–48                |
| Power      | –                              | 15 must be HIGH (dark on battery)   |
| Button     | none (`PIN_BUTTON -1`)         | 14 (KEY)                            |
| Battery    | ADC 4 – unused                 | ADC 4 – unused                      |

## Architecture Notes

**Waveshare RGB panel only** — the ST7789 has its own GRAM, so none of these apply to T-Display-S3:
- `kRgbClockHz` ≤ 14 MHz. At 18 MHz the LCD DMA's continuous PSRAM reads plus WiFi traffic cause vertical jitter.
- `cls()` is a single `fillSprite(colourOff)`. The old per-LED loop (2560 writes/frame) caused sync loss under WiFi. Do not restore.
- Sprite is always in PSRAM: the largest internal DMA block is ~250 KB at `setup()` entry (core 3.3.7), short of the 256 KB sprite, so the internal attempt in `initColours()` always falls back. Allocating earlier cannot help. Heap is rebuilt on every reset, so crash resets do not change this.

**T-Display-S3 backlight** is not PWM: each low pulse on GPIO38 steps the driver down one level (wrapping); low >2.5 ms = off. `blLevel` in `display.cpp` tracks the current level — never write GPIO38 elsewhere or the tracking desyncs.

**Fades**: `fade_down()` / `fade_up()` call `webLoop()` each step; without it the server is dead for ~575 ms per fade.

**Web asset gzip**: `serveFile()` prefers `<file>.gz`; Arduino `WebServer` adds `Content-Encoding: gzip` itself. Never `sendHeader("Content-Encoding", ...)` — duplicate header blanks the UI. After editing `data/`, run `gzip -9 -k -f data/clock.js data/style.css data/index.html` then `uploadfs`.

**WebServer routes**: every file referenced by `index.html` needs its own `server.on()` (incl. `/pong-logo.png`); Arduino-ESP32 3.x logs `[E]` for anything reaching `onNotFound`.

**Date interval** must be 1–59: 0 or 60 makes the next date minute equal the current one and repeats the date for a whole minute. Clamped on load and POST.

## NVS Keys (Preferences namespace `"pclock"`)

`mode`, `bright`, `ampm`, `onR` `onG` `onB`, `offR` `offG` `offB` (uint8/bool) · `tz`, `ntp`, `fwver` (string) · `dateInt` (uint8). A changed `fwver` resets `mode` to `DEFAULT_CLOCK_MODE`. WiFi credentials are stored by the ESP-IDF WiFi driver, not here.

## Toolchain

Platform pinned to pioarduino 55.03.37 (Arduino-ESP32 3.3.7, IDF 5.5.2) and LovyanGFX 1.2.32. An unpinned `espressif32` silently uses whatever platform is installed.

## Flash Notes

- Both envs use `upload_flags = --no-stub`: the stub flasher stops responding mid-`uploadfs` over USB CDC.
- After `uploadfs` with `--no-stub`, neither board auto-resets — press RST.
- A VS Code serial monitor holds the port and makes upload fail with "port is busy" — close it first.
- Both boards enumerate as the same `/dev/cu.usbmodem*` port. Check the MAC esptool prints (Waveshare `80:B5:4E:DA:61:F0`) or the boot banner before flashing — wrong-board firmware boots fine over serial but leaves the screen blank.
- Stuck: hold BOOT → press RST → release BOOT → retry → RST after flash.

## Global Rules That Do Not Apply Here

- CYD / TFT_eSPI rules — LovyanGFX is required (TFT_eSPI cannot drive the ST7701 RGB panel).
- Custom partition table — both boards use `default_16MB.csv`.
- XPT2046 / touch rules — no touch on either board; Waveshare is web UI only, T-Display-S3 adds GPIO14.
