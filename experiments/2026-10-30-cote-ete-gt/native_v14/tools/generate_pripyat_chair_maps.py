#!/usr/bin/env python3
"""Make seamless 2048², non-photographic PBR maps for outdoor chair materials."""
from pathlib import Path
import random
from PIL import Image, ImageChops, ImageFilter, ImageOps

OUT = Path(__file__).resolve().parents[1] / "assets/pripyat/textures/outdoor_chairs"
SIZE = 2048
OUT.mkdir(parents=True, exist_ok=True)

def make_maps(name, base, metal, rough, seed, grain="paint"):
    random.seed(seed)
    fine = Image.effect_noise((SIZE, SIZE), 38).convert("L")
    broad = Image.effect_noise((SIZE, SIZE), 22).convert("L").filter(ImageFilter.GaussianBlur(5))
    height = Image.blend(fine, broad, .35)
    if grain == "wood":
        lines = Image.effect_noise((SIZE, SIZE), 18).convert("L").filter(ImageFilter.GaussianBlur((0.7 if False else 1)))
        height = Image.blend(height, lines, .32)
    tint = ImageOps.colorize(height, tuple(max(0,c-13) for c in base), tuple(min(255,c+13) for c in base))
    # sparse chips on paint reveal dark rust; keep wear subtle at scene scale
    if grain == "paint":
        chip = Image.effect_noise((SIZE, SIZE), 62).convert("L").point(lambda p: 255 if p > 242 else 0).filter(ImageFilter.GaussianBlur(.6))
        rust = Image.new("RGB", (SIZE,SIZE), (94,61,38))
        tint = Image.composite(rust, tint, chip)
    gx = height.filter(ImageFilter.Kernel((3,3), [-1,0,1,-2,0,2,-1,0,1], scale=8, offset=128))
    gy = height.filter(ImageFilter.Kernel((3,3), [-1,-2,-1,0,0,0,1,2,1], scale=8, offset=128))
    normal = Image.merge("RGB", (gx, gy, Image.new("L", (SIZE,SIZE), 255)))
    metal_map = Image.new("L", (SIZE,SIZE), metal)
    rough_map = ImageChops.add(Image.new("L", (SIZE,SIZE), rough), fine.point(lambda p: (p-128)//18), scale=1, offset=0)
    mr = Image.merge("RGB", (metal_map, rough_map, Image.new("L", (SIZE,SIZE), 255)))
    tint.save(OUT / f"{name}_basecolor.png", optimize=True)
    normal.save(OUT / f"{name}_normal.png", optimize=True)
    mr.save(OUT / f"{name}_metallic_roughness.png", optimize=True)

make_maps("chair_painted_steel", (71,91,72), 65, 186, 51, "paint")
make_maps("chair_dark_steel", (62,54,43), 165, 133, 57, "paint")
make_maps("chair_weathered_wood", (101,76,49), 0, 214, 73, "wood")
