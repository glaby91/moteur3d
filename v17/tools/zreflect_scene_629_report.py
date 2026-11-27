"""Validate and summarize the native, uncapped Z-Reflect diagnostic."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

TAG = "scene-V17.629-42"
REV = "V17.629"
SCENE_COMMIT = "b01abf27cf96ee47867918a513c9f71a7222a976"
ARCHIVE_SHA256 = "8d56480f93a268f8c9028e41fbbb8d6e20a9e3b34e150ec2f296e9b7abb9d9b6"
MANIFEST_SHA256 = "d25af0751f3ca6fe707c79b0d6cec413b503dce8635f3eac57d6d58b0e3180d6"

def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def require(condition, message):
    if not condition:
        raise ValueError(message)

def digest(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()

def verify_release(archive, manifest, root):
    require(digest(archive) == ARCHIVE_SHA256, "Unexpected scene archive digest")
    require(digest(manifest) == MANIFEST_SHA256, "Unexpected published manifest digest")
    meta = load(manifest)
    require(meta["revision"] == REV and meta["repository"] == "glaby91/moteur3d-scene", "Unexpected release")
    require(meta["sourceCommit"] == SCENE_COMMIT, "Unexpected scene source commit")
    require(meta["scene"] == "benchmark-civic/scene.json", "Unexpected scene path")
    require(load(root / "SCENE_VERSION.json") == meta, "Archive manifest mismatch")
    require(load(root / meta["scene"])["revision"] == REV, "Scene revision mismatch")
    return meta

def summarize(catalog, meta, engine_commit):
    require(catalog["revision"] == REV and catalog["catalogRevision"] == 3, "Wrong native catalogue revision")
    require(catalog["status"] == "diagnostic-only" and catalog["policy"]["cameraLimitApplied"] is False, "Unexpected active or capped diagnostic")
    counts = catalog["counts"]
    planes = catalog["planes"]
    objects = catalog["objects"]
    require(len(planes) == counts["planes"] and len(objects) == counts["eligibleObjects"], "Count mismatch")
    ids = {p["cameraId"] for p in planes}
    require(len(ids) == len(planes), "Duplicate planar camera ID")
    require(all(p["state"] == "DORMANT" for p in planes), "Unexpected active render slot")
    views = catalog["cameraSimulation"]
    require(bool(views), "No authored camera views")
    for view in views:
        proj = view["projections"]
        require(len(proj) == len(planes) and {p["cameraId"] for p in proj} == ids, "Incomplete projection inventory")
        require(all(p["projectedFraction"] >= 0 for p in proj), "Invalid projection")
        require(sum(p["selected"] for p in proj) == view["selectedUncapped"], "Selection mismatch")
        require(sum(p["projectedFraction"] > 0 for p in proj) == view["visible"], "Visibility mismatch")
    distribution = Counter(v["selectedUncapped"] for v in views)
    total = sum(v["selectedUncapped"] for v in views)
    return {
        "sceneTag": TAG, "sceneManifest": meta, "sceneArchiveSha256": ARCHIVE_SHA256,
        "engineCommit": engine_commit, "status": "CPU geometry diagnostic, no GPU timing",
        "counts": counts, "policy": catalog["policy"], "authoredViewCount": len(views),
        "selectedDistribution": dict(sorted(distribution.items())),
        "maxSelectedSimultaneous": max(distribution), "meanSelected": total / len(views),
        "viewsOverFutureLimit": sum(n for k,n in distribution.items() if k > catalog["policy"]["futureCameraLimit"]),
        "eligibleAuthorizedObjects": sum(o["authorized"] for o in objects),
        "planes": [{k:p[k] for k in ("cameraId","normal","offset","side","area","objects")} for p in planes],
        "views": views,
    }

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--root", type=Path, required=True)
    p.add_argument("--engine-commit", required=True)
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    meta = verify_release(args.archive, args.manifest, args.root)
    if args.output:
        report = summarize(load(args.root / "benchmark-civic/precomputed/zreflect-candidates.json"), meta, args.engine_commit)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({k:v for k,v in report.items() if k not in ("planes","views")}, indent=2))
    else:
        print("Verified exact scene release " + TAG)

if __name__ == "__main__":
    main()
