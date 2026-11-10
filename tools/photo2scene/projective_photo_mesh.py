#!/usr/bin/env python3
"""Photo2Scene WARP: CPU reprojection of original image pixels onto 3D planes.

Uses relative SfM coordinates + measured planes. DOES NOT invent metric walls,
train Gaussian Splatting, or publish the user's source media.
Requires: numpy, opencv-python-headless, Pillow, trimesh.
"""
import argparse
import json
from pathlib import Path
import cv2
import numpy as np


def unit(n):
    n = np.asarray(n, dtype=float)
    norm = np.linalg.norm(n)
    if norm < 1e-10: raise ValueError("degenerate plane normal")
    return n / norm


def ray_plane_intersection(pixel, normal, center, K):
    """Intersect image ray with relative 3D plane in anchor-camera coordinates."""
    ray = np.linalg.solve(K, np.array([pixel[0], pixel[1], 1.], float))
    denominator = ray @ normal
    if abs(denominator) < 1e-8:
        raise ValueError("camera ray parallel to plane")
    scale = (center @ normal) / denominator
    if not 2. < scale < 90.:
        raise ValueError("unreliable image ray depth: "+str(scale))
    return scale * ray


def assemble_faces(cfg, K):
    polygons = cfg["source_quads_px"]
    fits = cfg["planes_from_264_sparse_points"]
    norm = {name:unit(data["normal"]) for name,data in fits.items()}
    cen = {name:np.array(data["centroid"],float) for name,data in fits.items()}
    ray = lambda pixel,n,c: ray_plane_intersection(pixel,n,c,K)
    roof = np.array([ray(p,norm["roof_main"],cen["roof_main"])
                     for p in polygons["main_roof"]])
    # The facade is constrained by the roof edge and roof perpendicular.
    # This fixes the former failure where unconstrained facade RANSAC
    # found an almost horizontal plane.
    facade_axis = roof[3] - roof[0]
    front_n = unit(np.cross(facade_axis,norm["roof_main"]))
    front_c = (roof[3]+roof[0])*.5
    side_axis = roof[2] - roof[3]
    side_n = unit(np.cross(side_axis,norm["roof_main"]))
    side_c = (roof[2]+roof[3])*.5
    rear_roof = np.array([ray(p,norm["rear_block"],cen["rear_block"])
                          for p in polygons["rear_roof"]])
    rear_axis = rear_roof[3] - rear_roof[0]
    rear_n = unit(np.cross(rear_axis,norm["rear_block"]))
    rear_c = (rear_roof[3]+rear_roof[0])*.5
    plane_specs = {
        "main_roof":(norm["roof_main"],cen["roof_main"]),
        "front_glazing":(front_n,front_c),
        "side_return":(side_n,side_c),
        "upper_block_wall":(norm["stage_tower"],cen["stage_tower"]),
        "rear_roof":(norm["rear_block"],cen["rear_block"]),
        "rear_wall":(rear_n,rear_c)
    }
    result=[]
    for name,pixels in polygons.items():
        normal,center=plane_specs[name]
        xyz=np.array([ray(p,normal,center) for p in pixels])
        result.append({"name":name,"pixels":np.float32(pixels),
                       "xyz":xyz,"normal":normal})
    return result


def camera_pose_nerfstudio(frame):
    # Nerfstudio camera-to-world is OpenGL style, but the recovered
    # SfM world is defined here in OpenCV anchor-camera coordinates.
    transform=np.array(frame["transform_matrix"],float)
    cv_to_gl=np.diag([1.,-1.,-1.,1.])
    w2c=np.linalg.inv(transform @ cv_to_gl)
    return w2c[:3,:3],w2c[:3,3].reshape(3,1)


def project(vertices,R,t,K):
    local=vertices@R.T + t.reshape(1,3)
    if np.any(local[:,2] <= 0.05):
        raise ValueError("projected panel intersects/behind camera")
    homogeneous=local@K.T
    return homogeneous[:,:2]/homogeneous[:,2,None],local[:,2]


def render(source,faces,R,t,K,background=None):
    h,w=source.shape[:2]
    image=np.full((h,w,3),247,np.uint8) if background is None else background.copy()
    valid=np.zeros((h,w),np.uint8)
    items=[]
    for face in faces:
        try:dst,depth=project(face["xyz"],R,t,K)
        except ValueError:continue
        items.append((np.median(depth),face,dst))
    # Far faces first. This sorting is only a preview approximation, not
    # a per-pixel depth buffer; intersections should be checked separately.
    for _,face,dst in sorted(items,key=lambda item:-item[0]):
        T=cv2.getPerspectiveTransform(face["pixels"],np.float32(dst))
        projected=cv2.warpPerspective(source,T,(w,h),flags=cv2.INTER_LINEAR)
        mask=np.zeros((h,w),np.uint8)
        cv2.fillConvexPoly(mask,np.rint(dst).astype(np.int32),255,cv2.LINE_AA)
        keep=mask>0
        image[keep]=projected[keep]
        valid[keep]=255
    return image,valid


