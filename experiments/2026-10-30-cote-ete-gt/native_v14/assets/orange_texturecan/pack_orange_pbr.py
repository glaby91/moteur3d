#!/usr/bin/env python3
"""Pack the CC0 TextureCan orange maps into the engine's compact mip format.
Requires Pillow and NumPy. The native build consumes the generated .m3dpbr file.
"""
from pathlib import Path
import struct
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "orange_peel_512.m3dpbr"
SIZE = 512
LEVELS = 7


def rgb565(rgb):
    q = np.clip(np.rint(rgb), 0, 255).astype(np.uint16)
    return ((q[..., 0] >> 3) << 11) | ((q[..., 1] >> 2) << 5) | (q[..., 2] >> 3)


def half_mean(a):
    h, w = a.shape[:2]
    return a[:h - h % 2, :w - w % 2].reshape(h // 2, 2, w // 2, 2, *a.shape[2:]).mean(axis=(1, 3))


color = np.asarray(Image.open(ROOT / "food_0022_color_1k.jpg").convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32)
normal_image = Image.open(ROOT / "food_0022_normal_opengl_1k.png").convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
normal = np.asarray(normal_image, dtype=np.float32) / 127.5 - 1.0
normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
rough = np.asarray(Image.open(ROOT / "food_0022_roughness_1k.jpg").convert("L").resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32)
rough = np.repeat(rough[..., None], 3, axis=2)

with OUT.open("wb") as f:
    f.write(b"M3DPBR1\0")
    f.write(struct.pack("<II", SIZE, LEVELS))
    for level in range(LEVELS):
        size = SIZE >> level
        f.write(struct.pack("<I", size))
        f.write(rgb565(color).astype("<u2", copy=False).tobytes())
        f.write(rgb565((normal + 1.0) * 127.5).astype("<u2", copy=False).tobytes())
        f.write(rgb565(rough).astype("<u2", copy=False).tobytes())
        if level != LEVELS - 1:
            color = half_mean(color)
            normal = half_mean(normal)
            normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
            rough = half_mean(rough)
print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")
