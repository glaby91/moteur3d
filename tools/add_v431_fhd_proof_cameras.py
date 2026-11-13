#!/usr/bin/env python3
"""V17.431: isolate twelve Full HD proof cameras from the V17.430 corrected scene.

Only the distributed camera JSON and a new report are changed. No geometry,
material, painting, renderer, or current 3x navigation code is altered.
"""
import argparse
import hashlib
import json
from pathlib import Path

NEW_CAMERAS = [
    dict(id="v9_guardrail_closed", name="01 Guardrail former gap closed",
         position=[-15, 6.25, -8.6], target=[-15, 5.55, -13.7], fovDegrees=69),
    dict(id="v9_joists_removed", name="02 Unsupported roof joists removed",
         position=[0, 5.2, -33.2], target=[-5, 17.25, -32], fovDegrees=76),
    dict(id="v9_clearance_overview", name="07 Opposite ends starting blocks and ladders",
         position=[0, 15.2, -32], target=[0, -0.55, -32], fovDegrees=126),
    dict(id="v9_benches_removed", name="08 No wooden benches or feet",
         position=[-7.5, 2.55, -15.2], target=[-9, 0.8, -31], fovDegrees=82),
]

PROOFS = [
    ("01", "Garde-corps ferme", "v9_guardrail_closed", 14.75),
    ("02", "Poutres parasites supprimees", "v9_joists_removed", 14.75),
    ("03", "Escalier trois niveaux", "v8_three_level_stairs", 14.75),
    ("04", "Montants et vitrages raccordes", "v8_glazing_raccords", 14.75),
    ("05", "Eau et echelles inox", "v8_ladder_stainless", 14.75),
    ("06", "Plots numeros 1 a 10", "v8_pool_blocks", 14.75),
    ("07", "Separation plots et echelles", "v9_clearance_overview", 14.75),
    ("08", "Bancs et supports retires", "v9_benches_removed", 14.75),
    ("09", "Six luminaires de plafond", "v8_skylight_lighting", 19.00),
    ("10", "Vue generale piscine", "v4_piscine", 14.75),
    ("11", "Vue generale fresque conservee", "v7_pool_fresco", 14.75),
    ("12", "Verriere en journee", "v8_skylight_lighting", 14.75),
]

def main(root: Path):
    assert (root / "scene-v7-corrections.json").exists()
    report_path = root / "reports/v8-nine-anomalies.json"
    report = json.loads(report_path.read_text(encoding="utf8"))
    art = root / "textures/energetik_fresco_mosaic.jpg"
    before_hash = hashlib.sha256(art.read_bytes()).hexdigest()
    assert before_hash == report["frescoOriginalSha256"]
    cam_path = root / "cameras/v8-nine-anomalies.json"
    original = cam_path.read_bytes()
    old_file = root / "cameras/v8-nine-anomalies-before-fhd.json"
    old_file.write_bytes(original)
    cameras = json.loads(original)["cameras"]
    ids = {c["id"] for c in cameras}
    for camera in NEW_CAMERAS:
        assert camera["id"] not in ids, camera["id"]
        ids.add(camera["id"])
        cameras.append(camera)
    for number, label, camera, hour in PROOFS:
        assert camera in ids, (number, label, camera)
    assert len(PROOFS) == 12
    assert len({v[0] for v in PROOFS}) == 12
    output = json.loads(original)
    output["cameras"] = cameras
    cam_path.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    assert hashlib.sha256(art.read_bytes()).hexdigest() == before_hash
    manifest = {
        "campaign": "CSE WARP V17.431 — 12 genuine 1920x1080 DX11 WARP photographs",
        "scene": "V17.430 geometry / V8 nine-anomaly corrections",
        "resolution": [1920, 1080],
        "texture_resolution": 2048,
        "raster_backend": "Microsoft DirectX11 software WARP",
        "frescoSha256": before_hash,
        "originalCameraSha256": hashlib.sha256(original).hexdigest(),
        "proofs": [
            dict(index=number, anomaly=label, cameraId=camera,
                 lightingHour=hour, file=f"{number}-{camera}-FHD.png")
            for number, label, camera, hour in PROOFS
        ],
    }
    output = root / "reports/v9-fhd-12-proofs.json"
    output.parent.mkdir(exist_ok=True, parents=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf8")
    print("PASS: 12 FHD cameras; 9 defects + 3 overview images; fresco SHA-256 unchanged")
    print(json.dumps(manifest, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    main(parser.parse_args().root)
