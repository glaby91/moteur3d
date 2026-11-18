#!/usr/bin/env python3
"""CPU geometry-only light-transfer audit for the packaged V17 pool scene.

This does not pretend to reproduce the renderer's BSDF or GI. It answers the
preceding geometric question first: can paths scattered from the fresco reach
the water or cupola glass without another scene surface blocking them? It
writes an SVG side section, per-ray CSV and machine-readable JSON report.
Only Python's standard library is required.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
from collections import Counter
from pathlib import Path


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def vadd(a, b): return tuple(a[i] + b[i] for i in range(3))
def vsub(a, b): return tuple(a[i] - b[i] for i in range(3))
def vmul(a, s): return tuple(x * s for x in a)
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def norm(a):
    d = math.sqrt(dot(a, a))
    return tuple(x / d for x in a) if d > 1e-12 else (0.0, 0.0, 0.0)


def transform(p, inst):
    scale = inst.get("size", [1, 1, 1])
    pos = inst.get("position", [0, 0, 0])
    angle = math.radians(float(inst.get("rotationY", 0)))
    x, y, z = p[0]*scale[0], p[1]*scale[1], p[2]*scale[2]
    return (pos[0] + x*math.cos(angle) + z*math.sin(angle),
            pos[1] + y,
            pos[2] - x*math.sin(angle) + z*math.cos(angle))


def tri_hit(origin, direction, tri, limit):
    a, b, c = tri[0], tri[1], tri[2]
    e1, e2 = vsub(b, a), vsub(c, a)
    h = cross(direction, e2)
    det = dot(e1, h)
    if -1e-9 < det < 1e-9:
        return None
    inv = 1.0 / det
    s = vsub(origin, a)
    u = inv * dot(s, h)
    if u < -1e-7 or u > 1.0000001:
        return None
    q = cross(s, e1)
    vv = inv * dot(direction, q)
    if vv < -1e-7 or u + vv > 1.0000001:
        return None
    t = inv * dot(e2, q)
    return t if 1e-4 < t < limit else None


def box_faces():
    v = [(-.5,-.5,-.5),(.5,-.5,-.5),(.5,.5,-.5),(-.5,.5,-.5),
         (-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]
    faces = [(0,1,2),(0,2,3),(4,6,5),(4,7,6),
             (0,4,5),(0,5,1),(1,5,6),(1,6,2),
             (2,6,7),(2,7,3),(3,7,4),(3,4,0)]
    return v, faces


def scene_triangles(package: Path, sector):
    meshes = {}
    for rel in sector.get("geometry", []):
        doc = read(package / rel)
        for mesh in doc.get("meshes", []):
            meshes[mesh["id"]] = mesh
    instances = []
    for rel in sector.get("instances", []):
        instances.extend(read(package / rel).get("instances", []))

    triangles, unsupported = [], Counter()
    region = (-32.0, 32.0, -2.0, 26.0, -57.0, -8.0)  # x, y, z window around pool
    unit_vertices, unit_faces = box_faces()
    for inst in instances:
        mesh = meshes.get(inst.get("geometry"))
        mesh_type = mesh.get("type") if mesh else inst.get("geometry")
        if mesh_type == "box" or inst.get("geometry") == "unit_box":
            local_vertices, faces = unit_vertices, unit_faces
        elif mesh_type == "triangle_mesh":
            local_vertices = mesh.get("vertices", [])
            faces = mesh.get("faces", [])
        else:
            unsupported[str(mesh_type)] += 1
            continue
        world = [transform(p, inst) for p in local_vertices]
        for face in faces:
            if len(face) < 3:
                continue
            for j in range(1, len(face)-1):
                tri = (world[face[0]], world[face[j]], world[face[j+1]])
                xs = [p[0] for p in tri]; ys = [p[1] for p in tri]; zs = [p[2] for p in tri]
                if (max(xs) < region[0] or min(xs) > region[1] or
                    max(ys) < region[2] or min(ys) > region[3] or
                    max(zs) < region[4] or min(zs) > region[5]):
                    continue
                triangles.append((*tri, inst.get("id", "?"), inst.get("material", "?"),
                                  inst.get("parent", "")))
    if not triangles:
        raise ValueError("No supported triangles found in the pool audit region")
    return triangles, instances, unsupported


def first_hit(origin, direction, triangles, max_distance, source_id):
    nearest = max_distance
    result = None
    for tri in triangles:
        if tri[3] == source_id:
            continue
        t = tri_hit(origin, direction, tri[:3], nearest)
        if t is not None:
            nearest = t
            result = (tri, t)
    return result


def sample_points(instances, triangles):
    fresco = next((i for i in instances if i.get("material") == "cse_energetik_fresco"), None)
    water = next((i for i in instances if i.get("material") == "cse_pool_water"), None)
    glass = next((i for i in instances if i.get("material") == "v8_clear_roof_glass"), None)
    if not fresco or not water or not glass:
        raise ValueError(f"Required targets missing: fresco={bool(fresco)}, water={bool(water)}, glass={bool(glass)}")

    fp, fs = fresco["position"], fresco["size"]
    # The mural's pool-facing face is +Z; take six interior points, offset
    # toward the room so their rays do not begin on the source triangles.
    sources = []
    for fy in (.25, .75):
        for fx in (.2, .5, .8):
            p = transform((fx-.5, fy-.5, .5005), fresco)
            sources.append((p, f"fresco_u{fx:.1f}_v{fy:.2f}"))

    wp, ws = water["position"], water["size"]
    water_targets = []
    for fx in (.2, .5, .8):
        for fz in (.2, .4, .6, .8):
            p = transform((fx-.5, .5001, fz-.5), water)
            water_targets.append((p, water["id"], "water"))

    glass_tris = [t for t in triangles if t[3] == glass["id"]]
    if not glass_tris:
        raise ValueError("Cupola glass instance has no triangles in the audit region")
    stride = max(1, len(glass_tris)//24)
    glass_targets = []
    for tri in glass_tris[::stride][:24]:
        p = tuple(sum(v[k] for v in tri[:3])/3.0 for k in range(3))
        glass_targets.append((p, glass["id"], "cupola_glass"))
    return sources, water_targets + glass_targets, fresco


def audit(package: Path, output: Path):
    scene = read(package / "scene.json")
    sectors = scene.get("sectors", [])
    if len(sectors) != 1:
        raise ValueError(f"Expected one packaged scene sector, found {len(sectors)}")
    triangles, instances, unsupported = scene_triangles(package, sectors[0])
    sources, targets, fresco = sample_points(instances, triangles)
    counts = Counter(); blockers = Counter(); rows = []; svg_rays = []
    target_tests = 0
    for source, source_name in sources:
        for target, target_id, kind in targets:
            direction_vector = vsub(target, source)
            distance = math.sqrt(dot(direction_vector, direction_vector))
            direction = vmul(direction_vector, 1.0/distance)
            origin = vadd(source, (0.0, 0.0, .015))
            hit = first_hit(origin, direction, triangles, distance+.02, fresco["id"])
            target_tests += 1
            if hit and hit[0][3] == target_id:
                status, obstruction, endpoint = "VISIBLE_PATH", kind, target
                counts[kind] += 1
            elif hit:
                status, obstruction = "BLOCKED", f"{hit[0][4]} | {hit[0][5]}"
                endpoint = vadd(origin, vmul(direction, hit[1]))
                blockers[obstruction] += 1
            else:
                status, obstruction, endpoint = "CLEAR_TO_TARGET", "none", target
                counts[kind] += 1
            rows.append({"source":source_name,"target_type":kind,"target_id":target_id,
                         "status":status,"first_obstruction":obstruction,
                         "source_x":source[0],"source_y":source[1],"source_z":source[2],
                         "target_x":target[0],"target_y":target[1],"target_z":target[2]})
            svg_rays.append((origin, endpoint, kind if status != "BLOCKED" else "blocked", status))

    gi_source_path = (Path(__file__).resolve().parent.parent /
        "experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_gi_bake.cpp")
    gi_source = gi_source_path.read_text(encoding="utf-8") if gi_source_path.is_file() else ""
    avg_mip_lookup = "colorMip.back().p.front()" in gi_source
    uv_aware_albedo = ("V albedo(const Scene& scene,int index," in gi_source or
                       "V albedo(const Scene& scene, int index," in gi_source)

    output.mkdir(parents=True, exist_ok=True)
    with (output / "fresco-ray-paths.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)

    report = {
        "scene_revision": scene.get("revision"),
        "scene_name": scene.get("name"),
        "camera_reference": {"position_m":[32.918,2.245,-23.540],
                             "heading_deg":274.6,"inclination_deg":11.3,"fov_deg":68},
        "method": "CPU ray/triangle visibility from six points on the pool-facing fresco face to the packaged water and cupola-glass surfaces; renderer shading is not evaluated",
        "geometry_triangles_tested": len(triangles),
        "instances_skipped_by_primitive_type": dict(unsupported),
        "fresco_instance": fresco.get("id"),
        "target_path_tests": target_tests,
        "visible_or_clear_paths_by_target": dict(counts),
        "blocked_paths": sum(blockers.values()),
        "top_blockers": [{"surface":k,"count":v} for k,v in blockers.most_common(12)],
        "cpu_gi_source_inspection": {
            "source_file": "experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_gi_bake.cpp",
            "found_coarsest_mip_first_texel_lookup": avg_mip_lookup,
            "albedo_function_accepts_hit_uv": uv_aware_albedo,
            "finding": ("GI albedo lookup uses one coarsest-mip texel per material; mural pixel colors are averaged away in the GI bounce."
                        if avg_mip_lookup and not uv_aware_albedo else
                        "Review the current GI albedo source path; the simple average-mip pattern was not confirmed."),
        },
        "interpretation": (
            "At least one geometric path reaches the target; investigate CPU GI/BSDF texture transport if the rendered color bounce is absent."
            if counts else
            "All sampled paths are occluded; scene geometry blocks direct fresco-to-target transfer in this sample set."
        ),
        "limitations": [
            "This is a geometric visibility test, not the engine's shading or Monte Carlo GI image.",
            "It does not treat glass refraction/reflection, Fresnel, water transmission, or diffuse BRDF energy.",
            "Sphere and foliage primitives are reported as skipped; structural boxes and triangle meshes are included."
        ]
    }
    (output / "fresco-ray-report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    write_svg(output / "fresco-ray-bundle.svg", svg_rays, sources, targets)
    print(json.dumps(report,ensure_ascii=False,indent=2))
    if not any(r["status"] == "VISIBLE_PATH" for r in rows):
        raise SystemExit("FAIL: no direct geometric path from the fresco reached water or cupola glass")


def write_svg(path, rays, sources, targets):
    w,h=1400,760; left,right,top,bottom=90,30,55,75
    zmin,zmax=-58.0,-8.0; ymin,ymax=-2.0,28.0
    def xy(p):
        return (left+(p[2]-zmin)/(zmax-zmin)*(w-left-right),
                top+(ymax-p[1])/(ymax-ymin)*(h-top-bottom))
    pieces=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
            '<rect width="100%" height="100%" fill="#101820"/>',
            '<text x="90" y="30" fill="#eaf4fb" font-size="20" font-family="sans-serif">CSE WARP — faisceaux géométriques depuis la fresque</text>']
    # Reference surfaces in the same z/y section. Rays are projected; exact
    # first-hit decisions were computed in 3D, not in this drawing.
    wx1,wy1=xy((0,.18,-44.5)); wx2,wy2=xy((0,.18,-19.5))
    pieces.append(f'<line x1="{wx1:.1f}" y1="{wy1:.1f}" x2="{wx2:.1f}" y2="{wy2:.1f}" stroke="#34c9d4" stroke-width="9"/>')
    fx1,fy1=xy((0,.57,-54.11)); fx2,fy2=xy((0,4.35,-54.11))
    pieces.append(f'<line x1="{fx1:.1f}" y1="{fy1:.1f}" x2="{fx2:.1f}" y2="{fy2:.1f}" stroke="#f4c96b" stroke-width="12"/>')
    # Elliptical cupola section from the exact installed dimensions.
    arch=[]
    for i in range(65):
        a=-math.pi/2+math.pi*i/64
        z=-30+17*math.sin(a); y=19.9+3.35*math.cos(a)
        x,y=xy((0,y,z)); arch.append(f'{x:.1f},{y:.1f}')
    pieces.append(f'<polyline points="{" ".join(arch)}" fill="none" stroke="#77b9ff" stroke-width="7"/>')
    color={"water":"#42d8e5","cupola_glass":"#87bdff","blocked":"#ff6d60"}
    for a,b,kind,status in rays:
        x1,y1=xy(a);x2,y2=xy(b)
        pieces.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color[kind]}" stroke-width="1.15" opacity=".55"/>')
    for p,_ in sources:
        x,y=xy(p);pieces.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="#fff0a3"/>')
    pieces += ['<text x="100" y="720" fill="#f4c96b" font-size="15">Fresque</text>',
               '<text x="470" y="720" fill="#42d8e5" font-size="15">Surface de l’eau</text>',
               '<text x="890" y="720" fill="#87bdff" font-size="15">Verrière de la coupole</text>',
               '<text x="1080" y="720" fill="#ff6d60" font-size="15">Rayon bloqué</text>',
               '<text x="90" y="745" fill="#aebfcb" font-size="12">Projection en coupe Y-Z ; intersections calculées en 3D sur la géométrie du paquet.</text>',
               '</svg>']
    path.write_text("\n".join(pieces)+"\n",encoding="utf-8")


def self_test():
    tri=((-.5,-.5,1),(.5,-.5,1),(0,.5,1))
    assert abs(tri_hit((0,0,0),(0,0,1),tri,10)-1.0)<1e-9
    assert tri_hit((2,0,0),(0,0,1),tri,10) is None
    print("fresco ray audit math self-test: PASS")


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--package",type=Path)
    ap.add_argument("--output",type=Path,default=Path("fresco-ray-audit"))
    ap.add_argument("--self-test",action="store_true")
    args=ap.parse_args()
    if args.self_test:
        self_test()
        if args.package is None: return
    if args.package is None:
        ap.error("--package is required unless only --self-test is used")
    audit(args.package,args.output)


if __name__ == "__main__":
    main()
