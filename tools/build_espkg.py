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
import struct
import sys
import zlib
from datetime import datetime

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
    args = ap.parse_args()

    if not os.path.isdir(args.web):
        sys.exit(f"Website dir not found: {args.web}")
    if args.firmware and not os.path.isfile(args.firmware):
        sys.exit(f"Firmware not found: {args.firmware}")

    build(args.web, args.firmware, args.out, args.version, args.extra_files)


if __name__ == "__main__":
    main()
