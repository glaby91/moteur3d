#!/usr/bin/env python3
"""Photo2Scene: calcul reproductible de géométrie projective depuis annotations.

Usage: python tools/photo2scene/photo2scene.py analyse data.json -o result.json
Toutes les positions pixels sont des observations, pas des inférences fiables de l'IA.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path


class GeometryError(ValueError):
    pass


def least_squares(rows: list[list[float]], values: list[float], n: int) -> list[float]:
    if len(rows) < n or len(rows) != len(values):
        raise GeometryError("Insufficient or malformed constraints")
    # Petits systèmes normalisés, pivotage partiel. Pour les gros réseaux:
    # SVD et solveur robuste (SciPy/OpenCV), dans une étape ultérieure.
    a = [[sum(row[i] * row[j] for row in rows) for j in range(n)]
         for i in range(n)]
    b = [sum(row[i] * v for row, v in zip(rows, values)) for i in range(n)]
    scale = max(abs(a[i][j]) for i in range(n) for j in range(n))
    for i in range(n):
        p = max(range(i, n), key=lambda k: abs(a[k][i]))
        if not scale or abs(a[p][i]) <= 1e-11 * scale:
            raise GeometryError("Degenerate or nearly parallel constraints")
        a[i], a[p] = a[p], a[i]
        b[i], b[p] = b[p], b[i]
        for k in range(i + 1, n):
            f = a[k][i] / a[i][i]
            for j in range(i, n):
                a[k][j] -= f * a[i][j]
            b[k] -= f * b[i]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        x[i] = (b[i] - sum(a[i][j] * x[j] for j in range(i + 1, n))) / a[i][i]
    if not all(math.isfinite(v) for v in x):
        raise GeometryError("Non-finite solution")
    return x


def line(a: list[float], b: list[float]) -> tuple[float, float, float]:
    if len(a) != 2 or len(b) != 2:
        raise GeometryError("A line segment requires two 2D endpoints")
    aa, bb, cc = a[1] - b[1], b[0] - a[0], a[0] * b[1] - b[0] * a[1]
    norm = math.hypot(aa, bb)
    if norm < 1e-9:
        raise GeometryError("Coincident line endpoints")
    return aa / norm, bb / norm, cc / norm


def vanishing_point(segments: list[dict]) -> dict:
    if len(segments) < 2:
        raise GeometryError("At least two line segments required per direction")
    lines = [line(s["a"], s["b"]) for s in segments]
    px, py = least_squares([[a, b] for a, b, _ in lines],
                           [-c for _, _, c in lines], 2)
    err = [abs(a * px + b * py + c) for a, b, c in lines]
    return {"pixel": [px, py], "line_residual_rms_px":
            math.sqrt(sum(e * e for e in err) / len(err)), "segments": len(lines)}


def fov_from_orthogonal_vps(vp1: list[float], vp2: list[float],
                             width: int, height: int,
                             principal: list[float] | None = None) -> dict:
    # (v1-c).(v2-c) + f^2 = 0 si directions 3D orthogonales,
    # pixels carrés, skew nul, caméra sténopé et distorsion négligeable.
    cx, cy = principal if principal is not None else (width / 2, height / 2)
    f2 = -((vp1[0]-cx)*(vp2[0]-cx)+(vp1[1]-cy)*(vp2[1]-cy))
    if not math.isfinite(f2) or f2 <= 1e-8:
        raise GeometryError("Orthogonal-VP FOV is not identifiable under assumptions")
    focal = math.sqrt(f2)
    return {"focal_length_px": focal,
            "fov_horizontal_deg": math.degrees(2*math.atan(width/(2*focal))),
            "fov_vertical_deg": math.degrees(2*math.atan(height/(2*focal))),
            "assumptions": ["orthogonal 3D vanishing directions", "pinhole camera",
                            "square pixels", "zero skew", "no lens distortion",
                            "principal point supplied or image center"]}


def _pn(pixel: list[float], w: int, h: int) -> tuple[float, float]:
    return (pixel[0] - w/2)/(w/2), (pixel[1] - h/2)/(h/2)


def _project(h: list[float], u: float, v: float) -> tuple[float, float]:
    den = h[6]*u+h[7]*v+1
    if abs(den) < 1e-10:
        raise GeometryError("Point projects onto the line at infinity")
    return ((h[0]*u+h[1]*v+h[2])/den, (h[3]*u+h[4]*v+h[5])/den)


def _back_project(h: list[float], x: float, y: float) -> tuple[float, float]:
    a, b, c, d, e, f, g, k = h
    aa, bb, cc = a-x*g, b-x*k, x-c
    dd, ee, ff = d-y*g, e-y*k, y-f
    den = aa*ee-bb*dd
    if abs(den) < 1e-10:
        raise GeometryError("Pixel ray is parallel to the reference plane")
    return ((cc*ee-bb*ff)/den, (aa*ff-cc*dd)/den)


def fit_plane(anchors: list[dict], width: int, height: int,
              queries: list[dict] | None = None,
              holdout: list[dict] | None = None) -> dict:
    if len(anchors) < 4:
        raise GeometryError("A planar homography needs >= 4 non-collinear anchors")
    rows, rhs = [], []
    for anchor in anchors:
        u, v = anchor["scene_uv"]
        x, y = _pn(anchor["pixel"], width, height)
        rows.extend([[u,v,1,0,0,0,-u*x,-v*x],
                     [0,0,0,u,v,1,-u*y,-v*y]])
        rhs.extend([x,y])
    h = least_squares(rows,rhs,8)

    def errors(items: list[dict]) -> dict:
        e = []
        for a in items:
            u, v = a["scene_uv"]
            px, py = _project(h,u,v)
            tx, ty = _pn(a["pixel"],width,height)
            e.append(math.hypot((px-tx)*width/2,(py-ty)*height/2))
        return {"count":len(e),"rms_px":math.sqrt(sum(v*v for v in e)/len(e)) if e else None,
                "max_px":max(e) if e else None}

    converted = []
    for q in queries or []:
        x,y = _pn(q["pixel"],width,height)
        u,v = _back_project(h,x,y)
        converted.append({"id":q["id"],"scene_uv":[u,v],
                          "assumption":"queried pixel lies on this calibrated reference plane"})
    return {"homography_scene_to_normalized_pixel":
            [[h[0],h[1],h[2]],[h[3],h[4],h[5]],[h[6],h[7],1]],
            "fitting":errors(anchors),"held_out_validation":errors(holdout or []),
            "derived_plane_points":converted,
            "warning":"Cannot infer off-plane depth or metric scale from this homography"}


def analyse(data: dict) -> dict:
    image=data["image"]
    w,h=int(image["width"]),int(image["height"])
    if w < 2 or h < 2:
        raise GeometryError("Invalid image dimensions")
    result={"photo_id":image["id"],"image_size_px":[w,h],
            "method":"deterministic geometry from annotated image pixels","status":"calculated"}
    if "vanishing" in data:
        cfg=data["vanishing"]
        vp={k:vanishing_point(v) for k,v in cfg["directions"].items()}
        result["vanishing_points"]=vp
        pair=cfg.get("orthogonal_pair")
        if pair is not None:
            if len(pair)!=2 or pair[0] not in vp or pair[1] not in vp:
                raise GeometryError("Unknown vanishing directions in orthogonal pair")
            result["camera_intrinsics_estimate"]=fov_from_orthogonal_vps(
                vp[pair[0]]["pixel"],vp[pair[1]]["pixel"],w,h,
                cfg.get("principal_point"))
    if "planes" in data:
        result["planes"]={}
        for plane in data["planes"]:
            result["planes"][plane["id"]]=fit_plane(
                plane["anchors"],w,h,plane.get("query"),plane.get("holdout"))
    return result


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command",choices=["analyse"])
    parser.add_argument("input",type=Path,help="JSON of annotated pixel geometry")
    parser.add_argument("-o","--output",type=Path,help="Write report JSON here")
    a=parser.parse_args()
    try:
        result=analyse(json.loads(a.input.read_text(encoding="utf-8")))
    except (OSError, KeyError, TypeError, ValueError, IndexError) as exc:
        parser.exit(2,f"Photo2Scene: {exc}\n")
    output=json.dumps(result,indent=2,ensure_ascii=False,allow_nan=False)+"\n"
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(output,encoding="utf-8")
    else:
        print(output,end="")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
