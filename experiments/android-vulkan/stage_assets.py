#!/usr/bin/env python3
"""Stage exact V17.634 and original GLSL; no generated substitute scene."""
import hashlib, json, pathlib, shutil, subprocess, sys, zipfile, urllib.request
repo = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(sys.argv[1]).resolve()
assets = out / "assets/cse"
assets.mkdir(parents=True, exist_ok=True)
release = "https://github.com/glaby91/moteur3d-scene/releases/download/scene-reflect-V17.634/"
work = out.parent / "scene-download"
work.mkdir(parents=True, exist_ok=True)
for name, digest in [
    ("V17-scene.zip", "f975639311e5b5d48ae996ac1f4f986994b232befb29617290f51bdf2bfbb703"),
    ("SCENE_VERSION.json", "77d501dc6afa54f2a1793d1c26da8c0cd1645c0cc554636b417ecfcdf7ebcb73")]:
    dest = work / name
    subprocess.run(["curl", "--fail", "--location", "--retry", "3", release + name, "-o", str(dest)], check=True)
    assert hashlib.sha256(dest.read_bytes()).hexdigest() == digest, name
with zipfile.ZipFile(work / "V17-scene.zip") as z:
    for item in z.infolist():
        target = (assets / item.filename).resolve()
        assert target.is_relative_to(assets), item.filename
    z.extractall(assets)
assert (assets/"SCENE_VERSION.json").read_bytes() == (work/"SCENE_VERSION.json").read_bytes()
assert json.loads((assets/"benchmark-civic/scene.json").read_text())["revision"] == "V17.634"
shaders = repo / "v17/vulkan/shaders"
spv = assets / "vulkan"
spv.mkdir(exist_ok=True)
for src, dest, flags in [
    *[(x, x+".spv", []) for x in ("raster.vert","raster.frag","sky.vert","sky.frag","raster_box.vert","raster_foliage.vert","atmosphere.frag")],
    ("atmosphere.frag","atmosphere_ms.frag.spv",["-DZATM_MSAA=1"]),
    ("raster.frag","raster_fast.frag.spv",["-DCSE_WARP_VK_FAST_FRAGMENT=1"])]:
    subprocess.run(["glslangValidator","-V","--target-env","vulkan1.3",*flags,str(shaders/src),"-o",str(spv/dest)],check=True)
    subprocess.run(["spirv-val","--target-env","vulkan1.3",str(spv/dest)],check=True)
(assets/"materials").mkdir(exist_ok=True)
shutil.copy2(repo/"v17/materials/zmaterial8192.json", assets/"materials/zmaterial8192.json")
shutil.copy2(repo/"engine/render_modules.json", assets/"render_modules.json")
files = sorted(p.relative_to(assets).as_posix() for p in assets.rglob("*") if p.is_file() and p.name != "index.txt")
assert all("\n" not in p and "\r" not in p for p in files)
(assets/"index.txt").write_text("\n".join(files)+"\n")
print("Staged",len(files),"files for exact scene V17.634 and Vulkan SPIR-V")
