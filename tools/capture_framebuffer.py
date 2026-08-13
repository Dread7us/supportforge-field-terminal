"""Capture the firmware's authoritative packed 4-bpp composition buffer."""

import argparse
import re
import time
from pathlib import Path

import serial


FRAME_BYTES = 259_200


def capture(port: str, page: str, destination: Path, baud: int = 115_200) -> None:
    with serial.Serial(port, baudrate=baud, timeout=2, write_timeout=2) as device:
        time.sleep(2.0)
        device.reset_input_buffer()
        device.write(f"framebuffer dump {page}\n".encode("ascii"))
        device.flush()
        header = device.read_until(b"\n")
        deadline = time.monotonic() + 12
        while b"FRAMEBUFFER_DUMP_BEGIN" not in header:
            if time.monotonic() >= deadline:
                raise TimeoutError(f"dump header not received; last line={header!r}")
            header = device.read_until(b"\n")
        match = re.search(rb"bytes=(\d+)", header)
        if not match or int(match.group(1)) != FRAME_BYTES:
            raise ValueError(f"unexpected dump header: {header!r}")
        packed = device.read(FRAME_BYTES)
        while len(packed) < FRAME_BYTES and time.monotonic() < deadline:
            packed += device.read(FRAME_BYTES - len(packed))
        if len(packed) != FRAME_BYTES:
            raise IOError(f"expected {FRAME_BYTES} bytes, received {len(packed)}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(packed)
        print(f"Captured {len(packed)} authoritative bytes for {page.upper()} to {destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("page")
    parser.add_argument("destination", type=Path)
    parser.add_argument("--baud", type=int, default=115_200)
    args = parser.parse_args()
    capture(args.port, args.page, args.destination, args.baud)


if __name__ == "__main__":
    main()