# Changelog

## [2.0.5] 2026-04-21

### Fixed
- Display jitter (vertical roll) when WiFi is active: `cls()` was calling `fillRoundRect(colourOff)` 2560 times (80×32 grid) over an already-`colourOff` background — visually a no-op but 2560 individual PSRAM writes per frame at 50 fps. Combined with the LCD DMA's continuous PSRAM reads and WiFi's PSRAM buffer traffic, this saturated the PSRAM bus and caused intermittent DMA underruns that manifested as display sync loss. Fix: `cls()` is now a single `fillSprite(colourOff)` bulk-fill.
- RGB pixel clock reduced from 18 MHz to 14 MHz (`kRgbClockHz` in `config.h`), reducing the LCD DMA's continuous PSRAM read rate from ~27 MB/s to ~21 MB/s and providing sufficient bandwidth headroom for WiFi under sustained load.
- LittleFS filesystem upload via USB CDC (`pio run -t uploadfs`) stalling at 0% with "The chip stopped responding": added `upload_flags = --no-stub` to `platformio.ini`, bypassing the stub flasher and using the ROM bootloader directly. The stub flasher's higher-speed protocol caused the USB CDC connection to drop mid-transfer on this board.
- Sprite allocation failure after a crash/watchdog reset: `createSprite()` defaults to `MALLOC_CAP_DMA` (internal SRAM only). After a non-clean reset the internal heap is fragmented enough that a contiguous 256 KB DMA block is unavailable, causing the sprite to fail even with 7+ MB of PSRAM free. Fix: try DMA allocation first (preferred — avoids PSRAM bus pressure during `pushSprite`); if that fails, `setPsram(true)` and retry, falling back to PSRAM.

### Changed
- `FW_VERSION` bumped to `2.0.5` in `include/config.h`.

## [2.0.4] 2026-04-21

### Fixed
- All 5 clock mode `while` loops now use `keepRunningMode(n)` instead of bare `run_mode()`, so a mode change via the web API takes effect immediately without waiting for the current loop iteration to complete. Port of CYD_PongClock commit `7842f8c`.
- Pong, Digits, Word Clock, and Invaders `ledColourChanged` handlers now call `cls()` before repaint, clearing stale off-state pixels that persisted in the old colour. Slide mode already had this fix. Port of CYD_PongClock commit `e38f682`.
- Invaders `invader_scroll()`: added `clock_mode != 4` guard at the start of each scroll step and in the per-step delay loop, so an externally triggered mode change (e.g. via web API) exits the scroll immediately rather than waiting for the current scroll pass to finish. Port of CYD_PongClock commit `7842f8c`.
- WiFi portal timeout now set to `0` (no timeout) on first boot when no saved credentials exist, so the captive portal stays open until the user connects. Previously the portal would time out after 60 s even on a blank device. Port of CYD_PongClock commit `7842f8c`.

### Changed
- Web UI: mode selection is now pills-only. The duplicate mode radio buttons in the Config tab have been removed — mode changes always go through the Clock tab pill row, which saves the selection to the device immediately. Port of CYD_PongClock commits `868ce75` / `7842f8c`.
- `FW_VERSION` bumped to `2.0.4` in `include/config.h`.

## [2.0.3] 2026-04-11

### Fixed
- LED colour change full redraw: when LED on/off colours are changed via the web UI, all clock modes now immediately redraw using the new colours without waiting for the next natural refresh cycle.
  - **Slide mode**: added `ledColourChanged` handler that re-renders the current digits, colons, and date row immediately, then resets `old_secs` so the next-second guard does not suppress the redraw for up to ~1 second.
  - **Pong mode**: added `ledColourChanged` handler that sets `bat1_upd = true` and `bat2_upd = true`, ensuring bats are redrawn in the new colour on the next frame (ball, score, net, and date already redraw every frame and self-recover within ~20 ms).
  - Digits, Word Clock, and Invaders modes already handled `ledColourChanged` correctly.

### Changed
- `FW_VERSION` bumped to `2.0.3` in `include/config.h`.

## [2.0.2] 2026-04-11

### Added
- Boot IP address display: after WiFi connects, `showBootIpOnMatrix()` clears the LED canvas and shows "IP" (5×7 font, centred, row 6) and the assigned IP address (3×5 tiny font, centred, row 22) for 2500 ms before NTP sync begins. The 80-col grid always fits any IPv4 address statically — no scroll required (max 15 chars × 4 px = 59 px < 80 px). The display is also shown as footer text at y=290/305 on the display surface. Port of CYD_PongClock `showBootIpOnMatrix()`.

### Changed
- `FW_VERSION` bumped to `2.0.2` in `include/config.h`.

## [2.0.1] 2026-04-11

