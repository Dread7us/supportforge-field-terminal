"""Measure glyph values in authoritative packed framebuffer dumps."""

import argparse
import json
from collections import Counter
from pathlib import Path

from PIL import Image

from framebuffer_to_png import convert


ROOT = Path(__file__).parents[1]


def nibble_histogram(image: Image.Image, bounds: tuple[int, int, int, int],
                     background_nibble: int = 15) -> dict:
    x, y, width, height = bounds
    values = [image.getpixel((px, py)) // 17 for py in range(y, y + height)
              for px in range(x, x + width)]
    ink = [value for value in values if value != background_nibble]
    counts = Counter(ink)
    pure_black = counts[0]
    dark_gray = sum(counts[value] for value in range(1, 5))
    medium_light_gray = sum(counts[value] for value in range(5, 15))
    return {
        "bounds": [x, y, width, height],
        "pure_black": pure_black,
        "dark_gray": dark_gray,
        "medium_light_gray": medium_light_gray,
        "glyph_ink_pixels": len(ink),
        "pure_black_percent": round(100 * pure_black / len(ink), 2) if ink else 0.0,
        "background_nibble": background_nibble,
        "foreground_core_nibble": 15 if background_nibble == 0 else 0,
        "foreground_core_pixels": counts[15 if background_nibble == 0 else 0],
        "foreground_core_percent": round(
            100 * counts[15 if background_nibble == 0 else 0] / len(ink), 2
        ) if ink else 0.0,
        "minimum_nibble": min(ink) if ink else 15,
        "maximum_nibble": max(ink) if ink else 15,
        "nibble_histogram": {str(value): counts[value] for value in range(15) if counts[value]},
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    images = {}
    results = {}
    for label, entry in manifest.items():
        source = ROOT / entry["dump"]
        if source not in images:
            converted = source.with_suffix(".analysis.png")
            convert(source, converted, portrait=True)
            images[source] = Image.open(converted)
        results[label] = nibble_histogram(
            images[source], tuple(entry["bounds"]), entry.get("background_nibble", 15)
        )
        results[label]["dump"] = entry["dump"]
    args.output.write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()