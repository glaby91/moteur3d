#!/usr/bin/env python3
"""PHOTON-RT experimental 25%-budget adaptive sampling of CSE WARP renders.

This is an offline image-domain research prototype, NOT a ray-budget change
in the native CSE WARP renderer. Requires numpy, scipy, Pillow, scikit-image.
"""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt
from skimage.metrics import peak_signal_noise_ratio, structural_similarity


def reconstruct(rgb, sampled):
    _, indices = distance_transform_edt(~sampled, return_indices=True)
    return rgb[indices[0], indices[1]]


def compare(rgb, seed=100):
    h, w = rgb.shape[:2]
    y, x = np.indices((h, w))
    pilot = (y % 4 == 0) & (x % 4 == 0)
    uniform = (y % 2 == 0) & (x % 2 == 0)
    guide = reconstruct(rgb, pilot).astype(np.float32)
    luminance = guide @ np.array([.2126, .7152, .0722], dtype=np.float32)
    gy, gx = np.gradient(luminance)
    edge = np.hypot(gx, gy)
    p90 = max(float(np.percentile(edge, 90)), 1.0)
    importance = .5 + 3.0 * np.minimum(edge / p90, 2.0)
    available = np.flatnonzero(~pilot.ravel())
    remaining = int(uniform.sum()) - int(pilot.sum())
    probability = importance.ravel()[available].astype(np.float64)
    probability /= probability.sum()
    chosen = np.random.default_rng(seed).choice(
        available, size=remaining, replace=False, p=probability
    )
    adaptive = pilot.copy()
    adaptive.ravel()[chosen] = True
    results = {}
    for name, mask in [('uniform', uniform), ('adaptive', adaptive)]:
        output = reconstruct(rgb, mask)
        results[name] = {
            'image': output,
            'rays': int(mask.sum()),
            'psnr_db': float(peak_signal_noise_ratio(rgb, output, data_range=255)),
            'ssim': float(structural_similarity(rgb, output, channel_axis=2, data_range=255)),
            'mae': float(np.abs(rgb.astype(np.float32) - output).mean()),
        }
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('images', nargs='+', type=Path, help='Actual CSE WARP PNG renders')
    parser.add_argument('--output', type=Path, default=Path('photon-rt-results'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for i, path in enumerate(args.images):
        rgb = np.asarray(Image.open(path).convert('RGB'))
        results = compare(rgb, 100 + i)
        for method, result in results.items():
            Image.fromarray(result['image']).save(args.output / f'{path.stem}_{method}.png')
            rows.append({'view': path.name, 'method': method,
                         **{k: v for k, v in result.items() if k != 'image'}})
    with (args.output / 'metrics.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=['view', 'method', 'rays', 'psnr_db', 'ssim', 'mae'])
        writer.writeheader()
        writer.writerows(rows)
    report = {'images': len(args.images), 'sampling_budget': .25,
              'pilot_fraction': .0625, 'measurements': rows,
              'warning': 'Image-domain simulation; no native ray savings or FPS measured.'}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'images': len(args.images), 'output': str(args.output)}, indent=2))


if __name__ == '__main__':
    main()
