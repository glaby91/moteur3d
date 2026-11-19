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


def ribbon_mesh(mesh_id, points, width, y):
    """Create a flat, continuous ribbon following a polyline in the XZ plane."""
    half = width * 0.5
    left, right = [], []
    for index, (x, z) in enumerate(points):
        before = points[max(0, index - 1)]
        after = points[min(len(points) - 1, index + 1)]
        dx, dz = after[0] - before[0], after[1] - before[1]
        length = math.hypot(dx, dz)
        if length < 1e-6:
            raise ValueError(f"Repeated path point in {mesh_id}")
        tx, tz = dx / length, dz / length
        nx, nz = -tz, tx
        if index and index + 1 < len(points):
            prev = points[index][0] - points[index - 1][0], points[index][1] - points[index - 1][1]
            nxt = points[index + 1][0] - points[index][0], points[index + 1][1] - points[index][1]
            lp, ln = math.hypot(*prev), math.hypot(*nxt)
            npx, npz = -prev[1] / lp, prev[0] / lp
            nnx, nnz = -nxt[1] / ln, nxt[0] / ln
            mx, mz = npx + nnx, npz + nnz
            ml = math.hypot(mx, mz)
            if ml > 1e-6:
                mx, mz = mx / ml, mz / ml
                denom = max(0.45, abs(mx * nnx + mz * nnz))
                nx, nz = mx / denom, mz / denom
        offset = min(half * 2.0, half * math.hypot(nx, nz))
        ox, oz = nx * offset, nz * offset
        left.append([round(x + ox, 5), round(y, 5), round(z + oz, 5)])
        right.append([round(x - ox, 5), round(y, 5), round(z - oz, 5)])
    vertices, uv, faces = [], [], []
    distance = 0.0
    for index, (a, b) in enumerate(zip(left, right)):
        if index:
            distance += math.dist(points[index - 1], points[index])
        vertices.extend((a, b))
        uv.extend(([0.0, distance], [1.0, distance]))
    for index in range(len(points) - 1):
        a, b = 2 * index, 2 * index + 1
        c, d = a + 2, a + 3
        # Up-facing triangles. Split at each polyline node so the paved route
        # remains continuous around each of its broad, walkable bends.
        faces.extend(([a, c, d], [a, d, b]))
    return {"id": mesh_id, "type": "triangle_mesh", "vertices": vertices,
            "uv": uv, "faces": faces}


