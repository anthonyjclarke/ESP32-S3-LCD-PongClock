# CLAUDE.md — ESP32-S3-LCD-3.16

## Target Hardware

**Waveshare ESP32-S3-LCD-3.16**
- MCU: ESP32-S3R8 · dual-core 240 MHz
- Flash: 16 MB
- PSRAM: 8 MB octal OPI (mapped via `qio_opi`)
- Display: 3.16" · 320 × 820 · ST7701 · RGB parallel bus (not SPI)
- Serial: USB CDC on native USB port (`-DARDUINO_USB_CDC_ON_BOOT=1`)

## What This Project Does

PongClock port for the Waveshare ESP32-S3-LCD-3.16. Drives the ST7701 RGB panel in landscape mode (`setRotation(1)`, logical 820×320) with an 80×32 virtual LED grid at 10 px/LED (800×320 sprite). Five clock modes: Slide, Pong, Digits, Word Clock, Invaders. WiFiManager provisioning, ezTime NTP sync, GPIO4 tactile button for mode/brightness control, and a LittleFS-served web UI with live canvas preview. After WiFi connects, the assigned IP address is displayed on the LED matrix canvas for 2.5 s before the clock starts.

## Non-Obvious Pin Assignments

- **Backlight**: GPIO6, inverted LEDC PWM — duty 0 = full brightness, 255 = off. This is opposite to typical active-high setups.
- **LCD command interface**: GPIO0 (CS), GPIO1 (MOSI), GPIO2 (SCK), GPIO16 (RST) — this is a sideband SPI interface for ST7701 init commands, separate from the RGB pixel data bus.
- **RGB bus**: 16-bit parallel across GPIOs 3, 5, 8–14, 17–18, 21, 45–48 — see `config.h` for full pin mapping.
- **USB CDC**: Serial routes over native USB, not CP2102. Port is `/dev/cu.usbmodem1301`.

## Source Structure

| File | Purpose |
|:-----|:--------|
| `src/main.cpp` | `setup()`, `loop()`, splash screen, boot IP display, WiFi/NTP init |
| `src/display.cpp` | Sprite, backlight LEDC, `plot()`, `cls()`, `pushMatrix()`, fades |
| `src/clock.cpp` | All five clock modes, button handling, font render helpers |
| `src/web.cpp` | `WebServer` on port 80, LittleFS file serving, REST API handlers |
| `src/config_nvs.cpp` | NVS persistence via `Preferences` |
| `include/waveshare_display.h` | `WaveshareDisplay` class + `WavesharePanel` with ST7701 init |
| `include/config.h` | All constants — pins, timings, colours, WiFi AP name |
| `include/fonts.h` | PROGMEM font arrays: 5×7, 3×5, 10×14, invader sprites |
| `data/` | LittleFS web UI: `index.html`, `clock.js`, `style.css`, `pong-logo.png` + `.gz` variants |

## Library

LovyanGFX (`lovyan03/LovyanGFX`) is required. `TFT_eSPI` cannot drive an ST7701 RGB panel — do not suggest switching.

`WaveshareDisplay` is defined in `include/waveshare_display.h` and extends `lgfx::LGFX_Device`. It contains an inner class `WavesharePanel` that extends `lgfx::Panel_ST7701_Base` and overrides `getInitCommands()` with the full vendor init sequence.

LovyanGFX framebuffer is allocated in PSRAM (`use_psram = 1` in `panel_.config_detail()`). PSRAM must be present and correctly configured — if it is absent, display init will silently fail or produce garbage output.

## Web Server Architecture

- Uses Arduino `WebServer` (port 80) — single-threaded, synchronous. Not `AsyncWebServer`.
- `server.handleClient()` is called via `webLoop()`, which is called from:
  - `serviceNetworkDelay(ms)` in `clock.cpp` — the cooperative-yield helper used inside all clock mode loops. Calls `webLoop()` every 5 ms for the specified duration.
  - `tickHousekeeping()` — called at the top of each clock mode's inner `while` loop.