def export_glb(destination,source_image,faces):
    import trimesh
    from PIL import Image
    src=Image.fromarray(cv2.cvtColor(source_image,cv2.COLOR_BGR2RGB))
    width,height=src.size
    scene=trimesh.Scene()
    for face in faces:
        mesh=trimesh.Trimesh(vertices=face["xyz"],
             faces=[[0,1,2],[0,2,3]],process=False)
        xy=face["pixels"]
        uv=np.column_stack((xy[:,0]/width,1-xy[:,1]/height))
        mesh.visual=trimesh.visual.TextureVisuals(uv=uv,image=src.copy())
        scene.add_geometry(mesh,geom_name=face["name"])
    scene.export(destination)


def look_at(eye,center,up):
    forward=unit(center-eye)
    right=unit(np.cross(forward,up))
    down=unit(np.cross(forward,right))
    R=np.stack([right,down,forward])
    return R,(-R@eye).reshape(3,1)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dataset",type=Path,required=True,
                    help="existing 13-pose dataset (images + transforms)")
    ap.add_argument("--constraints",type=Path,required=True,
                    help="source pixel polygons + relative plane fits")
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    cfg=json.loads(args.constraints.read_text(encoding="utf8"))
    meta=json.loads((args.dataset/"transforms.json").read_text(encoding="utf8"))
    width,height=meta["w"],meta["h"]
    if [width,height]!=cfg["source_px_size"]:
        raise ValueError("source reference resolution inconsistent with data")
    K=np.array([[meta["fl_x"],0,meta["cx"]],[0,meta["fl_y"],meta["cy"]],
                [0,0,1]],float)
    if abs(K[0,0]-cfg["focal_px_assumed"])>1e-6:
        raise ValueError("incompatible source camera focal length")
    source=cv2.imread(str(args.dataset/"images/frame_04.png"))
    if source is None:raise RuntimeError("missing anchor photograph")
    faces=assemble_faces(cfg,K)
    args.output.mkdir(parents=True,exist_ok=True)
    export_glb(args.output/"energetik_photo_projected_nonmetric.glb",source,faces)
    rows=[]
    for item in meta["frames"]:
        ts=float(item["time_s"])
        if ts not in (373.,373.5,374.,374.75,375.,375.5):continue
        R,t=camera_pose_nerfstudio(item)
        rendered,coverage=render(source,faces,R,t,K)
        cv2.imwrite(str(args.output/f"projection_{ts:.2f}.png"),rendered)
        frame_path=args.dataset/item["file_path"]
        orig=cv2.imread(str(frame_path))
        mask=cv2.imread(str(args.dataset/item["mask_path"]),0)
        active=(coverage>0)&(mask>0)
        if np.sum(active):
            rmse=float(np.sqrt(np.mean((rendered[active].astype(float)-
                                         orig[active].astype(float))**2)))
            psnr=float(20*np.log10(255/max(rmse,1e-9)))
        else:psnr=None
        rows.append(dict(time_s=ts,holdout=ts in (374.,375.),
                         pixels_evaluated=int(active.sum()),psnr_rgb_db=psnr))
    center=np.vstack([f["xyz"] for f in faces]).mean(axis=0)
    up=-faces[0]["normal"]
    positions={
        "three_quarter":[2.8,-.45,.45],
        "roof_high":[.5,-2.4,.2],
        "side_left":[-2.6,-.2,.35],
        "oblique_far":[1.3,-.6,-1.5]
    }
    for name,position in positions.items():
        R,t=look_at(np.array(position,float),center,up)
        img,coverage=render(source,faces,R,t,K)
        cv2.imwrite(str(args.output/f"view_{name}.png"),img)
    output={
        "status":"NONMETRIC_PHOTO_TEXTURED_PLANES_NOT_COMPLETE_BUILDING",
        "face_count":len(faces),
        "source_pixels":"original user drone frame",
        "test_views":rows,
        "focal_calibration": "ASSUMED—not verified from original DJI metadata",
        "metric_scale":None,
        "geometry":[{"name":f["name"],"xyz_nonmetric":f["xyz"].tolist(),
                     "image_quad_px":f["pixels"].tolist()}
                    for f in faces],
        "known_limitations":["vegetation occlusion","surfaces outside source views missing",
                             "stage tower not dimensioned","no absolute scale",
                             "preview renderer uses coarse depth sorting"]
    }
    (args.output/"projection_validation.json").write_text(
        json.dumps(output,indent=2,ensure_ascii=False)+"\n",encoding="utf8")
    print(json.dumps({"planes":len(faces),"validation":rows},indent=2))


if __name__=="__main__":
    main()
