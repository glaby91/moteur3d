#!/usr/bin/env python3
"""Photo2Scene WARP: dense CPU Gaussian initialization from three real drone views.

Does NOT train CUDA 3DGS, recover metric scale, or inpaint missing building walls.
The input video remains on the user's machine (no media uploaded to GitHub).
Requires numpy, opencv-python-headless.
"""
import argparse
import json
import math
from pathlib import Path
import cv2
import numpy as np

cv2.setRNGSeed(20261110)
SH0 = .28209479177387814

def frame(cap, sec):
    cap.set(cv2.CAP_PROP_POS_MSEC, sec*1000)
    ret, image=cap.read()
    if not ret: raise ValueError("Could not read frame at "+str(sec))
    return image

def mask(image, polys):
    m=np.zeros(image.shape[:2],np.uint8)
    for poly in polys: cv2.fillPoly(m,[np.int32(poly)],255)
    return cv2.erode(m,np.ones((3,3),np.uint8))

def features(image,m):
    gray=cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)
    return cv2.SIFT_create(nfeatures=5000,contrastThreshold=.012,edgeThreshold=13).detectAndCompute(gray,m)

def matches(a,b):
    bf=cv2.BFMatcher(cv2.NORM_L2)
    good=sorted([x for x,y in bf.knnMatch(a[1],b[1],k=2) if x.distance<.77*y.distance],
                key=lambda v:v.distance)
    xy0=np.float64([a[0][x.queryIdx].pt for x in good])
    xy1=np.float64([b[0][x.trainIdx].pt for x in good])
    return good,xy0,xy1

def dense_flow(image0,image1):
    flow=cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)
    flow.setUseSpatialPropagation(True)
    return flow.calc(cv2.cvtColor(image0,cv2.COLOR_BGR2GRAY),
                     cv2.cvtColor(image1,cv2.COLOR_BGR2GRAY),None)

def sample(arr,points,interpolation=cv2.INTER_LINEAR):
    return cv2.remap(arr,points[:,0].reshape((-1,1)),points[:,1].reshape((-1,1)),
                     interpolation,borderMode=cv2.BORDER_CONSTANT).reshape((-1,)+arr.shape[2:])

def reconstruct(images,polys,focal=550.0,stride=2):
    if len(images)!=3: raise ValueError("Exactly three ordered frames required")
    hh,ww=images[0].shape[:2]
    if any(im.shape!=images[0].shape for im in images):
        raise ValueError("Image resolution changed")
    masks=[mask(im,p) for im,p in zip(images,polys)]
    fs=[features(im,m) for im,m in zip(images,masks)]
    track02,a,b=matches(fs[0],fs[2]);track01,_,_=matches(fs[0],fs[1])
    if len(track02)<30:raise ValueError("Too few matches for SfM")
    K=np.float64([[focal,0,ww/2],[0,focal,hh/2],[0,0,1.]])
    E,maskE=cv2.findEssentialMat(a,b,K,cv2.RANSAC,.999,1.5,maxIters=12000)
    if E is None:raise ValueError("Cannot estimate essential matrix")
    _,R,t,inlier=cv2.recoverPose(E[:3,:3],a,b,K,mask=maskE)
    keep=inlier.ravel().astype(bool)
    P0=K@np.c_[np.eye(3),np.zeros((3,1))]
    P2=K@np.c_[R,t]
    hom=cv2.triangulatePoints(P0,P2,a[keep].T,b[keep].T)
    xyz_sparse=(hom[:3]/hom[3]).T
    goodtrack=[track02[i] for i in np.flatnonzero(keep)]
    linked={v.queryIdx:v.trainIdx for v in track01}
    ids=[j for j,v in enumerate(goodtrack) if v.queryIdx in linked and xyz_sparse[j,2]>0]
    if len(ids)<12:raise ValueError("Too few tracks to locate intermediate camera")
    xyz=np.float64([xyz_sparse[j] for j in ids])
    uv=np.float64([fs[1][0][linked[goodtrack[j].queryIdx]].pt for j in ids])
    ok,rvec,tvec,pnp=cv2.solvePnPRansac(xyz,uv,K,None,flags=cv2.SOLVEPNP_EPNP,
                                       reprojectionError=3.,iterationsCount=1200,confidence=.999)
    if not ok:raise ValueError("Middle camera pose failed")
    R1=cv2.Rodrigues(rvec)[0]
    f02=dense_flow(images[0],images[2]);f20=dense_flow(images[2],images[0])
    f01=dense_flow(images[0],images[1]);f10=dense_flow(images[1],images[0])
    xs,ys=np.meshgrid(np.arange(0,ww,stride),np.arange(0,hh,stride))
    seed=np.c_[xs.ravel(),ys.ravel()].astype(np.float32)
    seed=seed[masks[0][seed[:,1].astype(int),seed[:,0].astype(int)]>0]
    p2=seed+sample(f02,seed);p1=seed+sample(f01,seed)
    bounds=lambda p:(p[:,0]>2)&(p[:,0]<ww-3)&(p[:,1]>2)&(p[:,1]<hh-3)
    matchgood=(bounds(p2)&bounds(p1)&
      (sample(masks[2],p2,cv2.INTER_NEAREST)>0)&
      (sample(masks[1],p1,cv2.INTER_NEAREST)>0)&
      (np.linalg.norm(p2+sample(f20,p2)-seed,axis=1)<1.)&
      (np.linalg.norm(p1+sample(f10,p1)-seed,axis=1)<1.))
    seed,p1,p2=seed[matchgood],p1[matchgood],p2[matchgood]
    h3=cv2.triangulatePoints(P0,P2,seed.T,p2.T)
    xyz=(h3[:3]/h3[3]).T
    xyz1=xyz@R1.T+tvec.T;xyz2=xyz@R.T+t.T
    xp=xyz1@K.T;xp=xp[:,:2]/xp[:,2,None]
    error=np.linalg.norm(xp-p1,axis=1)
    # F is computed from the returned relative camera pose, not fit to dense flow.
    T=np.array([[0,-t[2,0],t[1,0]],[t[2,0],0,-t[0,0]],[-t[1,0],t[0,0],0]])
    F=np.linalg.inv(K).T @ (T@R) @ np.linalg.inv(K)
    h0=np.c_[seed,np.ones(len(seed))]
    h2=np.c_[p2,np.ones(len(p2))]
    l2=h0@F.T;l0=h2@F
    sampson=(np.sum(h2*l2,axis=1)**2)/(l0[:,0]**2+l0[:,1]**2+l2[:,0]**2+l2[:,1]**2+1.e-9)
    good=(np.isfinite(xyz).all(axis=1)&(xyz[:,2]>2)&(xyz[:,2]<100)&
          (xyz1[:,2]>0)&(xyz2[:,2]>0)&(error<1.5)&(sampson<2.25))
    points=xyz[good].astype(np.float32)
    pixels=seed[good].astype(np.int32)
    colors=cv2.cvtColor(images[0],cv2.COLOR_BGR2RGB)[pixels[:,1],pixels[:,0]]/255.
    scale=np.clip(points[:,2]/focal*stride*.85,.003,.40)
    # Conservative 3D disks oriented toward first camera (initial values only).
    scales=np.stack([scale,scale,scale*.42],axis=1).astype(np.float32)
    alpha=np.full(len(points),.92,np.float32)
    if len(points)<300:raise ValueError("Too few high-confidence dense matches")
    metrics=dict(raw_sift_matches=len(track02),essential_inliers=int(sum(keep)),
                 pnp_inliers=len(pnp),initial_flow_points=len(seed),
                 accepted_gaussians=len(points),
                 middle_view_median_reprojection_px=float(np.median(error[good])),
                 middle_view_rms_reprojection_px=float(np.sqrt(np.mean(error[good]**2))),
                 focal_assumed_px=focal,baseline_m=None,measured_scale_m=None,
                 stage="CPU photogrammetric initialization, NOT full 3DGS training")
    return points,colors.astype(np.float32),scales,alpha,K,metrics

