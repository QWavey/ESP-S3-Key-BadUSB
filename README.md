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
