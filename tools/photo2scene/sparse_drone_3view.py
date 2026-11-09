#!/usr/bin/env python3
"""Sparse 3-view Photo2Scene reconstruction — NOT a metric Energetik survey.

Example:
python tools/photo2scene/sparse_drone_3view.py --video local_drone.mp4 \
  --roi tools/photo2scene/data/pripyat/energetik-drone-3view-rois.json \
  --out /tmp/energetik-3view --focal 550

The 3D gauge is arbitrary (camera baseline=1). User-owned footage stays local.
"""
import argparse
import json
import math
from pathlib import Path

import cv2
import numpy as np

cv2.setRNGSeed(20261110)


def extract_frame(cap, time_s):
    cap.set(cv2.CAP_PROP_POS_MSEC, time_s * 1000)
    ok, frame = cap.read()
    if not ok:
        raise ValueError(f"Cannot read frame at {time_s} s")
    return frame


def detect(frame, polygons):
    mask = np.zeros(frame.shape[:2], np.uint8)
    for polygon in polygons:
        cv2.fillPoly(mask, [np.asarray(polygon, np.int32)], 255)
    detector = cv2.SIFT_create(nfeatures=5000, contrastThreshold=.012, edgeThreshold=13)
    return detector.detectAndCompute(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY), mask)


def pairs(kpa, da, kpb, db, ratio=.77):
    bf = cv2.BFMatcher(cv2.NORM_L2)
    good = sorted((a for a, b in bf.knnMatch(da, db, k=2)
                   if a.distance < ratio * b.distance), key=lambda v: v.distance)
    xy1 = np.float64([kpa[m.queryIdx].pt for m in good])
    xy2 = np.float64([kpb[m.trainIdx].pt for m in good])
    return good, xy1, xy2


