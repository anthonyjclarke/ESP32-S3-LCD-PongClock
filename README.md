# ESP32-S3-LCD-PongClock

<!-- Update version badge when FW_VERSION changes in include/config.h -->
![Version](https://img.shields.io/badge/version-2.0.3-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange.svg)
![Board](https://img.shields.io/badge/Waveshare-ESP32--S3--LCD--3.16-yellow.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)
![Status](https://img.shields.io/badge/status-active-brightgreen.svg)

---

**Repository:** https://github.com/anthonyjclarke/ESP32-S3-LCD-PongClock

---

PongClock port for the Waveshare ESP32-S3-LCD-3.16. Drives the 3.16" ST7701 RGB parallel panel in landscape mode via LovyanGFX, rendering an 80×32 virtual LED grid at 10 px/LED (800×320 sprite). Five clock modes cycle via a tactile button or the web UI. WiFiManager handles captive-portal provisioning; ezTime handles NTP sync with Olson timezone and DST.

---

## Hardware

| Feature    | Detail                                    |
|:-----------|:------------------------------------------|
| MCU        | ESP32-S3R8 · dual-core 240 MHz            |
| Flash      | 16 MB                                     |
| PSRAM      | 8 MB octal (OPI)                          |
| Display    | 3.16" · 320 × 820 · ST7701 · RGB parallel |
| Backlight  | GPIO6 · inverted LEDC PWM                 |
| Button     | GPIO4 · active LOW (INPUT_PULLUP to GND)  |
| Serial     | USB CDC (native USB port)                 |

---

## Clock Modes

| # | Mode       | Description                                      |
|:--|:-----------|:-------------------------------------------------|
| 0 | Slide      | Hours and minutes slide in from opposite sides   |
| 1 | Pong       | Classic Pong game with the score as the time     |
| 2 | Digits     | Large digit display with morphing transitions    |
| 3 | Word Clock | Time spelled out in words on the LED grid        |
| 4 | Invaders   | Space Invaders sprites animate the current time  |

**Button behaviour:**
- Short press — advance to next mode
- Long press ≥ 600 ms — cycle brightness (4 steps: 60 / 120 / 180 / 255)

Settings (mode, brightness, LED colours, timezone, NTP server) are persisted to NVS and survive power cycles.

---

## Web UI

After WiFi connects, the IP address is displayed on the LED matrix for 2.5 seconds — "IP" in large font with the address below in small font — before the clock starts. It is also printed to the serial monitor.

Once connected to WiFi, the board serves a single-page app on port 80. Open `http://<ip-address>/` in a browser.

| Endpoint          | Method   | Description                                  |
|:------------------|:---------|:---------------------------------------------|
| `/`               | GET      | Web UI (LittleFS SPA)                        |
| `/api/config`     | GET/POST | Read or write all config fields as JSON      |
| `/api/info`       | GET      | Firmware version, heap, PSRAM, uptime        |
| `/screenshot.bmp` | GET      | Current TFT frame as a BMP snapshot          |

The IP address is printed on the display after WiFi connects and is also logged to serial. If the board cannot find a saved network it opens a captive portal AP named `ESP32-S3-PongClock`.

---

## Pin Assignment

### LCD command interface (sideband SPI — ST7701 init only)

| Signal | GPIO |
|:-------|:-----|
| CS     | 0    |
| MOSI   | 1    |
| SCK    | 2    |
| RST    | 16   |

### RGB data bus

| Channel | GPIOs                 |
|:--------|:----------------------|
| Red     | 17, 46, 3, 8, 18      |
| Green   | 14, 13, 12, 11, 10, 9 |
| Blue    | 21, 5, 45, 48, 47     |

Pins are wired BGR on the PCB; `rgb_order = true` in the panel config corrects colour output.

### RGB timing

| Signal | GPIO |
|:-------|:-----|
| HSYNC  | 38   |
| VSYNC  | 39   |
| DE     | 40   |
| PCLK   | 41   |

RGB pixel clock: 18 MHz.

---

## Build and Flash

### 1. Build firmware

```bash
pio run
```

### 2. Flash firmware

```bash
pio run -t upload
```

If upload fails, enter the bootloader manually:

1. Close any serial monitor on `/dev/cu.usbmodem1301`
2. Hold `BOOT`, press and release `RST`, then release `BOOT`
3. Retry the upload command
4. Press `RST` once after upload completes if the board does not auto-run

### 3. Flash the filesystem

The web UI assets live in LittleFS and must be uploaded separately:

```bash
pio run -t uploadfs
```

**This step is required on first flash and any time you change files in `data/`.** Without it the web UI returns 404 and `/api/*` endpoints do not respond.

---

## Filesystem Assets and Gzip

The `data/` folder contains the web UI source files and their pre-compressed equivalents:

| File                | Raw size | Gzip size | Notes                   |
|:--------------------|:--------:|:---------:|:------------------------|
| `clock.js`          | 37.5 KB  | 9.6 KB    | PongClock canvas engine |
| `style.css`         |  9.0 KB  | 2.2 KB    | Dark amber theme        |
| `index.html`        |  6.2 KB  | 1.7 KB    | SPA shell               |
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

Key settings required for this board (not present in the generic `esp32-s3-devkitm-1` profile):

- `board_build.arduino.memory_type = qio_opi` — required for 8 MB OPI PSRAM
- `board_build.partitions = default_16MB.csv` — 16 MB flash layout
- `-DBOARD_HAS_PSRAM` — enables `ps_malloc()` and PSRAM APIs
- `-DARDUINO_USB_CDC_ON_BOOT=1` — routes `Serial` over USB CDC

---

## Display Notes

The ST7701 is not a simple SPI TFT. It uses a 16-bit RGB parallel pixel bus with a separate sideband SPI interface for init commands. `TFT_eSPI` cannot drive this panel — LovyanGFX with `Panel_ST7701_Base` is required.

The backlight is active-low at the firmware level:

- LEDC duty `0` → full brightness (backlight fully on)
- LEDC duty `255` → backlight off
- `cfg::backlightDutyFromBrightness(brightness)` inverts the mapping: `duty = 0xFF - brightness`

If the display goes dark after init but briefly shows a valid image during reset, the RGB data path is likely working — suspect backlight polarity first.

---

## Serial Monitor

```bash
pio device monitor -b 115200 -p /dev/cu.usbmodem1301
```

Expected boot output:

```
[INFO]  === ESP32-S3 PongClock 2.0.3 starting ===
[INFO]  Config loaded: mode=0 bright=180 ampm=0 tz=Australia/Sydney
[INFO]  WiFi connected: 192.168.1.180
[INFO]  Time synced: 14:32:05 11-Apr-2026  tz=AEST  offset=+1000
[INFO]  LittleFS mounted — 104 KB used / 3375 KB total
[INFO]  Web server started — http://192.168.1.180/
[INFO]  Setup complete — heap=241232 bytes  PSRAM=8355840 bytes
```

After WiFi connects, "IP" and the address appear on the LED matrix for 2.5 seconds before NTP sync begins.

---

## Known Quirks

- The board may show a valid image briefly during reset even when backlight control is wrong — RGB data path is working; check backlight polarity.
- PSRAM requires `board_build.arduino.memory_type = qio_opi`. Without it, PSRAM is unavailable and the sprite allocation silently fails.
- `kScreenHeight = 820` is correct — this is a tall 320×820 portrait panel; `setRotation(1)` produces the logical 820×320 landscape orientation used by PongClock.
- Backlight LEDC uses channel 1 on timer 3. Any additional LEDC peripherals must use different channel and timer numbers.
- USB CDC port is `/dev/cu.usbmodem1301`. It disappears during upload and reappears after reset — this is normal.
