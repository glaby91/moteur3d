#!/usr/bin/env bash
set -euo pipefail
mkdir -p android-evidence
pkg=org.csewarp.androidprobe
log_pid=""
capture() {
  if [ -n "$log_pid" ]; then kill "$log_pid" 2>/dev/null || true; wait "$log_pid" 2>/dev/null || true; fi
  adb logcat -d -v threadtime > android-evidence/logcat.txt || true
  adb exec-out screencap -p > android-evidence/screen-final.png || true
  adb shell run-as "$pkg" find files -type f -name '*.png' > android-evidence/files.txt || true
  for name in triangle.png scene.png; do
    p="$(adb shell run-as "$pkg" find files -name "$name" 2>/dev/null | tr -d '\r' | head -n1 || true)"
    if [ -n "$p" ]; then adb exec-out run-as "$pkg" cat "$p" > "android-evidence/$name" || true; fi
  done
}
trap capture EXIT
adb shell getprop > android-evidence/getprop.txt
adb shell pm list features > android-evidence/features.txt
adb shell cmd gpu vkjson > android-evidence/vkjson.txt 2>&1 || true
adb shell wm size 1080x1920
adb shell wm density 160
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
adb install -r apk/*.apk
adb shell run-as "$pkg" pwd
adb logcat -c
adb logcat -v threadtime > android-evidence/stream-log.txt &
log_pid=$!
adb shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1
wait_marker() {
  local marker="$1" seconds="$2"
  for ((i=0;i<seconds;i+=5)); do
    if grep -q "CSE_ANDROID FAIL" android-evidence/stream-log.txt; then grep CSE_ANDROID android-evidence/stream-log.txt; return 1; fi
    if grep -q "$marker" android-evidence/stream-log.txt; then return 0; fi
    sleep 5
  done
  echo "Missing marker: $marker"; return 1
}
wait_marker "CSE_ANDROID PASS TRIANGLE" 360
wait_marker "CSE_ANDROID PASS SCENE" 600
wait_marker "CSE_ANDROID PASS WSI.*readback 0" 180
adb exec-out screencap -p > android-evidence/screen-native.png
adb shell input swipe 400 400 700 400 500
wait_marker "CSE_ANDROID PASS TOUCH_CAMERA" 120
adb exec-out screencap -p > android-evidence/screen-touch.png
for name in triangle.png scene.png; do
  p="$(adb shell run-as "$pkg" find files -name "$name" | tr -d '\r' | head -n1)"
  test -n "$p"
  adb exec-out run-as "$pkg" cat "$p" > "android-evidence/$name"
done
python3 - <<'PY'
import pathlib, struct
root = pathlib.Path("android-evidence")
for name, expected in [("triangle.png", (320,180)), ("scene.png", (640,360)), ("screen-native.png", None), ("screen-touch.png", None)]:
    data = (root/name).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", name
    dims = struct.unpack(">II", data[16:24])
    assert min(dims)>0 and (expected is None or dims==expected), (name, dims)
    assert data[-8:-4] == b"IEND", name
    print(name, dims, len(data), "bytes")
PY
echo "PASS emulator software Vulkan: real triangle, V17.634 scene, native surface and touch camera (not tablet performance)" | tee android-evidence/result.txt
