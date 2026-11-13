#!/usr/bin/env python3
"""Analyze matched V17.443 diagnostic raster runs without inventing shader/ shadow attribution."""
from __future__ import annotations
import csv, hashlib, json, statistics, sys
from collections import defaultdict
from pathlib import Path

def median(xs):
    return statistics.median(xs) if xs else None
def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))
def main(directory):
    root=Path(directory).resolve()
    manifest=read_json(root/"manifest.json")
    hw=read_json(root/"hardware.json")
    rows=[]
    for trial in manifest:
        path=Path(trial["summary"])
        if not path.is_file():
            # The manifest may be moved from another computer with the data.
            paths=list((root / f'P{int(trial["pass"]):02d}_{trial["id"]}').rglob("summary.json"))
            if len(paths)!=1: raise RuntimeError(f"No unique summary for {trial['id']}/{trial['pass']}")
            path=paths[0]
        doc=read_json(path)
        if doc.get("system",{}).get("version")!="17.443":
            raise RuntimeError("Version mismatch: "+str(path))
        system=doc["system"]
        # Verify each HLSL flag reached the separate DX11 process.
        expected={
            "pixelShaderOptimized":not bool(trial.get("shaderLegacy")),
            "experimentalLightAabbCull":bool(trial.get("lightAabbCull")),
            "diagnosticNoShadowSampling":bool(trial.get("noShadowSampling")),
            "diagnosticNoMaterialDetail":bool(trial.get("noMaterialDetail"))
        }
        for name,value in expected.items():
            if system.get(name)!=value:
                raise RuntimeError(f"Missing/inconsistent shader flag {name} in {path}: got {system.get(name)!r}, expected {value}")
        quality=system.get("measurementQuality",{})
        if quality:
            for metadata,runkey in (("msaaRequested","msaa"),("anisotropy","anisotropy"),
                                    ("lodPercent","lodPercent")):
                if int(quality[metadata])!=int(trial[runkey]):
                    raise RuntimeError(f"Quality mismatch {metadata} {path}")
        cases=[c for c in doc.get("cases",[])
               if c.get("status")=="ok" and
               ("raster" in c.get("backend","").lower() or
                "d3d11" in c.get("backend","").lower()) and
               int(c.get("width",0))>0]
        if len(cases)!=1:
            raise RuntimeError(f"Expected a single successful raster case in {path}; got {len(cases)}")
        c=cases[0]
        if int(c["frames"])<6:
            raise RuntimeError(f"Insufficient measured frames in {path}")
        w,h=map(int,trial["resolution"].split("x"))
        if (c["width"],c["height"])!=(w,h):
            raise RuntimeError(f"Unexpected resolution in {path}")
        if not c.get("gpuTimingValid"):
            raise RuntimeError(f"GPU timer invalid, cannot attribute draw times: {path}")
        img=c.get("image","")
        imgpath=path.parent/img if img else None
        img_sha=(hashlib.sha256(imgpath.read_bytes()).hexdigest()
                 if imgpath and imgpath.is_file() else "")
        rows.append(dict(
          id=trial["id"],group=trial["group"],passIndex=trial["pass"],
          width=w,height=h,pixels=w*h,frames=int(c["frames"]),
          msaa=trial["msaa"],anisotropy=trial["anisotropy"],
          shaderLegacy=trial["shaderLegacy"],lightAabbCull=trial["lightAabbCull"],
          noShadowSampling=trial.get("noShadowSampling",0),
          noMaterialDetail=trial.get("noMaterialDetail",0),
          culling=trial["culling"],lodPercent=trial["lodPercent"],
          lodEnabled=trial["lodEnabled"],
          meanMs=float(c["meanMs"]),medianMs=float(c.get("medianMs",0)),
          p95Ms=float(c.get("p95Ms",0)),gpuDrawMs=float(c.get("gpuDrawMs",0)),
          gpuTransferMs=float(c.get("gpuTransferMs",0)),
          cpuSubmitMs=float(c.get("cpuSubmitMs",0)),
          cpuMapWaitMs=float(c.get("cpuMapWaitMs",0)),
          cpuPixelConvertMs=float(c.get("cpuPixelConvertMs",0)),
          triangles=float(c.get("submittedTriangles",0)),
          gpuDrawCalls=float(c.get("gpuDrawCalls",0)),
          shadowBuildMs=float(doc["system"].get("shadowBuildMs",0)),
          sceneBuildMs=float(doc["system"].get("sceneBuildMs",0)),
          imageSha256=img_sha,source=str(path.relative_to(root))
        ))
    grouped=defaultdict(list)
    for row in rows:grouped[row["id"]].append(row)
    baselines=grouped.get("REF",[])
    if not baselines: raise RuntimeError("Reference runs missing")
    ref_median=median([x["meanMs"] for x in baselines])
    totals=[]
    for case,runs in sorted(grouped.items()):
        value=median([r["meanMs"] for r in runs])
        draw=median([r["gpuDrawMs"] for r in runs])
        triangles=median([r["triangles"] for r in runs])
        calls=median([r["gpuDrawCalls"] for r in runs])
        totals.append(dict(
            id=case,group=runs[0]["group"],repetitions=len(runs),
            framesEach=sorted(set(r["frames"] for r in runs)),
            meanOfRunMeansMs=round(statistics.mean(r["meanMs"] for r in runs),3),
            medianOfRunMeansMs=round(value,3),
            minRunMeanMs=round(min(r["meanMs"] for r in runs),3),
            maxRunMeanMs=round(max(r["meanMs"] for r in runs),3),
            fpsFromMedianMean=round(1000/value,3),
            speedupVsReference=round(ref_median/value,4),
            changePercentVsReference=round(100*(value/ref_median-1),2),
            medianGpuDrawMs=round(draw,3),
            medianCpuMapWaitMs=round(median([r["cpuMapWaitMs"] for r in runs]),3),
            medianTriangles=round(triangles),
            medianDrawCalls=round(calls),
            medianShadowBuildMs=round(median([r["shadowBuildMs"] for r in runs]),3),
            imageHashesIdenticalWithinCase=(len(set(r["imageSha256"] for r in runs))==1)
        ))
    with (root/"diagnostic_raw.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=rows[0].keys())
        writer.writeheader();writer.writerows(rows)
    with (root/"diagnostic_summary.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=totals[0].keys())
        writer.writeheader();writer.writerows(totals)
    result=dict(
      engine="CSE WARP V17.443 - diagnostic des goulots",
      cpu=hw.get("cpus"),videoDevices=hw.get("displayControllers"),
      forcedSoftwareWarp=hw.get("softwareWarp"),
      referenceMedianFrameMs=round(ref_median,3),
      verifiedCases=len(totals),verifiedRuns=len(rows),
      caveats=[
        "Elapsed frame includes real GPU drawing and readback; estimated FPS is not Qt onscreen FPS.",
        "Pixel shader optimization and optional light AABB cull are real A/B flags, but not an isolated instruction profiler.",
        "GPU draw-time difference when changing resolution estimates pixel-related work, not a pure per-pixel shader cost.",
        "Frustum/LOD variants alter submitted geometry and sometimes the visible result; compare screenshots.",
        "shadowBuildMs measures CPU shadow-map generation. SHADOW_SAMPLING_OFF isolates nine texture taps in the pixel shader, not the shadow-building work or all effects of altered lighting.",
        "PBR_DETAIL_OFF isolates normal-map detail and roughness lookups but changes normals/highlights; do not treat results as visually equivalent.",
        "Two or three runs cannot establish long-run thermal stability or reliable p99; do not extrapolate to a different GPU.",
        "Installed graphics adapters may not identify the actual DX11 rendering adapter."
      ],
      cases=totals)
    (root/"diagnostic_summary.json").write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    with (root/"diagnostic_report.md").open("w",encoding="utf-8") as f:
        f.write("# CSE WARP V17.443 — benchmark de diagnostic\n\n")
        f.write("Comparaison : mediane des moyennes par execution ; une variable a la fois.\n\n")
        f.write("| Variante | Rép. | ms/image | FPS estimés | Delta % vs REF | GPU dessin (ms) | Triangles | Draw calls |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in totals:
            f.write(f"| {r['id']} | {r['repetitions']} | {r['medianOfRunMeansMs']:.1f} | "
                    f"{r['fpsFromMedianMean']:.2f} | {r['changePercentVsReference']:+.1f}% | "
                    f"{r['medianGpuDrawMs']:.1f} | {r['medianTriangles']} | {r['medianDrawCalls']} |\n")
        f.write("\n## Limites\n")
        for note in result["caveats"]: f.write(f"- {note}\n")
    print((root/"diagnostic_report.md").read_text(encoding="utf-8"))
if __name__=="__main__":
    if len(sys.argv)!=2: raise SystemExit("Usage: python tools/analyze_diagnostic_v443.py DIRECTORY")
    main(sys.argv[1])
