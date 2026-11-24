#!/usr/bin/env python3
"""Photo2Scene WARP: calcul reproductible de géométrie projective depuis annotations.

Usage: python tools/photo2scene/photo2scene.py analyse data.json -o result.json
Toutes les positions pixels sont des observations, pas des inférences fiables de l'IA.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

class GeometryError(ValueError):
    pass


def least_squares(rows: list[list[float]], values: list[float], n: int) -> list[float]:
    if len(rows) < n or len(rows) != len(values):
        raise GeometryError("Insufficient or malformed constraints")
    # Petits systèmes normalisés, pivotage partiel. Pour les gros réseaux:
    # SVD et solveur robuste (SciPy/OpenCV), dans une étape ultérieure.
    matrix = [
        [sum(row[i] * row[j] for row in rows) for j in range(n)]
        for i in range(n)
    ]
    target = [sum(row[i] * value for row, value in zip(rows, values)) for i in range(n)]
    matrix_scale = max(abs(matrix[i][j]) for i in range(n) for j in range(n))
    for column in range(n):
        pivot = max(range(column, n), key=lambda row: abs(matrix[row][column]))
        if not matrix_scale or abs(matrix[pivot][column]) <= 1e-11 * matrix_scale:
            raise GeometryError("Degenerate or nearly parallel constraints")
        matrix[column], matrix[pivot] = matrix[pivot], matrix[column]
        target[column], target[pivot] = target[pivot], target[column]
        for row in range(column + 1, n):
            factor = matrix[row][column] / matrix[column][column]
            for entry in range(column, n):
                matrix[row][entry] -= factor * matrix[column][entry]
            target[row] -= factor * target[column]

    solution = [0.0] * n
    for row in range(n - 1, -1, -1):
        known_terms = sum(
            matrix[row][column] * solution[column]
            for column in range(row + 1, n)
        )
        solution[row] = (target[row] - known_terms) / matrix[row][row]
    if not all(math.isfinite(value) for value in solution):
        raise GeometryError("Non-finite solution")
    return solution


def line(a: list[float], b: list[float]) -> tuple[float, float, float]:
    if len(a) != 2 or len(b) != 2:
        raise GeometryError("A line segment requires two 2D endpoints")
    coefficient_a = a[1] - b[1]
    coefficient_b = b[0] - a[0]
    coefficient_c = a[0] * b[1] - b[0] * a[1]
    normalizer = math.hypot(coefficient_a, coefficient_b)
    if normalizer < 1e-9:
        raise GeometryError("Coincident line endpoints")
    return (
        coefficient_a / normalizer,
        coefficient_b / normalizer,
        coefficient_c / normalizer,
    )


def vanishing_point(segments: list[dict]) -> dict:
    if len(segments) < 2:
        raise GeometryError("At least two line segments required per direction")
    lines = [line(s["a"], s["b"]) for s in segments]
    point_x, point_y = least_squares(
        [[a, b] for a, b, _ in lines],
        [-c for _, _, c in lines],
        2,
    )
    errors = [
        abs(a * point_x + b * point_y + c)
        for a, b, c in lines
    ]
    return {
        "pixel": [point_x, point_y],
        "line_residual_rms_px": math.sqrt(sum(error * error for error in errors) / len(errors)),
        "segments": len(lines),
    }


def fov_from_orthogonal_vps(vp1: list[float], vp2: list[float],
                             width: int, height: int,
                             principal: list[float] | None = None) -> dict:
    # (v1-c).(v2-c) + f^2 = 0 si directions 3D orthogonales,
    # pixels carrés, skew nul, caméra sténopé et distorsion négligeable.
    cx, cy = principal if principal is not None else (width / 2, height / 2)
    focal_squared = -(
        (vp1[0] - cx) * (vp2[0] - cx)
        + (vp1[1] - cy) * (vp2[1] - cy)
    )
    if not math.isfinite(focal_squared) or focal_squared <= 1e-8:
        raise GeometryError("Orthogonal-VP FOV is not identifiable under assumptions")
    focal = math.sqrt(focal_squared)
    return {
        "focal_length_px": focal,
        "fov_horizontal_deg": math.degrees(2 * math.atan(width / (2 * focal))),
        "fov_vertical_deg": math.degrees(2 * math.atan(height / (2 * focal))),
        "assumptions": [
            "orthogonal 3D vanishing directions",
            "pinhole camera",
            "square pixels",
            "zero skew",
            "no lens distortion",
            "principal point supplied or image center",
        ],
    }


def _pn(pixel: list[float], w: int, h: int) -> tuple[float, float]:
    return (pixel[0] - w / 2) / (w / 2), (pixel[1] - h / 2) / (h / 2)


def _project(h: list[float], u: float, v: float) -> tuple[float, float]:
    denominator = h[6] * u + h[7] * v + 1
    if abs(denominator) < 1e-10:
        raise GeometryError("Point projects onto the line at infinity")
    pixel_x = (h[0] * u + h[1] * v + h[2]) / denominator
    pixel_y = (h[3] * u + h[4] * v + h[5]) / denominator
    return pixel_x, pixel_y


def _back_project(h: list[float], x: float, y: float) -> tuple[float, float]:
    h00, h01, h02, h10, h11, h12, h20, h21 = h
    coefficient_00 = h00 - x * h20
    coefficient_01 = h01 - x * h21
    right_x = x - h02
    coefficient_10 = h10 - y * h20
    coefficient_11 = h11 - y * h21
    right_y = y - h12
    determinant = coefficient_00 * coefficient_11 - coefficient_01 * coefficient_10
    if abs(determinant) < 1e-10:
        raise GeometryError("Pixel ray is parallel to the reference plane")
    scene_u = (
        right_x * coefficient_11 - coefficient_01 * right_y
    ) / determinant
    scene_v = (
        coefficient_00 * right_y - right_x * coefficient_10
    ) / determinant
    return scene_u, scene_v


def fit_plane(anchors: list[dict], width: int, height: int,
              queries: list[dict] | None = None,
              holdout: list[dict] | None = None) -> dict:
    if len(anchors) < 4:
        raise GeometryError("A planar homography needs >= 4 non-collinear anchors")
    rows, targets = [], []
    for anchor in anchors:
        scene_u, scene_v = anchor["scene_uv"]
        pixel_x, pixel_y = _pn(anchor["pixel"], width, height)
        rows.extend([
            [scene_u, scene_v, 1, 0, 0, 0, -scene_u * pixel_x, -scene_v * pixel_x],
            [0, 0, 0, scene_u, scene_v, 1, -scene_u * pixel_y, -scene_v * pixel_y],
        ])
        targets.extend([pixel_x, pixel_y])
    homography = least_squares(rows, targets, 8)

    def errors(items: list[dict]) -> dict:
        pixel_errors = []
        for item in items:
            scene_u, scene_v = item["scene_uv"]
            projected_x, projected_y = _project(homography, scene_u, scene_v)
            observed_x, observed_y = _pn(item["pixel"], width, height)
            pixel_errors.append(math.hypot(
                (projected_x - observed_x) * width / 2,
                (projected_y - observed_y) * height / 2,
            ))
        return {
            "count": len(pixel_errors),
            "rms_px": (
                math.sqrt(sum(error * error for error in pixel_errors) / len(pixel_errors))
                if pixel_errors else None
            ),
            "max_px": max(pixel_errors) if pixel_errors else None,
        }

    converted = []
    for q in queries or []:
        pixel_x, pixel_y = _pn(q["pixel"], width, height)
        scene_u, scene_v = _back_project(homography, pixel_x, pixel_y)
        converted.append({
            "id": q["id"],
            "scene_uv": [scene_u, scene_v],
            "assumption": "queried pixel lies on this calibrated reference plane",
        })
    return {
        "homography_scene_to_normalized_pixel": [
            [homography[0], homography[1], homography[2]],
            [homography[3], homography[4], homography[5]],
            [homography[6], homography[7], 1],
        ],
        "fitting": errors(anchors),
        "held_out_validation": errors(holdout or []),
        "derived_plane_points": converted,
        "warning": "Cannot infer off-plane depth or metric scale from this homography",
    }


def _analyse_vanishing(config: dict, width: int, height: int) -> dict:
    vanishing_points = {
        name: vanishing_point(segments)
        for name, segments in config["directions"].items()
    }
    result = {"vanishing_points": vanishing_points}
    orthogonal_pair = config.get("orthogonal_pair")
    if orthogonal_pair is not None:
        if (
            len(orthogonal_pair) != 2
            or orthogonal_pair[0] not in vanishing_points
            or orthogonal_pair[1] not in vanishing_points
        ):
            raise GeometryError("Unknown vanishing directions in orthogonal pair")
        result["camera_intrinsics_estimate"] = fov_from_orthogonal_vps(
            vanishing_points[orthogonal_pair[0]]["pixel"],
            vanishing_points[orthogonal_pair[1]]["pixel"],
            width,
            height,
            config.get("principal_point"),
        )
    return result


def _analyse_planes(planes: list[dict], width: int, height: int) -> dict:
    return {
        plane["id"]: fit_plane(
            plane["anchors"],
            width,
            height,
            plane.get("query"),
            plane.get("holdout"),
        )
        for plane in planes
    }


def analyse(data: dict) -> dict:
    image = data["image"]
    width, height = int(image["width"]), int(image["height"])
    if width < 2 or height < 2:
        raise GeometryError("Invalid image dimensions")
    result = {
        "photo_id": image["id"],
        "image_size_px": [width, height],
        "method": "deterministic geometry from annotated image pixels",
        "status": "calculated",
    }
    if "vanishing" in data:
        result.update(_analyse_vanishing(data["vanishing"], width, height))
    if "planes" in data:
        result["planes"] = _analyse_planes(data["planes"], width, height)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["analyse"])
    parser.add_argument("input", type=Path, help="JSON of annotated pixel geometry")
    parser.add_argument("-o", "--output", type=Path, help="Write report JSON here")
    args = parser.parse_args()
    try:
        result = analyse(json.loads(args.input.read_text(encoding="utf-8")))
    except (OSError, KeyError, TypeError, ValueError, IndexError) as exc:
        parser.exit(2, f"Photo2Scene WARP: {exc}\n")
    output = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
