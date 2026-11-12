#!/usr/bin/env python3
"""CC0 Kenney Nature Kit -> CSE WARP GLB triangle-mesh pilot scene.

Does not replace original V4 assets. Deterministic output:
scene-v4-trees-pilot.json + geometry/trees-pilot.json +
instances/v4-trees-pilot.json + reports/trees-pilot.json.
"""
import argparse
import copy
import hashlib
import io
import json
import pathlib
import urllib.request
import zipfile

SOURCE = "https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip"
EXPECTED = "fa7974a0d342bfe63c38664ba9f8ec1a4aab8ea25f099bdc56870e33588c4d9d"
MODELS = [
    "tree_oak", "tree_default", "tree_detailed", "tree_pineDefaultA",
    "tree_pineDefaultB", "tree_fat", "tree_oak", "tree_detailed",
    "tree_pineDefaultA", "tree_default",
]
LOCATIONS = [
    (-62, 31, 10.0, 0), (62, 35, 11.0, 38), (-62, 50, 10.8, 94),
    (63, 54, 12.0, 153), (-60, 72, 11.6, 212), (61, 76, 10.2, 274),
    (-63, 94, 12.0, 316), (63, 97, 10.5, 35), (-63, 111, 11.7, 85),
    (63, 112, 10.2, 190),
]

def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")

def read(path):
    return json.loads(path.read_text(encoding="utf-8"))

def load_source(cache):
    if cache.exists():
        raw = cache.read_bytes()
    else:
        with urllib.request.urlopen(SOURCE, timeout=90) as req:
            raw = req.read()
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_bytes(raw)
    actual = hashlib.sha256(raw).hexdigest()
    if actual != EXPECTED:
        raise RuntimeError("Kenney Nature Kit SHA256 mismatch: " + actual)
    return zipfile.ZipFile(io.BytesIO(raw))

def convert(zf, model, trimesh):
    import numpy as np
    key = "Models/GLTF format/" + model + ".glb"
    if key not in zf.namelist():
        raise RuntimeError("Missing source GLB: " + key)
    source = trimesh.load(io.BytesIO(zf.read(key)), file_type="glb", force="scene", process=False)
    merged = {"pilot_bark": [], "pilot_leaf": []}
    all_tris = []
    for node_name in source.graph.nodes_geometry:
        transform, geometry_name = source.graph[node_name]
        mesh = source.geometry[geometry_name]
        vertices = trimesh.transform_points(mesh.vertices, transform)
        faces = np.asarray(mesh.faces, dtype=np.int64)
        if len(faces) == 0:
            continue
        face_rgba = None
        if mesh.visual.kind == "vertex":
            face_rgba = np.asarray(mesh.visual.vertex_colors)[faces].mean(axis=1)
        elif mesh.visual.kind == "face":
            face_rgba = np.asarray(mesh.visual.face_colors)
        else:
            c = getattr(getattr(mesh.visual, "material", None), "main_color", [90, 150, 70, 255])
            face_rgba = np.tile(np.asarray(c)[:3], (len(faces), 1))
        uv = getattr(mesh.visual, "uv", None)
        if uv is None or len(uv) != len(vertices):
            uv = np.zeros((len(vertices), 2), dtype=float)
        for ids, color in zip(faces, face_rgba):
            color = np.asarray(color).reshape(-1)
            material = "pilot_leaf" if len(color) >= 3 and color[1] > color[0] * 0.80 and color[1] > color[2] * 0.86 else "pilot_bark"
            merged[material].append((vertices[ids].copy(), np.asarray(uv)[ids].copy()))
            all_tris.append(vertices[ids].copy())
    if not all_tris:
        raise RuntimeError("Empty model: " + model)
    coords = np.vstack(all_tris)
    center_xz = (coords[:, [0, 2]].min(axis=0) + coords[:, [0, 2]].max(axis=0)) * 0.5
    floor = float(coords[:, 1].min())
    height = float(coords[:, 1].max() - floor)
    if height <= 0:
        raise RuntimeError("Flat model: " + model)
    result = []
    for material, triangles in merged.items():
        if not triangles:
            continue
        vertices, uvs, faces = [], [], []
        for points, tex in triangles:
            face = []
            for point, uv in zip(points, tex):
                face.append(len(vertices))
                vertices.append([
                    round((float(point[0]) - center_xz[0]) / height, 6),
                    round((float(point[1]) - floor) / height, 6),
                    round((float(point[2]) - center_xz[1]) / height, 6)
                ])
                uvs.append([round(float(uv[0]), 6), round(float(uv[1]), 6)])
            faces.append(face)
        result.append({
            "id": "kenney_" + model + "_" + material, "type": "triangle_mesh",
            "vertices": vertices, "faces": faces, "uv": uvs,
            "source": key, "sourceLicense": "CC0-1.0"
        })
    return result

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=pathlib.Path, required=True)
    ap.add_argument("--cache", type=pathlib.Path, default=pathlib.Path("build-v17/kenney_nature-kit.zip"))
    args = ap.parse_args()
    import trimesh
    zf = load_source(args.cache)
    root = args.root
    scene = copy.deepcopy(read(root / "scene-v4.json"))
    instances = copy.deepcopy(read(root / "instances/v4.json"))
    geometry = {"format": "m3d-geometry-v1", "units": "m", "meshes": []}
    for model in sorted(set(MODELS)):
        geometry["meshes"].extend(convert(zf, model, trimesh))
    by_model = {}
    for mesh in geometry["meshes"]:
        name = mesh["id"].split("_pilot_")[0]
        by_model.setdefault(name, []).append(mesh)
    pilot_ids = []
    for i, (model, (x, z, height, angle)) in enumerate(zip(MODELS, LOCATIONS), 1):
        material_parts = by_model.get("kenney_" + model, [])
        if not material_parts:
            raise RuntimeError("No geometry for model: " + model)
        for mesh in material_parts:
            identifier = "pilot_cc0_" + str(i).zfill(2) + "_" + mesh["id"]
            instances["instances"].append({
                "id": identifier, "parent": "Nature/Arbres pilotes GLB CC0",
                "geometry": mesh["id"],
                "material": "prp_bark_wet" if mesh["id"].endswith("pilot_bark") else "prp_foliage",
                "position": [x, 0, z], "size": [height, height, height], "rotationY": angle
            })
            pilot_ids.append(identifier)
    assert len(pilot_ids) >= 10
    scene["name"] = str(scene.get("name", "V4")) + " — 10 vrais arbres Kenney GLB pilotes"
    scene["sectors"][0]["geometry"].append("geometry/trees-pilot.json")
    scene["sectors"][0]["instances"] = ["instances/v4-trees-pilot.json"]
    save(root / "scene-v4-trees-pilot.json", scene)
    save(root / "geometry/trees-pilot.json", geometry)
    save(root / "instances/v4-trees-pilot.json", instances)
    report = {
        "author": "Kenney", "source": SOURCE, "sha256": EXPECTED, "license": "CC0-1.0",
        "trees": 10, "sourceModels": MODELS, "positions": LOCATIONS,
        "instancesAdded": len(pilot_ids), "meshCount": len(geometry["meshes"]),
        "triangles": sum(len(m["faces"]) for m in geometry["meshes"]),
        "note": "Pilot quality: Kenney is stylized/low-poly; imported actual GLB triangle geometry, not photorealistic."
    }
    save(root / "reports/trees-pilot.json", report)
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
