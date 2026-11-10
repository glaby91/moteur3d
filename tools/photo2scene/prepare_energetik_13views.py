#!/usr/bin/env python3
"""Energetik palace-only: prepare 3DGS camera dataset from continuous drone footage.

NO metric survey, no trained splats. Reject montage cuts instead of inventing poses.
Output is Nerfstudio transforms.json, PNG frames/masks, sparse relative XYZ and QA.
"""
import argparse, json, math
from pathlib import Path
import cv2
import numpy as np

cv2.setRNGSeed(20261110)
BASE_POLYS={
 373.0: [[[135,51],[233,37],[254,113],[169,123]],[[172,110],[322,91],[489,109],[507,160],[447,224],[173,166]],[[330,59],[513,62],[512,125],[325,120]],[[167,161],[507,160],[472,250],[174,201]]],
 375.5: [[[103,47],[215,32],[238,114],[129,119]],[[135,112],[323,99],[501,126],[495,187],[456,273],[132,174]],[[326,61],[546,64],[544,129],[320,124]],[[133,165],[495,180],[473,283],[136,208]]]
}

def polygons_for(t):
    a=(float(t)-373.)/2.5
    return [(np.asarray(p)*(1-a)+np.asarray(q)*a).round().astype(int)
            for p,q in zip(BASE_POLYS[373.0],BASE_POLYS[375.5])]

def mask_for(frame,t):
    mask=np.zeros(frame.shape[:2],np.uint8)
    for polygon in polygons_for(t):cv2.fillPoly(mask,[polygon],255)
    # This is a hand-guided projected building ROI, NOT AI semantic vegetation.
    return cv2.erode(mask,np.ones((3,3),np.uint8))

def frames_from_video(video,times):
    cap=cv2.VideoCapture(str(video))
    if not cap.isOpened():raise RuntimeError('Could not open video '+str(video))
    frames=[]
    try:
        for t in times:
            cap.set(cv2.CAP_PROP_POS_MSEC,t*1000)
            ok,frame=cap.read()
            if not ok:raise RuntimeError('Missing frame '+str(t))
            frames.append(frame)
    finally:cap.release()
    if len(set(f.shape for f in frames))!=1:raise RuntimeError('Source resolution changed')
    return frames

def detect(frame,mask):
    return cv2.SIFT_create(nfeatures=8000,contrastThreshold=.012,edgeThreshold=13).detectAndCompute(
        cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY),mask)

def matches(fa,fb):
    if fa[1] is None or fb[1] is None:return []
    raw=cv2.BFMatcher(cv2.NORM_L2).knnMatch(fa[1],fb[1],k=2)
    return [m for m,n in raw if m.distance<.76*n.distance]

def solve_poses(times,frames,feat,first,last,focal):
    w,h=frames[0].shape[1],frames[0].shape[0]
    K=np.array([[focal,0,w/2],[0,focal,h/2],[0,0,1.]],np.float64)
    common=matches(feat[first],feat[last])
    if len(common)<60:raise RuntimeError('Insufficient stable baseline matches')
    a=np.float64([feat[first][0][m.queryIdx].pt for m in common])
    b=np.float64([feat[last][0][m.trainIdx].pt for m in common])
    E,e_mask=cv2.findEssentialMat(a,b,K,cv2.RANSAC,.999,1.5,maxIters=12000)
    if E is None:raise RuntimeError('Essential matrix failed')
    _,R,t,pose_mask=cv2.recoverPose(E[:3,:3],a,b,K,mask=e_mask)
    keep=pose_mask.ravel().astype(bool)
    p0,p1=a[keep],b[keep]
    accepted=[m for m,good in zip(common,keep) if good]
    hxyz=cv2.triangulatePoints(K@np.c_[np.eye(3),np.zeros((3,1))],K@np.c_[R,t],p0.T,p1.T)
    xyz=(hxyz[:3]/hxyz[3:]).T
    x2=xyz@R.T+t.T
    r0=cv2.undistortPoints(p0.astype(np.float32).reshape(-1,1,2),K,None).reshape(-1,2)
    r1=cv2.undistortPoints(p1.astype(np.float32).reshape(-1,1,2),K,None).reshape(-1,2)
    rays0=np.c_[r0,np.ones(len(r0))];rays1=np.c_[r1,np.ones(len(r1))]@R
    rays0/=np.linalg.norm(rays0,axis=1,keepdims=True)
    rays1/=np.linalg.norm(rays1,axis=1,keepdims=True)
    par=np.degrees(np.arccos(np.clip(np.sum(rays0*rays1,axis=1),-1,1)))
    good=(xyz[:,2]>0)&(x2[:,2]>0)&np.isfinite(xyz).all(axis=1)&(par>.85)&(par<40)
    sparse={m.queryIdx:xyz[i] for i,m in enumerate(accepted) if good[i]}
    poses={}
    for i,timestamp in enumerate(times):
        if i==first:
            poses[i]=(np.eye(3),np.zeros((3,1)),{'status':'anchor-origin','n':int(good.sum())})
            continue
        if i==last:
            poses[i]=(R,t,{'status':'anchor-end','n':int(good.sum())})
            continue
        pair=matches(feat[first],feat[i])
        linked=[(np.float64(sparse[m.queryIdx]),feat[i][0][m.trainIdx].pt)
                for m in pair if m.queryIdx in sparse]
        if len(linked)<12:
            poses[i]=(None,None,{'status':'insufficient-correspondences','n':len(linked)})
            continue
        X=np.float64([v[0] for v in linked]);uv=np.float64([v[1] for v in linked])
        order=np.random.default_rng(20261110+i).permutation(len(linked))
        n=int(len(order)*.8);train=order[:n];holdout=order[n:]
        ok,rv,tv,inliers=cv2.solvePnPRansac(X[train],uv[train],K,None,
            flags=cv2.SOLVEPNP_EPNP,reprojectionError=3.,iterationsCount=1200,confidence=.999)
        if not ok or inliers is None or len(inliers)<10:
            poses[i]=(None,None,{'status':'pnp-failed','n':len(linked)})
            continue
        selected=train[inliers.ravel()]
        try:rv,tv=cv2.solvePnPRefineLM(X[selected],uv[selected],rv,tv)
        except cv2.error:pass
        projected=cv2.projectPoints(X[holdout],rv,tv,K,None)[0].reshape(-1,2)
        error=np.linalg.norm(uv[holdout]-projected,axis=1)
        poses[i]=(cv2.Rodrigues(rv)[0],tv,{
            'status':'pnp','n':len(linked),'inliers':len(selected),
            'heldout_n':len(holdout),'heldout_median_px':float(np.median(error)),
            'heldout_rms_px_all':float(np.sqrt(np.mean(error**2))),
            'heldout_under_3px':int(np.sum(error<3))})
    stats={'matched_baseline':len(common),'essential_inliers':int(keep.sum()),
           'triangulated_positive_with_parallax':int(good.sum()),
           'median_parallax_deg':float(np.median(par[good])) if good.any() else None}
    return K,poses,xyz[good],p0[good],stats