def triangulate(k0, k1, k2, f0, f1, f2, width, height, focal):
    match02, px0, px2 = pairs(k0, f0, k2, f2)
    if len(match02) < 30:
        raise ValueError("Too few two-view matches")
    H, hom = cv2.findHomography(px0, px2, cv2.RANSAC, 2.5, maxIters=12000)
    K = np.array([[focal, 0, width / 2], [0, focal, height / 2], [0, 0, 1.]], dtype=float)
    E, e_mask = cv2.findEssentialMat(px0, px2, K, cv2.RANSAC, .999, 1.5,
                                    maxIters=12000)
    if E is None:
        raise ValueError("Essential matrix could not be estimated")
    if E.shape[0] > 3:
        E = E[:3, :3]
    valid, R, t, mask = cv2.recoverPose(E, px0, px2, K, mask=e_mask)
    keep = mask.ravel().astype(bool)
    a, b = px0[keep], px2[keep]
    accepted = [match02[i] for i in np.flatnonzero(keep)]
    P0 = K @ np.c_[np.eye(3), np.zeros((3, 1))]
    P2 = K @ np.c_[R, t]
    homogeneous = cv2.triangulatePoints(P0, P2, a.T, b.T)
    X = (homogeneous[:3] / homogeneous[3]).T
    xcam2 = X @ R.T + t.T

    bearings0 = cv2.undistortPoints(a.reshape(-1, 1, 2).astype(np.float32), K, None)
    bearings2 = cv2.undistortPoints(b.reshape(-1, 1, 2).astype(np.float32), K, None)
    r0 = np.c_[bearings0.reshape(-1, 2), np.ones(len(a))]
    r2 = np.c_[bearings2.reshape(-1, 2), np.ones(len(a))] @ R
    r0 /= np.linalg.norm(r0, axis=1, keepdims=True)
    r2 /= np.linalg.norm(r2, axis=1, keepdims=True)
    angles = np.degrees(np.arccos(np.clip((r0 * r2).sum(axis=1), -1, 1)))
    good = (X[:, 2] > 0) & (xcam2[:, 2] > 0) & np.isfinite(X).all(axis=1) & (angles >= .8)
    good &= (angles <= 45)
    if sum(good) < 30:
        raise ValueError("Insufficient positive-depth 3D points with parallax")

    match01, _, _ = pairs(k0, f0, k1, f1)
    reverse, _, _ = pairs(k1, f1, k0, f0, ratio=.75)
    rev_index = {m.queryIdx: m.trainIdx for m in reverse}
    join = {m.queryIdx: m.trainIdx for m in match01
            if rev_index.get(m.trainIdx) == m.queryIdx}
    triples = [(i, k1[join[m.queryIdx]].pt) for i, m in enumerate(accepted)
               if good[i] and m.queryIdx in join]
    rng = np.random.default_rng(20261110)
    indexes = rng.permutation(len(triples))
    train = [triples[i] for i in indexes[:int(.75 * len(indexes))]]
    holdout = [triples[i] for i in indexes[int(.75 * len(indexes)):]]
    if len(train) < 10 or len(holdout) < 5:
        raise ValueError("Too few triple-view matches for independent PnP test")
    fit_xyz = np.float64([X[i] for i, _ in train])
    fit_xy = np.float64([pix for _, pix in train])
    ok, rvec, tvec, inliers = cv2.solvePnPRansac(
        fit_xyz, fit_xy, K, None, flags=cv2.SOLVEPNP_EPNP,
        reprojectionError=3.5, iterationsCount=1500, confidence=.999)
    if not ok:
        raise ValueError("Independent third-camera PnP failed")
    test_xyz = np.float64([X[i] for i, _ in holdout])
    test_xy = np.float64([p for _, p in holdout])
    estimated = cv2.projectPoints(test_xyz, rvec, tvec, K, None)[0].reshape(-1, 2)
    residuals = np.linalg.norm(estimated - test_xy, axis=1)
    return {
        "xyz": X[good], "pixel0": a[good], "pixel2": b[good],
        "angles": angles[good],
        "metrics": {
            "status": "NONMETRIC_SPARSE_CLOUD_NOT_ARCHITECTURAL_SURVEY",
            "image_size_px": [width, height],
            "baseline_units": 1.0, "meters_per_unit": None,
            "intrinsics": "ASSUMED_PINHOLE_NO_LENS_DISTORTION",
            "assumed_focal_px": focal,
            "assumed_horizontal_fov_deg": round(math.degrees(2 * math.atan(width / (2 * focal))), 3),
            "raw_sift_matches": len(match02),
            "homography_inliers": int(hom.sum()),
            "essential_and_cheirality_inliers": int(sum(keep)),
            "triangulated_points": int(sum(good)),
            "median_parallax_deg": round(float(np.median(angles[good])), 3),
            "intermediate_triples": len(triples),
            "holdout_points": len(holdout),
            "holdout_rms_px_all": round(float(np.sqrt(np.mean(residuals ** 2))), 3),
            "holdout_median_px": round(float(np.median(residuals)), 3),
            "holdout_under_5px": int(sum(residuals < 5)),
            "holdout_fraction_under_5px": round(float(np.mean(residuals < 5)), 3),
            "scene_geometry_modified": False,
        }
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--video", required=True, type=Path)
    ap.add_argument("--roi", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--focal", type=float, default=550)
    args = ap.parse_args()
    conf = json.loads(args.roi.read_text(encoding="utf-8"))
    timestamps = conf["frame_seconds"]
    if len(timestamps) != 3 or sorted(timestamps) != timestamps:
        raise ValueError("Three ordered timestamps required")
    if args.focal <= 0:
        raise ValueError("Focal length must be positive")
    cap = cv2.VideoCapture(str(args.video))
    try:
        frames = [extract_frame(cap, timestamp) for timestamp in timestamps]
    finally:
        cap.release()
    if len({tuple(frame.shape) for frame in frames}) != 1:
        raise ValueError("Video resolution changed within selected frames")
    feat = [detect(frame, conf["roi_polygons_px"][str(float(t))])
            for frame, t in zip(frames, timestamps)]
    geom = triangulate(feat[0][0], feat[1][0], feat[2][0],
                       feat[0][1], feat[1][1], feat[2][1],
                       frames[0].shape[1], frames[0].shape[0], args.focal)
    args.out.mkdir(parents=True, exist_ok=True)
    from trimesh import PointCloud
    rgb = cv2.cvtColor(frames[0], cv2.COLOR_BGR2RGB)
    coords = np.rint(geom["pixel0"]).astype(int)
    coords[:, 0] = np.clip(coords[:, 0], 0, rgb.shape[1] - 1)
    coords[:, 1] = np.clip(coords[:, 1], 0, rgb.shape[0] - 1)
    colors = rgb[coords[:, 1], coords[:, 0]]
    cloud = PointCloud(vertices=geom["xyz"], colors=np.c_[
        colors, np.full(len(colors), 255, dtype=np.uint8)])
    cloud.export(args.out / "energetik_sparse_nonmetric.ply")
    cloud.export(args.out / "energetik_sparse_nonmetric.glb")
    (args.out / "validation.json").write_text(
        json.dumps(geom["metrics"], indent=2) + "\n", encoding="utf-8")
    (args.out / "image_to_xyz.json").write_text(json.dumps([
        {"point0_px": a.round(2).tolist(), "point2_px": b.round(2).tolist(),
         "xyz_nonmetric": p.round(6).tolist(), "parallax_deg": round(float(angle), 3)}
        for a, b, p, angle in zip(geom["pixel0"], geom["pixel2"], geom["xyz"], geom["angles"])
    ], indent=2) + "\n", encoding="utf-8")
    print(json.dumps(geom["metrics"], indent=2))


if __name__ == "__main__":
    main()
