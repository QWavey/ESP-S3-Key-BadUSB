# ESP-BadUSB-S3-Key

BadUSB firmware for the **ESP32-S3-Dongle / ESP32-S3-Key** USB stick board
(schematic marking: `ESP32-S3-Dongle v1.0g`).

This is a hardware variant of the ESP32-S3 DevKitC BadUSB project, adapted for
the small USB-A "key" form factor, which has **no NeoPixel** — just a single
plain blue LED and one button.

## Board hardware map

| Function | GPIO | Details |
|----------|------|---------|
| Onboard LED | **GPIO1** | Single **blue** LED, active-HIGH (`GPIO1 → 1 kΩ → LED → GND`). Driven via PWM. |
| Button (BOOT/stop) | **GPIO0** | Push button `S1` to GND, active-LOW. |
| SD card (SPI mode) | CS **34**, MOSI **35**, MISO **37**, SCK **36** | Card is wired for SDMMC (`SD_D3/SD_CMD/SD_D0/SD_CLK`); used here in 1-bit SPI mode. |
| USB | native | ESP32-S3 USB-OTG (TinyUSB HID keyboard). VID `0x303A`, PID `0x0002`. |

## Differences from the DevKitC version

- **`GpioLed.h`** — a small drop-in shim that mimics the `Adafruit_NeoPixel`
  subset used by the project (`begin`, `setBrightness`, `Color`,
  `setPixelColor`, `show`) but drives a single GPIO LED. Any non-black color
  turns the LED on; brightness and color intensity map to PWM duty. All LED
  status modes (solid/blink/warning/completion) work — just in blue on/off.
- **`Config.h`** — `LED_PIN = 1`, SD pins set to 34/35/37/36.
- No `Adafruit_NeoPixel` dependency.

Everything else (web UI, DuckyScript interpreter, WiFi AP, BLE, SD scripts,
logging) is identical to the base project.

## What's new in this variant

- **Live Keyboard tab** — type in the browser and every keystroke is sent to the
  host in real time over HID (like a wireless keyboard). Enter/Backspace/Tab/Esc,
  arrows and Ctrl/Alt/Gui combos are forwarded. Endpoint: `POST /api/live-type`.
- **Silent Startup (stealth HID)** — optional Settings toggle. When on, the device
  does **not** present a USB keyboard at boot (only draws power); the HID keyboard
  is attached only while a script runs or Live typing is active, then detached
  again. Uses TinyUSB `tud_connect()` / `tud_disconnect()`. Ducky commands
  `HID_ATTACH` / `HID_DETACH` are also available.
- **Mobile-responsive UI** — the control panel is now fluid (was a fixed 1000 px
  layout that forced phones to zoom out); the tab bar wraps and grids collapse to
  one column on small screens.
- **Tab-navigation fix** — removed a duplicate `openTab()` definition and hardened
  tab switching (a failing per-tab data load can no longer wedge the UI).
- **Blue-LED-accurate commands** — added `LED_ON`; Help now documents that this
  board only has ON / OFF / BLINK (legacy colour commands still light blue).
- **Bundled `.espkg` updates** — Settings → *Firmware Update* takes a single
  `.espkg` package that updates the web UI **and** the firmware in one upload:
  the website files are written to the SD card first, then the firmware is
  flashed over-the-air and the device reboots. Live upload + apply progress bar.

## Building & applying a `.espkg` update

A `.espkg` bundles the website files and (optionally) a firmware image. Build one
with the included Python tool:

```bash
# Web UI + firmware
python tools/build_espkg.py \
    --web "Website [ESP SD]" \
    --firmware build/ESP-BadUSB-S3-Key.ino.bin \
    --version 2.1 \
    -o firmware.espkg

# Web UI only (no firmware flash — just refresh the SD website files)
python tools/build_espkg.py --web "Website [ESP SD]" --version 2.1-web -o web.espkg
```

Then open the WebUI → **Settings → Firmware Update**, choose the `.espkg`, and
press *Upload & Apply*. The device writes the website files to the SD card,
flashes the firmware, and reboots. **Do not unplug during the update.**

