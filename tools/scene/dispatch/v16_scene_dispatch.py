#!/usr/bin/env python3
"""V16 modular scene manifest resolver and validator (Python 3 standard library)."""
import argparse
import json
from pathlib import Path

KINDS = ("geometry", "instances", "materials", "pbr", "lighting", "cameras", "environment")
class SceneError(ValueError):
    pass

def _read(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise SceneError(f"{path}: {exc}") from exc

def resolve(manifest):
    manifest = Path(manifest).resolve()
    root = manifest.parent
    doc = _read(manifest)
    if doc.get("format") != "m3d-scene-v16":
        raise SceneError("Unsupported scene format")
    sectors = doc.get("sectors")
    if not isinstance(sectors, list):
        raise SceneError("sectors must be a list")
    loaded = {}
    for sector in sectors:
        if not isinstance(sector, dict) or not isinstance(sector.get("id"), str):
            raise SceneError("Sector requires an id")
        sid = sector["id"]
        if sid in loaded:
            raise SceneError(f"Duplicate sector {sid}")
        resources = {}
        for kind in KINDS:
            refs = sector.get(kind, [])
            if not isinstance(refs, list):
                raise SceneError(f"{sid}.{kind} must be a list")
            resources[kind] = []
            for ref in refs:
                if not isinstance(ref, str):
                    raise SceneError(f"Invalid resource reference in {sid}.{kind}")
                target = (root / ref).resolve()
                if not target.is_relative_to(root):
                    raise SceneError(f"Path escapes scene root: {ref}")
                data = _read(target)
                if not isinstance(data, dict):
                    raise SceneError(f"Resource must be an object: {ref}")
                resources[kind].append({"path": ref, "data": data})
        loaded[sid] = resources
    return {"manifest": doc, "sectors": loaded}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest")
    parser.add_argument("--sector", action="append", help="Validate only selected sector IDs")
    args = parser.parse_args()
    scene = resolve(args.manifest)
    ids = args.sector or list(scene["sectors"])
    for sid in ids:
        if sid not in scene["sectors"]:
            parser.error(f"Unknown sector: {sid}")
        resources = scene["sectors"][sid]
        print(sid + ": " + ", ".join(f"{kind}={len(items)}" for kind, items in resources.items()))
    print("V16 manifest valid")

if __name__ == "__main__":
    try:
        main()
    except SceneError as exc:
        raise SystemExit(f"Scene validation failed: {exc}")
