#!/usr/bin/env python3
"""Photo2Scene WARP: CONDITIONAL three-camera fit of one Energetik facade plane.

Run with Python + numpy + scipy + opencv-python-headless.
No authentic 3D depth or absolute scale is estimated by this prototype.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import cv2
import numpy as np
from scipy.optimize import least_squares


def prepare(data):
    photos=[]
    for item in data["photos"]:
        train,holdout=[],[]
        for level in ("top","bottom"):
            for bay,x,y in item[level]:
                entry={"id":f"bay{bay}-{level}","scene_xy":[-float(bay),float(level=="top")],"pixel":[x,y]}
                (train if bay%2==0 else holdout).append(entry)
        if len(train)<6 or not holdout:
            raise ValueError("Not enough holdout and training points")
        photos.append({**item,"train":train,"holdout":holdout})
    return photos


def solve(data, initial_focals=None):
    photos=prepare(data)
    initial_focals=initial_focals or [530.,2200.,2700.]
    height_init=2.4
    params=[math.log(height_init)]
    for i,photo in enumerate(photos):
        w,h=photo["image_size_px"]
        focal=initial_focals[i] if i<len(initial_focals) else w*1.3
        K=np.array([[focal,0,w/2],[0,focal,h/2],[0,0,1.]],float)
        xyz=np.array([[a["scene_xy"][0],a["scene_xy"][1]*height_init,0] for a in photo["train"]],float)
        uv=np.array([a["pixel"] for a in photo["train"]],float)
        success,r,t=cv2.solvePnP(xyz,uv,K,None,flags=cv2.SOLVEPNP_ITERATIVE)
        if not success:
            raise ValueError("Camera initialization failed for "+photo["id"])
        params.extend([math.log(focal),*r.ravel(),*t.ravel()])
    def predict(i,points,p):
        height=math.exp(float(p[0]))
        k=1+7*i
        focal=math.exp(float(p[k]))
        w,h=photos[i]["image_size_px"]
        K=np.array([[focal,0,w/2],[0,focal,h/2],[0,0,1.]],float)
        xyz=np.array([[a["scene_xy"][0],a["scene_xy"][1]*height,0] for a in points],float)
        pts,_=cv2.projectPoints(xyz,p[k+1:k+4],p[k+4:k+7],K,None)
        return pts.reshape(-1,2)
    def residual(p):
        return np.concatenate([(predict(i,photo["train"],p)-
            np.array([a["pixel"] for a in photo["train"]],float)).ravel()
            for i,photo in enumerate(photos)])
    low=[math.log(1.)];high=[math.log(6.)]
    for _ in photos:
        low.extend([math.log(180)]+[-np.inf]*6)
        high.extend([math.log(10000)]+[np.inf]*6)
    opt=least_squares(residual,np.array(params),loss="soft_l1",
        f_scale=3.0,bounds=(low,high),max_nfev=400,
        ftol=1e-10,xtol=1e-10,gtol=1e-10)
    p=opt.x
    output={"format":"photo2scene-warp-conditional-front-plane-v1",
       "status":"EXPERIMENTAL_UNVERIFIED_LANDMARKS",
       "height_in_assumed_bay_units":math.exp(float(p[0])),
       "units":{"X":"1 unit = one assumed facade bay","Y":"same X unit","Z":"0 (no depth recovered)"},
       "solver":{"converged":bool(opt.success),"cost":float(opt.cost),
                 "method":"robust least_squares soft_l1 3px"},
       "fov_and_camera_poses":"CONDITIONAL_ON_MANUAL_BAY_MATCHES",
       "photos":[],"unknown":["facade actual dimensions","side return depth","roof geometry",
              "authentic camera focal lengths","physical meters-per-bay"]}
    for i,photo in enumerate(photos):
        k=1+7*i
        f=math.exp(float(p[k]))
        w,h=photo["image_size_px"]
        R,_=cv2.Rodrigues(p[k+1:k+4])
        cam=-R.T@p[k+4:k+7]
        record={"id":photo["id"],"image_size_px":[w,h],"source_sha256":photo["sha256"],
            "focal_px":f,"fov_horizontal_deg":math.degrees(2*math.atan(w/(2*f))),
            "camera_position_units":cam.tolist(),
            "rvec_world_to_camera":p[k+1:k+4].tolist(),
            "tvec_world_to_camera":p[k+4:k+7].tolist()}
        for label in ("train","holdout"):
            points=photo[label]
            errs=np.linalg.norm(predict(i,points,p)-
                    np.array([a["pixel"] for a in points],float),axis=1)
            record[label+"_rms_px"]=float(np.sqrt(np.mean(errs**2)))
            record[label+"_max_px"]=float(np.max(errs))
        output["photos"].append(record)
    return output


def export_obj(height,count=14):
    lines=["# Photo2Scene WARP: conditional facade plane ONLY, no measured depth",
           "# One X unit is one UNVERIFIED bay. Z is fixed to zero."]
    for bay in range(count):
        for y in (0.,height):
            lines.append(f"v {-bay:.8f} {y:.8f} 0")
    for bay in range(count):
        lines.append(f"l {2*bay+1} {2*bay+2}")
    for off in (1,2):
        lines.append("l "+" ".join(str(2*bay+off) for bay in range(count)))
    return "\n".join(lines)+"\n"


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("annotations",type=Path)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--obj",type=Path,default=None)
    a=p.parse_args()
    report=solve(json.loads(a.annotations.read_text(encoding="utf-8")))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    if a.obj:
        a.obj.parent.mkdir(parents=True,exist_ok=True)
        a.obj.write_text(export_obj(report["height_in_assumed_bay_units"]),encoding="utf-8")
    print("Calculated",len(report["photos"]),"conditional cameras")
    for item in report["photos"]:
        print(item["id"],"holdout RMS",round(item["holdout_rms_px"],2),
              "FOV conditional",round(item["fov_horizontal_deg"],2))


if __name__=="__main__":
    main()
