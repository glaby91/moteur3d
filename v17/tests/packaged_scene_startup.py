"""Exercise the packaged GUI with the Windows scene preference used by the updater."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import winreg


user32 = ctypes.WinDLL("user32", use_last_error=True)
window_callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [window_callback, wintypes.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]


def windows_for(pid):
    windows = {}

    @window_callback
    def collect(hwnd, unused):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            title = ctypes.create_unicode_buffer(1024)
            if user32.GetWindowTextW(hwnd, title, len(title)):
                windows[hwnd] = title.value
        return True

    user32.EnumWindows(collect, 0)
    return windows


def check_launch(exe, case, expected, scene_argument=None, expect_error=False):
    command = [str(exe)]
    if scene_argument is not None:
        command.append(str(scene_argument))
    with tempfile.TemporaryFile() as log:
        child = subprocess.Popen(command, cwd=exe.parent, stdout=log, stderr=subprocess.STDOUT)
        titles = {}
        try:
            deadline = time.monotonic() + 110
            while child.poll() is None and time.monotonic() < deadline:
                titles = windows_for(child.pid)
                for hwnd, title in titles.items():
                    if title == "Chargement de la scène":
                        if expect_error:
                            print(f"PASS {case}: {title}", flush=True)
                            return
                        raise RuntimeError(f"{case}: unexpected load error")
                    if title.startswith("CSE WARP Beta X V") and " — Scène " in title:
                        if expect_error or not title.endswith(expected):
                            raise RuntimeError(f"{case}: opened {title!r}")
                        print(f"PASS {case}: {title}", flush=True)
                        user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
                        return
                time.sleep(0.1)
            raise RuntimeError(f"{case}: exit={child.poll()}, windows={list(titles.values())}")
        finally:
            if child.poll() is None:
                if expect_error or sys.exc_info()[0] is not None:
                    child.terminate()
                try:
                    child.wait(timeout=12)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
            log.seek(0)
            tail = log.read()[-3000:].decode("utf-8", errors="replace")
            if tail:
                print(tail, flush=True)


def main():
    exe = Path(sys.argv[1]).resolve(strict=True)
    source = Path(sys.argv[2]).resolve(strict=True)
    bundled = json.loads((exe.parent / "benchmark-civic/scene.json").read_text(encoding="utf-8-sig"))
    revision = int(bundled["revision"].split(".")[1])
    bundled_title = f" — {bundled['name']} — Scène {bundled['revision']}"
    with tempfile.TemporaryDirectory(prefix="CSE WARP startup ") as tmp:
        cached = Path(tmp) / "benchmark-civic"
        shutil.copytree(source, cached)
        scene = cached / "scene.json"
        descriptor = json.loads(scene.read_text(encoding="utf-8-sig"))
        pbr = cached / descriptor["sectors"][0]["pbr"][0]
        textures = json.loads(pbr.read_text(encoding="utf-8-sig"))
        for entry in textures["sets"]:
            entry["resolution"] = 64

        def set_fixture(number, first_size):
            descriptor.update(name="Cached regression scene", revision=f"V17.{number:03d}")
            textures["sets"][0]["resolution"] = first_size
            pbr.write_text(json.dumps(textures), encoding="utf-8")
            scene.write_text(json.dumps(descriptor), encoding="utf-8")
            return f" — {descriptor['name']} — Scène {descriptor['revision']}"

        key_path = r"Software\CSE WARP\CSE WARP V17\Scene"
        with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, key_path, access=winreg.KEY_READ | winreg.KEY_WRITE) as key:
            name = "InstalledLatestPath"
            try:
                previous = winreg.QueryValueEx(key, name)
            except FileNotFoundError:
                previous = None
            try:
                winreg.SetValueEx(key, name, 0, winreg.REG_SZ, str(scene))
                set_fixture(revision - 1, 1200)  # Historical truck-sign descriptor.
                check_launch(exe, "older invalid cache / no arguments", bundled_title)
                newer = set_fixture(revision + 1, 64)
                check_launch(exe, "newer usable cache / no arguments", newer)
                set_fixture(revision + 1, 1200)
                check_launch(exe, "newer invalid cache / no arguments", bundled_title)
                check_launch(exe, "explicit invalid file", "", scene, expect_error=True)
                set_fixture(revision, 64)
                check_launch(exe, "same revision / no arguments", bundled_title)
            finally:
                if previous is None:
                    winreg.DeleteValue(key, name)
                else:
                    winreg.SetValueEx(key, name, 0, previous[1], previous[0])


if __name__ == "__main__":
    main()
