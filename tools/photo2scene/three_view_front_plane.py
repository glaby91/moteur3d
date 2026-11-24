#!/usr/bin/env python3
"""Photo2Scene WARP: CONDITIONAL three-camera fit of one Energetik facade plane.

Run with Python + numpy + scipy + opencv-python-headless.
No authentic 3D depth or absolute scale is estimated by this prototype.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import cv2
import numpy as np
from scipy.optimize import least_squares

CAMERA_PARAMETER_COUNT = 7
INITIAL_HEIGHT = 2.4


def prepare(data):
    photos = []
    for item in data["photos"]:
        train, holdout = [], []
        for level in ("top", "bottom"):
            for bay, x, y in item[level]:
                anchor = {
                    "id": f"bay{bay}-{level}",
                    "scene_xy": [-float(bay), float(level == "top")],
                    "pixel": [x, y],
                }
                (train if bay % 2 == 0 else holdout).append(anchor)
        if len(train) < 6 or not holdout:
            raise ValueError("Not enough holdout and training points")
        photos.append({**item, "train": train, "holdout": holdout})
    return photos


def camera_matrix(focal: float, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    return np.array(
        [
            [focal, 0, width / 2],
            [0, focal, height / 2],
            [0, 0, 1],
        ],
        dtype=float,
    )


def scene_points(anchors: list[dict], height: float) -> np.ndarray:
    return np.asarray(
        [
            [anchor["scene_xy"][0], anchor["scene_xy"][1] * height, 0]
            for anchor in anchors
        ],
        dtype=float,
    )


def _camera_slice(index: int) -> slice:
    start = 1 + CAMERA_PARAMETER_COUNT * index
    return slice(start, start + CAMERA_PARAMETER_COUNT)


def _initialize_parameters(photos, initial_focals) -> np.ndarray:
    parameters = [math.log(INITIAL_HEIGHT)]
    for index, photo in enumerate(photos):
        size = photo["image_size_px"]
        width, _ = size
        focal = initial_focals[index] if index < len(initial_focals) else width * 1.3
        image_points = np.asarray(
            [anchor["pixel"] for anchor in photo["train"]],
            dtype=float,
        )
        success, rotation, translation = cv2.solvePnP(
            scene_points(photo["train"], INITIAL_HEIGHT),
            image_points,
            camera_matrix(focal, size),
            None,
            flags=cv2.SOLVEPNP_ITERATIVE,
        )
        if not success:
            raise ValueError("Camera initialization failed for " + photo["id"])
        parameters.extend([
            math.log(focal),
            *map(float, rotation.ravel()),
            *map(float, translation.ravel()),
        ])
    return np.asarray(parameters, dtype=float)


def _project_anchors(photo, anchors, height, camera_parameters) -> np.ndarray:
    focal = math.exp(float(camera_parameters[0]))
    points, _ = cv2.projectPoints(
        scene_points(anchors, height),
        camera_parameters[1:4],
        camera_parameters[4:7],
        camera_matrix(focal, photo["image_size_px"]),
        None,
    )
    return points.reshape(-1, 2)


def _residual(parameters: np.ndarray, photos) -> np.ndarray:
    height = math.exp(float(parameters[0]))
    residuals = []
    for index, photo in enumerate(photos):
        camera_parameters = parameters[_camera_slice(index)]
        observed_pixels = np.asarray(
            [anchor["pixel"] for anchor in photo["train"]],
            dtype=float,
        )
        residuals.append(
            (_project_anchors(photo, photo["train"], height, camera_parameters)
             - observed_pixels).ravel()
        )
    return np.concatenate(residuals)


def _parameter_bounds(camera_count: int) -> tuple[list[float], list[float]]:
    lower = [math.log(1.0)]
    upper = [math.log(6.0)]
    for _ in range(camera_count):
        lower.extend([math.log(180), *([-np.inf] * 6)])
        upper.extend([math.log(10000), *([np.inf] * 6)])
    return lower, upper


def _fit(photos, initial_focals):
    lower, upper = _parameter_bounds(len(photos))
    return least_squares(
        _residual,
        _initialize_parameters(photos, initial_focals),
        args=(photos,),
        loss="soft_l1",
        f_scale=3.0,
        bounds=(lower, upper),
        max_nfev=400,
        ftol=1e-10,
        xtol=1e-10,
        gtol=1e-10,
    )


def _camera_report(photo, index, parameters, height):
    camera_parameters = parameters[_camera_slice(index)]
    focal = math.exp(float(camera_parameters[0]))
    width, image_height = photo["image_size_px"]
    rotation, _ = cv2.Rodrigues(camera_parameters[1:4])
    camera_position = -(rotation.T @ camera_parameters[4:7])
    record = {
        "id": photo["id"],
        "image_size_px": [width, image_height],
        "source_sha256": photo["sha256"],
        "focal_px": focal,
        "fov_horizontal_deg": math.degrees(2 * math.atan(width / (2 * focal))),
        "camera_position_units": camera_position.tolist(),
        "rvec_world_to_camera": camera_parameters[1:4].tolist(),
        "tvec_world_to_camera": camera_parameters[4:7].tolist(),
    }
    for label in ("train", "holdout"):
        anchors = photo[label]
        observed_pixels = np.asarray(
            [anchor["pixel"] for anchor in anchors],
            dtype=float,
        )
        errors = np.linalg.norm(
            _project_anchors(photo, anchors, height, camera_parameters) - observed_pixels,
            axis=1,
        )
        record[label + "_rms_px"] = float(np.sqrt(np.mean(errors**2)))
        record[label + "_max_px"] = float(np.max(errors))
    return record


def _build_report(photos, optimization):
    parameters = optimization.x
    height = math.exp(float(parameters[0]))
    output = {
        "format": "photo2scene-warp-conditional-front-plane-v1",
        "status": "EXPERIMENTAL_UNVERIFIED_LANDMARKS",
        "height_in_assumed_bay_units": height,
        "units": {
            "X": "1 unit = one assumed facade bay",
            "Y": "same X unit",
            "Z": "0 (no depth recovered)",
        },
        "solver": {
            "converged": bool(optimization.success),
            "cost": float(optimization.cost),
            "method": "robust least_squares soft_l1 3px",
        },
        "fov_and_camera_poses": "CONDITIONAL_ON_MANUAL_BAY_MATCHES",
        "photos": [],
        "unknown": [
            "facade actual dimensions",
            "side return depth",
            "roof geometry",
            "authentic camera focal lengths",
            "physical meters-per-bay",
        ],
    }
    output["photos"] = [
        _camera_report(photo, index, parameters, height)
        for index, photo in enumerate(photos)
    ]
    return output


def solve(data, initial_focals=None):
    photos = prepare(data)
    if not initial_focals:
        initial_focals = [530.0, 2200.0, 2700.0]
    optimization = _fit(photos, initial_focals)
    return _build_report(photos, optimization)


def export_obj(height, count=14):
    lines = [
        "# Photo2Scene WARP: conditional facade plane ONLY, no measured depth",
        "# One X unit is one UNVERIFIED bay. Z is fixed to zero.",
    ]
    for bay in range(count):
        for vertical_position in (0.0, height):
            lines.append(f"v {-bay:.8f} {vertical_position:.8f} 0")
    for bay in range(count):
        lines.append(f"l {2 * bay + 1} {2 * bay + 2}")
    for offset in (1, 2):
        lines.append("l " + " ".join(
            str(2 * bay + offset) for bay in range(count)
        ))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("annotations", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--obj", type=Path, default=None)
    args = parser.parse_args()

    data = json.loads(args.annotations.read_text(encoding="utf-8"))
    report = solve(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    if args.obj:
        args.obj.parent.mkdir(parents=True, exist_ok=True)
        args.obj.write_text(
            export_obj(report["height_in_assumed_bay_units"]),
            encoding="utf-8",
        )
    print("Calculated", len(report["photos"]), "conditional cameras")
    for photo in report["photos"]:
        print(
            photo["id"],
            "holdout RMS",
            round(photo["holdout_rms_px"], 2),
            "FOV conditional",
            round(photo["fov_horizontal_deg"], 2),
        )


if __name__=="__main__":
    main()
