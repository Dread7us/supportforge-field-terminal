"""Convert an actual 960x540 packed 4-bit EPDiy framebuffer dump to PNG."""

import argparse
from pathlib import Path

from PIL import Image

WIDTH, HEIGHT, STRIDE = 960, 540, 480
FRAME_BYTES = STRIDE * HEIGHT


def convert(source: Path, destination: Path, portrait: bool):
    packed = source.read_bytes()
    if len(packed) != FRAME_BYTES:
        raise ValueError(f"expected {FRAME_BYTES} bytes, received {len(packed)}")
    image = Image.new("L", (WIDTH, HEIGHT))
    pixels = image.load()
    for y in range(HEIGHT):
        for byte_x in range(STRIDE):
            value = packed[y * STRIDE + byte_x]
            pixels[byte_x * 2, y] = (value & 0x0F) * 17
            pixels[byte_x * 2 + 1, y] = (value >> 4) * 17
    if portrait:
        # Firmware uses EPD_ROT_INVERTED_PORTRAIT: logical top-left maps to the
        # physical frame's bottom-left, so authoritative review rotates clockwise.
        image = image.rotate(-90, expand=True)
    image.save(destination)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--physical", action="store_true", help="keep physical 960x540 orientation")
    args = parser.parse_args()
    convert(args.source, args.destination, portrait=not args.physical)
    print(f"Converted authoritative packed framebuffer dump to {args.destination}")


if __name__ == "__main__":
    main()