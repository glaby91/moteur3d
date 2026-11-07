#!/usr/bin/env python3
"""Pack the generated Energetik mosaic albedo into the native M3D PBR format."""

from __future__ import annotations

import gzip
import struct
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/pripyat/textures/energetik_fresco_mosaic.jpg"
OUTPUT = ROOT / "assets/pripyat/textures/energetik_fresco_mosaic.m3dpbr.gz"
SIZE = 2048


def pack565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def make_square_atlas(source: Image.Image) -> Image.Image:
    image = source.convert("RGB")
    width, height = image.size
    if width != SIZE or width / height < 1.8 or width / height > 2.6:
        raise ValueError(f"Unexpected mural dimensions: {width}x{height}")
    top = (SIZE - height) // 2
    bottom = SIZE - height - top
    atlas = Image.new("RGB", (SIZE, SIZE))
    atlas.paste(image.crop((0, 0, width, 1)).resize((SIZE, top)), (0, 0))
    atlas.paste(image, (0, top))
    atlas.paste(image.crop((0, height - 1, width, height)).resize((SIZE, bottom)), (0, top + height))
    return atlas


def color_words(image: Image.Image) -> bytes:
    red, green, blue = image.convert("RGB").split()
    zero = Image.new("L", image.size, 0)
    # M3DPBR2 stores RGB888 in a little-endian uint32: [B,G,R,0].
    return Image.merge("RGBA", (blue, green, red, zero)).tobytes()


def main() -> None:
    if not SOURCE.is_file():
        raise FileNotFoundError(SOURCE)
    atlas = make_square_atlas(Image.open(SOURCE))
    count = 9  # 2048, 1024, …, 8; the native reader extends the tail to 1x1.
    normal_word = pack565(128, 128, 255)
    roughness_word = pack565(184, 184, 184)
    payload = bytearray(b"M3DPBR2\0")
    payload.extend(struct.pack("<II", SIZE, count))
    current = atlas
    for _ in range(count):
        size = current.width
        payload.extend(struct.pack("<I", size))
        payload.extend(color_words(current))
        payload.extend(struct.pack("<H", normal_word) * (size * size))
        payload.extend(struct.pack("<H", roughness_word) * (size * size))
        if size > 8:
            current = current.resize((size // 2, size // 2), Image.Resampling.LANCZOS)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", compresslevel=9, mtime=0) as compressed:
            compressed.write(payload)
    print(f"Wrote {OUTPUT} ({OUTPUT.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
