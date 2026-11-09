#!/usr/bin/env python3
"""Photo2Scene WARP — deterministic video evidence, never inferred metric depth.

Inputs stay local. GitHub stores only code and annotations, NOT user footage.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import cv2
import numpy as np

def frame_at(video, seconds):
    cap=cv2.VideoCapture(str(video))
    if not cap.isOpened(): raise RuntimeError("Video inaccessible: "+str(video))
    cap.set(cv2.CAP_PROP_POS_MSEC,1000*float(seconds))
    ok,frame=cap.read()
    cap.release()
    if not ok: raise RuntimeError("Frame unavailable at "+str(seconds))
    return frame

def find_lines(frame,roi,min_length=18):
    x0,y0,x1,y1=roi
    gray=cv2.cvtColor(frame[y0:y1,x0:x1],cv2.COLOR_BGR2GRAY)
    gray=cv2.createCLAHE(clipLimit=2.5,tileGridSize=(8,8)).apply(gray)
    lines=cv2.createLineSegmentDetector(cv2.LSD_REFINE_STD).detect(gray)[0]
    if lines is None:return []
    result=[]
    for row in lines.reshape((-1,4)):
        a,b,c,d=map(float,row); length=math.hypot(c-a,d-b)
        if length<min_length:continue
        angle=math.degrees(math.atan2(d-b,c-a))%180
        category="vertical" if 75<angle<105 else ("horizontal" if angle<15 or angle>165 else "oblique")
        result.append({"line_px":[round(a+x0,2),round(b+y0,2),round(c+x0,2),round(d+y0,2)],
                       "length_px":round(length,2),"category":category})
    return sorted(result,key=lambda v:-v["length_px"])

def rectify(frame,corners,size=(1100,250)):
    corners=np.float32(corners)
    if abs(cv2.contourArea(corners))<50:raise ValueError("Invalid quadrilateral")
    w,h=size
    dest=np.float32([[0,0],[w-1,0],[w-1,h-1],[0,h-1]])
    H=cv2.getPerspectiveTransform(corners,dest)
    if abs(np.linalg.det(H))<1e-12:raise ValueError("Singular warp")
    return cv2.warpPerspective(frame,H,size),H

def match(a,b,exclusions=()):
    h,w=a.shape[:2];mask=np.full((h,w),255,np.uint8)
    for x0,y0,x1,y1 in exclusions:cv2.rectangle(mask,(x0,y0),(x1,y1),0,-1)
    orb=cv2.ORB_create(nfeatures=3500,fastThreshold=9)
    ka,da=orb.detectAndCompute(cv2.cvtColor(a,cv2.COLOR_BGR2GRAY),mask)
    kb,db=orb.detectAndCompute(cv2.cvtColor(b,cv2.COLOR_BGR2GRAY),mask)
    result={"matches":0,"homography_inliers":0,"fundamental_inliers":0,
            "status":"insufficient_matches","metric_3d_available":False}
    if da is None or db is None:return result
    pairs=cv2.BFMatcher(cv2.NORM_HAMMING).knnMatch(da,db,k=2)
    good=[p for p,q in pairs if p.distance<0.75*q.distance]
    result["matches"]=len(good)
    if len(good)<16:return result
    pa=np.float32([ka[p.queryIdx].pt for p in good])
    pb=np.float32([kb[p.trainIdx].pt for p in good])
    _,hm=cv2.findHomography(pa,pb,cv2.RANSAC,3.0)
    F,fm=cv2.findFundamentalMat(pa,pb,cv2.FM_RANSAC,2.0,0.995)
    hom=int(hm.sum()) if hm is not None else 0
    fund=int(fm.sum()) if fm is not None else 0
    fraction=hom/len(good)
    result.update({"homography_inliers":hom,"fundamental_inliers":fund,
                   "homography_fraction":round(fraction,3),
                   "median_displacement_px":round(float(np.median(np.linalg.norm(pb-pa,axis=1))),2),
                   "status":("planar_or_rotation_degenerate" if fraction>=0.8 else
                             ("uncalibrated_multiview_candidate" if fund>=24 else "weak_matches"))})
    # 2D correspondences never prove calibrated 3D; preserve the unknown gauge.
    return result

def write_image_plane(destination,texture,aspect):
    """OBJ represents a dimensionless *image reference plane*, not built architecture."""
    h=float(aspect)
    obj=("# Unit-width photographic plane; not metres or a reconstructed wall\n"
         "mtllib facade_reference.mtl\no evidence_nonmetric\n"
         f"v 0 0 0\nv 1 0 0\nv 1 0 {h}\nv 0 0 {h}\n"
         "vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
         "usemtl photo\nf 1/1 2/2 3/3\nf 1/1 3/3 4/4\n")
    destination.write_text(obj,encoding="utf-8")
    (destination.parent/"facade_reference.mtl").write_text(
        "newmtl photo\nKd 1 1 1\nmap_Kd "+texture.name+"\n",encoding="utf-8")
    results={"obj":destination.name,"texture":texture.name,"units":"normalized only"}
    try:
        import trimesh
        from PIL import Image
        verts=np.array([[0,0,0],[1,0,0],[1,0,h],[0,0,h]])
        faces=np.array([[0,1,2],[0,2,3]])
        uvs=np.array([[0,1],[1,1],[1,0],[0,0]])
        mesh=trimesh.Trimesh(vertices=verts,faces=faces,process=False,
                    visual=trimesh.visual.TextureVisuals(uv=uvs,image=Image.open(texture)))
        mesh.export(destination.with_suffix(".glb"))
        results["glb"]=destination.with_suffix(".glb").name
    except ImportError:
        results["glb"]=None
    return results

def process(root,config,out):
    out.mkdir(parents=True,exist_ok=True)
    frames={}
    report={"schema":"photo2scene/video-evidence-v1",
            "source_policy":"user-provided videos; never commit footage or stills to public repo",
            "scale":"unknown","camera_calibration":"not available","metric_3d_available":False,
            "views":[],"comparisons":[],"reference_mesh":None}
    for view in config["views"]:
        name=view["id"]
        frame=frame_at(root/view["file"],view["time_s"])
        frames[name]=frame
        h,w=frame.shape[:2]
        lines=find_lines(frame,view.get("roi",[0,0,w,h]))
        cv2.imwrite(str(out/(name+".jpg")),frame)
        annotated=frame.copy()
        for segment in lines[:65]:
            x1,y1,x2,y2=map(int,segment["line_px"])
            cv2.line(annotated,(x1,y1),(x2,y2),(0,220,255),1)
        if "polyline_px" in view:
            cv2.polylines(annotated,[np.int32(view["polyline_px"])],False,(0,0,255),3)
        if "quad_px" in view:
            cv2.polylines(annotated,[np.int32(view["quad_px"])],True,(0,255,0),2)
            patch,H=rectify(frame,view["quad_px"])
            texture=out/"facade_reference.jpg"
            cv2.imwrite(str(texture),patch)
            (out/"facade_warp.json").write_text(json.dumps({
                "image_to_patch_H":H.tolist(),"quad_px":view["quad_px"],
                "annotation_status":"manual, not independently verified",
                "patch_size_px":[1100,250],"metric_scale":None},indent=2))
            report["reference_mesh"]=write_image_plane(
                out/"facade_evidence_normalized.obj",texture,250/1100)
        cv2.imwrite(str(out/(name+"_lines.jpg")),annotated)
        report["views"].append({"id":name,"video":view["file"],"time_s":view["time_s"],
           "image_size_px":[w,h],"role":view["role"],"detected_line_segments":len(lines),
           "vertical_segments":sum(s["category"]=="vertical" for s in lines),
           "segments_sample":lines[:30]})
    for a,b in config.get("pairs",[]):
        exclusions=config.get("exclude_rectangles",{}).get(a,[])
        report["comparisons"].append({"a":a,"b":b,**match(frames[a],frames[b],exclusions)})
    (out/"report.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    with (out/"segments.csv").open("w",encoding="utf-8",newline="") as fp:
        writer=csv.writer(fp);writer.writerow(["view","x1","y1","x2","y2","length_px","category"])
        for view in report["views"]:
            for s in view["segments_sample"]:
                writer.writerow([view["id"],*s["line_px"],s["length_px"],s["category"]])
    return report

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--root",type=Path,required=True)
    parser.add_argument("--config",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    a=parser.parse_args()
    r=process(a.root,json.loads(a.config.read_text(encoding="utf-8")),a.output)
    print(json.dumps({"n_views":len(r["views"]),"comparisons":r["comparisons"],
                      "metric_3d":r["metric_3d_available"]},indent=2))
