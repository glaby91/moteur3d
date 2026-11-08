#!/usr/bin/env python3
"""Compare two actual Moteur3D PNG captures, same camera and resolution.

Usage: python tools/compare_visual_captures.py before.png after.png
Optional: --diff difference.png --json metrics.json
Requires Pillow. No fabricated quality score: these are objective image-change
metrics, not an assessment of artistic beauty.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

def compare(before: Path, after: Path):
    for capture in (before, after):
        if not capture.is_file() or capture.stat().st_size == 0:
            raise FileNotFoundError(f"Missing or empty capture: {capture}")
    with Image.open(before) as im:
        a = im.convert("RGB")
    with Image.open(after) as im:
        b = im.convert("RGB")
    if a.size != b.size:
        raise ValueError(f"Resolution mismatch: {a.size} vs {b.size}")
    diff = ImageChops.difference(a, b)
    stat = ImageStat.Stat(diff)
    # ImageStat computes the RMS independently for each RGB channel.
    # Iterating a flattened histogram here incorrectly treated bin counts as lists.
    rms = stat.rms
    result = {
        "before": str(before), "after": str(after),
        "resolution": list(a.size),
        "mean_absolute_difference_rgb": [round(x, 4) for x in stat.mean],
        "rms_difference_rgb": [round(x, 4) for x in rms],
        "changed_pixel_fraction": round(
            sum(1 for p in diff.getdata() if p != (0, 0, 0)) / (a.width * a.height), 6),
    }
    return result, diff

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("before", type=Path)
    p.add_argument("after", type=Path)
    p.add_argument("--diff", type=Path)
    p.add_argument("--json", type=Path)
    args = p.parse_args()
    metrics, diff = compare(args.before, args.after)
    if args.diff:
        diff.save(args.diff)
    report = json.dumps(metrics, indent=2, ensure_ascii=False)
    if args.json:
        args.json.write_text(report + "\n", encoding="utf-8")
    print(report)

if __name__ == "__main__":
    main()
