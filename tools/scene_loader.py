#!/usr/bin/env python3
"""Moteur3D modular scene manifest validator/assembler (reference tooling).
Usage: python tools/scene_loader.py scenes/pavilion_01/scene.json
No graphics backend is invoked. This is not yet the native Qt scene loader.
"""
import json
import sys
from pathlib import Path

class SceneError(Exception):
    pass

def _read(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SceneError(f"Cannot read {path}: {exc}") from exc

def _within(base, path):
    root = base.resolve()
    target = (root / path).resolve()
    if not target.is_relative_to(root):
        raise SceneError(f"Include escapes scene root: {path}")
    return target

def load_scene(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    root = manifest_path.parent
    manifest = _read(manifest_path)
    if manifest.get("schema_version") != "0.2.0":
        raise SceneError("Unsupported scene schema")
    result = {"scene_id": manifest["scene_id"], "objects": [], "lights": [], "cameras": [], "environment": {}}
    seen_files, seen_ids = set(), set()
    for inc in manifest.get("includes", []):
        path = _within(root, inc["path"])
        if path in seen_files:
            raise SceneError(f"Duplicate include: {path}")
        seen_files.add(path)
        part = _read(path)
        kind = inc["kind"]
        if kind in ("objects", "lights", "cameras"):
            for item in part.get(kind, []):
                identifier = item.get("id")
                if not identifier or identifier in seen_ids:
                    raise SceneError(f"Missing or duplicate entity id: {identifier}")
                seen_ids.add(identifier)
                result[kind].append(item)
        elif kind == "environment":
            for key, value in part.items():
                if key != "schema_version":
                    if key in result["environment"]:
                        raise SceneError(f"Duplicate environment key: {key}")
                    result["environment"][key] = value
        else:
            raise SceneError(f"Unknown include kind: {kind}")
    catalog_path = (root / manifest["asset_catalog"]).resolve()
    catalog = _read(catalog_path)
    asset_ids = {x["id"] for x in catalog.get("asset_definitions", [])}
    for obj in result["objects"]:
        if obj.get("asset") not in asset_ids:
            raise SceneError(f"Unknown asset: {obj.get('asset')}")
    result["asset_catalog"] = str(catalog_path)
    return result

if __name__ == "__main__":
    try:
        scene = load_scene(sys.argv[1])
        print(json.dumps({"scene_id": scene["scene_id"], "objects": len(scene["objects"]), "lights": len(scene["lights"]), "cameras": len(scene["cameras"]), "status": "manifest_valid_assets_not_loaded"}, indent=2))
    except (IndexError, SceneError, KeyError) as exc:
        print(f"Scene validation failed: {exc}", file=sys.stderr)
        sys.exit(1)
