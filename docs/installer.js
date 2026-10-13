const buttons = [...document.querySelectorAll('[data-board]')];
const confirmation = document.getElementById('confirm');
const status = document.getElementById('status');
const progress = document.getElementById('progress');
const log = document.getElementById('log');
const supported = window.isSecureContext && 'serial' in navigator;
let busy = false;

function updateButtons() {
  buttons.forEach(button => { button.disabled = busy || !supported || !confirmation.checked; });
  confirmation.disabled = busy;
}
function appendLog(message) {
  log.textContent = (log.textContent + message + '\n').slice(-20000);
  log.scrollTop = log.scrollHeight;
}
function hex(buffer) {
  return [...new Uint8Array(buffer)].map(byte => byte.toString(16).padStart(2, '0')).join('');
}
async function fetchChecked(url) {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) throw new Error(`Cannot download ${url.pathname} (${response.status}).`);
  return response;
}

async function install(environment) {
  if (busy || !supported || !confirmation.checked) return;
  busy = true;
  updateButtons();
  log.textContent = '';
  progress.value = 0;
  let transport;
  let port;
  let unload;
  try {
    // Call immediately from the button click to preserve browser user activation.
    port = await navigator.serial.requestPort();
    unload = event => { event.preventDefault(); event.returnValue = ''; };
    window.addEventListener('beforeunload', unload);
    status.textContent = 'Downloading and checking installation files…';
    const manifestURL = new URL(`firmware/${environment}/manifest.json`, location.href);
    const manifest = await (await fetchChecked(manifestURL)).json();
    if (manifest.environment !== environment || manifest.chipFamily !== 'ESP32-S3') {
      throw new Error('Firmware manifest does not match the selected board.');
    }
    const fileArray = await Promise.all(manifest.parts.map(async part => {
      const data = new Uint8Array(await (await fetchChecked(new URL(part.path, manifestURL))).arrayBuffer());
      if (data.length !== part.size || hex(await crypto.subtle.digest('SHA-256', data)) !== part.sha256) {
        throw new Error(`Download integrity check failed: ${part.path}`);
      }
      return { data, address: part.offset };
    }));
    const { ESPLoader, Transport } = await import('https://unpkg.com/esptool-js@0.7.0/bundle.js');
    transport = new Transport(port);
    const loader = new ESPLoader({ transport, baudrate: 115200,
      terminal: { clean() {}, writeLine: appendLog, write: appendLog } });
    status.textContent = `Connecting to ${manifest.board} · v${manifest.version}…`;
    // main() always runs the stub. Use detectChip() to stay in ROM instead.
    await loader.detectChip('no_reset');
    if (loader.chip.CHIP_NAME !== 'ESP32-S3') throw new Error('Only ESP32-S3 chips are supported.');
    if (loader.secureDownloadMode) throw new Error('Secure download mode is not supported by this installer.');
    // ROM SPI_ATTACH has an extra legacy word compared with the stub command.
    await loader.checkCommand('attach flash', loader.ESP_SPI_ATTACH, new Uint8Array(8));
    if (await loader.detectFlashSize() !== '16MB') throw new Error('This firmware requires a 16 MB flash chip.');
    // Tell ROM about all 16 MB so the LittleFS partition above 4 MB is writable.
    // Mirrors Python esptool's flash_set_parameters (SPI_SET_PARAMS = 0x0B).
    const flashParameters = new Uint8Array(24);
    const parametersView = new DataView(flashParameters.buffer);
    [0, 0x1000000, 0x10000, 0x1000, 256, 0xFFFF].forEach((value, index) => {
      parametersView.setUint32(index * 4, value, true);
    });
    await loader.checkCommand('configure 16 MB flash', 0x0B, flashParameters);
    // Small ROM blocks avoid native USB CDC packet/buffer limitations.
    loader.FLASH_WRITE_SIZE = 0x400;
    const totalBytes = fileArray.reduce((sum, file) => sum + file.data.length, 0);
    await loader.writeFlash({ fileArray, flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: false, compress: true,
      reportProgress(index, written, total) {
        const previous = fileArray.slice(0, index).reduce((sum, file) => sum + file.data.length, 0);
        const percentage = Math.floor(100 * (previous + fileArray[index].data.length * written / total) / totalBytes);
        progress.value = percentage;
        status.textContent = `Installing on ${manifest.board} · ${percentage}%`;
      }
    });
    status.textContent = 'Verifying flash contents…';
    for (const part of manifest.parts) {
      const packet = new Uint8Array(16);
      const view = new DataView(packet.buffer);
      view.setUint32(0, part.offset, true);
      view.setUint32(4, part.size, true);
      // ROM returns 32 ASCII hex characters; the stub returns 16 binary bytes.
      const digest = await loader.checkCommand('verify flash', loader.ESP_SPI_FLASH_MD5,
        packet, 0, 32, loader.timeoutPerMb(loader.MD5_TIMEOUT_PER_MB, part.size));
      if (new TextDecoder().decode(digest).toLowerCase() !== part.md5) {
        throw new Error(`Flash verification failed: ${part.path}. Enter download mode and retry.`);
      }
      appendLog(`Verified ${part.path}`);
    }
    progress.value = 100;
    status.textContent = `Installed PongClock v${manifest.version} on ${manifest.board}. Press RST to start the clock.`;
  } catch (error) {
    status.textContent = error.name === 'NotFoundError' ? 'No serial port selected. Choose your board to try again.' :
      `Installation failed: ${error.message} Enter BOOT + RST download mode and retry, or use PlatformIO.`;
    appendLog(status.textContent);
  } finally {
    if (transport) {
      try { await transport.disconnect(); } catch (error) { appendLog(`Disconnect: ${error.message}`); }
    } else if (port?.readable) {
      try { await port.close(); } catch (error) { appendLog(`Close port: ${error.message}`); }
    }
    if (unload) window.removeEventListener('beforeunload', unload);
    busy = false;
    updateButtons();
  }
}
confirmation.addEventListener('change', updateButtons);
buttons.forEach(button => button.addEventListener('click', () => install(button.dataset.board)));
status.textContent = supported ? 'Ready. Check your board model and tick the confirmation to enable installation.' :
  'Browser installation needs desktop Chrome or Edge over HTTPS (or localhost). Use PlatformIO in other browsers.';
updateButtons();
