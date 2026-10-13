# Code and documentation review — 13 October 2026

Scope: firmware sources and headers, device web assets, build configuration and project documentation. Both existing firmware environments compile successfully. Browser installer tests use simulated serial hardware; physical device behavior and GitHub Actions deployment have not been exercised.

## Recommended firmware improvements

1. **High: bound config-change logging (`src/web.cpp:261`).** `LOGCAT` adds the return value of `snprintf` to `pos`, but that value is the length that would have been written. A sequence of changes with long old/new timezone and NTP strings can exceed the 320-byte buffer. Subsequent appends can address memory beyond the buffer and underflow the remaining size; the final whitespace trim and terminator also access out of bounds. Clamp the append position to bytes actually stored, or log fields separately. Test a multi-field POST with maximum-length old and new strings.

2. **Medium: serialize config JSON with ArduinoJson (`src/web.cpp:162`).** GET `/api/config` inserts timezone and NTP strings directly into quoted JSON. POST accepts quotes, backslashes and control characters in those strings, so a saved value can make the next GET invalid JSON and prevent the UI loading configuration. Use the same ArduinoJson serialization approach already used by `/api/info`.

3. **Medium: start the web server after a late WiFi connection (`src/web.cpp:329`).** `initWeb()` returns when setup is offline and is called only once. `webLoop()` only services an already-started server. If WiFi reconnects later, the clock can have an IP while its controls remain unavailable until reboot. Add connection-state handling that starts the server once connectivity is established; verify with an unavailable saved network brought online after boot.

4. **Medium: persist hardware mode changes (`src/clock.cpp:430` and the other mode loops).** KEY short presses change `clock_mode` but never update `rtCfg.clockMode` or save it. `/api/info` shows the new mode while `/api/config` shows the old one, and reboot restores the old mode despite the README's persistence claim. Centralize mode selection in one function used by the API and all button paths; persist only actual changes.

5. **Medium: reject invalid config types and failed timezone changes (`src/web.cpp:207`).** Numeric fields are converted directly to `uint8_t` without type/range validation, while empty or invalid timezone/NTP strings can be persisted. A failed timezone lookup still returns success and stores the requested timezone, although the active timezone may remain different; reboot applies the fixed Sydney POSIX fallback. Validate a complete candidate config before applying it, return a useful 400 response for invalid input, and keep the last valid timezone on lookup failure. Avoid writing NVS on unchanged values.

6. **Medium: handle missing sprite storage in the screenshot route (`src/web.cpp:108`).** `initColours()` logs allocation failure and continues. `/screenshot.bmp` then dereferences `matrixSprite.getBuffer()` without checking for null. Return 503 when the buffer is unavailable, and consider a visible startup error for missing PSRAM rather than proceeding through clock rendering.

7. **Low: calculate button brightness steps from the current brightness (`src/clock.cpp:57`).** `brightnessStep` is initialized at boot and changes only on hardware long presses. Adjusting brightness in the web UI leaves the step stale, so the next long press can jump down unexpectedly. Derive the next level from `currentBrightness` at the moment of the press.

## Build and documentation improvements

- Remove the machine-specific Waveshare `monitor_port` and `upload_port` defaults from `platformio.ini`, or document command-line overrides next to the upload instructions. Other users' ports will differ.
- Pin ArduinoJson to an exact version for reproducible public firmware artifacts; `^6.21.0` currently allows updates despite the documentation saying libraries are pinned.
- The filesystem instructions incorrectly said API endpoints also need LittleFS. The endpoints are registered independently of `fsReady`; README now explains that only static web files need the filesystem image.
- README now clearly identifies the board for each browser install option, unsupported display variants, ROM/download-mode requirements, the manual reset step, settings retention and Pages deployment setup.
- Asset sizes and transfer-time estimates vary by board and browser. Prefer generated sizes and measured timings with board/toolchain context over multiple inconsistent hard-coded examples.

## Installer validation

- Firmware: both `waveshare_lcd316` and `tdisplay_s3` builds succeed.
- Installer: automated tests cover both board choices, preserving the project URL prefix, ROM-only connection, wrong chip/flash size, manifest mismatch, corrupt downloads, flash verification failure, write failure, unsupported browsers and cancelled port selection.
- Packaging reads offsets from each compiled partition table, includes bootloader, partition table, OTA initialization, application and LittleFS, and validates ESP32-S3 image headers and partition size limits.
- Still required on hardware: first installation, upgrade retaining WiFi/NVS, retry after interrupted transfer, manual reset and web UI access on both boards.

The firmware findings above are suggestions; this change implements the installer and documentation updates without altering firmware behavior.
