# Documentation images

Board photos identify the supported hardware; they show manufacturer demonstrations rather than PongClock running on a board. Manufacturer images remain the property of their respective owners and are not covered by this project's MIT code license.

| Local file | Manufacturer source | Product documentation |
|:-----------|:--------------------|:----------------------|
| `board-waveshare.jpg` | [Waveshare product photo](https://www.waveshare.com/media/catalog/product/cache/1/image/800x800/9df78eab33525d08d6e5fb8d27136e95/e/s/esp32-s3-lcd-3.16-1.jpg) | [Product](https://www.waveshare.com/product/esp32-s3-lcd-3.16.htm) · [Wiki](https://www.waveshare.com/wiki/ESP32-S3-LCD-3.16) |
| `board-tdisplay-s3.jpg` | [LilyGO product photo](https://lilygo.cc/cdn/shop/files/H569_03.jpg?v=1774661646&width=800) | [Product](https://lilygo.cc/en-us/products/t-display-s3) · [Wiki](https://wiki.lilygo.cc/products/t-display-series/t-display-s3/) |

Sources checked on 13 October 2026.

## Browser screenshots

`mode-slide.png`, `mode-pong.png`, `mode-digits.png`, `mode-word.png` and `mode-invaders.png` are captures of the actual `#ledCanvas` in `data/index.html`, rendered by `data/clock.js`. `webui-waveshare.png`, `webui-lilygo.png` and `webui-config.png` capture the actual UI with example API responses. The web UI's PCB drawing is decorative.

The capture script uses an example Sydney time on 13 October 2026, seeded randomness, controlled animation timers and illustrative network/device data. It never connects to or modifies a real board. External font requests are blocked so capture does not rely on a font service. These examples are not evidence of physical display output or installer validation.

To regenerate with Node.js and Playwright:

```bash
npm install --prefix /tmp/pongclock-docs playwright
NODE_PATH=/tmp/pongclock-docs/node_modules CHROME_PATH="/path/to/chrome" node scripts/capture_docs.mjs
```

On macOS, Chrome is usually `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`. Alternatively install Playwright's Chromium (`/tmp/pongclock-docs/node_modules/.bin/playwright install chromium`) and omit `CHROME_PATH`. Inspect each screenshot after generation, especially the Pong and Invaders animation frames.

To document a real device instead, download `http://<device-ip>/screenshot.bmp` for each selected mode and capture the browser tabs directly. The BMP contains the sprite rather than the board, bezel or display margins. Label physical captures separately from the example browser previews.
