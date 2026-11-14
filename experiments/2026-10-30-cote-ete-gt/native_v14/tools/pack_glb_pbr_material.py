#!/usr/bin/env python3
"""Pack one glTF material's base-color, normal and roughness maps as M3DPBR2."""
from __future__ import annotations

import gzip
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image


SIZE = 2048


def image(path: str, kind: str) -> np.ndarray:
    if path == "-":
        fill = (255, 255, 255) if kind == "color" else ((128, 128, 255) if kind == "normal" else (220, 220, 220))
        return np.broadcast_to(np.array(fill, dtype=np.uint8), (SIZE, SIZE, 3)).copy()
    with Image.open(path) as src:
        if kind == "roughness":
            src = src.convert("RGB")
            src = src.getchannel("G")
            src = src.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
            plane = np.asarray(src, dtype=np.uint8)
            return np.repeat(plane[..., None], 3, axis=2)
        src = src.convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
        return np.asarray(src, dtype=np.uint8).copy()


def pack565(rgb: np.ndarray) -> np.ndarray:
    rgb = rgb.astype(np.uint16)
    return ((rgb[..., 0] >> 3) << 11) | ((rgb[..., 1] >> 2) << 5) | (rgb[..., 2] >> 3)


def pack_rgb888(rgb: np.ndarray) -> np.ndarray:
    rgb = rgb.astype(np.uint32)
    return (rgb[..., 0] << 16) | (rgb[..., 1] << 8) | rgb[..., 2]


def next_mip(a: np.ndarray, kind: str) -> np.ndarray:
    p = a.astype(np.float32).reshape(a.shape[0] // 2, 2, a.shape[1] // 2, 2, 3).mean(axis=(1, 3))
    if kind == "normal":
        p = p / 127.5 - 1.0
        p /= np.maximum(np.linalg.norm(p, axis=2, keepdims=True), 1e-8)
        p = (p + 1.0) * 127.5
    return np.clip(np.rint(p), 0, 255).astype(np.uint8)


def main() -> None:
    if len(sys.argv) != 5:
        raise SystemExit("usage: pack_glb_pbr_material.py BASECOLOR NORMAL METALLIC_ROUGHNESS OUT.m3dpbr.gz")
    maps = {name: image(path, name) for name, path in zip(("color", "normal", "roughness"), sys.argv[1:4])}
    levels = []
    size = SIZE
    while True:
        levels.append((size, [pack_rgb888(maps["color"]), pack565(maps["normal"]), pack565(maps["roughness"])]))
        if size == 1:
            break
        maps = {k: next_mip(v, k) for k, v in maps.items()}
        size //= 2
    raw = bytearray(b"M3DPBR2\0") + bytearray(struct.pack("<II", SIZE, len(levels)))
    for size, channels in levels:
        raw.extend(struct.pack("<I", size))
        for index, channel in enumerate(channels):
            raw.extend(channel.astype("<u4" if index == 0 else "<u2", copy=False).tobytes())
    out = Path(sys.argv[4])
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as f:
        with gzip.GzipFile(filename="", mode="wb", fileobj=f, compresslevel=6, mtime=0) as gz:
            gz.write(raw)


if __name__ == "__main__":
    main()