### Fixed
- Web server unresponsive / intermittent page-load failures: `fade_down()` and `fade_up()` now call `webLoop()` in each fade step. At default brightness the fade runs ~23 × 25 ms ≈ 575 ms; without this fix the server was completely dark during that window. Word Clock hit this every minute; all modes hit it on every mode switch.
- `[E] _handleRequest(): request handler not found` logged by Arduino ESP32 3.x WebServer on every page load: added explicit `server.on("/pong-logo.png", ...)` route so the logo no longer falls through to `onNotFound`. In 3.x the fallback path logs at ERROR level even when the request is handled correctly.
- TFT animation freezes while loading the web UI: `streamFile()` blocks the Arduino task during TCP write; `clock.js` at 37 KB was the main offender (~300–600 ms block). Added gzip support to `serveFile()` — it now prefers `<path>.gz` from LittleFS with `Content-Encoding: gzip`. Pre-compressed versions of `clock.js` (9.6 KB), `style.css` (2.2 KB), and `index.html` (1.7 KB) added to `data/`. Total streaming time reduced ~4×.
- Web UI completely blank after the `.gz` files were first uploaded to LittleFS: `serveFile()` was calling `server.sendHeader("Content-Encoding","gzip")` explicitly, but Arduino ESP32 `WebServer::_streamFileCore()` already adds that header automatically whenever the file name passed to `streamFile()` ends in `.gz`. The result was a duplicate `Content-Encoding: gzip` header, which per RFC 7231 §3.1.2.2 tells browsers the body was gzipped twice — they attempt to decompress twice, the second decode fails on plain content, and the response is silently aborted. Fix: remove the manual `sendHeader` call and rely on `_streamFileCore` to add it based on filename suffix. CYD_PongClock never hit this because it doesn't ship `.gz` files.
- `[E][vfs_api.cpp:105] open(): /littlefs/pong-logo.png.gz does not exist` logged on every page load: `serveFile()` was unconditionally probing for a `.gz` variant of every file, including PNG. `LittleFS.exists()` on a missing file triggers an `[E]` from the VFS layer even in read-only mode. Fix: only probe for `.gz` when the path ends in `.html`, `.js`, or `.css`; binary types skip the probe entirely.
- LDR auto-brightness removed: no LDR is wired on this board. Removed `LDR_*` defines from `config.h`, `ldrEnabled` field from `RuntimeConfig`, NVS persistence, `updateBrightness()` from `display.cpp`, `tickHousekeeping()` LDR call, and all web UI / JSON references. No functional change — auto-brightness was never active (`LDR_ENABLED 0`).

### Changed
- `CLAUDE.md`: gzip notes updated — added double-header warning, PNG probe skip, `_streamFileCore` auto-header behaviour; `data/` table row updated to include `.gz` variants.
- `README.md` rewritten for v2.0.1: PongClock description, five clock modes, button behaviour, web UI endpoints, two-step flash instructions, gzip re-compress command and rationale, CYD vs ESP32-S3 comparison.
- `FW_VERSION` bumped to `2.0.1` in `include/config.h`.
- Pong mode: added intro animation (`pong_setup()`), net drawn at midfield, `beginModeLoop()` / `activeMode` tracking for clean external mode switches.
- All `delay()` calls in clock modes replaced with `serviceNetworkDelay()` so the web server is serviced continuously rather than only between frames.

## [2.0.0] 2026-04-10

### Added
- Full PongClock port from CYD_PongClock (ESP32-2432S028R / ILI9341 / TFT_eSPI)
- Five clock modes: Slide, Pong, Digits, Word Clock, Invaders
- 80×32 virtual LED grid at 10 px/LED — 800×320 PSRAM sprite (RGB332, 256 KB)
- Landscape layout via `setRotation(1)` on the 820×320 panel; 10 px left/right margins
- WiFiManager captive-portal provisioning (`ESP32-S3-PongClock` AP)
- ezTime NTP sync with Olson timezone and POSIX DST fallback
- GPIO4 tactile button: short press = next mode, long press ≥ 600 ms = brightness cycle
- Web UI: LittleFS-served SPA with live canvas preview and config form
- `/api/config` GET/POST, `/api/info`, `/screenshot.bmp` REST endpoints
- NVS persistence via `config_nvs.cpp` (mode, brightness, AM/PM, LED colours, timezone, NTP server, date interval)
- Splash screen with CRT-collapse animation
- `data/clock.js` — JavaScript PongClock engine mirroring all C++ mode logic at 800×320
- `data/index.html` + `data/style.css` — dark amber-theme web UI

### Changed
- `FW_VERSION` bumped to `2.0.0`
- `WaveshareDisplay` class moved to `include/waveshare_display.h`
- Display layer split into `src/display.cpp` / `include/display.h`
- Three-scene demo replaced by PongClock mode dispatcher
- `platformio.ini` gains `tzapu/WiFiManager`, `ropg/ezTime`, `bblanchon/ArduinoJson@^6.21.0`, `board_build.filesystem = littlefs`

## [1.0.0] 2026-04-10

### Added
- Initial LovyanGFX bring-up for Waveshare ESP32-S3-LCD-3.16
- ST7701 RGB parallel panel driver via `WaveshareDisplay` class extending `Panel_ST7701_Base`
- Inverted LEDC PWM backlight control on GPIO6
- Three-scene cycling demo: RGB/gradient, geometry, system stats
- `debug.h` leveled debug macro system
- `FIRMWARE_VERSION` in `config.h`
- Project scaffolding: `LICENSE`, `CHANGELOG.md`, `CLAUDE.md`, `.gitignore`