def pose_to_opengl(R,t):
    w2c=np.eye(4);w2c[:3,:3]=R;w2c[:3,3]=t.ravel()
    c2w=np.linalg.inv(w2c)
    c2w[:3,:3]=c2w[:3,:3]@np.diag([1.,-1.,-1.])
    return c2w.tolist()

def run(video,out,focal):
    times=[372.+i*.25 for i in range(17)]
    frames=frames_from_video(video,times)
    masks=[mask_for(f,t) for f,t in zip(frames,times)]
    feats=[detect(f,m) for f,m in zip(frames,masks)]
    K,poses,xyz,pix,baseline=solve_poses(times,frames,feats,times.index(373.),times.index(375.5),focal)
    out.mkdir(parents=True,exist_ok=True)
    imdir=out/'images';mdir=out/'masks';imdir.mkdir(exist_ok=True);mdir.mkdir(exist_ok=True)
    train=[];val=[];meta=[]
    for i,(t,f,mask) in enumerate(zip(times,frames,masks)):
        cv2.imwrite(str(imdir/f'frame_{i:02d}.png'),f)
        cv2.imwrite(str(mdir/f'frame_{i:02d}.png'),mask)
        R,tr,status=poses[i]
        meta.append({'time_s':t,**status})
        if R is None:continue
        entry={'file_path':f'images/frame_{i:02d}.png',
               'mask_path':f'masks/frame_{i:02d}.png',
               'transform_matrix':pose_to_opengl(R,tr),'time_s':t}
        (val if t in (372.75,374.,375.) else train).append(entry)
    if len(train)<9 or len(val)!=3:raise RuntimeError('Not enough coherent views')
    allframes=train+val
    transform={'camera_model':'OPENCV','fl_x':focal,'fl_y':focal,
        'cx':float(K[0,2]),'cy':float(K[1,2]),'w':frames[0].shape[1],'h':frames[0].shape[0],
        'k1':0,'k2':0,'p1':0,'p2':0,'frames':allframes,
        'train_filenames':[f['file_path'] for f in train],
        'val_filenames':[f['file_path'] for f in val],
        'test_filenames':[f['file_path'] for f in val]}
    (out/'transforms.json').write_text(json.dumps(transform,indent=2))
    np.savez_compressed(out/'baseline_sparse_nonmetric.npz',
                        xyz=xyz.astype(np.float32),pixels_anchor=pix.astype(np.float32))
    report={'frames_tested':len(times),'poses_accepted':len(allframes),
        'train_count':len(train),'eval_count':len(val),'baseline':baseline,
        'focal_assumed_px':focal,'metric_scale':None,'CUDA_training_executed':False,
        'user_footage_not_published':True,
        'masking':'rough ROI, not precise semantic vegetation',
        'per_frame':meta}
    (out/'metrics.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--video',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--focal',type=float,default=550.)
    args=parser.parse_args()
    run(args.video,args.output,args.focal)

if __name__=='__main__':main()
