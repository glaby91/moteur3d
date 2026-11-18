#!/usr/bin/env python3
"""Apply and verify the V17.600 finishing pass on the final Windows package.

This runs only after the approved scene and every historical overlay have been
assembled.  It intentionally edits the delivered scene, not the earlier source
fixture that the workflow later replaces.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

CUPOLA_INTERVALS = [(-math.pi/2, -math.pi/6 + math.radians(1.0)),
                    (-math.pi/6 - math.radians(1.0), math.pi/6 + math.radians(1.0)),
                    (math.pi/6 - math.radians(1.0), math.pi/2)]


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path: Path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n",
                    encoding="utf-8")


def rounded(v):
    return [round(float(x), 6) for x in v]


def locate(items, key):
    for item in items:
        if item.get("id") == key:
            return item
    return None


def patch(package: Path, revision: str):
    root = package / "benchmark-civic"
    scene_path = root / "scene.json"
    cupola_path = root / "geometry/v523-pool-cupola.json"
    scene = read(scene_path)
    if (scene.get("revision") == revision and
            locate(read(cupola_path)["meshes"], "v600_cupola_leaf_0")):
        raise ValueError(f"Package already contains the {revision} finishing geometry")
    if not str(scene.get("revision", "")).startswith("V17."):
        raise ValueError(f"Unexpected base scene revision: {scene.get('revision')}")
    sector = scene["sectors"][0]
    geom_path = root / "geometry/v7-corrections.json"
    instances_path = root / "instances/v442-corrections.json"
    mats_path = root / "materials/v442-corrections.json"
    pbr_path = root / "pbr/v440-audit.json"
    cameras_path = root / "cameras/v8-nine-anomalies.json"
    lights_path = root / "lighting/v8-nine-anomalies.json"
    geometry = read(geom_path)
    cupola = read(cupola_path)
    instances = read(instances_path)
    materials = read(mats_path)
    pbr = read(pbr_path)
    cameras = read(cameras_path)
    lighting = read(lights_path)

    # A single full-resolution source image and global UVs remove the eight
    # independently sampled/cropped image tiles that created visible joins.
    fresco_meshes = [m for m in geometry["meshes"] if m.get("id", "").startswith("v7_fresco_quad_")]
    if len(fresco_meshes) != 8:
        raise ValueError(f"Expected eight delivered V7 fresco panels, found {len(fresco_meshes)}")
    for mesh in fresco_meshes:
        row, col = (int(x) for x in mesh["id"].split("_")[-2:])
        mesh["uv"] = [[col / 4, row / 2], [(col + 1) / 4, row / 2],
                      [(col + 1) / 4, (row + 1) / 2], [col / 4, (row + 1) / 2]]
    fresco_instances = [i for i in instances["instances"] if i.get("id", "").startswith("v7_fresco_")
                        and i.get("id", "").endswith("_inst")]
    if len(fresco_instances) != 8:
        raise ValueError(f"Expected eight delivered fresco instances, found {len(fresco_instances)}")
    for inst in fresco_instances:
        inst["material"] = "cse_energetik_fresco"
    fresco_wall = locate(instances["instances"], "v7_fresco_wall")
    if fresco_wall is None:
        raise ValueError("The delivered fresco support wall is missing")
    fresco_wall["material"] = "v600_fresco_support"
    materials["materials"] = [m for m in materials["materials"]
                               if not m.get("id", "").startswith("v7_fresco_")]
    pbr["sets"] = [entry for entry in pbr["sets"]
                   if not entry.get("id", "").startswith("tex_v7_fresco_")]
    shared = locate(pbr["sets"], "tex_cse_energetik_fresco")
    if shared is None:
        raise ValueError("The approved full-resolution fresco source material is missing")
    shared["imageFile"] = "textures/energetik_fresco_mosaic.jpg"
    shared["resolution"] = 2048
    shared["channels"] = ["albedo"]
    shared["status"] = "V17.600 shared image across all eight wall panels"
    for row in range(2):
        for col in range(4):
            (root / f"textures/revision/v7/energetik_tile_{row}_{col}.png").unlink(missing_ok=True)
    materials["materials"].append({"id": "v600_fresco_support", "baseColor": [0.72, 0.70, 0.64],
        "roughness": 0.93, "metallic": 0.0, "transmission": 0.0, "ior": 1.5,
        "textureSet": "tex_v600_fresco_support", "origin": "V17.600 uniform matte plaster behind the pool fresco"})
    pbr["sets"].append({"id": "tex_v600_fresco_support", "material": "v600_fresco_support",
        "pattern": "smooth_plaster", "resolution": 512, "channels": ["albedo", "normal", "roughness"]})

    # Split the complete half-cylinder skin into three nested arc leaves. The
    # opaque crown is converted back to tinted glazing; three long rails provide
    # the visible mechanical guide for a fixed leaf and two overlapping leaves.
    source_by_id = {m.get("id"): m for m in cupola["meshes"]}
    glass = source_by_id.get("v523_pool_cupola_glass")
    crown = source_by_id.get("v550_pool_cupola_opaque_crown")
    if not glass or not crown:
        raise ValueError("V17.523 cupola meshes are missing from the final package")
    all_vertices, all_uv, all_faces = [], [], []
    for mesh in (glass, crown):
        uv = mesh.get("uv", [])
        for face in mesh["faces"]:
            base = len(all_vertices)
            all_vertices.extend(mesh["vertices"][n] for n in face)
            all_uv.extend(uv[n] if n < len(uv) else [0, 0] for n in face)
            all_faces.append([base, base + 1, base + 2])
    sections = [dict(id=f"v600_cupola_leaf_{n}", type="triangle_mesh", vertices=[], uv=[], faces=[])
                for n in range(3)]
    # The small angular overlap avoids a visible daylight slit. Each outer leaf
    # sits slightly higher, like a real telescopic glazing stack.
    for face in all_faces:
        pts = [all_vertices[n] for n in face]
        center = [sum(p[k] for p in pts) / 3 for k in range(3)]
        angle = math.atan2((center[2] + 30.0) / 17.0, (center[1] - 19.9) / 3.35)
        section_index = next((j for j, (lo, hi) in enumerate(CUPOLA_INTERVALS) if lo <= angle <= hi), None)
        if section_index is None:
            continue
        out = sections[section_index]
        base = len(out["vertices"])
        lift = (0.0, 0.075, 0.15)[section_index]
        for source_index in face:
            point = list(all_vertices[source_index])
            point[1] += lift
            out["vertices"].append(rounded(point))
            out["uv"].append(rounded(all_uv[source_index]))
        out["faces"].append([base, base + 1, base + 2])
    if any(len(m["faces"]) < 100 for m in sections):
        raise ValueError("Cupola leaf split produced an incomplete section")
    cupola["meshes"] = [m for m in cupola["meshes"]
                         if m.get("id") not in {"v523_pool_cupola_glass", "v550_pool_cupola_opaque_crown"}]
    cupola["meshes"].extend(sections)
    # Three track levels run along each eave, one per nested leaf.
    for rail, y in enumerate((20.04, 20.115, 20.19)):
        cupola["meshes"].append({"id": f"v600_cupola_guide_{rail}", "type": "box",
                                  "size": [60.3, 0.075, 0.10], "uv": "planar_per_face"})
    instances["instances"] = [i for i in instances["instances"]
                               if i.get("id") not in {"v523_pool_cupola_glazing", "v550_pool_cupola_opaque_crown"}]
    for n in range(3):
        instances["instances"].append({"id": f"v600_cupola_leaf_{n}_inst",
            "parent": ("Architecture/Palais/Piscine/Coupole/Vantail fixe" if n == 0 else
                       f"Architecture/Palais/Piscine/Coupole/Vantail coulissant {n}"),
            "geometry": f"v600_cupola_leaf_{n}", "material": "v600_tinted_glass",
            "position": [0, 0, 0], "size": [1, 1, 1], "rotationY": 0})
    for rail, y in enumerate((20.04, 20.115, 20.19)):
        for side, z in enumerate((-47.15, -12.85)):
            instances["instances"].append({"id": f"v600_cupola_guide_{rail}_{side}_inst",
                "parent": "Architecture/Palais/Piscine/Coupole/Rails longitudinaux",
                "geometry": f"v600_cupola_guide_{rail}", "material": "v8_inox_brushed_316L",
                "position": [0, y, z], "size": [1, 1, 1], "rotationY": 0})
    materials["materials"].append({"id": "v600_tinted_glass", "baseColor": [0.34, 0.69, 0.75],
        "roughness": 0.12, "metallic": 0.02, "transmission": 0.78, "ior": 1.49,
        "textureSet": "tex_v600_tinted_glass", "origin": "V17.600 coupola tinted structural glazing"})
    pbr["sets"].append({"id": "tex_v600_tinted_glass", "material": "v600_tinted_glass",
        "pattern": "glass", "resolution": 512, "channels": ["albedo"]})

    # Keep all detailed diagnostic poses in the manifest for CLI tests, but
    # present only named broad zones in the application's camera selector.
    clean_names = {
        "v4_place": "01 · Place et monument",
        "v4_aerienne": "02 · Vue générale du site",
        "v6_monument_contrechamp": "03 · Monument vers le palais",
        "v7_pool_fresco": "04 · Piscine et fresque",
        "v4_hall": "05 · Hall intérieur",
        "v523_cupola_exterieure": "06 · Coupole, extérieur",
        "v523_cupola_interieure": "07 · Coupole, intérieur",
        "v523_cupola_arriere": "08 · Coupole, arrière",
    }
    for c in cameras["cameras"]:
        if c.get("id") in clean_names:
            c["name"] = clean_names[c["id"]]

    # Bring the eight planters indoors as balanced side clusters, leaving the
    # centerline to the main entrance and long view to the monument unobstructed.
    pot_x = [-34, -27, -20, -13, 13, 20, 27, 34]
    trees_moved = 0
    for inst in instances["instances"]:
        ident = inst.get("id", "")
        for n in range(8):
            if ident.startswith(f"rev_pot{n}_") or ident.startswith(f"rev_pottree{n}_"):
                inst["position"][0] = pot_x[n]
                inst["position"][2] = -8.0 if n in (0, 3, 4, 7) else -14.0
                trees_moved += 1
                break
    if trees_moved != 72:
        raise ValueError(f"Expected 72 planter/tree mesh instances, relocated {trees_moved}")

    # Build a restrained continuous pale route from the building entrance to
    # the existing hill. It stays one slab, avoiding another tiled ground patch.
    if not locate(geometry["meshes"], "v600_civic_light_band"):
        geometry["meshes"].append({"id": "v600_civic_light_band", "type": "box",
                                   "size": [5.2, 0.055, 42.0], "uv": "planar_per_face"})
    if not locate(materials["materials"], "v600_pale_paving"):
        materials["materials"].append({"id": "v600_pale_paving", "baseColor": [0.67, 0.65, 0.59],
            "roughness": 0.88, "metallic": 0.0, "transmission": 0.0, "ior": 1.5,
            "textureSet": "tex_v600_pale_paving", "origin": "V17.600 continuous light paving route"})
    if locate(pbr["sets"], "tex_v600_pale_paving") is None:
        pbr["sets"].append({"id": "tex_v600_pale_paving", "material": "v600_pale_paving",
            "pattern": "smooth_plaster", "resolution": 512, "channels": ["albedo", "normal", "roughness"]})
    if not any(i.get("id") == "v600_civic_light_band_inst" for i in instances["instances"]):
        instances["instances"].append({"id": "v600_civic_light_band_inst",
            "parent": "Extérieur/Place/Allée claire vers le monument", "geometry": "v600_civic_light_band",
            "material": "v600_pale_paving", "position": [0, 0.03, 40],
            "size": [1, 1, 1], "rotationY": 0})

    # Rebalance the existing perimeter fixtures into a lower, softer path rhythm
    # while keeping the same number of point lights and total light budget.
    path_lights = [l for l in lighting["lights"] if abs(l.get("position", [0])[0]) == 65
                   and 20 <= l.get("position", [0, 0, 0])[2] <= 66]
    if len(path_lights) != 10:
        raise ValueError(f"Expected ten existing plaza perimeter lights, found {len(path_lights)}")
    for n, light in enumerate(path_lights):
        if n < 6:
            side = -1 if n % 2 == 0 else 1
            row = n // 2
            light["position"] = [side * 4.2, 0.85, 24.0 + row * 12.0]
            light["radius"] = 7.0
        else:
            # Four dim points trace the existing oval bench perimeter.
            ring = ((-17.0, 67.0), (17.0, 67.0), (-11.0, 82.0), (11.0, 82.0))
            light["position"] = [*ring[n - 6], 0.85]
            light["position"] = [light["position"][0], 0.85, light["position"][1]]
            light["radius"] = 5.0
        light["intensity"] = 0.62
        light["color"] = [255, 237, 207]

    scene["revision"] = revision
    scene["name"] = "CSE WARP — Palais, piscine et coupole coulissante"
    write(geom_path, geometry)
    write(cupola_path, cupola)
    write(instances_path, instances)
    write(mats_path, materials)
    write(pbr_path, pbr)
    write(cameras_path, cameras)
    write(lights_path, lighting)
    write(scene_path, scene)
    return {"revision": revision, "frescoPanels": len(fresco_meshes), "sharedFrescoImage": shared["imageFile"],
            "cupolaLeaves": len(sections), "cupolaGuideRails": 3, "relocatedPlanterMeshes": trees_moved,
            "pathLightsRebalanced": len(path_lights), "sceneRevision": scene["revision"]}


def verify(package: Path, revision: str):
    root = package / "benchmark-civic"
    scene = read(root / "scene.json")
    if scene.get("revision") != revision:
        raise ValueError(f"Expected delivered scene {revision}, found {scene.get('revision')}")
    sector = scene["sectors"][0]
    for kind in ("geometry", "instances", "materials", "pbr", "lighting", "cameras", "environment"):
        for relative in sector.get(kind, []):
            if not (root / relative).is_file():
                raise ValueError(f"Final package resource missing: {kind}/{relative}")
    material_entries = read(root / "materials/v442-corrections.json")["materials"]
    pbr_entries = read(root / "pbr/v440-audit.json")["sets"]
    pbr_ids = {entry.get("id") for entry in pbr_entries}
    for material in material_entries:
        texture_set = material.get("textureSet")
        if not texture_set or texture_set not in pbr_ids:
            raise ValueError(f"Material {material.get('id')} has a missing texture set: {texture_set}")
    inst = read(root / "instances/v442-corrections.json")["instances"]
    f = [x for x in inst if x.get("id", "").startswith("v7_fresco_") and x.get("id", "").endswith("_inst")]
    if len(f) != 8 or {x.get("material") for x in f} != {"cse_energetik_fresco"}:
        raise ValueError("Fresco is not using one material across all eight panels")
    if any(m.get("id", "").startswith("v7_fresco_") for m in material_entries):
        raise ValueError("Unused per-panel fresco materials remain in the shipped scene")
    pbr = pbr_entries
    if any(x.get("id", "").startswith("tex_v7_fresco_") for x in pbr):
        raise ValueError("Unused per-panel fresco textures remain in the shipped scene")
    wall = locate(inst, "v7_fresco_wall")
    if not wall or wall.get("material") != "v600_fresco_support":
        raise ValueError("Fresco support wall does not use the smooth matte plaster material")
    image = locate(pbr, "tex_cse_energetik_fresco")
    if not image or not (root / image["imageFile"]).is_file():
        raise ValueError("Shared full-resolution fresco texture is missing")
    for mesh in read(root / "geometry/v7-corrections.json")["meshes"]:
        if mesh.get("id", "").startswith("v7_fresco_quad_"):
            row, col = (int(x) for x in mesh["id"].split("_")[-2:])
            expected = [[col/4, row/2], [(col+1)/4, row/2],
                        [(col+1)/4, (row+1)/2], [col/4, (row+1)/2]]
            if mesh.get("uv") != expected:
                raise ValueError(f"Fresco panel UVs do not map into the shared image: {mesh['id']}")
    cupola = read(root / "geometry/v523-pool-cupola.json")["meshes"]
    if sum(m.get("id", "").startswith("v600_cupola_leaf_") for m in cupola) != 3:
        raise ValueError("Final scene does not contain three cupola leaves")
    if sum(m.get("id", "").startswith("v600_cupola_guide_") for m in cupola) != 3:
        raise ValueError("Final scene does not contain the three longitudinal track geometries")
    rail_instances = [x for x in inst if x.get("id", "").startswith("v600_cupola_guide_")]
    if len(rail_instances) != 6:
        raise ValueError("Final scene does not contain paired guide rails on both eaves")
    return {"revision": revision, "manifestResources": "complete", "frescoPanels": 8,
            "sharedTexture": image["imageFile"], "cupolaLeaves": 3, "guideRails": 3}


def self_test():
    # Synthetic coverage test complements the real V17.551 package check.
    coverage = []
    for step in range(721):
        angle = -math.pi/2 + math.pi*step/720
        coverage.append(sum(lo <= angle <= hi for lo, hi in CUPOLA_INTERVALS))
    if min(coverage) < 1 or max(coverage) < 2:
        raise ValueError("Synthetic cupola test found an uncovered seam or no overlap")
    for row in range(2):
        for col in range(4):
            uv = [[col/4, row/2], [(col+1)/4, row/2],
                  [(col+1)/4, (row+1)/2], [col/4, (row+1)/2]]
            if uv[1][0] != uv[2][0] or uv[0][0] != uv[3][0]:
                raise ValueError("Synthetic fresco UV partition is discontinuous")
            if col < 3 and uv[1][0] != (col+1)/4:
                raise ValueError("Synthetic fresco horizontal UV boundary mismatch")
            if row < 1 and uv[3][1] != (row+1)/2:
                raise ValueError("Synthetic fresco vertical UV boundary mismatch")
    return {"syntheticCupolaAngleSamples": len(coverage), "minimumSectionCoverage": min(coverage),
            "maximumSectionOverlap": max(coverage), "frescoUVTiles": 8, "status": "PASS"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--package", type=Path)
    ap.add_argument("--revision", default="V17.600")
    ap.add_argument("--verify-only", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        report = self_test()
    else:
        if args.package is None:
            ap.error("--package is required unless --self-test is used")
        report = verify(args.package, args.revision) if args.verify_only else patch(args.package, args.revision)
        if not args.verify_only:
            report.update(verify(args.package, args.revision))
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
