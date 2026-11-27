# Synthetic benchmark, not execution of WARP shaders.
import numpy as np, json, math
from pathlib import Path
P=Path(__file__).resolve().parent
families=json.loads((P/'zmaterial8192.json').read_text())['families']
rows=[dict(metalness=f['metallic'],roughness=r,base_linear=c,category='special' if f['transmission'] or f['emission'] else 'opaque') for f in families.values() for c in f['colors'] for r in f['roughness']]
mats=[m for m in rows if m['category']=='opaque']
contexts=[('jour',.55,1.,1.,.02,.65,.18,.08,.10),('couvert',.43,.65,.30,.04,.25,.10,.12,.06),('interieur',.25,.55,.22,.18,.40,-.26,.16,.13),('ombre',.10,.30,.05,.05,.22,.25,-.15,.09),('nuit_lampes',.085,0.,0.,.32,.18,-.36,.18,.12),('nuit_ext',.055,0.,0.,.08,.55,.15,-.05,.06)]
K=256;k=np.arange(K);z=(k+.5)/K;phi=k*math.pi*(3-math.sqrt(5));rr=np.sqrt(1-z*z);N0=np.stack([rr*np.cos(phi),rr*np.sin(phi),z],axis=1);views=np.array([[0.,0.,1.],[0.,.6,.8],[.8,0.,.6]])
N=np.concatenate([N0@np.array([[v[2],0.,v[0]],[0.,1.,0.],[-v[0],0.,v[2]]]).T if v[1]==0 else N0@np.array([[1.,0.,0.],[0.,v[2],v[1]],[0.,-v[1],v[2]]]).T for v in views]);V=np.repeat(views,K,axis=0);R=2*np.sum(N*V,axis=1)[:,None]*N-V
light=np.array([.28,.87,.40]);light/=np.linalg.norm(light);lamp=np.array([-.6,.6,.53]);lamp/=np.linalg.norm(lamp);nl=np.maximum(0,N@light);nlocal=np.maximum(0,N@lamp)
m=np.array([x['metalness'] for x in mats])[:,None];r=np.array([x['roughness'] for x in mats])[:,None];lum=np.array([x['base_linear'] for x in mats])@np.array([.2126,.7152,.0722]);lum=lum[:,None];p=np.maximum(0,2/r**2-2);a=(p+1)/(p+2);b=(p+1)/(p+3);y=R[:,1][None,:];second=b*y*y+(1-b)/2*(1-y*y)-1/3
methods=['actuel','sans_diffus','uniforme','directionnel'];mask=m[:,0]>0;summary={};tot=np.zeros(4);max_nonmetal=0.;count=0
for name,amb,day,vis,local,cy,cx,cz,c2 in contexts:
 diff=amb*(.86+.24*(N[:,1]*.5+.5))+.56*day*vis*nl+local*nlocal;env=amb*(1+a*(R@np.array([cx,cy,cz]))+c2*second);ref=lum*((1-m)*diff+m*env);base=lum*diff
 pred=[base,lum*(1-m)*diff,lum*((1-m)*diff+m*amb),lum*((1-m)*diff+m*amb*(1+.55*(1-r*r)*y))]
 errors=np.array([np.abs(v[mask]-ref[mask]).mean()*100 for v in pred]);tot+=errors;summary[name]=dict(zip(methods,errors.tolist()));max_nonmetal=max(max_nonmetal,max(float(np.max(np.abs(v[~mask]-base[~mask]))) for v in pred));count+=ref.size
out={'catalogue':len(rows),'opaque_benchmarked':len(mats),'special_not_benchmarked':len(rows)-len(mats),'orientations':len(N),'contexts':len(contexts),'cases':count,'metal_cases':int(mask.sum())*len(N)*len(contexts),'metal_mae_linear_points':dict(zip(methods,(tot/len(contexts)).tolist())),'nonmetal_max_change':max_nonmetal,'contexts_results':summary,'reference':'synthetic cosine-power directional environment, exact analytic moments; not real shader execution'}
(P/'benchmark_resultats.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
