#!/usr/bin/env python3
"""Build an optional, openly licensed Ukrainian-language radio-style montage.

Requires: Python 3.9+ and FFmpeg in PATH. No network requests at runtime in CSE WARP.
This is a modern reconstruction, NOT an authentic 1970s broadcast.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "sources.json"
CACHE = HERE / "cache"
OUTPUT = HERE / "generated" / "ukrainian_radio_reconstruction.ogg"
MAX_BYTES = 80 * 1024 * 1024


def download(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file() and destination.stat().st_size > 0:
        print("Cached:", destination.name)
        return
    url = "https://commons.wikimedia.org/wiki/Special:Redirect/file/" + urllib.parse.quote(source["commons_filename"])
    request = urllib.request.Request(url, headers={"User-Agent": "CSE-WARP-open-audio/1.0 (Wikimedia Commons source attribution)"})
    temp = destination.with_suffix(destination.suffix + ".part")
    try:
        with urllib.request.urlopen(request, timeout=90) as response, temp.open("wb") as stream:
            written = 0
            while True:
                chunk = response.read(256 * 1024)
                if not chunk:
                    break
                written += len(chunk)
                if written > MAX_BYTES:
                    raise ValueError("Download exceeds safety limit: " + source["commons_filename"])
                stream.write(chunk)
        if written < 1024:
            raise ValueError("Downloaded file is suspiciously small")
        temp.replace(destination)
        print("Downloaded:", destination.name, written, "bytes")
    finally:
        temp.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Print playlist and exit (no downloads)")
    parser.add_argument("--download-only", action="store_true", help="Download original OGG files without transcoding")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    tracks = manifest["tracks"]
    print("Program:", manifest["title"])
    print("Not a historical broadcast. Approximate duration:", sum(t["duration_seconds"] for t in tracks), "seconds")
    for track in tracks:
        print(" -", track["title"], "|", track["license"], "|", track["source_page"])
    if args.dry_run:
        return 0

    paths = []
    for track in tracks:
        path = CACHE / track["commons_filename"]
        download(track, path)
        paths.append(path)

    if args.download_only:
        return 0
    if not shutil.which("ffmpeg"):
        print("FFmpeg missing. Source files downloaded; install FFmpeg to build montage.", file=sys.stderr)
        return 2

    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Phone/radio-band audio, with no artificial music, narration or unsupported historic claims.
    filters = []
    for idx in range(len(paths)):
        filters.append(f"[{idx}:a]aresample=44100,aformat=sample_fmts=fltp:channel_layouts=mono,"
                       f"highpass=f=240,lowpass=f=3500,volume=0.86[a{idx}]")
    filters.append("".join(f"[a{idx}]" for idx in range(len(paths))) +
                   f"concat=n={len(paths)}:v=0:a=1[out]")
    cmd = ["ffmpeg", "-hide_banner", "-y"]
    for path in paths:
        cmd += ["-i", str(path)]
    cmd += ["-filter_complex", ";".join(filters), "-map", "[out]", "-c:a", "libvorbis",
            "-q:a", "4", "-metadata", "title=" + manifest["title"],
            "-metadata", "comment=Modern reconstruction; CC BY-SA 3.0 attribution: see CREDITS.md",
            str(args.output)]
    subprocess.run(cmd, check=True)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    print("Created:", args.output)
    print("SHA256:", digest)
    print("Attribution required: CREDITS.md. Do not label as a 1970s recording.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
