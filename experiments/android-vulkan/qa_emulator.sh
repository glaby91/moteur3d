#!/usr/bin/env bash
set -euo pipefail
mkdir -p android-evidence
pkg=org.csewarp.androidprobe
capture() {
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
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
adb install -r apk/*.apk
adb logcat -c
adb shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1
wait_marker() {
  local marker="$1" seconds="$2"
  for ((i=0;i<seconds;i+=5)); do
    adb logcat -d > android-evidence/current-log.txt
    if grep -q "CSE_ANDROID FAIL" android-evidence/current-log.txt; then cat android-evidence/current-log.txt | grep CSE_ANDROID; return 1; fi
    if grep -q "$marker" android-evidence/current-log.txt; then return 0; fi
    sleep 5
  done
  echo "Missing marker: $marker"; return 1
}
wait_marker "CSE_ANDROID PASS TRIANGLE" 360
wait_marker "CSE_ANDROID PASS SCENE" 600
wait_marker "CSE_ANDROID PASS WSI" 180
adb exec-out screencap -p > android-evidence/screen-native.png
adb shell input swipe 400 400 700 400 500
wait_marker "CSE_ANDROID PASS TOUCH_CAMERA" 120
adb exec-out screencap -p > android-evidence/screen-touch.png
echo "PASS emulator software Vulkan: real triangle, V17.634 scene, native surface and touch camera (not tablet performance)" | tee android-evidence/result.txt
