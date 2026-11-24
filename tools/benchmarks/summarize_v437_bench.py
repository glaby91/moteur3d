#!/usr/bin/env python3
"""Combine V17.433 GPU trajectory A/B runs into a single V17.437 comparison.
Output is deterministic given the run summaries; no invented FPS or metrics.
"""
import argparse
import csv
import json
from pathlib import Path

PROFILES = ("default_z_on_aabb_off", "reference_z_off_aabb_off", "candidate_z_on_aabb_on")
COLUMNS = (
    "profile", "z_prepass", "light_aabb_cull", "samples",
    "median_render_ms", "p95_render_ms", "p99_render_ms",
    "mean_render_ms", "fps_during_calculation", "gpu_draw_mean_ms",
    "scene", "summary_file",
)

def collect(root):
    rows = []
    for profile in PROFILES:
        folder = root / profile
        files = sorted(folder.glob("**/summary.json"))
        if len(files) != 1:
            raise ValueError(f"{profile}: exactly one summary.json required (found {len(files)})")
        payload = json.loads(files[0].read_text(encoding="utf-8"))
        s = payload["summary"]
        expect_z = profile != "reference_z_off_aabb_off"
        expect_aabb = profile == "candidate_z_on_aabb_on"
        assert bool(s["zPrepassEnabled"]) == expect_z, f"{profile}: wrong z prepass"
        assert bool(s["lightAabbCullEnabled"]) == expect_aabb, f"{profile}: wrong light culling"
        if int(s["sampleFrames"]) < 4:
            raise ValueError(f"{profile}: not enough benchmark frames")
        rows.append(dict(zip(COLUMNS, (
            profile, expect_z, expect_aabb, s["sampleFrames"],
            s["medianWallMs"], s["p95WallMs"], s["p99WallMs"],
            s["meanWallMs"], s["fpsDuringCalculation"],
            s.get("gpuDrawMeanMs"), s.get("sceneFile", ""),
            str(files[0].relative_to(root))
        ))))
    return rows

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory
    rows = collect(root)
    with (root / "COMPARAISON-V17.437.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    (root / "COMPARAISON-V17.437.json").write_text(
        json.dumps({"format":"cse-warp-v17-437-benchmark-v1","profiles":rows},
                   indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("PASS: all three equivalent GPU routes compared, with separate P95/P99.")
    for r in rows:
        print(r["profile"], "median:", r["median_render_ms"],
              "p99:", r["p99_render_ms"], "ms")
    print("Actual UI presentation/frame pacing must be exported separately via Outils.")

if __name__ == "__main__":
    main()
