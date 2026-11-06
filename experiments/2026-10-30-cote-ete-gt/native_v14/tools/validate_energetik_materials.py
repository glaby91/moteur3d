"""Fail the build if room/material registry diverges from scene or PBR assets."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
registry = json.loads((ROOT / "energetik_room_materials.json").read_text(encoding="utf-8"))
scene = (ROOT / "scene_pripyat_central.js").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_energetik_room_pbr.py").read_text(encoding="utf-8")
names = set(re.findall(r'^    "([A-Za-z][A-Za-z0-9]+)": \(\(', generator, re.M))
special = {"energetikFresco", "glass", "windowdark", "windowlit", "cinemaScreen", "seatFabric", "seatFabricClean", "neon", "sign", "lamp"}
errors = []
for room, roles in registry["rooms"].items():
    for role, entry in roles.items():
        mats = entry if isinstance(entry, list) else entry["materials"]
        status = entry.get("status") if isinstance(entry, dict) else "assigned"
        for material in mats:
            if material not in names and material not in special and material not in scene:
                errors.append(f"{room}.{role}: unknown material {material}")
            if status != "planned" and material not in scene and material not in special:
                errors.append(f"{room}.{role}: material {material} not referenced by scene")
for material in ("floorTerrazzo", "floorCeramic", "floorParquet", "floorGym", "floorConcreteDust", "stageWood", "boxingCanvas", "boxingRope"):
    if material not in scene:
        errors.append(f"Missing room-specific assignment: {material}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Validated {len(registry['rooms'])} room groups and {len(names)} PBR material definitions")
