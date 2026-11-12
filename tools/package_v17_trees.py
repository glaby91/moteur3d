#!/usr/bin/env python3
"""Assemble a validated V17 Windows distribution with the 10 Kenney CC0 tree pilots.
Called AFTER build-moteur3d-v17.yml has staged and deployed Qt in dist-v17.
"""
from __future__ import annotations
import argparse
import copy
import json
import shutil
from pathlib import Path

def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def write(path: Path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def bat(scene):
    return (
        '@echo off\r\n'
        'cd /d "%~dp0"\r\n'
        'set "M3D_V17_EXPERIMENT_Z_PREPASS="\r\n'
        'start "" "moteur3d_v17.exe" "benchmark-civic/' + scene + '"\r\n'
    )

def assemble(dist: Path, source: Path):
    dst = dist / "benchmark-civic"
    assert (dist / "moteur3d_v17.exe").is_file(), "Missing V17 application"
    assert (dist / "Qt6Core.dll").is_file(), "Missing Qt6 deployment"
    assert (dist / "moteur3d_v16_core.dll").is_file(), "Missing native V16 core"
    assert (dst / "scene-v4.json").is_file(), "Missing V4 scene"

    # The V4 generator already contains the corrected pool ladders.
    original = load(dst / "scene-v4.json")
    baseline = load(dst / "instances/v4.json")
    pilot = load(source / "instances/v4-trees-pilot.json")
    meshes = load(source / "geometry/trees-pilot.json")["meshes"]
    report = load(source / "reports/trees-pilot.json")
    tree_parts = [x for x in pilot["instances"] if
                  x.get("parent") == "Nature/Arbres pilotes GLB CC0"]
    assert len(baseline["instances"]) >= 9706, "Missing corrected V4 pool ladders"
    assert len(tree_parts) == 20 and report["trees"] == 10, "Incomplete pilot"
    assert len(meshes) == 20 and all(m["type"] == "triangle_mesh" for m in meshes), "Invalid meshes"
    assert sum(len(m["faces"]) for m in meshes) == report["triangles"]
    assert pilot["instances"][:len(baseline["instances"])] == baseline["instances"], "Pilot not generated from latest V4"

    for relative in ("geometry/trees-pilot.json", "instances/v4-trees-pilot.json",
                     "reports/trees-pilot.json"):
        target = dst / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)

    manifest = load(source / "scene-v4-trees-pilot.json")
    manifest["name"] = "CSE WARP V17 - V4 corrected + 10 imported CC0 Kenney trees"
    original["name"] = "CSE WARP V17 - V4 corrected, without pilot trees"
    write(dst / "scene-v4-sans-arbres.json", original)
    write(dst / "scene-v4-trees-pilot.json", manifest)
    # EXE defaultPath points to scene-v4.json: opening EXE alone now loads trees.
    write(dst / "scene-v4.json", manifest)

    for scene in ("scene-v4.json", "scene-v4-trees-pilot.json", "scene-v4-sans-arbres.json"):
        sector = load(dst / scene)["sectors"][0]
        for section in ("geometry","instances","materials","pbr","lighting","cameras","environment"):
            for dep in sector[section]:
                target = dst / dep
                assert target.is_file(), f"Missing {scene} dependency {target}"
                load(target)
    for launcher, scene in (
        ("DEMARRER_CSE_WARP_10_ARBRES.bat","scene-v4-trees-pilot.json"),
        ("DEMARRER_V4_SANS_ARBRES.bat","scene-v4-sans-arbres.json"),
        ("START_V4_REFERENCE.bat","scene-v4.json"),
    ):
        (dist / launcher).write_bytes(bat(scene).encode("ascii"))

    (dist / "LIRE_EN_PREMIER-10-ARBRES.txt").write_text(
        "CSE WARP V17 - 10 imported Kenney CC0 trees\n"
        "Extract all files to a new directory; run DEMARRER_CSE_WARP_10_ARBRES.bat.\n"
        "Double-clicking moteur3d_v17.exe also loads the tree scene.\n"
        "To compare the same V4 without trees, run DEMARRER_V4_SANS_ARBRES.bat.\n"
        "Normal launch disables the experimental Z-prepass (camera fall risk).\n"
        "Kenney Nature Kit 2.1 - CC0-1.0 - https://kenney.nl/assets/nature-kit\n",
        encoding="utf-8",
    )
    print("CSE WARP FULL TREES PACKAGE READY:", dist,
          "V4 instances:", len(baseline["instances"]), "tree parts:", len(tree_parts),
          "unique imported triangles:", report["triangles"])

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--dist", default="dist-v17")
    ap.add_argument("--source", default="scenes/v16/benchmark-civic")
    args = ap.parse_args()
    assemble(Path(args.dist), Path(args.source))
