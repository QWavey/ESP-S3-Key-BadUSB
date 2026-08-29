#!/usr/bin/env python3
"""
build_espkg.py — build a bundled .espkg update package for ESP-BadUSB-S3-Key.

A .espkg bundles the website files (written to the SD card) and, optionally, a
new firmware image (flashed over-the-air). The device applies the website files
first, then the firmware (which reboots the chip), so a single upload updates
both the UI and the firmware.

Container format (little-endian):
    [0]    6 bytes   magic  = b'ESPKG\\x01'
    [6]    uint32    manifest length (M)
    [10]   M bytes   manifest JSON (utf-8)
    [10+M] payloads, concatenated in this exact order:
             every sd[] file's bytes (in listed order), then the fw bytes.

Manifest JSON:
    {
      "version": "2.1",
      "created": "2026-08-28T00:00:00",
      "sd":  [ {"path": "/index.html", "size": 12345, "crc32": "aabbccdd"}, ... ],
      "fw":  {"size": 1581156, "crc32": "11223344"}   # size 0 => no firmware
    }

Examples:
    # Web UI + firmware
    python build_espkg.py --web "../Website [ESP SD]" \\
        --firmware build/ESP-BadUSB-S3-Key.ino.bin --version 2.1 -o firmware.espkg

    # Web UI only (no firmware flash, just refresh the SD website files)
    python build_espkg.py --web "../Website [ESP SD]" --version 2.1-web -o web.espkg
"""

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib
from datetime import datetime

# Toolchain defaults — mirror the sketch's pinned build settings.
DEFAULT_FQBN = ("esp32:esp32:esp32s3:USBMode=default,FlashSize=8M,"
                "CDCOnBoot=default,UploadMode=default,"
                "PartitionScheme=default_8MB,PSRAM=disabled")
DEFAULT_SKETCH = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "ESP-BadUSB-S3-Key"))
DEFAULT_BUILD_DIR = os.path.join(os.environ.get("TEMP", "/tmp"), "esp32-key-build")


