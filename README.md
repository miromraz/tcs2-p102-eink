# TCS2-P102 E-Ink Display Test

Getting a 10.2" 1024x1280 monochrome e-ink display working with the MpicoSys TCS2-P102-231 timing controller, using a Raspberry Pi Pico.

## Hardware

- **MCU**: Raspberry Pi Pico (RP2040)
- **Display**: EZ102CT011 (10.2", 1024x1280, 1-bit B&W, Pervasive Displays / E Ink Vizplex)
- **Controller**: TCS2-P102-231 v1.1 (MpicoSys Generation 2 timing controller)
- **Interface**: SPI Mode 3, 1 MHz

## Wiring

Connect the Pico to the TCS2 controller board's 10-pin header:

| Pico Pin | GPIO | TCS2 Pin | Function |
|----------|------|----------|----------|
| Pin 14 | GPIO10 | SCK | SPI1 Clock |
| Pin 15 | GPIO11 | MOSI | SPI1 TX |
| Pin 16 | GPIO12 | MISO | SPI1 RX |
| Pin 17 | GPIO13 | CS | Chip Select (active LOW) |
| Pin 21 | GPIO16 | BUSY | Controller Status (HIGH = ready) |
| Pin 22 | GPIO17 | EN | Enable (active LOW) |
| 3V3 OUT | - | VDDIN | 3.3V Digital Power |
| GND | - | GND | Ground |

The TCS2 controller also needs analog power (VIN, 2.0-5.5V) which can share the same 3.3V supply or use a separate source.

## Flashing the Pico

The test firmware is in `pico_eink_test/pico_eink_test.ino`. Flash it using Arduino IDE:

1. Install the [Arduino-Pico](https://github.com/earlephilhower/arduino-pico) board package
2. Select **Raspberry Pi Pico** as the board
3. Open `pico_eink_test/pico_eink_test.ino`
4. Upload (hold BOOTSEL on the Pico if it's the first flash)

Open the serial monitor at **115200 baud**. You should see `EINK_READY` once the controller initializes.

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

## Troubleshooting

- **SPI errors (response not 0x9000)**: Check wiring, especially CS and BUSY. Verify 3.3V power to TCS2.
- **TIMEOUT during upload**: Reduce serial baud rate or add flow control. Check USB cable is data-capable.
- **Display doesn't refresh**: The BUSY pin must be connected. The controller signals readiness through this pin.
- **All black / all white**: Try `--invert` flag with `upload_image.py`. Ensure the EPD header matches your panel (0x3D = 10.2").

## Next Steps

See [DASHBOARD.md](DASHBOARD.md) for the ESP32-S3 port and Victron/weather dashboard roadmap.