- **`fade_down()` / `fade_up()` must call `webLoop()` in each step** — at default brightness 180, a fade runs for ~23 steps × 25 ms ≈ 575 ms. Without `webLoop()` calls in the fade loop, the server is completely unresponsive for that window. Word Clock triggers this every minute; all modes trigger it on mode switch. This was the primary cause of intermittent page-load failures.
- The web server cannot handle parallel connections — browsers that open multiple simultaneous connections (HTML + JS + CSS) will queue them up, each waiting for the previous `handleClient()` to return.
- **Every file referenced in `index.html` must have an explicit `server.on()` route.** In Arduino ESP32 3.x, any URL without a matching route logs `[E] _handleRequest(): request handler not found` at ERROR level before falling back to `onNotFound` — even when `onNotFound` serves it correctly. `onNotFound` is kept as a safe fallback for unknown paths, but known static assets (including `/pong-logo.png`) must be in the route table.
- `/screenshot.bmp` streams 768 KB (800×320 @ 24-bit BMP). During streaming the clock frame loop is paused. This endpoint is only triggered by the download button, not auto-polled.
- **Web asset gzip**: `serveFile()` checks for `<path>.gz` in LittleFS first (text assets only: `.html`, `.js`, `.css`). If found, `streamFile()` serves it directly — Arduino ESP32 `WebServer::_streamFileCore()` **automatically** adds `Content-Encoding: gzip` whenever the file name ends in `.gz`. **Do NOT call `server.sendHeader("Content-Encoding","gzip")` manually** — doing so alongside `streamFile()` on a `.gz` file produces a duplicate header. Browsers interpret `Content-Encoding: gzip, gzip` as double-compression and silently abort the response; the web UI goes completely blank with no error. The gz probe is skipped for `.png` and other binary types because `LittleFS.exists()` on a missing file triggers a spurious `[E][vfs_api.cpp] open(): does not exist` error log. Pre-compressed versions of `clock.js` (9.6 KB), `style.css` (2.2 KB), and `index.html` (1.7 KB) live alongside the originals in `data/`. **When editing any web asset, re-run** `gzip -9 -k -f data/clock.js data/style.css data/index.html` then `pio run -t uploadfs`. Without this, the `.gz` files go stale and browsers receive old content.
- **TFT freeze on page load**: `streamFile()` is a blocking TCP write that holds the Arduino task for the duration of the transfer. The LCD DMA continues from the last framebuffer but the clock animation pauses — visible as a static frame freeze. Gzip compression reduces `clock.js` from 37 KB to ~9.6 KB, cutting blocking time by ~4×. This is inherent to single-threaded `WebServer`; `Cache-Control: max-age=300` limits it to once per 5-minute window.

## Fonts

Clock mode rendering uses PROGMEM byte arrays in `include/fonts.h`: `myfont[68][5]` (5×7 dot-matrix), `mybigfont[10][20]` (large digit segments), `mytinyfont[41][3]` (3×5 tiny), and `invader_sprites[3][2][2][5]`. These are display-library-agnostic and render via `putChar()`/`putBigChar()`/`putTinyChar()` into the sprite. No LovyanGFX built-in fonts are used in clock mode.

## Known Hardware Quirks

- **Boot IP display**: after WiFi connects, `showBootIpOnMatrix()` clears the LED canvas and shows "IP" (5×7 font, centred) on row 6 and the IP address in 3×5 tiny font on row 22, holding for 2500 ms (`kBootIpHoldMs`). The 80-col grid always fits any IPv4 without scrolling (max 15 chars × 4 px = 59 px < 80 px). The matrix content is overwritten the moment the first clock mode renders. `showStatus()` and `showIpLine()` also overlay footer text at y=290/305 on the display surface (no physical margin — overlays the bottom of the sprite area temporarily). Port of CYD_PongClock's `showBootIpOnMatrix()`.
- Board shows a brief valid image during reset even with incorrect backlight control. This means the RGB path is working; suspect backlight polarity first if the screen goes dark after init.
- PSRAM requires `board_build.arduino.memory_type = qio_opi` — the generic devkitm-1 profile does not set this.
- `default_16MB.csv` is used (not a custom partition table) — the 16 MB flash layout is handled by the PlatformIO built-in file.
- `kScreenHeight = 820` is correct for this panel — it is a tall narrow 320 × 820 display.
- RGB bus data pins are wired in BGR order (d0–d4 = Blue, d5–d10 = Green, d11–d15 = Red). The panel config sets `rgb_order = true` to swap R/B and produce correct colours. If colours look wrong, check this flag first.
- Backlight LEDC uses **channel 1** (`kBacklightPwmChannel`) on **timer 3** (`LEDC_TIMER_3`). Any future LEDC peripheral (e.g. buzzer, additional PWM) must use a different channel and timer.
- `gfx.setRotation(1)` is landscape orientation (logical 820×320) used by PongClock. The physical panel is portrait (320×820); rotation 1 flips it so the wide axis is horizontal.
- **Button GPIO4**: wired to GND with `INPUT_PULLUP`. Active LOW. Not connected to the RGB bus or LCD interface.

## Flashing Notes

- Port: `/dev/cu.usbmodem1301`
- If upload fails: hold BOOT → press RST → release BOOT → retry upload → press RST after flash completes
- No special upload flags required beyond what is in `platformio.ini`

## Global Rules That Do Not Apply Here

- CYD / TFT_eSPI rules: this is not a CYD board and does not use TFT_eSPI
- Custom partition table: not required — uses `default_16MB.csv`
- XPT2046 touch: no touchscreen on this board; input is via GPIO4 tactile button
