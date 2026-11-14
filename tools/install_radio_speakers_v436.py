#!/usr/bin/env python3
"""Non-destructive V17.436 addon for V17.430 / V17.434 portable scenes.

Adds ONLY four PA horn assemblies to the existing final V8 instance/material
files, leaving the nine repaired architectural objects, fresco texture and
scene manifest untouched. All positions come from speaker_layout.json.
"""
import argparse
import copy
import json
from pathlib import Path
import tempfile

BASE = Path(__file__).resolve().parents[1]
LAYOUT = BASE / "assets/audio/pripyat_radio/speaker_layout.json"
PREFIX = "Architecture/Palais/Sonorisation exterieure/V17.436"

def load(path):
    return json.loads(path.read_text(encoding="utf-8"))

def save(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def patch(root, scene_name="scene-v7-corrections.json"):
    root = Path(root)
    layout = load(LAYOUT)
    if len(layout["loudspeakers"]) != 4:
        raise ValueError("Exactly four PA loudspeakers required")
    scene_path = root / scene_name
    scene_before = scene_path.read_bytes()
    scene = json.loads(scene_before)
    sector = scene["sectors"][0]
    ipath = root / sector["instances"][0]
    mpath = root / sector["materials"][0]
    instances = load(ipath)
    materials = load(mpath)
    objects = instances["instances"]
    if any(o.get("parent", "").startswith(PREFIX) for o in objects):
        raise ValueError("Radio speakers already present; refusing duplicate installation")
    ids = {o["id"] for o in objects}
    material_by_id = {m["id"]: m for m in materials["materials"]}
    donors = [
        material_by_id.get("prp_steel_painted"),
        material_by_id.get("prp_concrete_raw"),
        next(iter(materials["materials"]), None),
    ]
    donor = next((m for m in donors if m is not None), None)
    if donor is None:
        raise ValueError("No compatible material template found")
    new_materials = [
        ("v436_radio_orange", [.78,.25,.11], .62, .55),
        ("v436_radio_dark", [.14,.17,.16], .84, .26),
        ("v436_radio_mount", [.41,.42,.40], .72, .67),
    ]
    for mid, color, roughness, metallic in new_materials:
        if mid in material_by_id:
            raise ValueError("Material already exists: " + mid)
        item = copy.deepcopy(donor)
        item.update(id=mid, baseColor=color, roughness=roughness, metallic=metallic)
        # Preserve the valid donor textureSet / all other original PBR keys.
        materials["materials"].append(item)
    counter = 0
    def add(parent, material, xyz, dims, geometry="unit_box"):
        nonlocal counter
        counter += 1
        name = f"v436_radio_{counter:03}"
        if name in ids:
            raise ValueError("Generated object ID collision " + name)
        if any(float(n) <= 0 for n in dims):
            raise ValueError("Invalid dimensions")
        objects.append({
            "id": name, "parent": parent, "geometry": geometry,
            "material": material,
            "position": [round(float(v), 3) for v in xyz],
            "size": [round(float(v), 3) for v in dims], "rotationY": 0
        })
    for sp in layout["loudspeakers"]:
        x, y, z = sp["position"]
        g = PREFIX + "/" + sp["id"]
        add(g+"/platine", "v436_radio_mount", (x,y,z-.37), (.33,.39,.08))
        add(g+"/bras", "v436_radio_mount", (x,y,z-.16), (.13,.17,.47))
        add(g+"/corps", "v436_radio_orange", (x,y,z+.12), (.47,.43,.45))
        add(g+"/col", "v436_radio_dark", (x,y-.06,z+.43), (.30,.24,.16))
        add(g+"/pavillon", "v436_radio_orange", (x,y-.10,z+.58), (.57,.38,.20))
        add(g+"/levre", "v436_radio_orange", (x,y-.16,z+.71), (.79,.47,.13))
        add(g+"/fond", "v436_radio_dark", (x,y-.16,z+.783), (.59,.31,.016))
        add(g+"/diffuseur", "v436_radio_mount", (x,y-.16,z+.807), (.14,.14,.055), "unit_sphere")
        for dx in (-.12,.12):
            for dy in (-.13,.13):
                add(g+"/boulons", "v436_radio_mount", (x+dx,y+dy,z-.322), (.045,.045,.04), "unit_sphere")
    if counter != 48:
        raise AssertionError("Expected 12 primitives per speaker")
    if len({o["id"] for o in objects}) != len(objects):
        raise ValueError("Duplicate instance IDs after patch")
    # No change to V8 architectural object entries / ordering.
    save(ipath, instances)
    save(mpath, materials)
    assert scene_path.read_bytes() == scene_before, "Historic scene manifest altered!"
    print(f"V17.436 OK: added {counter} pieces, 4 horns, kept scene manifest and existing geometry")
    return counter

def selftest():
    with tempfile.TemporaryDirectory() as folder:
        root = Path(folder)
        (root/"instances").mkdir()
        (root/"materials").mkdir()
        save(root/"scene-v7-corrections.json", {"sectors":[{
            "instances":["instances/v8.json"],"materials":["materials/v8.json"]}]})
        save(root/"instances/v8.json", {"instances":[{
            "id":"original", "parent":"Architecture/Palais/Colonnes",
            "geometry":"unit_box","material":"prp_steel_painted",
            "position":[0,0,0],"size":[1,1,1]}]})
        save(root/"materials/v8.json", {"materials":[{
            "id":"prp_steel_painted","baseColor":[.4,.4,.4],
            "roughness":.8,"metallic":.6,"textureSet":"tex_prp_steel_painted"}]})
        assert patch(root) == 48
        generated = load(root/"instances/v8.json")["instances"]
        assert len(generated)==49 and generated[0]["id"]=="original"
        assert len({x["parent"].split("/")[4] for x in generated[1:]})==4
        try:
            patch(root)
        except ValueError as e:
            assert "already present" in str(e)
        else:
            raise AssertionError("Duplicate installation was not rejected")
    print("V17.436 speaker addon self-test PASS")

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path)
    parser.add_argument("--self-test",action="store_true")
    args=parser.parse_args()
    if args.self_test:
        selftest()
    elif args.root:
        patch(args.root)
    else:
        parser.error("Provide --root or --self-test")