**Container format** (little-endian): `6-byte magic ESPKG\x01` · `uint32 manifest
length` · `manifest JSON {version, sd:[{path,size,crc32}], fw:{size,crc32}}` ·
payloads concatenated (all SD files first, firmware last). Applied website-first
because flashing the firmware reboots the chip.

## Feature status (verified against the code in this repo)

✅ Working &nbsp;•&nbsp; ⏳ Partial / planned &nbsp;•&nbsp; ❌ Not implemented

| #  | Feature                                             | Status |
| -- | --------------------------------------------------- | ------ |
| 1  | Basic key combinations                              | ✅ |
| 2  | Strings & variables                                 | ✅ |
| 3  | WiFi detection (`IF_PRESENT`, scan)                 | ✅ |
| 4  | LED control (single blue LED)                       | ✅ |
| 5  | Raw keycodes                                        | ✅ |
| 6  | SD card detection                                   | ✅ |
| 7  | Uploading files                                     | ✅ |
| 8  | Functions (`FUNCTION` / `RETURN`)                   | ✅ |
| 9  | Repeat / replay commands (`REPEAT`)                 | ✅ |
| 11 | Math operations (`VAR a = a + 1`)                   | ✅ |
| 13 | Hold keys (`HOLD`)                                  | ✅ |
| 14 | Rower payloads (sequential scripts)                 | ✅ |
| 15 | Boolean conditions (`IF ... true/false`)            | ✅ |
| 16 | Syntax-error highlighting (editor)                  | ✅ |
| 17 | Customisation WebGUI (Design tab)                   | ✅ |
| 19 | OS detection (`DETECT_OS`)                          | ✅ |
| 21 | Connecting to WiFi (STA join)                       | ✅ |
| 22 | Web actions (`DOWNLOAD_FILE`, HTTP)                 | ✅ |
| 23 | Copy / cut / paste + download SD↔PC                 | ✅ |
| 24 | Disable USB at boot / draw power only (**Silent Startup**) | ✅ |
| 28 | Start script on client connect                      | ✅ |
| 29 | Turn WiFi / Bluetooth on/off                        | ✅ |
| 32 | `FOR` loops                                          | ✅ |
| 34 | **Live typing to host** (new)                       | ✅ |
| 35 | **Bundled `.espkg` update** (web + firmware, new)   | ✅ |
| 10 | Custom fonts WebGUI                                  | ⏳ |
| 30 | Toggle WiFi/BT automatically when a WiFi is seen     | ⏳ |
| 33 | Advanced universal scripting example                | ⏳ |
| 12 | Visual programming blocks                            | ❌ |
| 18 | Expose SD card over USB (MSC)                        | ❌ |
| 20 | Keylogger addon                                      | ❌ |
| 25 | Mouse functionality                                 | ❌ |
| 26 | Silent OS detection                                 | ❌ |
| 27 | HID over Bluetooth                                  | ❌ |
| 31 | Chaining commands into universal conditions         | ❌ |

> Note vs. the original DevKitC roadmap: items 8, 9, 11, 13–17, 19, 21–24, 28,
> 29 and 32 — previously marked ⏳/❌ — are in fact implemented in the current
> code and are corrected to ✅ here. Item 24 (disable USB at boot) is delivered
> by this variant's **Silent Startup**.

## Build / flash

Arduino ESP32 core 3.x. Board: **ESP32S3 Dev Module**, with:

```
USB Mode:          USB-OTG (TinyUSB)
USB CDC On Boot:   Disabled
Flash Size:        8MB
Partition Scheme:  8M with spiffs (3MB APP/1.5MB SPIFFS)
PSRAM:             Disabled
```

FQBN:
```
esp32:esp32:esp32s3:USBMode=default,FlashSize=8M,CDCOnBoot=default,UploadMode=default,PartitionScheme=default_8MB,PSRAM=disabled
```

Requires a FAT32 microSD card inserted (firmware auto-creates
`/languages`, `/scripts`, `/logs`, `/uploads` on first boot).

Default AP: **ESP32-BadUSB** / `badusb123` → http://192.168.4.1
