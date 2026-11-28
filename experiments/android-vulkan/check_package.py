#!/usr/bin/env python3
"""Fail early on incorrect Qt deployment JSON; verify the actual APK and assets."""
import hashlib
import json
import pathlib
import sys
import tempfile
import zipfile

PACKAGE = "org.csewarp.androidprobe"

def need(condition, description):
    if not condition:
        raise ValueError(description)

def digest(file):
    with file as f:
        h = hashlib.sha256()
        for chunk in iter(lambda: f.read(1024*1024), b""):
            h.update(chunk)
        return h.hexdigest()

def deployment(data, abi, package):
    expected = {
        "sdkBuildToolsRevision": "36.0.0",
        "android-min-sdk-version": "36",
        "android-target-sdk-version": "36",
        "android-compile-sdk-version": "36",
        "android-package-name": PACKAGE,
        "application-binary": "cse_android_probe",
    }
    for key, value in expected.items():
        need(str(data.get(key, "")) == value, f"{key}: expected {value!r}, got {data.get(key)!r}")
    need(set(data.get("architectures", {})) == {abi}, "unexpected deployment ABI")
    need("libmoteur3d_v16_core" in str(data.get("android-extra-libs", "")), "engine library not declared")
    need(pathlib.Path(data.get("android-package-source-directory", "")).resolve() == package.resolve(),
         "incorrect package source directory")
    sdk = pathlib.Path(data["sdk"])
    need((sdk/"platforms/android-36/android.jar").is_file(), "android-36 SDK missing")
    for binary in ("aapt", "aapt2", "zipalign", "apksigner"):
        need((sdk/"build-tools/36.0.0"/binary).is_file(), f"Build Tools missing {binary}")
    need((package/"assets/cse/index.txt").is_file(), "asset index missing")

def verify_apk(apk, abi, package):
    root = package/"assets/cse"
    files = (root/"index.txt").read_text().splitlines()
    required = {"benchmark-civic/scene.json", "SCENE_VERSION.json", "vulkan/raster.vert.spv",
                "vulkan/raster.frag.spv", "vulkan/raster_fast.frag.spv", "materials/zmaterial8192.json"}
    need(required.issubset(files), "required original assets absent from index")
    with zipfile.ZipFile(apk) as z:
        names = z.namelist()
        need(len(names) == len(set(names)), "duplicate ZIP entry")
        for name in ("AndroidManifest.xml", "classes.dex"):
            need(name in names, f"missing {name}")
        libs = [n for n in names if n.startswith("lib/") and n.endswith(".so")]
        need(libs and all(n.split("/")[1] == abi for n in libs), "incorrect packaged native ABI")
        for pattern in ("libmoteur3d_v16_core", "libcse_android_probe", "libQt6Core", "libQt6Gui", "libplugins_platforms_qtforandroid", "libc++_shared"):
            need(any(pathlib.PurePosixPath(n).name.startswith(pattern) for n in libs), f"missing {pattern}")
        for name in ["index.txt", *files]:
            member = "assets/cse/" + name
            need(member in names, f"missing {member}")
            need(digest(z.open(member)) == digest((root/name).open("rb")), f"asset changed: {name}")
    print("PASS APK original asset hashes:", len(files), "files; ABI:", abi, "native libraries:", len(libs))

def selftest():
    with tempfile.TemporaryDirectory() as tmp:
        p = pathlib.Path(tmp)
        package = p/"package"
        root = package/"assets/cse"
        root.mkdir(parents=True)
        (root/"index.txt").write_text("")
        sdk = p/"sdk"
        for rel in ["platforms/android-36/android.jar", *("build-tools/36.0.0/"+x for x in ("aapt", "aapt2", "zipalign", "apksigner"))]:
            target = sdk/rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.touch()
        valid = {"sdkBuildToolsRevision":"36.0.0","android-min-sdk-version":"36",
                 "android-target-sdk-version":"36","android-compile-sdk-version":"36",
                 "android-package-name":PACKAGE,"application-binary":"cse_android_probe",
                 "architectures":{"arm64-v8a":"fixture"},"android-extra-libs":"libmoteur3d_v16_core.so",
                 "android-package-source-directory":str(package),"sdk":str(sdk)}
        deployment(valid,"arm64-v8a",package)
        for key,bad in [("sdkBuildToolsRevision",""),("android-compile-sdk-version","36-ext19"),
                        ("architectures",{"x86_64":"fixture"}),("android-extra-libs","")]:
            try:
                deployment(dict(valid, **{key:bad}),"arm64-v8a",package)
            except ValueError:
                continue
            raise AssertionError("preflight accepted invalid "+key)
    print("PASS deployment validator: positive fixture + 4 negative fixtures")

if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "selftest":
        selftest()
    else:
        source, abi, package = pathlib.Path(sys.argv[2]), sys.argv[3], pathlib.Path(sys.argv[4])
        if mode == "deployment":
            deployment(json.loads(source.read_text()), abi, package)
            print("PASS exact Qt deployment JSON:", abi, "SDK 36 / tools 36.0.0")
        elif mode == "apk":
            verify_apk(source, abi, package)
        else:
            raise ValueError(mode)
