#!/usr/bin/env python3
"""Package both PlatformIO builds for GitHub Pages; never use stale tracked bins."""
import argparse
import hashlib
import json
import re
import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOARDS = {
    "waveshare_lcd316": "Waveshare ESP32-S3-LCD-3.16",
    "tdisplay_s3": "LilyGO T-Display-S3",
}


def partitions(path):
    result = {}
    for entry in struct.iter_unpack("<HBBII16sI", path.read_bytes()[:0xC00]):
        magic, kind, subtype, offset, size, label, _ = entry
        if magic == 0x50AA:
            result[label.split(b"\0", 1)[0].decode()] = (offset, size)
    return result


def export(output, framework):
    version = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"',
                        (ROOT / "include/config.h").read_text()).group(1)
    output.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "installer.js", "style.css"):
        shutil.copyfile(ROOT / "docs" / name, output / name)
    (output / ".nojekyll").touch()
    for env, board in BOARDS.items():
        build = ROOT / ".pio/build" / env
        layout = partitions(build / "partitions.bin")
        # Guard the layout this installer supports, including preserving NVS.
        assert layout["nvs"] == (0x9000, 0x5000), "Unexpected NVS layout"
        assert layout["otadata"] == (0xE000, 0x2000), "Unexpected OTA layout"
        app_offset, app_size = layout["app0"]
        fs_offset, fs_size = layout["spiffs"]
        target = output / "firmware" / env
        target.mkdir(parents=True, exist_ok=True)
        sources = [
            (build / "bootloader.bin", 0, 0x8000),
            (build / "partitions.bin", 0x8000, 0x1000),
            (framework / "tools/partitions/boot_app0.bin", 0xE000, 0x2000),
            (build / "firmware.bin", app_offset, app_size),
            (build / "littlefs.bin", fs_offset, fs_size),
        ]
        parts = []
        for source, offset, limit in sources:
            data = source.read_bytes()
            if not data or len(data) > limit or offset + len(data) > 0x1000000:
                raise ValueError(f"Invalid image size: {source}")
            if source.name in ("bootloader.bin", "firmware.bin"):
                if data[0] != 0xE9 or struct.unpack_from("<H", data, 12)[0] != 9:
                    raise ValueError(f"Not an ESP32-S3 image: {source}")
            shutil.copyfile(source, target / source.name)
            parts.append({"path": source.name, "offset": offset, "size": len(data),
                          "sha256": hashlib.sha256(data).hexdigest(),
                          "md5": hashlib.md5(data).hexdigest()})
        manifest = {"name": f"PongClock — {board}", "board": board,
                    "version": version, "environment": env, "chipFamily": "ESP32-S3",
                    "parts": parts}
        (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Installer v{version} exported to {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".installer-site")
    parser.add_argument("--framework-dir", type=Path,
                        default=Path.home() / ".platformio/packages/framework-arduinoespressif32")
    args = parser.parse_args()
    export(args.output.resolve(), args.framework_dir.resolve())
