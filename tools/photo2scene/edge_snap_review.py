#!/usr/bin/env python3
"""Photo2Scene WARP: image-space-only facade line review.

Images remain local. Manual lines are softened by a <=7px Sobel normal search;
LSD lines are computer-vision candidates, never verified building edges.
Usage:
 python edge_snap_review.py input.jpg annotations.json -o review.json --overlay review.png

Annotations format:
 {"roi":[[x,y],...], "lines":[{"name":"corniche","points":[[x1,y1],[x2,y2]]}]}
"""
import argparse
import json
import math
from pathlib import Path
import cv2
import numpy as np


def locate_edge(image, endpoints, max_shift_px=7.0):
    gray=cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)
    gray=cv2.GaussianBlur(gray,(5,5),1.1)
    gx=cv2.Sobel(gray,cv2.CV_32F,1,0,ksize=3)
    gy=cv2.Sobel(gray,cv2.CV_32F,0,1,ksize=3)
    a,b=np.array(endpoints,dtype=np.float32)
    delta=b-a
    norm=float(np.linalg.norm(delta))
    if norm<2:raise ValueError("Cannot trace coincident endpoints")
    normal=np.array([-delta[1],delta[0]],np.float32)/norm
    t=np.linspace(.06,.94,max(20,int(norm/4)))[:,None]
    base=a[None,:]*(1-t)+b[None,:]*t
    h,w=gray.shape
    options=[]
    for off in np.linspace(-max_shift_px,max_shift_px,29):
        xy=base+off*normal
        x=np.clip(np.rint(xy[:,0]).astype(int),0,w-1)
        y=np.clip(np.rint(xy[:,1]).astype(int),0,h-1)
        amp=np.sort(np.abs(gx[y,x]*normal[0]+gy[y,x]*normal[1]))
        score=float(np.mean(amp[int(.2*len(amp)):]))
        options.append((score,float(off)))
    score,shift=max(options)
    return {"initial_endpoints_px":endpoints,
        "refined_endpoints_px":[(a+shift*normal).round(2).tolist(),
                                (b+shift*normal).round(2).tolist()],
        "normal_shift_px":round(shift,2),
        "gradient_support":round(score,2),
        "status":"ESTIMATED_IMAGE_EDGE_NOT_GROUND_TRUTH"}


def lsd_candidates(image, polygon, maximum=48):
    gray=cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)
    eq=cv2.createCLAHE(clipLimit=1.6,tileGridSize=(8,8)).apply(gray)
    output=cv2.createLineSegmentDetector(cv2.LSD_REFINE_STD).detect(eq)[0]
    if output is None:return []
    mask=np.zeros(gray.shape,dtype=np.uint8)
    cv2.fillPoly(mask,[np.array(polygon,np.int32)],255)
    min_len=max(18,min(gray.shape)*.037)
    lines=[]
    for line in output.reshape(-1,4):
        x1,y1,x2,y2=map(float,line)
        length=math.hypot(x2-x1,y2-y1)
        x,y=round((x1+x2)/2),round((y1+y2)/2)
        if length<min_len or not(0<=x<gray.shape[1] and 0<=y<gray.shape[0]
            and mask[y,x]):continue
        angle=math.degrees(math.atan2(y2-y1,x2-x1))%180
        if not(angle<=35 or angle>=145 or 68<=angle<=112):continue
        lines.append({"points": [[round(x1,2),round(y1,2)],
                                  [round(x2,2),round(y2,2)]],
                      "length_px":round(length,2),
                      "angle_deg":round(angle,2),
                      "status":"UNVERIFIED_AUTOMATIC_CANDIDATE"})
    lines.sort(key=lambda v:-v["length_px"])
    return lines[:maximum]


def analyse(image,annotation):
    mask=annotation["roi"]
    chosen=[{"name":x["name"],**locate_edge(image,x["points"])}
            for x in annotation.get("lines",[])]
    candidates=lsd_candidates(image,mask)
    return {"status":"IMAGE_SPACE_ONLY","metric_3d":None,
            "manual_edge_estimates":chosen,"automatic_candidates":candidates}


def main():
    p=argparse.ArgumentParser()
    p.add_argument("image",type=Path)
    p.add_argument("annotation",type=Path)
    p.add_argument("-o","--output",type=Path,required=True)
    p.add_argument("--overlay",type=Path)
    args=p.parse_args()
    src=cv2.imread(str(args.image))
    if src is None:raise SystemExit("Image cannot be decoded")
    cfg=json.loads(args.annotation.read_text(encoding="utf-8"))
    result=analyse(src,cfg)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    if args.overlay:
        img=src.copy()
        for cand in result["automatic_candidates"]:
            a,b=[tuple(map(int,v)) for v in cand["points"]]
            cv2.line(img,a,b,(225,165,35),1,cv2.LINE_AA)
        for edge in result["manual_edge_estimates"]:
            a,b=[tuple(map(int,v)) for v in edge["refined_endpoints_px"]]
            cv2.line(img,a,b,(40,220,240),2,cv2.LINE_AA)
        args.overlay.parent.mkdir(parents=True,exist_ok=True)
        cv2.imwrite(str(args.overlay),img)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
