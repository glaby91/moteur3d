#!/usr/bin/env python3
"""Install the approved, restrained glazed pool-vault in a packaged V17 scene.

This is scene-only geometry: the flat rooflight and its grid are replaced by a
low elliptical glass vault over the existing pool wing.  No renderer settings
or lighting are changed.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path: Path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n",
                    encoding="utf-8")


def rounded(point):
    return [round(float(value), 5) for value in point]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(v):
    length = math.sqrt(sum(x * x for x in v))
    if length < 1e-10:
        raise ValueError("Degenerate cupola beam")
    return tuple(x / length for x in v)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(v, factor):
    return tuple(x * factor for x in v)


def beam_box(vertices, faces, start, end, axis_a, axis_b, width_a, width_b):
    """Append a closed rectangular beam segment with flat, outward faces."""
    axis_a = normalize(axis_a)
    axis_b = normalize(axis_b)
    direction = normalize(sub(end, start))
    if sum(x * y for x, y in zip(direction, cross(axis_a, axis_b))) < 0.0:
        axis_b = mul(axis_b, -1.0)
    base = len(vertices)
    for center in (start, end):
        for sa, sb in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = add(add(center, mul(axis_a, sa * width_a * 0.5)),
                    mul(axis_b, sb * width_b * 0.5))
            vertices.append(rounded(p))
    quads = ((0, 3, 2, 1), (4, 5, 6, 7),
             (0, 1, 5, 4), (1, 2, 6, 5),
             (2, 3, 7, 6), (3, 0, 4, 7))
    for a, b, c, d in quads:
        faces.extend(((base + a, base + b, base + c),
                      (base + a, base + c, base + d)))


def build_geometry():
    # Existing glass opening: 60 x 34 m around the pool hall.  The new vault
    # sits inside that opening with a small service/roof margin on every edge.
    x0, x1 = -28.0, 28.0
    z_center, half_span = -30.0, 16.25
    base_y, rise = 19.90, 3.35
    arch_segments = 24
    arch_count = 11
    angles = [-math.pi / 2 + math.pi * i / arch_segments
              for i in range(arch_segments + 1)]

    def point(x, angle, radial_offset=0.0):
        # Elliptical half-vault, with the crown below the taller roof volume.
        z = z_center + (half_span + radial_offset) * math.sin(angle)
        y = base_y + (rise + radial_offset) * math.cos(angle)
        return (x, y, z)

    # Glass is divided into real, slightly curved panels between structural
    # ribs.  Double-sided triangles keep the inner surface visible in both
    # engine paths while retaining a single clean optical surface.
    glass_vertices = []
    glass_uv = []
    glass_faces = []
    rib_x = [x0 + (x1 - x0) * i / (arch_count - 1)
             for i in range(arch_count)]
    for bay in range(arch_count - 1):
        xa, xb = rib_x[bay], rib_x[bay + 1]
        for section in range(arch_segments):
            t0, t1 = angles[section], angles[section + 1]
            base = len(glass_vertices)
            glass_vertices.extend((rounded(point(xa, t0, 0.045)),
                                   rounded(point(xa, t1, 0.045)),
                                   rounded(point(xb, t1, 0.045)),
                                   rounded(point(xb, t0, 0.045))))
            glass_uv.extend(((bay / (arch_count - 1), section / arch_segments),
                             (bay / (arch_count - 1), (section + 1) / arch_segments),
                             ((bay + 1) / (arch_count - 1), (section + 1) / arch_segments),
                             ((bay + 1) / (arch_count - 1), section / arch_segments)))
            front = ((base, base + 1, base + 2), (base, base + 2, base + 3))
            glass_faces.extend(front)
            glass_faces.extend(tuple(reversed(face)) for face in front)

    arch_vertices, arch_faces = [], []
    for x in rib_x:
        for i in range(arch_segments):
            t0, t1 = angles[i], angles[i + 1]
            p0, p1 = point(x, t0), point(x, t1)
            tangent = normalize(sub(p1, p0))
            across = (1.0, 0.0, 0.0)
            depth = normalize(cross(tangent, across))
            beam_box(arch_vertices, arch_faces, p0, p1, across, depth, 0.30, 0.24)
        # Small steel shoes make the arch feet visibly land on the roof edge.
        for angle in (-math.pi / 2, math.pi / 2):
            foot = point(x, angle)
            shoe_a = (1.0, 0.0, 0.0)
            shoe_b = (0.0, 0.0, 1.0)
            beam_box(arch_vertices, arch_faces, add(foot, (0.0, -0.10, 0.0)),
                     add(foot, (0.0, 0.10, 0.0)), shoe_a, shoe_b, 0.48, 0.20)

    # Longitudinal purlins follow the ellipse and tie every arch together.
    rail_vertices, rail_faces = [], []
    rail_angles = [-1.35, -1.08, -0.81, -0.54, -0.27, 0.0,
                   0.27, 0.54, 0.81, 1.08, 1.35]
    for angle in rail_angles:
        for bay in range(arch_count - 1):
            xa, xb = rib_x[bay], rib_x[bay + 1]
            p0, p1 = point(xa, angle, 0.015), point(xb, angle, 0.015)
            along_arc = (0.0, -math.sin(angle), math.cos(angle))
            radial = (0.0, math.cos(angle), math.sin(angle))
            beam_box(rail_vertices, rail_faces, p0, p1, along_arc, radial, 0.12, 0.13)

    meshes = [
        {"id": "v523_pool_cupola_glass", "type": "triangle_mesh",
         "vertices": glass_vertices, "faces": glass_faces, "uv": glass_uv},
        {"id": "v523_pool_cupola_arches", "type": "triangle_mesh",
         "vertices": arch_vertices, "faces": arch_faces,
         "uv": [[0.0, 0.0] for _ in arch_vertices]},
        {"id": "v523_pool_cupola_purlins", "type": "triangle_mesh",
         "vertices": rail_vertices, "faces": rail_faces,
         "uv": [[0.0, 0.0] for _ in rail_vertices]},
    ]
    for mesh in meshes:
        if not mesh["vertices"] or not mesh["faces"]:
            raise ValueError(f"Empty cupola mesh: {mesh['id']}")
        if any(min(face) < 0 or max(face) >= len(mesh["vertices"])
               for face in mesh["faces"]):
            raise ValueError(f"Invalid triangle index in {mesh['id']}")
    triangle_count = sum(len(mesh["faces"]) for mesh in meshes)
    if not 3500 <= triangle_count <= 6000:
        raise ValueError(f"Cupola triangle budget outside expected range: {triangle_count}")
    return meshes, triangle_count, (x1 - x0, half_span * 2, rise)


def install(package: Path):
    package = package.resolve()
    scene_path = package / "scene.json"
    scene = read(scene_path)
    if scene.get("format") != "m3d-scene-v16" or len(scene.get("sectors", [])) != 1:
        raise ValueError("Expected the complete single-sector V17 civic scene")
    sector = scene["sectors"][0]
    geometry_path = "geometry/v523-pool-cupola.json"
    instance_relative = "instances/v442-corrections.json"
    if geometry_path in sector["geometry"] or "instances/v523-pool-cupola.json" in sector["instances"]:
        raise ValueError("V17.523 pool cupola appears to be already installed")
    if "v8_clear_roof_glass" not in {m["id"] for m in read(package / sector["materials"][0])["materials"]}:
        raise ValueError("Approved clear glass material is missing")
    if "v8_mullion_dark_steel" not in {m["id"] for m in read(package / sector["materials"][0])["materials"]}:
        raise ValueError("Approved dark steel material is missing")

    instance_path = package / instance_relative
    instances_doc = read(instance_path)
    instances = instances_doc["instances"]
    initial_instance_count = len(instances)
    old = [obj for obj in instances
           if obj.get("parent", "").startswith("Architecture/Palais/Verriere haute")]
    if len(old) != 19:
        raise ValueError(f"Expected 19 flat skylight/frame instances to replace; found {len(old)}")
    old_ids = {obj["id"] for obj in old}
    if "v17_000152" not in old_ids:
        raise ValueError("Flat roof glass surface not found")
    instances = [obj for obj in instances if obj["id"] not in old_ids]

    meshes, triangles, dims = build_geometry()
    write(package / geometry_path, {"format": "m3d-geometry-v1", "units": "m", "meshes": meshes})
    cupola_instances = [
        {"id": "v523_pool_cupola_glazing",
         "parent": "Architecture/Palais/Piscine/Verriere voutee/Vitrage",
         "geometry": "v523_pool_cupola_glass", "material": "v8_clear_roof_glass",
         "position": [0.0, 0.0, 0.0], "size": [1.0, 1.0, 1.0], "rotationY": 0},
        {"id": "v523_pool_cupola_arches",
         "parent": "Architecture/Palais/Piscine/Verriere voutee/Arcs porteurs",
         "geometry": "v523_pool_cupola_arches", "material": "v8_mullion_dark_steel",
         "position": [0.0, 0.0, 0.0], "size": [1.0, 1.0, 1.0], "rotationY": 0},
        {"id": "v523_pool_cupola_purlins",
         "parent": "Architecture/Palais/Piscine/Verriere voutee/Lisses longitudinales",
         "geometry": "v523_pool_cupola_purlins", "material": "v8_mullion_dark_steel",
         "position": [0.0, 0.0, 0.0], "size": [1.0, 1.0, 1.0], "rotationY": 0},
    ]
    instances.extend(cupola_instances)
    instances_doc["instances"] = instances
    write(instance_path, instances_doc)
    sector["geometry"].append(geometry_path)
    camera_path = sector["cameras"][0]
    cameras_doc = read(package / camera_path)
    cameras = cameras_doc["cameras"]
    camera_ids = ("v523_cupola_exterieure", "v523_cupola_arriere",
                  "v523_cupola_interieure")
    for camera_id in camera_ids:
        if any(camera.get("id") == camera_id for camera in cameras):
            raise ValueError(f"Camera already exists: {camera_id}")
    cameras.extend((
        {"id": "v523_cupola_exterieure", "name": "V17.523 · Coupole piscine, vue extérieure",
         "position": [82.0, 43.0, 18.0], "target": [0.0, 16.0, -30.0], "fovDegrees": 58},
        {"id": "v523_cupola_arriere", "name": "V17.523 · Coupole piscine, vue arrière",
         "position": [82.0, 43.0, -90.0], "target": [0.0, 16.0, -30.0], "fovDegrees": 58},
        {"id": "v523_cupola_interieure", "name": "V17.523 · Sous la coupole, vue piscine",
         "position": [0.0, 2.0, -12.0], "target": [0.0, 21.5, -32.0], "fovDegrees": 68},
    ))
    write(package / camera_path, cameras_doc)
    scene["revision"] = "V17.523"
    scene["name"] = "CSE WARP V17.523 — Palais et coupole vitrée de la piscine"
    write(scene_path, scene)

    report = {
        "revision": "V17.523",
        "feature": "approved restrained glazed pool cupola",
        "removedFlatSkylightInstances": len(old),
        "addedCupolaMeshes": len(meshes),
        "addedCupolaInstances": len(cupola_instances),
        "addedTriangles": triangles,
        "cupolaDimensionsMeters": {"length": dims[0], "span": dims[1], "rise": dims[2]},
        "baseElevationMeters": 19.9,
        "cameras": list(camera_ids),
        "materialsReused": ["v8_clear_roof_glass", "v8_mullion_dark_steel"],
        "lightingChanged": False,
        "rendererChanged": False,
    }
    write(package / "V523_POOL_CUPOLA.json", report)
    if len(instances) != initial_instance_count - len(old) + len(cupola_instances):
        raise AssertionError("Scene instance count did not settle after cupola installation")
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=Path,
                        help="Packaged V17 benchmark-civic directory")
    args = parser.parse_args()
    report = install(args.package)
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
