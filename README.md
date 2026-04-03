# TCS2-P102 E-Ink Display Driver

ESP32-S3 firmware for driving a 10.2" 1024x1280 monochrome e-ink display via the MpicoSys TCS2-P102-231 timing controller.

## Hardware

- **MCU**: Seeed Studio XIAO ESP32-S3 Sense
- **Display**: EZ102CT011 (10.2", 1024x1280, 1-bit B&W, Pervasive Displays / E Ink Vizplex)
- **Controller**: TCS2-P102-231 v1.1 (MpicoSys Generation 2 timing controller)
- **Interface**: SPI Mode 3, 1 MHz

## Wiring

Connect the XIAO ESP32-S3 to the TCS2 controller board's 10-pin header:

| XIAO Pin | GPIO | TCS2 Pin | Function |
|----------|------|----------|----------|
| D8 | GPIO7 | SCK | SPI Clock |
| D10 | GPIO9 | MOSI | SPI Data Out |
| D9 | GPIO8 | MISO | SPI Data In |
| D0 | GPIO1 | CS | Chip Select (active LOW) |
| D1 | GPIO2 | BUSY | Controller Status (HIGH = ready) |
| D2 | GPIO3 | EN | Enable (active LOW) |
| 3V3 | - | VDDIN | 3.3V Digital Power |
| GND | - | GND | Ground |

The TCS2 controller also needs analog power (VIN, 2.0-5.5V) which can share the same 3.3V supply or use a separate source.

## Building and Flashing

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Flash (connect XIAO via USB)
pio run -t upload

# If the board isn't detected, enter bootloader mode:
# Hold BOOT, press RESET, release BOOT
# Then retry upload

# Monitor serial output
pio device monitor
```

The serial port will typically be `/dev/ttyACM0` on Linux (USB CDC).

## What It Does

On boot the firmware:

1. Initializes the TCS2 controller over SPI
2. Queries and prints device info to serial (verifies SPI communication works)
3. Draws a checkerboard test pattern on the display
4. Prints `EINK_READY` and waits for serial commands

## Uploading Images

Use `upload_image.py` to send images from your PC:

```bash
pip install pyserial Pillow

python3 upload_image.py <image_file> [--invert] [serial_port]

# Examples:
python3 upload_image.py photo.jpg
python3 upload_image.py photo.jpg /dev/ttyACM0
python3 upload_image.py photo.jpg --invert    # flip black/white
```

The script converts any image (PNG, JPG, BMP, etc.) to 1-bit dithered 1024x1280, wraps it in the EPD format, and streams it to the display over serial.

### Serial Protocol

| PC Sends | MCU Responds | Description |
|----------|-------------|-------------|
| `PING\n` | `PONG\n` | Connection check |
| `IMG\n` | `READY\n` | Start image upload |
| 250 bytes | `OK\n` | Per-chunk acknowledgement |
| (after all data) | `REFRESH\n` | Display update started |
| (wait ~5s) | `DONE\n` | Refresh complete |

Total upload: 163,856 bytes (16-byte EPD header + 163,840 image bytes).

## Library

Uses the [Arduino-TCM2](https://github.com/oxullo/Arduino-TCM2) library (included in `lib/`) with one ESP32 compatibility fix (`#include <Arduino.h>` added to `TCM2.cpp`).

## Troubleshooting

- **Board not detected**: Enter bootloader mode (BOOT + RESET). Check `lsusb` for Espressif device.
- **SPI errors (0x6F00)**: Check wiring, especially CS and BUSY. Verify 3.3V power to TCS2.
- **No serial output**: The XIAO ESP32-S3 uses USB CDC. Give it 2 seconds after reset before opening the monitor.
- **Display doesn't refresh**: The BUSY pin must be connected. The controller signals readiness through this pin.
