#!/usr/bin/env python3
"""Rebuild the native RGB565 base-colour mip chains from the bundled CC0 GLB."""
import io
import json
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).parent
GLB = ROOT / "1970s-coupe.glb"
DATA = GLB.read_bytes()


def chunks():
    offset = 12
    while offset < len(DATA):
        size, kind = struct.unpack_from("<II", DATA, offset)
        offset += 8
        yield kind, DATA[offset:offset + size]
        offset += size


json_chunk, bin_chunk = None, None
for kind, payload in chunks():
    if kind == 0x4E4F534A:
        json_chunk = json.loads(payload.decode("utf-8").rstrip("\0 "))
    elif kind == 0x004E4942:
        bin_chunk = payload
if json_chunk is None or bin_chunk is None:
    raise SystemExit("Invalid GLB 2.0 file")
views = json_chunk["bufferViews"]


def image_by_name(name):
    for item in json_chunk["images"]:
        if item.get("name") == name:
            view = views[item["bufferView"]]
            start = view.get("byteOffset", 0)
            return Image.open(io.BytesIO(bin_chunk[start:start + view["byteLength"]])).convert("RGB")
    raise KeyError(name)


def write_mips(image, output):
    image = image.resize((512, 512), Image.Resampling.LANCZOS)
    result = bytearray()
    size = 512
    while size >= 8:
        if image.size != (size, size):
            image = image.resize((size, size), Image.Resampling.LANCZOS)
        pixels = image.tobytes()
        for i in range(0, len(pixels), 3):
            r, g, b = pixels[i:i + 3]
            value = ((r * 31 + 127) // 255 << 11) | ((g * 63 + 127) // 255 << 5) | ((b * 31 + 127) // 255)
            result.extend(struct.pack("<H", value))
        size //= 2
        if size >= 8:
            image = image.resize((size, size), Image.Resampling.LANCZOS)
    output.write_bytes(result)


write_mips(image_by_name("1970s-coupe_body_basecolor"), ROOT / "classic_body.rgb565mip")
write_mips(image_by_name("1970s-coupe_tire_color"), ROOT / "classic_tire.rgb565mip")
print("Wrote body and tire RGB565 mip chains.")