def apply_garden_redesign(package: Path, revision: str):
    """Replace the weak straight strip with the approved route around the mound."""
    root = package / "benchmark-civic"
    geom_path = root / "geometry/v7-corrections.json"
    instances_path = root / "instances/v442-corrections.json"
    mats_path = root / "materials/v442-corrections.json"
    pbr_path = root / "pbr/v440-audit.json"
    lights_path = root / "lighting/v8-nine-anomalies.json"
    geometry, instances, materials, pbr, lighting = (
        read(geom_path), read(instances_path), read(mats_path), read(pbr_path), read(lights_path))

    old_geometry_ids = {"v600_civic_light_band", "v600_garden_red_inlay"}
    geometry["meshes"] = [m for m in geometry["meshes"] if
                           m.get("id") not in old_geometry_ids and
                           not m.get("id", "").startswith(("v600_garden_route_", "v600_garden_edge_"))]
    old_material_ids = {"v600_pale_paving", "v600_garden_red_inlay", "v600_garden_route", "v600_garden_edge"}
    materials["materials"] = [m for m in materials["materials"] if m.get("id") not in old_material_ids]
    pbr["sets"] = [m for m in pbr["sets"] if m.get("id") not in {
        "tex_v600_pale_paving", "tex_v600_garden_red_inlay", "tex_v600_garden_route", "tex_v600_garden_edge"}]

    instances["instances"] = [i for i in instances["instances"] if not (
        i.get("id", "").startswith(("v600_garden_tree_", "v600_garden_lamp_", "v600_garden_floor_light_",
                                      "v600_garden_red_inlay_", "v600_civic_light_band_",
                                      "v600_garden_route_", "v600_garden_edge_")) or
        i.get("parent") in ("Mobilier/Lampadaires", "Place/Eclairage/Lampadaires de lisière") or
        (i.get("id", "").startswith("rev_tree") and i.get("parent") == "Nature/Arbres medium-poly GLB"))]

    old_lamp_positions = {(round(side * 55.0, 3), round(z, 3))
                          for side in (-1, 1) for z in (28.0, 46.0, 64.0, 82.0)}
    lighting["lights"] = [light for light in lighting["lights"] if not (
        len(light.get("position", [])) == 3 and
        ((round(light["position"][0], 3), round(light["position"][2], 3)) in old_lamp_positions and
         abs(light["position"][1] - 3.45) < 0.02 or
         abs(light["position"][1] - 0.29) < 0.02 and abs(light["position"][0]) in (0.0, 38.0) and
         round(light["position"][2]) in (31, 64, 80, 108)))]

    route_specs = [
        ("south", [(0.0, 19.0), (0.0, 43.5)], 8.0),
        ("west", [(0.0, 43.5), (-15.0, 45.0), (-29.0, 49.0), (-37.0, 56.0),
                   (-39.0, 64.0), (-39.0, 80.0), (-37.0, 88.0), (-29.0, 95.0),
                   (-15.0, 99.0), (0.0, 100.0)], 6.0),
        ("east", [(0.0, 43.5), (15.0, 45.0), (29.0, 49.0), (37.0, 56.0),
                  (39.0, 64.0), (39.0, 80.0), (37.0, 88.0), (29.0, 95.0),
                  (15.0, 99.0), (0.0, 100.0)], 6.0),
        ("north", [(0.0, 100.0), (0.0, 122.0)], 8.0),
    ]
    routes = []
    for name, points, width in route_specs:
        route_id = f"v600_garden_route_{name}"
        route = ribbon_mesh(route_id, points, width, 0.168)
        geometry["meshes"].append(route)
        instances["instances"].append({"id": f"{route_id}_inst",
            "parent": "Place/Allée/Contourner la butte vers les marches",
            "geometry": route_id, "material": "v600_garden_route",
            "position": [0, 0, 0], "size": [1, 1, 1], "rotationY": 0})
        routes.append((name, points, width))
        for side in (-1, 1):
            edge_width = 0.55
            edge_points = _offset_path(points, side * (width * 0.5 - edge_width * 0.5))
            edge_id = f"v600_garden_edge_{name}_{'left' if side < 0 else 'right'}"
            geometry["meshes"].append(ribbon_mesh(edge_id, edge_points, edge_width, 0.178))
            instances["instances"].append({"id": f"{edge_id}_inst",
                "parent": "Place/Allée/Liseré rouge intégré",
                "geometry": edge_id, "material": "v600_garden_edge",
                "position": [0, 0, 0], "size": [1, 1, 1], "rotationY": 0})

    materials["materials"].extend([
        {"id": "v600_garden_route", "baseColor": [0.75, 0.72, 0.64],
         "roughness": 0.90, "metallic": 0.0, "transmission": 0.0, "ior": 1.5,
         "textureSet": "tex_v600_garden_route", "origin": "V17.600 warm stone allée"},
        {"id": "v600_garden_edge", "baseColor": [0.78, 0.10, 0.055],
         "roughness": 0.82, "metallic": 0.0, "transmission": 0.0, "ior": 1.5,
         "textureSet": "tex_v600_garden_edge", "origin": "V17.600 visible red brick inlay"},
    ])
    pbr["sets"].extend([
        {"id": "tex_v600_garden_route", "material": "v600_garden_route",
         "pattern": "smooth_plaster", "resolution": 512, "channels": ["albedo", "normal", "roughness"]},
        {"id": "tex_v600_garden_edge", "material": "v600_garden_edge",
         "pattern": "smooth_plaster", "resolution": 512, "channels": ["albedo", "normal", "roughness"]},
    ])

    # Ten mature trees frame the outer plaza in two measured groups. The center
    # axis, oval bench and two branches around the mound stay open and walkable.
    tree_positions = [(side * 51.0, z) for side in (-1, 1) for z in (31.0, 47.0, 97.0, 113.0)]
    tree_positions.extend([(-57.0, 71.0), (57.0, 71.0)])
    for n, (x, z) in enumerate(tree_positions):
        asset = "asset_tree1" if n % 2 == 0 else "asset_tree2"
        for part in (0, 1):
            instances["instances"].append({"id": f"v600_garden_tree_{n}_{part}",
                "parent": "Place/Jardins/Arbres de lisière",
                "geometry": f"{asset}_{part}",
                "material": f"asset_mat_{asset.removeprefix('asset_')}_{part}",
                "position": [x, 0.16, z], "size": [10.5, 13.0, 10.5],
                "rotationY": (n * 37.0) % 360})

    # Six low amber floor lights are set into the route. They replace the row
    # of tall posts and give a gentle guide without flooding the plaza.
    floor_lights = [(0.0, 31.0), (-38.0, 64.0), (38.0, 64.0),
                    (-38.0, 80.0), (38.0, 80.0), (0.0, 108.0)]
    for n, (x, z) in enumerate(floor_lights):
        instances["instances"].append({"id": f"v600_garden_floor_light_{n}",
            "parent": "Place/Eclairage/Points lumineux intégrés au sol",
            "geometry": "unit_sphere", "material": "lamp", "position": [x, 0.192, z],
            "size": [0.58, 0.055, 0.58], "rotationY": 0})
        lighting["lights"].append({"position": [x, 0.29, z], "radius": 6.0,
            "intensity": 0.30, "color": [255, 205, 150]})

    write(geom_path, geometry)
    write(instances_path, instances)
    write(mats_path, materials)
    write(pbr_path, pbr)
    write(lights_path, lighting)
    return {"walkwaySegments": len(route_specs), "walkwayWidthsMeters": [8.0, 6.0, 6.0, 8.0],
            "redInlayWidthMeters": 0.55, "redInlayStrips": 8,
            "gardenTrees": len(tree_positions), "groundLights": len(floor_lights),
            "moundAndOvalBenchPreserved": True, "entranceAxisClear": True}


