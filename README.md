# ESP32-S3-LCD-PongClock

<!-- Update version badge when FW_VERSION changes in include/config.h -->
![Version](https://img.shields.io/badge/version-2.1.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange.svg)
![Board](https://img.shields.io/badge/Waveshare-ESP32--S3--LCD--3.16-yellow.svg)
![Board](https://img.shields.io/badge/LilyGO-T--Display--S3-yellow.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)
![Status](https://img.shields.io/badge/status-active-brightgreen.svg)

---

**Repository:** https://github.com/anthonyjclarke/ESP32-S3-LCD-PongClock

---

PongClock for ESP32-S3 display boards. Renders an 80×32 virtual LED grid via LovyanGFX on either of two boards, chosen at build time:

| Board                        | Panel                          | LED pitch | PlatformIO env     |
|:-----------------------------|:-------------------------------|:----------|:-------------------|
| Waveshare ESP32-S3-LCD-3.16  | 3.16" ST7701 RGB, 320×820      | 10 px     | `waveshare_lcd316` |
| LilyGO T-Display-S3          | 1.9" ST7789V i8080, 170×320    | 4 px      | `tdisplay_s3`      |

Five clock modes, switched from the web UI on both boards or with the T-Display-S3's KEY button. WiFiManager handles captive-portal provisioning; ezTime handles NTP sync with Olson timezone and DST.

---

## Hardware

### Waveshare ESP32-S3-LCD-3.16

| Feature    | Detail                                    |
|:-----------|:------------------------------------------|
| MCU        | ESP32-S3R8 · dual-core 240 MHz            |
| Flash      | 16 MB                                     |
| PSRAM      | 8 MB octal (OPI)                          |
| Display    | 3.16" · 320 × 820 · ST7701 · RGB parallel |
| Backlight  | GPIO6 · inverted LEDC PWM                 |
| Button     | None – control is via the web UI only     |
| Touch      | None                                      |
| Battery    | ADC on GPIO4 (ADC1 CH3) – not used        |
| Serial     | USB CDC (native USB port)                 |

### LilyGO T-Display-S3

| Feature    | Detail                                       |
|:-----------|:---------------------------------------------|
| MCU        | ESP32-S3R8 · dual-core 240 MHz               |
| Flash      | 16 MB                                        |
| PSRAM      | 8 MB octal (OPI)                             |
| Display    | 1.9" · 170 × 320 · ST7789V · 8-bit i8080     |
| Backlight  | GPIO38 · pulse-count dimming, 16 levels      |
| Power      | GPIO15 must be HIGH (panel dark on battery)  |
| Button     | GPIO14 (KEY) · active LOW                    |
| Touch      | None (non-touch variant)                     |
| Battery    | ADC on GPIO4 – not used                      |
| Serial     | USB CDC (native USB port)                    |

The grid is drawn at 4 px/LED (320×128) centred with 21 px margins top and bottom; boot status text uses those margins. The KEY button on GPIO14 changes mode and brightness (see [Controls](#controls)).

---

## Clock Modes

| # | Mode       | Description                                      |
|:--|:-----------|:-------------------------------------------------|
| 0 | Slide      | Hours and minutes slide in from opposite sides   |
| 1 | Pong       | Classic Pong game with the score as the time     |
| 2 | Digits     | Large digit display with morphing transitions    |
| 3 | Word Clock | Time spelled out in words on the LED grid        |
| 4 | Invaders   | Space Invaders sprites animate the current time  |

### Controls

Neither board has a touchscreen. The web UI controls everything on both boards; the T-Display-S3 also has a hardware button.

| Board                        | Physical control                | Web UI |
|:-----------------------------|:--------------------------------|:-------|
| Waveshare ESP32-S3-LCD-3.16  | None                            | Yes    |
| LilyGO T-Display-S3          | KEY button on GPIO14            | Yes    |

KEY button actions (T-Display-S3):

| Action               | Result                                               |
|:---------------------|:-----------------------------------------------------|
| Short press          | Next clock mode                                      |
| Long press ≥ 600 ms  | Next brightness step: 60 → 120 → 180 → 255 → 60      |

The long-press cycle starts from the current brightness, so the first press always steps up from where the clock already is.

The Waveshare's only buttons are BOOT and RST (used for flashing) plus a power switch. BOOT is GPIO0, which is also the LCD command chip-select, so the clock does not read it. GPIO4 on both boards is the battery-voltage input and must never be configured as a button: `PIN_BUTTON` is `-1` on the Waveshare, which skips button handling entirely.

The T-Display-S3's BOOT button (GPIO0) is not used by the clock either.

Settings (mode, brightness, LED colours, timezone, NTP server, date interval) are persisted to NVS and survive power cycles.

---

## Web UI

After WiFi connects, the IP address is displayed on the LED matrix for 2.5 seconds — "IP" in large font with the address below in small font — before the clock starts. It is also printed to the serial monitor.

Once connected to WiFi, the board serves a single-page app on port 80. Open `http://<ip-address>/` in a browser.

| Endpoint          | Method   | Description                                  |
|:------------------|:---------|:---------------------------------------------|
| `/`               | GET      | Web UI (LittleFS SPA)                        |
| `/api/config`     | GET/POST | Read or write all config fields as JSON      |
| `/api/info`       | GET      | Device identity, network, memory, uptime     |
| `/screenshot.bmp` | GET      | Current TFT frame as a BMP snapshot          |

The IP address is printed on the display after WiFi connects and is also logged to serial. If the board cannot find a saved network it opens a captive portal AP named `ESP32-S3-PongClock`.

### Device panel

The Clock tab shows a Device panel below the mode buttons, so you can tell which board and firmware a page belongs to. The board name and panel also appear on the pictured PCB, and its decorative button is hidden on boards without one. Values come from `/api/info` and refresh once a minute.

| Field      | Example                                         |
|:-----------|:------------------------------------------------|
| Board      | Waveshare ESP32-S3-LCD-3.16                     |
| Display    | ST7701 820×320                                  |
| Firmware   | v2.1.0 · built 13-10-2026 12:36:29              |
| Controls   | web UI only / button GPIO14 + web UI            |
| Network    | 192.168.1.179 · esp32s3-DA61F0                  |
| WiFi       | SSID · RSSI in dBm                              |
| MAC        | 80:B5:4E:DA:61:F0                               |
| Chip       | ESP32-S3 v0.2 · 240 MHz                         |
| Memory     | flash 16 MB · PSRAM 8 MB                        |
| Free       | free heap · free PSRAM                          |
| Core       | Arduino-ESP32 3.3.7 · IDF v5.5.2                |
| Uptime     | 0h 1m                                           |
| Last reset | power-on, software, USB, panic, watchdog, …     |

`/api/info` returns the same data as JSON: `firmware`, `build`, `board`, `panel`, `controls`, `button` (`-1` = none), `chip`, `chipRev`, `cpuMHz`, `flashMB`, `psramMB`, `core`, `idf`, `resetReason`, `mac`, `ip`, `hostname`, `ssid`, `rssi`, `mode`, `modeName`, `brightness`, `uptime`, `freeHeap`, `freePsram`.

The build time is when `src/sysinfo.cpp` was last compiled. It includes `config.h`, so a version bump refreshes it; a build that only changes other source files keeps the previous time.

---

## Pin Assignment

### T-Display-S3

| Signal                | GPIO                           |
|:----------------------|:-------------------------------|
| D0–D7                 | 39, 40, 41, 42, 45, 46, 47, 48 |
| CS / DC / WR / RD     | 6 / 7 / 8 / 9                  |
| RST                   | 5                              |
| Backlight             | 38                             |
| Panel power enable    | 15                             |
| Button                | 14                             |

The panel is 170 px wide inside the ST7789's 240-wide RAM, so it uses `offset_x = 35` and colour inversion.

### Waveshare — LCD command interface (sideband SPI — ST7701 init only)

| Signal | GPIO |
|:-------|:-----|
| CS     | 0    |
| MOSI   | 1    |
| SCK    | 2    |
| RST    | 16   |

### Waveshare — RGB data bus

| Channel | GPIOs                 |
|:--------|:----------------------|
| Red     | 17, 46, 3, 8, 18      |
| Green   | 14, 13, 12, 11, 10, 9 |
| Blue    | 21, 5, 45, 48, 47     |

Pins are wired BGR on the PCB; `rgb_order = true` in the panel config corrects colour output.

### Waveshare — RGB timing

| Signal | GPIO |
|:-------|:-----|
| HSYNC  | 38   |
| VSYNC  | 39   |
| DE     | 40   |
| PCLK   | 41   |

RGB pixel clock: 14 MHz (ceiling with WiFi active — see Known Quirks).

---

## Build and Flash

Every command takes `-e <env>` to pick the board. Without it, `default_envs` (`waveshare_lcd316`) is used.

### 1. Build firmware

```bash
pio run -e tdisplay_s3
```

### 2. Flash firmware

```bash
pio run -e tdisplay_s3 -t upload
```

Use `-e waveshare_lcd316` for the Waveshare board.

If upload fails, enter the bootloader manually:

1. Close any serial monitor
2. Hold `BOOT`, press and release `RST`, then release `BOOT`
3. Retry the upload command
4. Press `RST` once after upload completes

### 3. Flash the filesystem

The web UI assets live in LittleFS and must be uploaded separately:

```bash
pio run -e tdisplay_s3 -t uploadfs
```

**This step is required on first flash and any time you change files in `data/`.** Without it the web UI returns 404 and `/api/*` endpoints do not respond.

> **Note:** both envs set `upload_flags = --no-stub`, which uses the ROM bootloader instead of the stub flasher. This is required for reliable filesystem upload over USB CDC on both boards. On the Waveshare the device does **not** auto-reset after `uploadfs` — press `RST` manually to boot the application.

---

## Filesystem Assets and Gzip

The `data/` folder contains the web UI source files and their pre-compressed equivalents:

| File                | Raw size | Gzip size | Notes                   |
|:--------------------|:--------:|:---------:|:------------------------|
| `clock.js`          | 38.7 KB  | 10.3 KB   | PongClock canvas engine |
| `style.css`         |  9.6 KB  | 2.3 KB    | Dark amber theme        |
| `index.html`        |  5.7 KB  | 1.6 KB    | SPA shell               |
| `pong-logo.png`     | 21.3 KB  | —         | PNG; not gzipped        |

### Why gzip is required on this board

`server.streamFile()` is a **blocking** TCP write — the Arduino task is stuck inside it for the entire transfer duration. During that window the TFT animation cannot update; the LCD DMA controller keeps refreshing the last framebuffer, so the display freezes on a static frame.

At 37 KB, `clock.js` blocked the Arduino task for approximately 300–600 ms per page load. With gzip the browser receives 9.6 KB instead, cutting the block to ~75–150 ms — a 4× improvement and no longer perceptible as a freeze.

`serveFile()` automatically prefers the `.gz` variant from LittleFS and adds the `Content-Encoding: gzip` header. The originals are kept as source of truth.

### Why CYD_PongClock does not need gzip

The original CYD_PongClock uses an ILI9341 SPI panel (240×320) with a 48×32 LED grid. `clock.js` for that grid is substantially smaller, and the ILI9341 is driven by SPI DMA which is largely independent of the WiFi stack. The same `streamFile()` block exists in the CYD code but its shorter duration (~50–100 ms) does not cause a visible freeze on that hardware.

The ESP32-S3 version has a larger canvas (80×32 grid, 800×320 sprite), a correspondingly larger `clock.js`, and the ST7701 RGB panel with a PSRAM-resident framebuffer — making the blocking window both longer and more visible.

### Re-compressing after editing

After editing any of `data/clock.js`, `data/style.css`, or `data/index.html`, regenerate the compressed versions and re-flash the filesystem:

```bash
gzip -9 -k -f data/clock.js data/style.css data/index.html
pio run -t uploadfs
```

Do **not** gzip `pong-logo.png` — PNG is already compressed internally and gzip saves less than 1%.

---

## PlatformIO Configuration

Shared `[env]` settings (the Waveshare env uses the generic `esp32-s3-devkitm-1` profile, which lacks these; T-Display-S3 uses `lilygo-t-display-s3`):

- `platform` pinned to pioarduino 55.03.37 (Arduino-ESP32 3.3.7) — an unpinned `espressif32` uses whatever platform version happens to be installed
- `board_build.arduino.memory_type = qio_opi` — required for 8 MB OPI PSRAM
- `board_build.partitions = default_16MB.csv` — 16 MB flash layout
- `-DBOARD_HAS_PSRAM` — enables `ps_malloc()` and PSRAM APIs
- `-DARDUINO_USB_CDC_ON_BOOT=1` — routes `Serial` over USB CDC

---

## Display Notes

### Waveshare

The ST7701 is not a simple SPI TFT. It uses a 16-bit RGB parallel pixel bus with a separate sideband SPI interface for init commands. `TFT_eSPI` cannot drive this panel — LovyanGFX with `Panel_ST7701_Base` is required.

The backlight is active-low at the firmware level:

- LEDC duty `0` → full brightness (backlight fully on)
- LEDC duty `255` → backlight off
- `cfg::backlightDutyFromBrightness(brightness)` inverts the mapping: `duty = 0xFF - brightness`

If the display goes dark after init but briefly shows a valid image during reset, the RGB data path is likely working — suspect backlight polarity first.

### T-Display-S3

The ST7789 keeps its own frame memory, so the ESP32 only writes to it on `pushSprite()` (about 4 ms per frame at 20 MHz). None of the Waveshare's PSRAM-bandwidth limits apply.

The backlight driver is not PWM-dimmed. It starts at full brightness, each low pulse on GPIO38 steps it down one of 16 levels (wrapping from dimmest back to brightest), and holding the pin low for more than 2.5 ms turns it off. Brightness 0–255 is mapped onto those 16 levels, so fades are slightly stepped.

---

## Serial Monitor

```bash
pio device monitor -e tdisplay_s3
```

The port number varies — use `ls /dev/cu.usbmodem*` to find the current name.

Every boot starts with a header that identifies the device, firmware and build. Expected output (Waveshare):

```
[INFO]  ==========================================================
[INFO]   ESP32-S3 PongClock v2.1.0   built 13-10-2026 12:36:29
[INFO]   Board    : Waveshare ESP32-S3-LCD-3.16
[INFO]   Display  : ST7701 820×320
[INFO]   Chip     : ESP32-S3 v0.2, 2 cores @ 240 MHz
[INFO]   Memory   : flash 16 MB, PSRAM 8 MB
[INFO]   MAC      : 80:B5:4E:DA:61:F0
[INFO]   Core     : Arduino-ESP32 3.3.7, IDF v5.5.2-729-g87912cd291
[INFO]   Reset    : USB
[INFO]   Controls : web UI only
[INFO]  ==========================================================
[INFO]  Config loaded: mode=2 bright=132 ampm=0 tz=Australia/Sydney
[INFO]  Sprite OK — buffer @ 0x3c1b4a30  heap=300012  PSRAM=7587276
[INFO]  No button on this board — control via web UI
[INFO]  WiFiManager portal timeout: 60 s (saved credentials)
[INFO]  WiFi connected: 192.168.1.179
[INFO]  Time synced: 12:39:09 13-Oct-2026  tz=AEDT offset=+1100
[INFO]  LittleFS mounted — 112 KB used / 3456 KB total
[INFO]  Web server started — http://192.168.1.179/
[INFO]  Setup complete — heap=253592 bytes  PSRAM=7575844 bytes
```

On the T-Display-S3 the header shows `LilyGO T-Display-S3`, `ST7789 320×170` and `Controls : button GPIO14 + web UI`, and the next log line reads `Button initialised on GPIO14`.

On screen, after the splash, the bottom status line shows the board, firmware version and build date (for example `LilyGO T-Display-S3  v2.1.0  13-10-2026`) while WiFi connects. Once WiFi connects, "IP" and the address appear on the LED matrix for 2.5 seconds before NTP sync begins.

---

## Known Quirks

### Waveshare ESP32-S3-LCD-3.16

- The board may show a valid image briefly during reset even when backlight control is wrong — RGB data path is working; check backlight polarity.
- PSRAM requires `board_build.arduino.memory_type = qio_opi`. Without it, PSRAM is unavailable and the sprite allocation silently fails.
- `kScreenHeight = 820` is correct — this is a tall 320×820 portrait panel; `setRotation(1)` produces the logical 820×320 landscape orientation used by PongClock.
- Backlight LEDC uses channel 1 on timer 3. Any additional LEDC peripherals must use different channel and timer numbers.
- USB CDC port is `/dev/cu.usbmodem2301` (number varies — re-enumerates on each reset). It disappears during upload and reappears after reset — this is normal.
- **RGB PCLK ceiling**: `kRgbClockHz` must be ≤ 14 MHz with WiFi active. At 18 MHz the LCD DMA saturates the shared PSRAM bus and causes vertical display jitter. Do not raise the clock without re-testing under WiFi load.
- **Sprite allocation**: the 800×320 sprite needs 256,000 bytes, but on Arduino-ESP32 3.3.7 the largest free block of internal DMA-capable RAM is about 250 KB from the very start of `setup()`. The internal attempt in `initColours()` therefore always fails, and the sprite lands in PSRAM via `setPsram(true)` on every boot (the boot log shows a `0x3C…` buffer address). This is expected. It is unrelated to crash resets, and allocating earlier would not help. It is also why the RGB pixel-clock and `cls()` limits above matter: the sprite and the LCD frame buffer share the PSRAM bus. The T-Display-S3's 40 KB sprite fits in internal RAM.

### LilyGO T-Display-S3

- GPIO15 must be driven HIGH before the display starts, or the panel stays dark when running on battery. `initBacklight()` does this.
- The 170-px-wide glass sits in the middle of the ST7789's 240-wide memory, hence `offset_x = 35`. Colours are inverted by the panel, hence `invert = true`.
- After `uploadfs` (with `--no-stub`) the board stays in the bootloader until you press RST, the same as the Waveshare.
- The touch version of the T-Display-S3 works, but its touch controller is not used.
