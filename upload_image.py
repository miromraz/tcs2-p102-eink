#!/usr/bin/env python3
"""
Upload an image to the TCM-P102-220X e-ink display via Pico RP2040.

Usage: python3 upload_image.py <image_file> [serial_port]

Converts any image to 1-bit 1024x1280, wraps in EPD format,
and streams to the Pico over serial.
"""

import sys
import time
import struct

try:
    import serial
except ImportError:
    print("Install pyserial: pip install pyserial")
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install Pillow")
    sys.exit(1)

# Display specs
WIDTH = 1024
HEIGHT = 1280
EPD_HEADER_SIZE = 16
IMAGE_BYTES = (WIDTH * HEIGHT) // 8  # 163840
EPD_TOTAL = EPD_HEADER_SIZE + IMAGE_BYTES  # 163856
CHUNK_SIZE = 250


def image_to_epd(image_path, invert=False):
    """Convert an image file to EPD format bytes."""
    img = Image.open(image_path)

    # Rotate if landscape
    w, h = img.size
    if w > h:
        img = img.rotate(90, expand=True)

    # Resize to fit display, maintaining aspect ratio
    img.thumbnail((WIDTH, HEIGHT), Image.LANCZOS)

    # Create white canvas and paste centered
    canvas = Image.new('L', (WIDTH, HEIGHT), 255)
    x = (WIDTH - img.size[0]) // 2
    y = (HEIGHT - img.size[1]) // 2
    # Convert to grayscale before pasting
    img = img.convert('L')
    canvas.paste(img, (x, y))

    # Convert to 1-bit (dithering for better quality)
    bw = canvas.convert('1')

    # Invert if needed (some images need this)
    from PIL import ImageOps
    if invert:
        bw = ImageOps.invert(bw.convert('L')).convert('1')

    # Build EPD header
    header = bytearray(EPD_HEADER_SIZE)
    header[0] = 0x3D       # panel type: 10.2"
    header[1] = 0x04        # X res high byte (1024 = 0x0400)
    header[2] = 0x00        # X res low byte
    header[3] = 0x05        # Y res high byte (1280 = 0x0500)
    header[4] = 0x00        # Y res low byte
    header[5] = 0x01        # color depth: 1-bit
    header[6] = 0x00        # pixel data format: type 0

    # Convert pixel data to EPD format type 0
    # Each byte = 8 pixels, bit 7 = first pixel
    # 1 = white, 0 = black
    pixels = bw.load()
    image_data = bytearray(IMAGE_BYTES)

    byte_idx = 0
    for y_pos in range(HEIGHT):
        for x_pos in range(0, WIDTH, 8):
            byte_val = 0
            for bit in range(8):
                if x_pos + bit < WIDTH:
                    pixel = pixels[x_pos + bit, y_pos]
                    if pixel:  # white = 1
                        byte_val |= (0x80 >> bit)
            image_data[byte_idx] = byte_val
            byte_idx += 1

    return bytes(header) + bytes(image_data)


def upload_to_display(epd_data, port='/dev/ttyACM0', baudrate=115200):
    """Stream EPD data to the Pico over serial."""
    print(f"Opening {port}...")
    ser = serial.Serial(port, baudrate, timeout=5)
    time.sleep(0.5)

    # Drain any pending data
    ser.reset_input_buffer()

    # Wait for EINK_READY or send PING
    ser.write(b'PING\n')
    time.sleep(0.5)
    response = ser.readline().decode('utf-8', errors='replace').strip()
    if 'PONG' not in response and 'READY' not in response:
        # Try reading more
        for _ in range(5):
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if 'PONG' in line or 'READY' in line:
                response = line
                break
        if 'PONG' not in response and 'READY' not in response:
            print(f"Unexpected response: '{response}'")
            print("Is the Pico running the image receiver firmware?")
            ser.close()
            return False

    print("Pico connected.")

    # Send IMG command
    ser.write(b'IMG\n')
    response = ser.readline().decode('utf-8', errors='replace').strip()
    if response != 'READY':
        print(f"Expected READY, got: '{response}'")
        ser.close()
        return False

    print(f"Uploading {len(epd_data)} bytes...")

    # Send data in chunks
    offset = 0
    chunk_num = 0
    total_chunks = (len(epd_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
    start_time = time.time()

    while offset < len(epd_data):
        remaining = len(epd_data) - offset
        chunk_size = min(CHUNK_SIZE, remaining)
        chunk = epd_data[offset:offset + chunk_size]

        ser.write(chunk)
        ser.flush()

        # Wait for OK
        response = ser.readline().decode('utf-8', errors='replace').strip()
        if response != 'OK':
            print(f"\nError at chunk {chunk_num}: '{response}'")
            ser.close()
            return False

        offset += chunk_size
        chunk_num += 1
        pct = (offset * 100) // len(epd_data)
        elapsed = time.time() - start_time
        print(f"\r  [{pct:3d}%] {offset}/{len(epd_data)} bytes ({elapsed:.1f}s)", end='')

    print()

    # Wait for REFRESH
    response = ser.readline().decode('utf-8', errors='replace').strip()
    if response == 'REFRESH':
        print("Display refreshing...")
    else:
        print(f"Expected REFRESH, got: '{response}'")

    # Wait for DONE (can take up to 10 seconds)
    response = ser.readline().decode('utf-8', errors='replace').strip()
    if response == 'DONE':
        elapsed = time.time() - start_time
        print(f"Done! Total time: {elapsed:.1f}s")
    else:
        print(f"Expected DONE, got: '{response}'")

    ser.close()
    return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 upload_image.py <image_file> [--invert] [serial_port]")
        print("  Supports: PNG, JPG, BMP, GIF, etc.")
        print(f"  Display: {WIDTH}x{HEIGHT} 1-bit monochrome")
        print("  --invert: Invert colors if they appear wrong")
        sys.exit(1)

    args = sys.argv[1:]
    invert = '--invert' in args
    if invert:
        args.remove('--invert')

    image_path = args[0]
    port = args[1] if len(args) > 1 else '/dev/ttyACM0'

    print(f"Converting {image_path}..." + (" (inverted)" if invert else ""))
    epd_data = image_to_epd(image_path, invert=invert)
    print(f"EPD data: {len(epd_data)} bytes")

    upload_to_display(epd_data, port)


if __name__ == '__main__':
    main()
