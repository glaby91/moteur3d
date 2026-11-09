#!/usr/bin/env python3
"""Photo2Scene WARP: evaluate architectural line families without inventing a camera."""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path

from photo2scene import GeometryError, least_squares


def fit_structural_lines(groups: dict, width: int, height: int) -> dict:
    """Fit each architectural level to y=m*x+b in the original image pixels.

    A common vanishing point is accepted only if independently fitted level
    intersections coincide in pixels. Repeated bays alone cannot establish
    two-view identities, focal length, or 3D depths.
    """
    if width <= 0 or height <= 0:
        raise GeometryError("Invalid photograph resolution")
    models = {}
    for name in ("roof", "upper_openings", "balcony_front"):
        segs = groups.get(name, [])
        if len(segs) < 2:
            raise GeometryError("Not enough independent segments for " + name)
        rows, values = [], []
        for segment in segs:
            if len(segment) != 4:
                raise GeometryError("Each line must contain x1,y1,x2,y2")
            x1,y1,x2,y2 = map(float, segment)
            rows.extend(([x1,1.0], [x2,1.0]))
            values.extend([y1,y2])
        slope,intercept=least_squares(rows,values,2)
        errors=[slope*x+intercept-y for (x,_),y in zip(rows,values)]
        rms=math.sqrt(sum(e*e for e in errors)/len(errors))
        models[name]={"slope":slope,"intercept_px":intercept,
                      "point_residual_rms_px":rms,"segment_count":len(segs)}
    names=list(models)
    intersections=[]
    for i in range(len(names)):
        for j in range(i+1,len(names)):
            a,b=models[names[i]],models[names[j]]
            dm=a["slope"]-b["slope"]
            if abs(dm)<1e-9:
                continue
            x=(b["intercept_px"]-a["intercept_px"])/dm
            y=a["slope"]*x+a["intercept_px"]
            intersections.append({"between":[names[i],names[j]],"pixel":[x,y]})
    if len(intersections)==3:
        xs=[p["pixel"][0] for p in intersections]
        ys=[p["pixel"][1] for p in intersections]
        spread=math.hypot(max(xs)-min(xs),max(ys)-min(ys))
    else:
        spread=None
    # This is a strict diagnostic, NOT an automatic FOV calculator.
    acceptable=spread is not None and spread<=max(10.0,width*.01)
    return {
        "image_size_px":[width,height],
        "structural_level_line_fits":models,
        "pairwise_vanishing_candidates":intersections,
        "vanishing_candidate_spread_px":spread,
        "stable_common_vanishing_point":acceptable,
        "calibration_status":"NOT_VALIDATED" if not acceptable else "CANDIDATE_REQUIRES_ARCHITECTURAL_REVIEW",
        "fov_horizontal_deg":None,
        "three_dimensional_distances":None,
        "explanation":"Only one horizontal building axis was recognized; precise FOV and 3D depth need independently constrained camera and verified two-view correspondences."
    }


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("input",type=Path)
    p.add_argument("-o","--output",type=Path)
    args=p.parse_args()
    data=json.loads(args.input.read_text(encoding="utf-8"))
    image=data["image"]
    result=fit_structural_lines(data["line_groups"],image["width"],image["height"])
    result["photo_id"]=image["id"]
    result["sha256"]=image.get("sha256")
    result["two_view_matches"]="NOT_VALIDATED"
    output=json.dumps(result,indent=2,ensure_ascii=False,allow_nan=False)+"\n"
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(output,encoding="utf-8")
    else:
        print(output,end="")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
