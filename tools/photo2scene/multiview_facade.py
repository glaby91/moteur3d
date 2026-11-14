#!/usr/bin/env python3
"""Photo2Scene WARP: geometric multi-view fit of one shared colonnade plane.

This is NOT autonomous photogrammetry: bay labels are human annotation
hypotheses. One bay fixes the arbitrary unit, no metric scale is inferred.
Only the front plane (Z=0) is reconstructed; the side/roof depth is unknown.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path

import cv2
import numpy as np
from scipy.optimize import least_squares


def camera_matrix(focal: float, size: tuple[int, int]) -> np.ndarray:
    w, h = size
    return np.array([[focal, 0., w/2], [0., focal, h/2], [0., 0., 1.]])


def scene_points(anchors: list[dict], height: float) -> np.ndarray:
    return np.float64([[-float(a["bay"]), height if a["level"]=="top" else 0., 0.]
                       for a in anchors])


def project(camera_params: np.ndarray, anchors: list[dict],
            height: float, size: tuple[int,int]) -> np.ndarray:
    focal=math.exp(float(camera_params[0]))
    rvec=camera_params[1:4]
    tvec=camera_params[4:7]
    pts,_=cv2.projectPoints(scene_points(anchors,height),rvec,tvec,
                            camera_matrix(focal,size),None)
    return pts.reshape((-1,2))


def observed(anchors: list[dict]) -> np.ndarray:
    return np.float64([a["pixel"] for a in anchors])


def split_anchors(photo: dict) -> tuple[list[dict],list[dict]]:
    a=photo["anchors"]
    train=[p for p in a if p["role"]=="fit"]
    hold=[p for p in a if p["role"]=="holdout"]
    if len(train)<6 or len(hold)<2:
        raise ValueError("Need >=6 fitted and >=2 independent holdout features")
    if len({(p["bay"],p["level"]) for p in a})!=len(a):
        raise ValueError("Duplicate bay/level annotation")
    return train,hold


def solve(data: dict) -> tuple[dict,np.ndarray]:
    photos=data["photos"]
    if len(photos)<2:
        raise ValueError("At least two independent photographs required")
    starts=[math.log(2.4)]
    fit=[]
    for photo in photos:
        size=(int(photo["width"]),int(photo["height"]))
        train,hold=split_anchors(photo)
        fit.append((size,train,hold))
        focal=float(photo["focal_initial_px"])
        ok,rvec,tvec=cv2.solvePnP(scene_points(train,2.4),observed(train),
                                  camera_matrix(focal,size),None,
                                  flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok:
            raise ValueError("PnP initialization failed: "+photo["id"])
        starts.extend([math.log(focal),*rvec.ravel(),*tvec.ravel()])

    lower=[math.log(.5)]
    upper=[math.log(10.)]
    for _ in photos:
        lower.extend([math.log(80.),*([-np.inf]*6)])
        upper.extend([math.log(20000.),*([np.inf]*6)])

    def residual(params: np.ndarray) -> np.ndarray:
        height=math.exp(float(params[0]))
        blocks=[]
        for i,(size,train,_) in enumerate(fit):
            p=params[1+i*7:8+i*7]
            blocks.append((project(p,train,height,size)-observed(train)).ravel())
        return np.concatenate(blocks)

    optimized=least_squares(residual,np.asarray(starts,dtype=float),
        loss="soft_l1",f_scale=3.,max_nfev=400,bounds=(lower,upper),
        ftol=1e-10,xtol=1e-10,gtol=1e-10)
    if not optimized.success:
        raise ValueError("Bundle optimization did not converge: "+optimized.message)
    params=optimized.x
    height=math.exp(float(params[0]))
    report={
      "format":"photo2scene-warp-constrained-front-plane-v1",
      "subject":data.get("subject"),
      "status":"EXPERIMENTAL_PARTIAL_FRONT_FACADE",
      "gauge":{"one_horizontal_bay":1.0,"front_plane_z":0.0,
               "height_visible_colonnade_in_bays":height,
               "meters_per_bay":None},
      "assumptions":[
        "Human-labelled bay indices represent the same physical columns",
        "All top and balcony endpoints belong to a single plane",
        "All cameras are pinhole with square pixels, centered principal point",
        "All cameras have zero skew and negligible lens distortion",
        "Vertical/horizontal bay directions are perpendicular",
        "Only fit points were used to estimate the camera poses"],
      "not_determined":["actual facade width beyond observed bays",
                        "rear/right facade depth","roof thickness and geometry",
                        "absolute metric scale","true scene accuracy without independently verified landmarks"],
      "optimization":{"solver":"SciPy robust least_squares, soft_l1 scale 3px",
                      "success":bool(optimized.success),"cost":float(optimized.cost),
                      "fitted_pixels":sum(len(x[1]) for x in fit)*2},
      "cameras":[]
    }
    for i,photo in enumerate(photos):
        size,train,hold=fit[i]
        p=params[1+i*7:8+i*7]
        focal=math.exp(float(p[0]))
        mat,_=cv2.Rodrigues(np.asarray(p[1:4]))
        center=-(mat.T@p[4:7])
        def stats(points):
            errors=np.linalg.norm(project(p,points,height,size)-observed(points),axis=1)
            return {"count":len(points),
                "rms_px":float(np.sqrt(np.mean(errors*errors))),
                "max_px":float(max(errors)),
                "errors_by_anchor_px":{a["id"]:float(e) for a,e in zip(points,errors)}}
        report["cameras"].append({
            "id":photo["id"],"source_sha256":photo["sha256"],"size_px":list(size),
            "focal_px":focal,
            "fov_horizontal_deg":math.degrees(2*math.atan(size[0]/(2*focal))),
            "position_in_bay_units":center.tolist(),
            "rvec_world_to_camera":list(map(float,p[1:4])),
            "tvec_world_to_camera":list(map(float,p[4:7])),
            "fit":stats(train),"holdout":stats(hold),
            "caution":"Pinhole pose is CONDITIONAL on manual bay assignments and model assumptions"
        })
    return report,params


def export_obj(path: Path, height: float, max_bay: int) -> None:
    # OBJ line geometry, no fictional wall thickness, roofs or back facade.
    lines=["# Photo2Scene WARP: normalized FRONT FACADE ONLY",
           "# X = -bay_index, Y = fitted normalized colonnade height, Z = 0",
           "# 1 unit along X is one ASSUMED repeated front bay. No meter scale."]
    for bay in range(max_bay+1):
        for y in [0.,height]:
            lines.append(f"v {-bay:.8f} {y:.8f} 0.00000000")
    for bay in range(max_bay+1):
        lines.append(f"l {2*bay+1} {2*bay+2}")
    lines.append("l "+" ".join(str(2*bay+1) for bay in range(max_bay+1)))
    lines.append("l "+" ".join(str(2*bay+2) for bay in range(max_bay+1)))
    path.write_text("\n".join(lines)+"\n",encoding="utf-8")


def export_overlay(path: Path, photo: dict, camera: dict, params: np.ndarray,
                   height: float, image_dir: Path, max_bay: int) -> str:
    original=image_dir/photo["file"]
    if not original.is_file():
        return "skipped: original not found"
    if hashlib.sha256(original.read_bytes()).hexdigest()!=photo["sha256"]:
        raise ValueError("Image checksum mismatch: "+str(original))
    image=cv2.imread(str(original))
    if image is None:
        raise ValueError("Cannot read image: "+str(original))
    size=(photo["width"],photo["height"])
    for bay in range(max_bay+1):
        anchors=[{"bay":bay,"level":"bottom"},{"bay":bay,"level":"top"}]
        pixels=np.round(project(params,anchors,height,size)).astype(int)
        cv2.line(image,tuple(pixels[0]),tuple(pixels[1]),(30,210,255),2,cv2.LINE_AA)
        if bay%2==0:
            cv2.putText(image,str(bay),tuple(pixels[1]+[0,-5]),
                cv2.FONT_HERSHEY_SIMPLEX,max(.4,min(.8,size[0]/2200)),
                (255,255,0),1,cv2.LINE_AA)
    for level in ("bottom","top"):
        anchors=[{"bay":bay,"level":level} for bay in range(max_bay+1)]
        pixels=np.round(project(params,anchors,height,size)).astype(int)
        cv2.polylines(image,[pixels],False,(60,245,45),2,cv2.LINE_AA)
    for anchor in photo["anchors"]:
        color=(80,240,80) if anchor["role"]=="fit" else (50,80,255)
        cv2.circle(image,tuple(map(int,anchor["pixel"])),3,color,-1,cv2.LINE_AA)
    heading=(f"Photo2Scene WARP / conditional 3D facade / "
             f"holdout {camera['holdout']['rms_px']:.1f}px")
    cv2.putText(image,heading,(15,25 if image.shape[1]<600 else 48),
                cv2.FONT_HERSHEY_SIMPLEX,.4 if image.shape[1]<600 else .7,
                (0,0,0),3,cv2.LINE_AA)
    cv2.putText(image,heading,(15,25 if image.shape[1]<600 else 48),
                cv2.FONT_HERSHEY_SIMPLEX,.4 if image.shape[1]<600 else .7,
                (255,255,255),1,cv2.LINE_AA)
    cv2.imwrite(str(path),image)
    return str(path)


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("annotations",type=Path)
    parser.add_argument("-o","--output-dir",type=Path,required=True)
    parser.add_argument("--originals-dir",type=Path,
                        help="Optional local originals; never commit redistributed photos")
    args=parser.parse_args()
    data=json.loads(args.annotations.read_text(encoding="utf-8"))
    report,params=solve(data)
    args.output_dir.mkdir(parents=True,exist_ok=True)
    max_bay=max(a["bay"] for p in data["photos"] for a in p["anchors"])
    report["exported_bays"]={"from":0,"to":max_bay,
                            "status":"repeated grid assumed; columns outside observed range omitted"}
    (args.output_dir/"facade-calibration.json").write_text(
        json.dumps(report,indent=2,ensure_ascii=False,allow_nan=False)+"\n",encoding="utf-8")
    export_obj(args.output_dir/"energetik-front-plane-units.obj",
               report["gauge"]["height_visible_colonnade_in_bays"],max_bay)
    if args.originals_dir:
        for i,photo in enumerate(data["photos"]):
            overlay=export_overlay(args.output_dir/f"photo-{i+1}-reprojection.png",
                photo,report["cameras"][i],params[1+i*7:8+i*7],
                report["gauge"]["height_visible_colonnade_in_bays"],
                args.originals_dir,max_bay)
            print(overlay)
    print(json.dumps({"status":report["status"],"height_in_bays":
          report["gauge"]["height_visible_colonnade_in_bays"],
          "holdout_rms_px":[x["holdout"]["rms_px"] for x in report["cameras"]],
          "fov_deg":[x["fov_horizontal_deg"] for x in report["cameras"]]},
          indent=2))


if __name__=="__main__":
    main()