def _offset_path(points, offset):
    """Return an offset polyline for an edge inlay, with bounded miter joins."""
    out = []
    for index, (x, z) in enumerate(points):
        before = points[max(0, index - 1)]
        after = points[min(len(points) - 1, index + 1)]
        dx, dz = after[0] - before[0], after[1] - before[1]
        length = math.hypot(dx, dz)
        nx, nz = -dz / length, dx / length
        out.append((x + nx * offset, z + nz * offset))
    return out


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
    # independently sampled/cropped image tiles. The D3D image path stores V=0
    # at the bottom edge, so reverse the source rows when assigning UV bands.
    fresco_meshes = [m for m in geometry["meshes"] if m.get("id", "").startswith("v7_fresco_quad_")]
    if len(fresco_meshes) != 8:
        raise ValueError(f"Expected eight delivered V7 fresco panels, found {len(fresco_meshes)}")
    for mesh in fresco_meshes:
        row, col = (int(x) for x in mesh["id"].split("_")[-2:])
        v0, v1 = 1 - (row + 1) / 2, 1 - row / 2
        mesh["uv"] = [[col / 4, v0], [(col + 1) / 4, v0],
                      [(col + 1) / 4, v1], [col / 4, v1]]
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
    shared["textureDetail"] = {"mode": "preserve", "minResolution": 2048}
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
        "v600_garden_view": "09 · Jardin, butte et façade",
    }
    garden_camera = locate(cameras["cameras"], "v600_garden_view")
    if garden_camera is None:
        garden_camera = {"id": "v600_garden_view"}
        cameras["cameras"].append(garden_camera)
    # Center the shot on the garden-to-palace axis from the garden side. This
    # keeps the hill, red transition strip, both tree rows, and full façade in
    # frame instead of looking diagonally across the plaza.
    garden_camera.update({"name": clean_names["v600_garden_view"],
        "position": [0.0, 18.0, 145.0], "target": [0.0, 8.0, 25.0],
        "fovDegrees": 78})
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

    # Remove the scattered exterior tree groups and replant a restrained,
    # symmetric line of broadleaf trees along the far edges of the plaza.
    # Keep the mound, oval seat, entrance axis, and the full façade sightline clear.
    before_trees = len(instances["instances"])
    instances["instances"] = [i for i in instances["instances"]
                               if not (i.get("id", "").startswith("rev_tree") and
                                       i.get("parent") == "Nature/Arbres medium-poly GLB")]
    removed_trees = before_trees - len(instances["instances"])
    if removed_trees != 52:
        raise ValueError(f"Expected to remove 26 exterior trees (52 meshes), removed {removed_trees}")
    garden_trees = []
    for side in (-1, 1):
        for n, z in enumerate((27.0, 52.0, 77.0)):
            asset = "asset_tree1" if n != 1 else "asset_tree2"
            for part in (0, 1):
                garden_trees.append({"id": f"v600_garden_tree_{side}_{n}_{part}",
                    "parent": "Place/Jardins/Arbres de lisière",
                    "geometry": f"{asset}_{part}",
                    "material": f"asset_mat_{asset.removeprefix('asset_')}_{part}",
                    "position": [side * 60.0, 0.16, z], "size": [7.2, 7.2, 7.2],
                    "rotationY": (side * 13.0 + n * 29.0) % 360})
    instances["instances"].extend(garden_trees)

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

    # Replace scattered exterior lamp-post geometry and data with eight
    # matching warm fixtures on the plaza's outer contour.
    old_lamps = [i for i in instances["instances"]
                 if i.get("parent") == "Mobilier/Lampadaires"]
    instances["instances"] = [i for i in instances["instances"]
                               if i.get("parent") != "Mobilier/Lampadaires"]
    if len(old_lamps) < 16 or len(old_lamps) % 2:
        raise ValueError(f"Expected paired exterior lamp geometry, found {len(old_lamps)} instances")
    exterior_lights = [l for l in lighting["lights"]
                       if abs(abs(l.get("position", [0])[0]) - 65.0) < 0.01 and
                       20.0 <= l.get("position", [0, 0, 0])[2] <= 66.0]
    if len(exterior_lights) != 10:
        raise ValueError(f"Expected ten old exterior point lights, found {len(exterior_lights)}")
    lighting["lights"] = [l for l in lighting["lights"] if l not in exterior_lights]
    garden_lamp_positions = [(side * 55.0, z) for side in (-1, 1) for z in (28.0, 46.0, 64.0, 82.0)]
    for n, (x, z) in enumerate(garden_lamp_positions):
        instances["instances"].append({"id": f"v600_garden_lamp_{n}_post",
            "parent": "Place/Eclairage/Lampadaires de lisière", "geometry": "unit_box",
            "material": "prp_steel_painted", "position": [x, 1.75, z],
            "size": [0.20, 3.5, 0.20], "rotationY": 0})
        instances["instances"].append({"id": f"v600_garden_lamp_{n}_globe",
            "parent": "Place/Eclairage/Lampadaires de lisière", "geometry": "unit_sphere",
            "material": "lamp", "position": [x, 3.58, z],
            "size": [0.42, 0.42, 0.42], "rotationY": 0})
        lighting["lights"].append({"position": [x, 3.45, z], "radius": 10.0,
            "intensity": 0.58, "color": [255, 224, 183]})

    # Thin red inlay across the plaza marks the transition between the palace
    # forecourt and garden. It sits just beyond the entrance stair run.
    if not locate(geometry["meshes"], "v600_garden_red_inlay"):
        geometry["meshes"].append({"id": "v600_garden_red_inlay", "type": "box",
                                   "size": [88.0, 0.045, 0.22], "uv": "planar_per_face"})
    materials["materials"].append({"id": "v600_garden_red_inlay", "baseColor": [0.58, 0.08, 0.055],
        "roughness": 0.78, "metallic": 0.0, "transmission": 0.0, "ior": 1.5,
        "textureSet": "tex_v600_garden_red_inlay", "origin": "V17.600 red plaza-to-garden inlay"})
    pbr["sets"].append({"id": "tex_v600_garden_red_inlay", "material": "v600_garden_red_inlay",
        "pattern": "smooth_plaster", "resolution": 512, "channels": ["albedo", "normal", "roughness"]})
    instances["instances"].append({"id": "v600_garden_red_inlay_inst",
        "parent": "Place/Transition vers la butte/Liseré rouge", "geometry": "v600_garden_red_inlay",
        "material": "v600_garden_red_inlay", "position": [0, 0.16, 34.0],
        "size": [1, 1, 1], "rotationY": 0})

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
    garden_report = apply_garden_redesign(package, revision)
    return {"revision": revision, "frescoPanels": len(fresco_meshes), "sharedFrescoImage": shared["imageFile"],
            "cupolaLeaves": len(sections), "cupolaGuideRails": 3, "relocatedPlanterMeshes": trees_moved,
            "exteriorTreesRemoved": removed_trees // 2,
            "gardenTreesReplanted": garden_report["gardenTrees"],
            "gardenLampPosts": 0, "redGardenInlay": True,
            "gardenPresentationCamera": "v600_garden_view", "sceneRevision": scene["revision"],
            "gardenRedesign": garden_report}


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
    if image.get("textureDetail") != {"mode": "preserve", "minResolution": 2048}:
        raise ValueError("Fresco high-resolution preservation policy is missing")
    for mesh in read(root / "geometry/v7-corrections.json")["meshes"]:
        if mesh.get("id", "").startswith("v7_fresco_quad_"):
            row, col = (int(x) for x in mesh["id"].split("_")[-2:])
            v0, v1 = 1 - (row + 1) / 2, 1 - row / 2
            expected = [[col/4, v0], [(col+1)/4, v0],
                        [(col+1)/4, v1], [col/4, v1]]
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
    garden_trees = [x for x in inst if x.get("id", "").startswith("v600_garden_tree_")]
    if len(garden_trees) != 20 or any(x.get("id", "").startswith("rev_tree") for x in inst):
        raise ValueError("Exterior trees were not replaced by ten controlled garden trees")
    available_meshes = {m.get("id") for rel in sector["geometry"]
                        for m in read(root / rel).get("meshes", [])}
    available_materials = {m.get("id") for rel in sector["materials"]
                           for m in read(root / rel).get("materials", [])}
    for tree in garden_trees:
        if tree.get("geometry") not in available_meshes or tree.get("material") not in available_materials:
            raise ValueError(f"Garden tree references missing scene data: {tree.get('id')}")
    garden_lamps = [x for x in inst if x.get("parent") == "Place/Eclairage/Points lumineux intégrés au sol"]
    if len(garden_lamps) != 6 or any(x.get("parent") in ("Mobilier/Lampadaires", "Place/Eclairage/Lampadaires de lisière") for x in inst):
        raise ValueError("Garden lighting must use six recessed ground fixtures without tall plaza posts")
    scene_geometry = [m for rel in sector["geometry"] for m in read(root / rel).get("meshes", [])]
    route_meshes = [m for m in scene_geometry if m.get("id", "").startswith("v600_garden_route_")]
    edge_meshes = [m for m in scene_geometry if m.get("id", "").startswith("v600_garden_edge_")]
    if len(route_meshes) != 4 or len(edge_meshes) != 8:
        raise ValueError("Garden route must contain four connected path segments and eight red edge strips")
    if not all(len(m.get("faces", [])) >= 2 for m in route_meshes + edge_meshes):
        raise ValueError("Garden route geometry contains an empty ribbon")
    expected_route_widths = {"south": 8.0, "west": 6.0, "east": 6.0, "north": 8.0}
    route_by_id = {m["id"]: m for m in route_meshes}
    for section, expected in expected_route_widths.items():
        mesh = route_by_id.get(f"v600_garden_route_{section}")
        if mesh is None or abs(math.dist(mesh["vertices"][0], mesh["vertices"][1]) - expected) > .03:
            raise ValueError(f"Garden route {section} is narrower than its approved design width")
    if any(abs(math.dist(m["vertices"][0], m["vertices"][1]) - .55) > .03 for m in edge_meshes):
        raise ValueError("Red garden inlays are not wide enough to read at the global camera distance")
    tree_sizes = [x["size"] for x in garden_trees]
    if any(size[1] < 12.9 for size in tree_sizes):
        raise ValueError("Garden tree scale is too small for the approved global view")
    garden_camera = locate(read(root / "cameras/v8-nine-anomalies.json")["cameras"], "v600_garden_view")
    if garden_camera is None:
        raise ValueError("Garden presentation camera is missing")
    if (garden_camera.get("position") != [0.0, 18.0, 145.0] or
            garden_camera.get("target") != [0.0, 8.0, 25.0]):
        raise ValueError("Garden camera must show the landscape from the garden side")
    return {"revision": revision, "manifestResources": "complete", "frescoPanels": 8,
            "sharedTexture": image["imageFile"], "cupolaLeaves": 3, "guideRails": 3,
            "gardenTrees": 10, "gardenGroundLights": 6, "gardenWalkwaySegments": 4,
            "redGardenEdgeStrips": 8, "redGardenInlay": True,
            "gardenCamera": "v600_garden_view"}


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
            v0, v1 = 1 - (row + 1) / 2, 1 - row / 2
            uv = [[col/4, v0], [(col+1)/4, v0],
                  [(col+1)/4, v1], [col/4, v1]]
            if uv[1][0] != uv[2][0] or uv[0][0] != uv[3][0]:
                raise ValueError("Synthetic fresco UV partition is discontinuous")
            if col < 3 and uv[1][0] != (col+1)/4:
                raise ValueError("Synthetic fresco horizontal UV boundary mismatch")
            if row == 0 and (uv[0][1] != .5 or uv[2][1] != 1.0):
                raise ValueError("Synthetic fresco top row samples the wrong half of the source")
            if row == 1 and (uv[0][1] != 0.0 or uv[2][1] != .5):
                raise ValueError("Synthetic fresco bottom row samples the wrong half of the source")
    ribbon = ribbon_mesh("synthetic_route", [(0.0, 0.0), (0.0, 5.0), (2.0, 8.0)], 3.0, 0.2)
    if len(ribbon["faces"]) != 4 or min(v[1] for v in ribbon["vertices"]) != .2:
        raise ValueError("Synthetic path ribbon has missing faces or incorrect elevation")
    for face in ribbon["faces"]:
        a, b, c = (ribbon["vertices"][index] for index in face)
        ab = (b[0]-a[0], b[1]-a[1], b[2]-a[2])
        ac = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
        if ab[2]*ac[0] - ab[0]*ac[2] <= 0:
            raise ValueError("Synthetic path triangle is not oriented toward the upper surface")
    return {"syntheticCupolaAngleSamples": len(coverage), "minimumSectionCoverage": min(coverage),
            "maximumSectionOverlap": max(coverage), "frescoUVTiles": 8,
            "frescoTopBottomOrientation": "corrected", "routeRibbonFaces": len(ribbon["faces"]),
            "routeRibbonOrientation": "upward", "status": "PASS"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--package", type=Path)
    ap.add_argument("--revision", default="V17.600")
    ap.add_argument("--verify-only", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--garden-only", action="store_true",
                    help="apply the garden route only to an already finished final package")
    args = ap.parse_args()
    if args.self_test:
        report = self_test()
    else:
        if args.package is None:
            ap.error("--package is required unless --self-test is used")
        if args.verify_only:
            report = verify(args.package, args.revision)
        elif args.garden_only:
            report = apply_garden_redesign(args.package, args.revision)
        else:
            report = patch(args.package, args.revision)
        if not args.verify_only:
            report.update(verify(args.package, args.revision))
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
