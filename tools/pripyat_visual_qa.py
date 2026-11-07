#!/usr/bin/env python3
"""Automated Pripyat snapshot health report, with a contact sheet.

Not an aesthetic-quality classifier. Requires Pillow. Exit nonzero on missing,
blank or invalid views; save measured exposure, sharpness proxy and clipping.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageStat, ImageFilter, ImageOps, ImageDraw

def analyze(path):
    with Image.open(path) as src:
        image = src.convert("RGB")
    if image.size != (640, 360):
        raise ValueError(f"{path}: unexpected resolution {image.size}")
    gray = ImageOps.grayscale(image)
    stat = ImageStat.Stat(image)
    gs = ImageStat.Stat(gray)
    edges = gray.filter(ImageFilter.FIND_EDGES)
    edge_std = ImageStat.Stat(edges).stddev[0]
    hist = gray.histogram()
    n = image.width * image.height
    return image, {
        "file": path.name,
        "mean_rgb": [round(v, 2) for v in stat.mean],
        "stddev_rgb": [round(v, 2) for v in stat.stddev],
        "luminance_mean": round(gs.mean[0], 2),
        "edge_stddev_proxy": round(edge_std, 2),
        "black_fraction_luma_le_4": round(sum(hist[:5]) / n, 5),
        "white_fraction_luma_ge_250": round(sum(hist[250:]) / n, 5),
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("directory", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--contact-sheet", type=Path, required=True)
    a = p.parse_args()
    images = sorted(a.directory.glob("view-*.png"))
    if len(images) != 13:
        raise SystemExit(f"Expected 13 views, found {len(images)}")
    results = []
    sheet = Image.new("RGB", (640 * 3, 390 * 5), (20, 23, 28))
    draw = ImageDraw.Draw(sheet)
    for idx, path in enumerate(images):
        img, metrics = analyze(path)
        if max(metrics["stddev_rgb"]) <= 2.0:
            raise SystemExit(f"Blank or near-uniform view: {path}")
        results.append(metrics)
        x, y = idx % 3 * 640, idx // 3 * 390
        sheet.paste(img, (x, y))
        draw.text((x + 12, y + 365), path.name, fill="white")
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.contact_sheet.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps({
        "method": "13 fixed Pripyat CPU snapshots; objective diagnostics, not artistic score",
        "view_count": len(results), "views": results,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    sheet.save(a.contact_sheet)
    print(f"Visual QA: {len(results)} valid images; report: {a.output}; contact sheet: {a.contact_sheet}")

if __name__ == "__main__":
    main()
