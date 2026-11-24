#!/usr/bin/env python3
"""Build an optional, openly licensed Ukrainian-language radio-style montage.

Requires: Python 3.9+ and FFmpeg in PATH. No network requests at runtime in CSE WARP.
This is a modern reconstruction, NOT an authentic 1970s broadcast.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "sources.json"
CACHE = HERE / "cache"
OUTPUT = HERE / "generated" / "ukrainian_radio_reconstruction.ogg"
MAX_BYTES = 80 * 1024 * 1024
MAX_DURATION_TOLERANCE_SECONDS = 5


def cache_path(filename):
    posix_path = PurePosixPath(filename) if isinstance(filename, str) else None
    windows_path = PureWindowsPath(filename) if isinstance(filename, str) else None
    if (
        not filename
        or posix_path is None
        or windows_path is None
        or posix_path.is_absolute()
        or windows_path.is_absolute()
        or posix_path.name != filename
        or windows_path.name != filename
        or windows_path.drive
        or not filename.lower().endswith(".ogg")
    ):
        raise ValueError("Invalid Wikimedia Commons OGG filename")

    cache_root = CACHE.resolve()
    destination = cache_root / filename
    resolved_destination = destination.resolve()
    if not resolved_destination.is_relative_to(cache_root):
        raise ValueError("Wikimedia Commons file path escapes the cache directory")
    if destination.is_symlink():
        raise ValueError("Wikimedia Commons cache entries cannot be symlinks")
    return destination


def expected_source_metadata(source):
    expected_size = source.get("size_bytes")
    expected_sha1 = source.get("commons_sha1")
    if (
        type(expected_size) is not int
        or expected_size < 1024
        or expected_size > MAX_BYTES
        or not isinstance(expected_sha1, str)
        or len(expected_sha1) != 40
        or any(character not in "0123456789abcdef" for character in expected_sha1)
    ):
        raise ValueError("Missing or invalid pinned audio size/checksum")
    return expected_size, expected_sha1


def matches_source(path, source):
    expected_size, expected_sha1 = expected_source_metadata(source)
    if not path.is_file() or path.is_symlink() or path.stat().st_size != expected_size:
        return False
    checksum = hashlib.sha1(usedforsecurity=False)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest() == expected_sha1


def download(source):
    destination = cache_path(source["commons_filename"])
    expected_size, expected_sha1 = expected_source_metadata(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink():
        raise ValueError("Wikimedia Commons cache entries cannot be symlinks")
    if matches_source(destination, source):
        print("Cached:", destination.name)
        return destination
    if destination.exists():
        destination.unlink()

    url = "https://commons.wikimedia.org/wiki/Special:Redirect/file/" + urllib.parse.quote(source["commons_filename"])
    request = urllib.request.Request(url, headers={"User-Agent": "CSE-WARP-open-audio/1.0 (Wikimedia Commons source attribution)"})
    file_descriptor, temp_name = tempfile.mkstemp(
        prefix=destination.name + ".",
        suffix=".part",
        dir=destination.parent,
    )
    temp = Path(temp_name)
    checksum = hashlib.sha1(usedforsecurity=False)
    try:
        with os.fdopen(file_descriptor, "wb") as stream:
            with urllib.request.urlopen(request, timeout=90) as response:
                written = 0
                while True:
                    chunk = response.read(256 * 1024)
                    if not chunk:
                        break
                    written += len(chunk)
                    if written > MAX_BYTES:
                        raise ValueError("Download exceeds safety limit: " + source["commons_filename"])
                    checksum.update(chunk)
                    stream.write(chunk)
        if written != expected_size:
            raise ValueError("Downloaded size does not match pinned source")
        if checksum.hexdigest() != expected_sha1:
            raise ValueError("Downloaded checksum does not match pinned source")
        os.replace(temp, destination)
        print("Downloaded:", destination.name, written, "bytes")
        return destination
    finally:
        temp.unlink(missing_ok=True)


def validate_audio(path, source):
    ffprobe = shutil.which("ffprobe")
    if not ffprobe:
        raise FileNotFoundError("FFprobe is required to validate downloaded audio")
    result = subprocess.run(
        [
            ffprobe,
            "-v", "error",
            "-show_entries", "format=format_name,duration:stream=codec_type,codec_name",
            "-of", "json",
            str(path),
        ],
        check=True,
        capture_output=True,
        text=True,
        timeout=30,
    )
    details = json.loads(result.stdout)
    streams = details.get("streams", [])
    formats = details.get("format", {}).get("format_name", "").split(",")
    audio_streams = [stream for stream in streams if stream.get("codec_type") == "audio"]
    if (
        "ogg" not in formats
        or len(streams) != 1
        or len(audio_streams) != 1
        or audio_streams[0].get("codec_name") != "vorbis"
    ):
        raise ValueError("Input is not a single-stream OGG/Vorbis file: " + str(path))

    duration = float(details["format"]["duration"])
    expected_duration = float(source["duration_seconds"])
    if (
        not math.isfinite(duration)
        or duration <= 0
        or abs(duration - expected_duration) > MAX_DURATION_TOLERANCE_SECONDS
    ):
        raise ValueError("Audio duration does not match manifest: " + str(path))


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
    included = []
    for track in tracks:
        try:
            path = download(track)
        except Exception as exc:
            # Wikimedia can return HTTP 429 after several large files.
            # If we have Ukrainian speech AND the CC BY-SA song, prefer a
            # shorter legal radio programme over a missing output.
            print("WARNING: unavailable open source", track["id"], str(exc), file=sys.stderr)
            continue
        paths.append(path)
        included.append(track)
    have_song = any(track["id"] == "lysenko-shevtchenko" for track in included)
    have_narration = any(track["id"].startswith("spoken-") for track in included)
    if not (have_song and have_narration):
        print("Need at least one licensed narration AND the Ukrainian song.", file=sys.stderr)
        return 3
    print("Included", len(included), "licensed sources; duration approx",
          sum(track["duration_seconds"] for track in included), "seconds")
    if args.download_only:
        return 0
    if not shutil.which("ffmpeg"):
        print("FFmpeg missing. Source files downloaded; install FFmpeg to build montage.", file=sys.stderr)
        return 2
    for path, track in zip(paths, included):
        validate_audio(path, track)
        if not matches_source(path, track):
            raise ValueError("Audio source changed after validation: " + str(path))

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
    # Windows PlaySoundW accepts PCM WAV, not Ogg/Vorbis.
    # Produce a low-bandwidth 16 kHz / 16-bit / mono offline copy.
    wav_path = args.output.parent / "pripyat_radio.wav"
    subprocess.run(["ffmpeg", "-hide_banner", "-y", "-i", str(args.output),
                    "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le",
                    str(wav_path)], check=True)
    print("Windows preview WAV:", wav_path)
    print("Copy WAV to <exe-directory>/audio/pripyat_radio.wav before launch.")
    print("Attribution required: CREDITS.md. Do not label as a 1970s recording.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
