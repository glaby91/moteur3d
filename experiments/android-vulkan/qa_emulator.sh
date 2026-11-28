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
  for name in triangle.png scene.png view-0.png view-1.png view-2.png view-3.png view-4.png views.json; do
    p="$(adb shell run-as "$pkg" find files -name "$name" 2>/dev/null | tr -d '\r' | head -n1 || true)"
    if [ -n "$p" ]; then adb exec-out run-as "$pkg" cat "$p" > "android-evidence/$name" || true; fi
  done
}
trap capture EXIT
adb shell getprop > android-evidence/getprop.txt
adb shell pm list features > android-evidence/features.txt
adb shell cmd gpu vkjson > android-evidence/vkjson.txt 2>&1 || true
adb shell wm size 1920x1080
adb shell wm density 160
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb shell settings put secure immersive_mode_confirmations confirmed
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
wait_marker "CSE_ANDROID PASS WALK_REGRESSION" 120
wait_marker "CSE_ANDROID DIAGNOSTIC_DEVICE" 120
wait_marker "CSE_ANDROID PASS TRIANGLE" 360
wait_marker "CSE_ANDROID PASS SCENE" 600
wait_marker "CSE_ANDROID PASS FIVE_VIEWS" 600
wait_marker "CSE_ANDROID PASS WSI.*readback 0" 180
wait_marker "CSE_ANDROID PASS HUD_DRAW 1920 1080" 120
sleep 8
adb exec-out screencap -p > android-evidence/screen-native.png
adb shell input swipe 1150 400 1450 400 800
wait_marker "CSE_ANDROID PASS TOUCH_CAMERA" 120
adb exec-out screencap -p > android-evidence/screen-touch.png
# A sustained left-half drag exercises actual walking, not just camera rotation.
adb shell input swipe 270 850 270 1030 2500
wait_marker "CSE_ANDROID INPUT MOVE_BEGIN" 60
wait_marker "CSE_ANDROID INPUT MOVE_END metres" 60
wait_marker "CSE_ANDROID PASS WALK_MOVE" 180
adb exec-out screencap -p > android-evidence/screen-walk.png
# Require measured horizontal displacement, not just a changed camera height.
python3 - <<'PY'
import re, pathlib
log=pathlib.Path("android-evidence/stream-log.txt").read_text(errors="replace")
values=[float(x) for x in re.findall(r"CSE_ANDROID INPUT MOVE_END metres ([0-9.eE+-]+)",log)]
assert values and max(values)>0.05, values
print("PASS horizontal walk displacement",max(values),"m")
PY
# Actual visible hit regions from WarpActivity, normalized to 1920x1080.
adb shell input tap 1830 910
wait_marker "CSE_ANDROID PASS JUMP_INPUT" 120
adb exec-out screencap -p > android-evidence/screen-jump.png
adb shell input tap 1625 910
wait_marker "CSE_ANDROID PASS CROUCH_INPUT" 120
adb exec-out screencap -p > android-evidence/screen-crouch.png
adb shell input tap 1510 65
wait_marker "CSE_ANDROID PASS HUD_HIDDEN" 60
adb exec-out screencap -p > android-evidence/screen-hud-off.png
adb shell input tap 1510 65
wait_marker "CSE_ANDROID PASS HUD_SHOWN" 60
adb shell input tap 1770 65
wait_marker "CSE_ANDROID PASS MENU_VISIBLE" 60
adb exec-out screencap -p > android-evidence/screen-menu.png
adb shell input tap 960 320
wait_marker "CSE_ANDROID PASS SCALE 75 1440 810 surface 1920 1080 readback 0" 180
adb shell input tap 960 320
wait_marker "CSE_ANDROID PASS SCALE 50 960 540 surface 1920 1080 readback 0" 180
adb exec-out screencap -p > android-evidence/screen-scale50.png
adb shell input tap 960 320
wait_marker "CSE_ANDROID PASS SCALE 100 1920 1080 surface 1920 1080 readback 0" 180
adb shell input tap 960 490
wait_marker "CSE_ANDROID PASS MSAA requested 2 actual" 180
adb shell input tap 960 490
wait_marker "CSE_ANDROID PASS MSAA requested 4 actual" 180
adb shell input tap 960 490
wait_marker "CSE_ANDROID PASS MSAA requested 1 actual 1" 180
adb shell input tap 960 660
wait_marker "CSE_ANDROID PASS CAMERA_MENU" 120
adb shell input tap 960 815
wait_marker "CSE_ANDROID PASS SENSITIVITY 2" 60
adb shell input tap 960 970
wait_marker "CSE_ANDROID PASS MENU_CLOSED" 60
adb shell input keyevent KEYCODE_HOME
wait_marker "CSE_ANDROID PAUSED" 90
adb shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1
wait_marker "CSE_ANDROID PASS RESUME" 180
adb exec-out screencap -p > android-evidence/screen-resume.png
for name in triangle.png scene.png view-0.png view-1.png view-2.png view-3.png view-4.png views.json; do
  p="$(adb shell run-as "$pkg" find files -name "$name" | tr -d '\r' | head -n1)"
  test -n "$p"
  adb exec-out run-as "$pkg" cat "$p" > "android-evidence/$name"
done
python3 - <<'PY'
import pathlib, struct
root = pathlib.Path("android-evidence")
for name, expected in [("triangle.png", (320,180)), ("scene.png", (640,360)), ("screen-native.png", (1920,1080)), ("screen-touch.png", (1920,1080)), ("screen-walk.png", (1920,1080)), *[(f"screen-{n}.png",(1920,1080)) for n in ("crouch","jump","hud-off","menu","scale50","resume")], *[(f"view-{i}.png",(640,360)) for i in range(5)]]:
    data = (root/name).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", name
    dims = struct.unpack(">II", data[16:24])
    assert min(dims)>0 and (expected is None or dims==expected), (name, dims)
    assert data[-8:-4] == b"IEND", name
    print(name, dims, len(data), "bytes")
PY
/usr/bin/python3 - <<'PY'
from PIL import Image
import pathlib
p=pathlib.Path("android-evidence")
def accent_count(name, box):
    image=Image.open(p/name).convert("RGB").crop(box)
    return sum(abs(r-103)<=3 and abs(g-222)<=3 and abs(b-207)<=3 for r,g,b in image.getdata())
# The opaque turquoise title and menu border must actually be visible in
# system-composited screenshots, not merely announced by the Java view.
visible=accent_count("screen-native.png",(30,20,1250,135))
hidden=accent_count("screen-hud-off.png",(30,20,1250,135))
menu=accent_count("screen-menu.png",(545,155,1375,1060))
print("HUD accent pixels:",visible,"hidden:",hidden,"menu:",menu)
assert visible>100 and visible>hidden+100, "HUD not visibly composited or hidden"
assert menu>200, "menu not visibly composited"
PY
echo "PASS emulator software Vulkan: five views, landscape, native surface, walk, HUD/menu, crouch/jump input, render scale, MSAA selection, camera and resume (not tablet performance)" | tee android-evidence/result.txt