def _find_arduino_cli():
    for p in [
        shutil.which("arduino-cli"),
        r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
        os.path.expandvars(r"%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
    ]:
        if p and os.path.isfile(p):
            return p
    sys.exit("arduino-cli not found. Install Arduino IDE 2.x or arduino-cli.")


def run_compile(sketch_dir, build_dir, fqbn, force_clean):
    cli = _find_arduino_cli()
    os.makedirs(build_dir, exist_ok=True)
    if force_clean:
        # Blow away any cached objects so the next compile is a full rebuild
        # (--clean would work too, but on some CLI versions is finicky).
        for name in os.listdir(build_dir):
            p = os.path.join(build_dir, name)
            (shutil.rmtree if os.path.isdir(p) else os.remove)(p)
        print(f"[compile] cleared {build_dir}")
    print(f"[compile] arduino-cli compile → {build_dir}")
    r = subprocess.run(
        [cli, "compile", "--fqbn", fqbn, "--output-dir", build_dir, sketch_dir],
        check=False,
    )
    if r.returncode != 0:
        sys.exit(f"[compile] arduino-cli exited {r.returncode}")
    fw = os.path.join(build_dir, "ESP-BadUSB-S3-Key.ino.bin")
    if not os.path.isfile(fw):
        sys.exit(f"[compile] compile succeeded but firmware .bin not found at {fw}")
    print(f"[compile] built {fw} ({os.path.getsize(fw):,} bytes)")
    return fw


def run_flash(build_dir, fqbn, port):
    cli = _find_arduino_cli()
    print(f"[flash] arduino-cli upload → {port}")
    r = subprocess.run(
        [cli, "upload", "--fqbn", fqbn, "--port", port, "--input-dir", build_dir],
        check=False,
    )
    if r.returncode != 0:
        sys.exit(f"[flash] arduino-cli exited {r.returncode}")
    print(f"[flash] uploaded to {port}")

MAGIC = b"ESPKG\x01"

# Default website files -> where they live on the SD card root.
DEFAULT_WEB_FILES = {
    "index.html": "/index.html",
    "style.css":  "/style.css",
    "script.js":  "/script.js",
}


def crc32_hex(data: bytes) -> str:
    return format(zlib.crc32(data) & 0xFFFFFFFF, "08x")


def build(web_dir, firmware, out_path, version, extra_files):
    sd_entries = []   # (sd_path, bytes)

    # Collect website files that exist.
    for local_name, sd_path in DEFAULT_WEB_FILES.items():
        p = os.path.join(web_dir, local_name)
        if os.path.isfile(p):
            with open(p, "rb") as f:
                sd_entries.append((sd_path, f.read()))
        else:
            print(f"  ! skipping missing {local_name}", file=sys.stderr)

    # Any explicitly listed extra files: "localpath:/sd/path"
    for spec in extra_files or []:
        if ":" not in spec:
            sys.exit(f"--file expects LOCAL:/sd/path, got: {spec}")
        local, sd_path = spec.split(":", 1)
        with open(local, "rb") as f:
            sd_entries.append((sd_path, f.read()))

    if not sd_entries and not firmware:
        sys.exit("Nothing to package: no website files found and no --firmware given.")

    fw_bytes = b""
    if firmware:
        with open(firmware, "rb") as f:
            fw_bytes = f.read()

    # Build the manifest.
    manifest = {
        "version": version,
        "created": datetime.now().replace(microsecond=0).isoformat(),
        "sd": [
            {"path": path, "size": len(data), "crc32": crc32_hex(data)}
            for path, data in sd_entries
        ],
        "fw": {"size": len(fw_bytes), "crc32": crc32_hex(fw_bytes) if fw_bytes else ""},
    }
    manifest_bytes = json.dumps(manifest, separators=(",", ":")).encode("utf-8")

    # Write the container.
    with open(out_path, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<I", len(manifest_bytes)))
        out.write(manifest_bytes)
        for _, data in sd_entries:      # website / SD files first
            out.write(data)
        out.write(fw_bytes)             # firmware last

    total = os.path.getsize(out_path)

    print(f"Wrote {out_path}  ({total:,} bytes)")
    print(f"  version : {version}")
    print(f"  manifest: {len(manifest_bytes)} bytes")
    for e in manifest["sd"]:
        print(f"  SD  {e['path']:<16} {e['size']:>8,} B  crc32={e['crc32']}")
    if fw_bytes:
        print(f"  FW  {'(firmware)':<16} {len(fw_bytes):>8,} B  crc32={manifest['fw']['crc32']}")
    else:
        print("  FW  (none — website-only package)")


def main():
    ap = argparse.ArgumentParser(description="Build a .espkg update package.")
    ap.add_argument("--web", default="Website [ESP SD]",
                    help="Website source directory (default: 'Website [ESP SD]')")
    ap.add_argument("--firmware", "-f", default=None,
                    help="Compiled firmware .bin to flash (optional; omit for a web-only package)")
    ap.add_argument("--out", "-o", default="firmware.espkg", help="Output .espkg path")
    ap.add_argument("--version", "-v", default="dev", help="Package version string")
    ap.add_argument("--file", action="append", dest="extra_files",
                    help="Extra file mapping LOCAL:/sd/path (repeatable)")
    ap.add_argument("--web-only", action="store_true",
                    help="Explicitly build a website-only package (rejects --firmware).")
    # arduino-cli integration -----------------------------------------------
    ap.add_argument("--force-compile", action="store_true",
                    help="Run arduino-cli compile (clean rebuild) before packaging; the "
                         "resulting firmware.bin is used as --firmware.")
    ap.add_argument("--flash-after-done", action="store_true",
                    help="After packaging, run arduino-cli upload to flash the compiled "
                         "firmware.bin directly to --port (NOT the .espkg).")
    ap.add_argument("--sketch", default=DEFAULT_SKETCH,
                    help="Sketch directory for --force-compile (default: ../ESP-BadUSB-S3-Key)")
    ap.add_argument("--build-dir", default=DEFAULT_BUILD_DIR,
                    help="Compile output directory (default: %(default)s)")
    ap.add_argument("--fqbn", default=DEFAULT_FQBN, help="arduino-cli FQBN (default: pinned)")
    ap.add_argument("--port", default="COM6", help="Serial port for --flash-after-done (default: COM6)")
    args = ap.parse_args()

    if args.web_only and (args.firmware or args.force_compile):
        sys.exit("--web-only can't be combined with --firmware / --force-compile.")

    if args.force_compile:
        args.firmware = run_compile(args.sketch, args.build_dir, args.fqbn, force_clean=True)

    if not os.path.isdir(args.web):
        sys.exit(f"Website dir not found: {args.web}")
    if args.firmware and not os.path.isfile(args.firmware):
        sys.exit(f"Firmware not found: {args.firmware}")

    build(args.web, args.firmware, args.out, args.version, args.extra_files)

    if args.flash_after_done:
        if not args.firmware:
            print("[flash] no firmware to flash (web-only build); skipping.")
        else:
            run_flash(args.build_dir, args.fqbn, args.port)


if __name__ == "__main__":
    main()
