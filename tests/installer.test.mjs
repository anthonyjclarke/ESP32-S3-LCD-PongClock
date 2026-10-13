import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { webcrypto, createHash } from 'node:crypto';
import vm from 'node:vm';

// Run the page's real control flow with simulated serial hardware and downloads.
const source = readFileSync(new URL('../docs/installer.js', import.meta.url), 'utf8')
  .replace("import('https://unpkg.com/esptool-js@0.7.0/bundle.js')", 'mockImporter()');

function harness({ environment = 'waveshare_lcd316', chip = 'ESP32-S3', flashSize = '16MB',
  corrupt = false, mismatch = false, supported = true, cancel = false, verificationFails = false,
  writeFails = false } = {}) {
  const elements = Object.fromEntries(['confirm', 'status', 'progress', 'log', 'completion', 'completion-detail'].map(id =>
    [id, { checked: true, hidden: true, textContent: '', value: 0, addEventListener() {}, scrollIntoView() {} }]));
  const buttons = [{ dataset: { board: environment }, addEventListener() {} }];
  const calls = [];
  const data = new Uint8Array([0xE9, 1, 2, 3]);
  const md5 = createHash('md5').update(data).digest('hex');
  const manifest = { board: environment, environment: mismatch ? 'wrong-board' : environment,
    version: '2.1.0', chipFamily: 'ESP32-S3', parts: [{ path: 'firmware.bin', offset: 0x10000,
      size: data.length, sha256: createHash('sha256').update(data).digest('hex'), md5 }] };
  class Transport {
    async disconnect() { calls.push('disconnect'); }
  }
  class ESPLoader {
    chip = { CHIP_NAME: chip };
    ESP_SPI_ATTACH = 0x0D;
    ESP_SPI_FLASH_MD5 = 0x13;
    MD5_TIMEOUT_PER_MB = 8000;
    async detectChip(mode) { calls.push(['detect', mode]); }
    async detectFlashSize() { return flashSize; }
    async main() { throw new Error('Stub flashing must never be used'); }
    async runStub() { throw new Error('Stub flashing must never be used'); }
    async checkCommand(_, command, packet, __, length) {
      calls.push(['command', command, packet.length, length]);
      if (command === 0x0B) {
        assert.equal(packet.length, 24);
        assert.equal(new DataView(packet.buffer).getUint32(4, true), 0x1000000);
      }
      if (command === this.ESP_SPI_FLASH_MD5) {
        assert.equal(elements.completion.hidden, true, 'Completion stays hidden until verification passes');
        assert.equal(new DataView(packet.buffer).getUint32(0, true), 0x10000);
        return new TextEncoder().encode(verificationFails ? '0'.repeat(32) : md5);
      }
    }
    timeoutPerMb() { return 3000; }
    async writeFlash(options) {
      calls.push('flash');
      assert.equal(this.FLASH_WRITE_SIZE, 0x400);
      assert.equal(options.eraseAll, false);
      assert.equal(options.flashSize, 'keep');
      assert.equal(options.fileArray[0].address, 0x10000);
      assert.deepEqual([...options.fileArray[0].data], [...data]);
      if (writeFails) throw new Error('USB disconnected');
      options.reportProgress(0, data.length, data.length);
    }
  }
  const context = vm.createContext({
    document: { querySelectorAll: () => buttons, getElementById: id => elements[id] },
    window: { isSecureContext: true, addEventListener() {}, removeEventListener() {} },
    navigator: supported ? { serial: { async requestPort() {
      calls.push('requestPort');
      if (cancel) throw Object.assign(new Error('Cancelled'), { name: 'NotFoundError' });
      return {};
    } } } : {},
    location: { href: 'https://example.org/ESP32-S3-LCD-PongClock/' },
    async fetch(url) {
      calls.push(url.href);
      return { ok: true, json: async () => manifest,
        arrayBuffer: async () => (corrupt ? new Uint8Array([0, 1, 2, 3]) : data).buffer };
    },
    mockImporter: async () => ({ ESPLoader, Transport }),
    URL, Uint8Array, DataView, TextDecoder, TextEncoder, crypto: webcrypto,
  });
  new vm.Script(source).runInContext(context);
  return { elements, buttons, calls, cancelNext() { cancel = true; },
    install: () => vm.runInContext(`install('${environment}')`, context) };
}

for (const environment of ['waveshare_lcd316', 'tdisplay_s3']) {
  test(`installs and verifies ${environment} without a stub`, async () => {
    const h = harness({ environment });
    await h.install();
    assert.match(h.elements.status.textContent, /Installed PongClock/);
    assert.equal(h.elements.progress.value, 100);
    assert.equal(h.calls[0], 'requestPort');
    assert.ok(h.calls.includes(`https://example.org/ESP32-S3-LCD-PongClock/firmware/${environment}/manifest.json`));
    assert.ok(h.calls.includes('disconnect'));
    assert.equal(h.elements.completion.hidden, false);
    assert.match(h.elements['completion-detail'].textContent, /verified successfully/);
    assert.match(h.elements.log.textContent, /COMPLETED/);
  });
}
for (const [name, options] of [
  ['wrong chip', { chip: 'ESP32' }], ['wrong flash size', { flashSize: '4MB' }],
  ['corrupt download', { corrupt: true }], ['wrong manifest', { mismatch: true }],
]) {
  test(`blocks flashing for ${name}`, async () => {
    const h = harness(options);
    await h.install();
    assert.match(h.elements.status.textContent, /Installation failed/);
    assert.ok(!h.calls.includes('flash'));
    assert.equal(h.buttons[0].disabled, false);
    assert.equal(h.elements.completion.hidden, true);
  });
}
test('rejects failed flash verification and releases the port', async () => {
  const h = harness({ verificationFails: true });
  await h.install();
  assert.match(h.elements.status.textContent, /Flash verification failed/);
  assert.ok(h.calls.includes('disconnect'));
  assert.equal(h.elements.completion.hidden, true);
});
test('releases the port after a write failure', async () => {
  const h = harness({ writeFails: true });
  await h.install();
  assert.match(h.elements.status.textContent, /USB disconnected/);
  assert.ok(h.calls.includes('disconnect'));
});
test('unsupported browsers cannot start an install', async () => {
  const h = harness({ supported: false });
  await h.install();
  assert.equal(h.calls.length, 0);
  assert.equal(h.buttons[0].disabled, true);
});
test('cancelled port selection permits retry', async () => {
  const h = harness({ cancel: true });
  await h.install();
  assert.match(h.elements.status.textContent, /No serial port selected/);
  assert.equal(h.buttons[0].disabled, false);
});
test('a new attempt clears the previous completion message', async () => {
  const h = harness();
  await h.install();
  assert.equal(h.elements.completion.hidden, false);
  h.cancelNext();
  await h.install();
  assert.equal(h.elements.completion.hidden, true);
  assert.equal(h.elements['completion-detail'].textContent, '');
});
