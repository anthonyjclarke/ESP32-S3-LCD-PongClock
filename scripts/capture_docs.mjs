// Capture the actual data/ web UI using example API responses, without hardware.
// Requires Playwright and Chrome; see docs/assets/README.md for regeneration.
import { createRequire } from 'node:module';
import { readFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const { chromium } = createRequire(import.meta.url)('playwright');
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const output = resolve(root, 'docs/assets');
mkdirSync(output, { recursive: true });
const version = readFileSync(resolve(root, 'include/config.h'), 'utf8').match(/#define\s+FW_VERSION\s+"([^"]+)"/)[1];
const modes = ['slide', 'pong', 'digits', 'word', 'invaders'];
const boardInfo = {
  waveshare: { board: 'Waveshare ESP32-S3-LCD-3.16', panel: 'ST7701 820×320',
    controls: 'web UI only', button: -1 },
  lilygo: { board: 'LilyGO T-Display-S3', panel: 'ST7789 320×170',
    controls: 'button GPIO14 + web UI', button: 14 },
};
const browser = await chromium.launch({ headless: true,
  ...(process.env.CHROME_PATH ? { executablePath: process.env.CHROME_PATH } : {}) });
try {
  for (const [board, details] of Object.entries(boardInfo)) {
    const page = await browser.newPage({ viewport: { width: 1000, height: 1080 },
      deviceScaleFactor: 1, timezoneId: 'Australia/Sydney' });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.clock.install({ time: new Date('2026-10-13T10:42:30+11:00') });
    await page.clock.pauseAt(new Date('2026-10-13T10:42:30+11:00'));
    await page.addInitScript(() => {
      let seed = 42;
      Math.random = () => { seed = (1664525 * seed + 1013904223) >>> 0; return seed / 0x100000000; };
    });
    const config = { mode: 1, brightness: 220, ampm: false, ledOnR: 255, ledOnG: 140,
      ledOnB: 0, ledOffR: 20, ledOffG: 8, ledOffB: 0, timezone: 'Australia/Sydney',
      ntpServer: 'pool.ntp.org', dateInterval: 10 };
    await page.route('**/*', route => {
      const url = new URL(route.request().url());
      if (url.hostname !== 'pongclock.example') return route.abort();
      if (url.pathname === '/api/config') {
        return route.fulfill({ json: route.request().method() === 'GET' ? config : { status: 'ok' } });
      }
      if (url.pathname === '/api/info') {
        return route.fulfill({ json: { ...details, firmware: version, build: '13-10-2026 10:00:00',
          chip: 'ESP32-S3', chipRev: 'v0.2', cpuMHz: 240, flashMB: 16, psramMB: 8,
          core: '3.3.7', idf: 'v5.5.2', resetReason: 'power-on', mac: '02:00:00:00:00:01',
          ip: '192.168.1.100', hostname: 'pongclock-example', ssid: 'Example WiFi',
          rssi: -48, mode: config.mode, modeName: 'Pong', brightness: config.brightness,
          uptime: 5400, freeHeap: 245760, freePsram: 7340032 } });
      }
      const assets = { '/': 'index.html', '/clock.js': 'clock.js', '/style.css': 'style.css',
        '/pong-logo.png': 'pong-logo.png' };
      const name = assets[url.pathname];
      if (!name) return route.fulfill({ status: 404, body: '' });
      return route.fulfill({ body: readFileSync(resolve(root, 'data', name)),
        contentType: name.endsWith('.js') ? 'text/javascript' : name.endsWith('.css') ?
          'text/css' : name.endsWith('.png') ? 'image/png' : 'text/html' });
    });
    await page.goto('https://pongclock.example/');
    await page.waitForFunction(() => document.getElementById('deviceInfo').textContent.includes('Example WiFi'));
    await page.clock.runFor(2500);
    if (board === 'waveshare') {
      for (const mode of modes) {
        await page.evaluate(mode => { activeMode = null; startMode(mode); }, mode);
        await page.clock.runFor(mode === 'pong' ? 3500 : mode === 'invaders' ? 5500 : 1200);
        await page.locator('#ledCanvas').screenshot({ path: resolve(output, `mode-${mode}.png`) });
      }
    }
    await page.evaluate(() => { activeMode = null; startMode('pong'); });
    await page.clock.runFor(3500);
    await page.screenshot({ path: resolve(output, `webui-${board}.png`), fullPage: true });
    if (board === 'waveshare') {
      await page.locator('[data-tab="config"]').click();
      await page.clock.runFor(300);
      await page.screenshot({ path: resolve(output, 'webui-config.png'), fullPage: true, animations: 'disabled' });
    }
    if (errors.length) throw new Error(errors.join('\n'));
    await page.close();
    console.log(`Captured ${board} browser previews`);
  }
} finally { await browser.close(); }