def export_ply(path,points,colors,scales,alpha):
    names=['x','y','z','nx','ny','nz','f_dc_0','f_dc_1','f_dc_2'] + [
        'f_rest_'+str(i) for i in range(45)] + ['opacity','scale_0','scale_1','scale_2',
                                                'rot_0','rot_1','rot_2','rot_3']
    arr=np.zeros((len(points),len(names)),np.float32)
    arr[:,:3]=points
    arr[:,6:9]=(colors-.5)/SH0
    arr[:,54]=np.log(alpha/(1-alpha))
    arr[:,55:58]=np.log(scales)
    arr[:,58]=1.0
    header=("ply\nformat binary_little_endian 1.0\n"
        +f"element vertex {len(points)}\n"
        +"".join("property float "+field+"\n" for field in names)+"end_header\n")
    with open(path,'wb') as fp:
        fp.write(header.encode('ascii'));fp.write(arr.tobytes())
    return arr

def export_splat(path,arr):
    # 32 byte legacy splat: XYZ f32, scale f32, RGB and alpha u8, quaternion u8.
    pos=np.ascontiguousarray(arr[:,:3]).astype('<f4')
    scl=np.ascontiguousarray(np.exp(arr[:,55:58])).astype('<f4')
    rgb=np.uint8(np.clip(np.rint((arr[:,6:9]*SH0+.5)*255),0,255))
    alp=np.uint8(np.round(255/(1+np.exp(-arr[:,54]))))
    rot=np.uint8(np.round(np.clip((arr[:,58:62]*.5+.5)*255,0,255)))
    buf=np.zeros((len(pos),32),np.uint8)
    buf[:,:12]=pos.view(np.uint8).reshape(-1,12)
    buf[:,12:24]=scl.view(np.uint8).reshape(-1,12)
    buf[:,24:27]=rgb
    buf[:,27]=alp
    buf[:,28:]=rot
    path.write_bytes(buf.tobytes())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--video',type=Path,required=True)
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--focal',type=float,default=550)
    p.add_argument('--stride',type=int,default=2)
    a=p.parse_args()
    cfg=json.loads(a.config.read_text(encoding='utf-8'))
    sec=cfg['frame_seconds']
    if not len(sec)==3:raise ValueError('Need three capture times')
    cap=cv2.VideoCapture(str(a.video))
    if not cap.isOpened():raise RuntimeError("Video unavailable")
    try: ims=[frame(cap,s) for s in sec]
    finally: cap.release()
    # Match the JSON's canonicalized float timestamp keys.
    polys=[cfg['roi_polygons_px'][str(float(s))] for s in sec]
    xyz,rgb,sc,alp,K,metrics=reconstruct(ims,polys,a.focal,a.stride)
    a.out.mkdir(parents=True,exist_ok=True)
    payload=export_ply(a.out/'energetik_gaussians_nonmetric.ply',xyz,rgb,sc,alp)
    export_splat(a.out/'energetik_gaussians_nonmetric.splat',payload)
    (a.out/'validation.json').write_text(json.dumps(metrics,indent=2)+'\n')
    print(json.dumps(metrics,indent=2))

if __name__=='__main__':
    main()
