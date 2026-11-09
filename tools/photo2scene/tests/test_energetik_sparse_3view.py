"""Synthetic calibrated 3-camera regression: no user video required by CI."""
import importlib.util
import math
from pathlib import Path

import cv2
import numpy as np

SCRIPT = Path(__file__).resolve().parents[1] / "sparse_drone_3view.py"
spec = importlib.util.spec_from_file_location("sfm", SCRIPT)
sfm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sfm)


def test_triangulation_and_third_view_holdout():
    rng = np.random.default_rng(20261110)
    k = np.array([[550, 0, 320], [0, 550, 180], [0, 0, 1]], dtype=float)
    points = np.column_stack([
        rng.uniform(-2.5, 2.5, 280),
        rng.uniform(-1.4, 1.4, 280),
        rng.uniform(6.0, 14.0, 280)
    ])
    def project(xyz, yaw, translation):
        R, _ = cv2.Rodrigues(np.array([0., yaw, 0.]))
        trans = np.array(translation)
        camera = (xyz @ R.T) + trans
        pix = (camera @ k.T)
        return pix[:, :2] / pix[:, 2, None]

    p0 = project(points, 0., [0, 0, 0])
    p1 = project(points, .016, [.28, -.02, 0])
    p2 = project(points, .032, [.56, -.04, .02])
    valid = np.logical_and.reduce([
        (p[:, 0] > 8) & (p[:, 0] < 632) &
        (p[:, 1] > 8) & (p[:, 1] < 352) for p in [p0, p1, p2]
    ])
    p0, p1, p2 = [p[valid] for p in [p0, p1, p2]]
    n = len(p0)
    assert n >= 150
    p0 += rng.normal(0, .09, p0.shape)
    p1 += rng.normal(0, .09, p1.shape)
    p2 += rng.normal(0, .09, p2.shape)
    kp = [[cv2.KeyPoint(float(x), float(y), 6) for x, y in p]
          for p in [p0, p1, p2]]
    base = rng.normal(0, 1, (n, 128)).astype(np.float32)
    descriptors = [base + rng.normal(0, .0003, base.shape).astype(np.float32)
                   for _ in range(3)]
    result = sfm.triangulate(
        kp[0], kp[1], kp[2], *descriptors, 640, 360, 550)
    metrics = result["metrics"]
    assert metrics["raw_sift_matches"] > 140, metrics
    assert metrics["triangulated_points"] > 125, metrics
    assert metrics["holdout_points"] > 20, metrics
    assert metrics["holdout_rms_px_all"] < 1.5, metrics
    assert metrics["meters_per_unit"] is None
    assert metrics["scene_geometry_modified"] is False
    print("Synthetic three-view reprojection test PASS:", metrics)


if __name__ == "__main__":
    test_triangulation_and_third_view_holdout()
