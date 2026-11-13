#!/usr/bin/env python3
"""CSE WARP V17.443. Compare DX11 raster, MSAA resolve, staging copy and readback
over multiple cameras. Does not claim equivalence of screen FPS and readback FPS."""
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

def load(p):
    return json.loads(p.read_text(encoding="utf-8-sig"))

def med(values):
    return statistics.median(values)

def main(root_arg):
    root = Path(root_arg).resolve()
    manifest = load(root / "manifest.json")
    if isinstance(manifest, dict):
        manifest = [manifest]
    hardware = load(root / "hardware.json")
    rows = []
    for run in manifest:
        path = Path(run["summary"])
        if not path.is_file():
            options = list((root / f'P{int(run["pass"]):02d}_{run["id"]}').rglob("summary.json"))
            if len(options) != 1:
                raise RuntimeError("Missing/duplicate summary for " + run["id"])
            path = options[0]
        doc = load(path)
        if doc.get("system", {}).get("version") != "17.443":
            raise RuntimeError("Wrong benchmark version: " + str(path))
        q = doc["system"].get("measurementQuality", {})
        if int(q.get("msaaRequested", -1)) != int(run["msaa"]):
            raise RuntimeError("MSAA mismatch " + str(path))
        if int(q.get("readbackDepth", -1)) != int(run["readbackDepth"]):
            raise RuntimeError("Readback depth mismatch " + str(path))
        seen = set()
        for case in doc["cases"]:
            if case.get("status") != "ok":
                raise RuntimeError("Failed scene camera: " + str(case))
            cam = case.get("camera")
            if not cam or cam in seen:
                raise RuntimeError("Missing or duplicate camera: " + str(path))
            seen.add(cam)
            if not case.get("gpuTimingValid") or not case.get("gpuTransferSplitValid"):
                raise RuntimeError("Actual DX11 GPU timing split not supported: " + str(path))
            if int(case.get("frames", 0)) != int(run["requestedFrames"]):
                raise RuntimeError("Frame count mismatch: " + str(path))
            w, h = (int(x) for x in run["resolution"].split("x"))
            if (int(case["width"]), int(case["height"])) != (w, h):
                raise RuntimeError("Resolution mismatch: " + str(path))
            if int(case.get("readbackDepth", 0)) != int(run["readbackDepth"]):
                raise RuntimeError("Depth mismatch: " + str(path))
            transfer = float(case["gpuTransferMs"])
            resolve = float(case["gpuResolveMs"])
            stage = float(case["gpuStagingCopyMs"])
            if abs((resolve + stage) - transfer) > max(0.15, transfer * 0.15):
                raise RuntimeError("GPU split inconsistent: " + str(path))
            rows.append({
                "camera": cam, "variant": run["id"], "pass": int(run["pass"]),
                "resolution": run["resolution"], "pixels": w*h,
                "msaaRequested": int(run["msaa"]),
                "depth": int(run["readbackDepth"]),
                "frames": int(case["frames"]),
                "meanMs": float(case["meanMs"]),
                "p95Ms": float(case.get("p95Ms", 0)),
                "fpsEstimated": float(case.get("estimatedFps", 0)),
                "streamMeanMs": float(case.get("streamMeanMs", 0)),
                "streamFps": float(case.get("streamFps", 0)),
                "gpuDrawMs": float(case["gpuDrawMs"]),
                "gpuResolveMs": resolve,
                "gpuStagingCopyMs": stage,
                "gpuTransferMs": transfer,
                "gpuTotalMs": float(case["gpuTotalMs"]),
                "cpuMapWaitMs": float(case.get("cpuMapWaitMs", 0)),
                "cpuPixelConvertMs": float(case.get("cpuPixelConvertMs", 0)),
                "triangles": float(case.get("submittedTriangles", 0)),
                "drawCalls": float(case.get("gpuDrawCalls", 0)),
                "src": str(path.relative_to(root))
            })
    if not rows:
        raise RuntimeError("No valid samples")
    cameras = sorted({x["camera"] for x in rows})
    variants = sorted({x["variant"] for x in rows})
    n_passes = len({x["pass"] for x in rows})
    for cam in cameras:
        for variant in variants:
            cases = [x for x in rows if x["camera"] == cam and x["variant"] == variant]
            if len(cases) != n_passes:
                raise RuntimeError(f"Incomplete repeats for camera={cam} variant={variant}")
    if len(cameras) < 2:
        raise RuntimeError("Need multiple scene camera views")
    out = root / "msaa_multicam_raw.csv"
    with out.open("w", encoding="utf-8", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=rows[0].keys())
        wr.writeheader()
        wr.writerows(rows)
    groups = defaultdict(list)
    for x in rows:
        groups[(x["camera"], x["variant"])].append(x)
    agg = []
    for (cam,variant), samples in sorted(groups.items()):
        a = {"camera": cam, "variant": variant, "repetitions": len(samples),
             "msaa": samples[0]["msaaRequested"], "depth": samples[0]["depth"],
             "resolution": samples[0]["resolution"]}
        for metric in ("meanMs","p95Ms","streamMeanMs","gpuDrawMs","gpuResolveMs",
                       "gpuStagingCopyMs","gpuTransferMs","gpuTotalMs","cpuMapWaitMs",
                       "cpuPixelConvertMs","triangles","drawCalls"):
            a[metric] = round(med([x[metric] for x in samples]), 4)
        a["fpsFromMedianLatency"] = round(1000.0/a["meanMs"], 3) if a["meanMs"] else 0
        a["streamFpsFromMedian"] = round(1000.0/a["streamMeanMs"], 3) if a["streamMeanMs"] else 0
        agg.append(a)
    for a in agg:
        base = next(x for x in agg if x["camera"] == a["camera"] and x["variant"] == "MSAA2_DEPTH1")
        a["deltaFrameVsSameCameraRefPct"] = round(100*(a["meanMs"]/base["meanMs"]-1), 2)
        a["deltaDrawVsSameCameraRefMs"] = round(a["gpuDrawMs"]-base["gpuDrawMs"], 3)
        a["deltaResolveVsSameCameraRefMs"] = round(a["gpuResolveMs"]-base["gpuResolveMs"], 3)
        a["deltaCopyVsSameCameraRefMs"] = round(a["gpuStagingCopyMs"]-base["gpuStagingCopyMs"], 3)
    with (root / "msaa_multicam_summary.csv").open("w", encoding="utf-8", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=agg[0].keys())
        wr.writeheader()
        wr.writerows(agg)
    caveats = [
      "gpuResolveMs measures the actual DX11 resolve for MSAA 2x/4x/8x and is zero on single-sample render; it does not measure MSAA render-target bandwidth consumed during drawing.",
      "gpuStagingCopyMs measures DX11 CopyResource GPU execution, not the CPU Map wait or pixel conversion; these are logged separately.",
      "Depth=3 may improve stream throughput without shortening frame latency; compare streamMeanMs with depth=1 in matched camera and quality.",
      "MSAA quality sample count is requested; hardware fallback may choose a lower actual level. Verify exact chosen count from the DX11 device when attributing unsupported levels.",
      "The multi-camera benchmark measures static snapshots and readback, not the live interactive exploration loop or its frame pacing.",
      "Installed video adapters may not identify the actual DirectX rendering adapter.",
      "FPS derived from readback latency is a diagnostic estimate, not on-screen FPS."
    ]
    (root / "msaa_multicam_summary.json").write_text(json.dumps(
       {"benchmark":"CSE WARP V17.443 DX11 MSAA/resolve/multi-camera",
        "hardware": hardware, "cameras": cameras, "repetitions": n_passes,
        "variants": variants, "results": agg, "caveats": caveats},
       ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    lines = ["# Diagnostic MSAA et transfert GPU — CSE WARP V17.443",
             "", f"{len(cameras)} caméras, {len(variants)} configurations, {n_passes} répétitions.",
             "", "| Caméra | Variante | ms/image | GPU dessin | MSAA resolve | Copie GPU | CPU Map | Δ temps vs ref |",
             "|---|---|---:|---:|---:|---:|---:|---:|"]
    for a in agg:
        lines.append(f'| {a["camera"]} | {a["variant"]} | {a["meanMs"]:.2f} | '
                     f'{a["gpuDrawMs"]:.2f} | {a["gpuResolveMs"]:.2f} | '
                     f'{a["gpuStagingCopyMs"]:.2f} | {a["cpuMapWaitMs"]:.2f} | '
                     f'{a["deltaFrameVsSameCameraRefPct"]:+.1f}% |')
    lines.extend(["", "## Limites"])
    lines += ["- "+x for x in caveats]
    (root/"msaa_multicam_report.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
    print("\n".join(lines))
if __name__ == "__main__":
    if len(sys.argv)!=2:
        raise SystemExit("Usage: python tools/analyze_msaa_multicam_v443.py DIR")
    main(sys.argv[1])
