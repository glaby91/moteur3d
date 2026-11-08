#!/usr/bin/env python3
"""Local, authenticated HTTP control plane for Moteur3D Qt.

No shell, no arbitrary executable/paths, and no public network listener.
Use an approved outbound connector for remote ChatGPT access.
"""
import argparse
import hmac
import json
import os
from pathlib import Path
import secrets
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

MAX_BODY = 4096
MAX_JOBS = 100
TIMEOUT = 900
VALID_MODES = {"cpu": "--snapshot", "d3d11_compute": "--snapshot-compute"}


def create_server(executable: Path, root: Path, token: str, port: int):
    executable = executable.resolve(strict=True)
    root = root.resolve(strict=True)
    if not executable.is_file():
        raise ValueError("Moteur3D executable is not a file")
    if not token or len(token) < 24:
        raise ValueError("Token must contain at least 24 characters")
    jobs = {}
    lock = threading.Lock()
    output_dir = root / "remote-renders"
    output_dir.mkdir(parents=True, exist_ok=True)

    class Handler(BaseHTTPRequestHandler):
        def _reply(self, code, data, content_type="application/json"):
            raw = json.dumps(data, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", content_type + "; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def _authorized(self):
            supplied = self.headers.get("Authorization", "")
            return hmac.compare_digest(supplied, "Bearer " + token)

        def _route(self):
            if not self._authorized():
                self._reply(401, {"error": "unauthorized"})
                return None
            return urlsplit(self.path).path

        def do_GET(self):
            path = self._route()
            if path is None:
                return
            if path == "/api/status":
                with lock:
                    self._reply(200, {"service": "moteur3d-local-agent", "jobs": len(jobs),
                                      "modes": list(VALID_MODES)})
                return
            parts = path.strip("/").split("/")
            if len(parts) == 3 and parts[:2] == ["api", "jobs"]:
                with lock:
                    job = jobs.get(parts[2])
                    data = dict(job) if job else None
                self._reply(200 if data else 404, data or {"error": "unknown job"})
                return
            if len(parts) == 4 and parts[:2] == ["api", "jobs"] and parts[3] == "image":
                with lock:
                    job = jobs.get(parts[2])
                    ready = job and job["status"] == "completed"
                    image_path = output_dir / (parts[2] + ".png")
                if not ready or not image_path.is_file():
                    self._reply(404, {"error": "image not ready"})
                    return
                image_bytes = image_path.read_bytes()
                if not image_bytes.startswith(b"\\x89PNG\\r\\n\\x1a\\n"):
                    self._reply(500, {"error": "invalid PNG"})
                    return
                self.send_response(200)
                self.send_header("Content-Type", "image/png")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(image_bytes)))
                self.end_headers()
                self.wfile.write(image_bytes)
                return
            self._reply(404, {"error": "unknown endpoint"})

        def do_POST(self):
            path = self._route()
            if path is None:
                return
            if path != "/api/render":
                self._reply(404, {"error": "unknown endpoint"})
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_BODY:
                    raise ValueError("invalid body size")
                request = json.loads(self.rfile.read(length))
                if not isinstance(request, dict) or set(request) != {"camera", "backend"}:
                    raise ValueError("expected only camera and backend")
                camera, backend = request["camera"], request["backend"]
                if type(camera) is not int or not 1 <= camera <= 13:
                    raise ValueError("camera must be integer 1..13")
                if backend not in VALID_MODES:
                    raise ValueError("unsupported backend")
            except (ValueError, TypeError, json.JSONDecodeError) as exc:
                self._reply(400, {"error": str(exc)})
                return
            with lock:
                if len(jobs) >= MAX_JOBS:
                    self._reply(429, {"error": "job limit reached; restart agent to clear history"})
                    return
                if any(job["status"] in ("queued", "running") for job in jobs.values()):
                    self._reply(409, {"error": "another render is running"})
                    return
                job_id = secrets.token_hex(12)
                jobs[job_id] = {"id": job_id, "status": "queued",
                                "camera": camera, "backend": backend}
            threading.Thread(target=run_render, args=(job_id, camera, backend), daemon=True).start()
            self._reply(202, {"job_id": job_id, "status": "queued",
                              "status_url": "/api/jobs/" + job_id})

        def log_message(self, fmt, *args):
            # Never log bearer tokens, request payloads or filesystem paths.
            print("%s - %s" % (self.address_string(), fmt % args), flush=True)

    def run_render(job_id, camera, backend):
        target = output_dir / (job_id + ".png")
        with lock:
            jobs[job_id]["status"] = "running"
        start = time.monotonic()
        try:
            env = os.environ.copy()
            if backend == "d3d11_compute":
                # Use WARP only if explicitly set by operator in the agent environment.
                pass
            proc = subprocess.run(
                [str(executable), VALID_MODES[backend], str(camera), str(target)],
                cwd=str(root), env=env, shell=False, timeout=TIMEOUT,
                capture_output=True, text=True, errors="replace",
            )
            if proc.returncode != 0 or not target.is_file() or target.stat().st_size == 0:
                raise RuntimeError("render failed (exit %d): %s" %
                                   (proc.returncode, proc.stderr[-1000:]))
            with lock:
                jobs[job_id].update(status="completed",
                                    elapsed_ms=round((time.monotonic()-start)*1000),
                                    image_url="/api/jobs/"+job_id+"/image")
        except (OSError, subprocess.TimeoutExpired, RuntimeError) as exc:
            with lock:
                jobs[job_id].update(status="failed", error=str(exc)[:1200])

    return ThreadingHTTPServer(("127.0.0.1", port), Handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path, help="Path to moteur3d_qt.exe")
    parser.add_argument("--runtime", required=True, type=Path, help="Directory containing scene assets")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    token = os.environ.get("M3D_AGENT_TOKEN", "")
    if not token:
        parser.error("Set M3D_AGENT_TOKEN to a randomly generated secret (24+ characters)")
    server = create_server(args.exe, args.runtime, token, args.port)
    print("Moteur3D local agent listening on http://127.0.0.1:%d" % args.port)
    server.serve_forever()


if __name__ == "__main__":
    main()
