// Standalone numerical experiment: geometry and visibility are independent.
import assert from 'node:assert/strict';
import fs from 'node:fs';
const W=320,H=180, dot=(a,b)=>a.reduce((s,v,i)=>s+v*b[i],0),sub=(a,b)=>a.map((v,i)=>v-b[i]),add=(a,b)=>a.map((v,i)=>v+b[i]),mul=(a,s)=>a.map(v=>v*s);
function projection(x,y,eye){
  const p=[(x/W-.5)*10,0,2+y/H*9];
  const d=sub(p,eye),r=[d[0],-d[1],d[2]];
  if(r[2]<=1e-8)return null;
  const t=(14-p[2])/r[2],q=add(p,mul(r,t));
  if(t<=0||q[0]<-5||q[0]>5||q[1]<0||q[1]>6)return null;
  return {u:(q[0]+5)/10,v:q[1]/6,q,p};
}
function visible(q,thin){
  const d=sub(q.q,q.p),t=(12-q.p[2])/d[2],p=add(q.p,mul(d,t));
  return !(p[1]<4&&p[1]>.4&&p[0]>(thin?.12:-.65)&&p[0]<(thin?.19:.65));
}
function adapt(eye,thin,limit){
  const cache=new Map(),mask=new Float64Array(W*H);let rays=0,unknown=0;
  function sample(x,y){
    const k=y*W+x;if(cache.has(k))return cache.get(k);
    const q=projection(x,y,eye);let v=-1;
    if(q){if(rays>=limit)return -2;rays++;v=+visible(q,thin);}
    cache.set(k,v);return v;
  }
  function visit(x0,y0,x1,y1){
    const xm=(x0+x1)>>1,ym=(y0+y1)>>1;
    const v=[sample(x0,y0),sample(x1,y0),sample(x0,y1),sample(x1,y1),sample(xm,ym),sample(xm,y0),sample(xm,y1),sample(x0,ym),sample(x1,ym)];
    const diff=v.some(x=>x!==v[0]),span=Math.max(x1-x0,y1-y0);
    if(diff&&span>2&&rays<limit){
      for(const [a,b] of x0===x1?[[x0,x1]]:[[x0,xm],[xm+1,x1]])for(const [c,d] of y0===y1?[[y0,y1]]:[[y0,ym],[ym+1,y1]])visit(a,c,b,d);
      return;
    }
    for(let y=y0;y<=y1;y++)for(let x=x0;x<=x1;x++){
      if(!projection(x,y,eye))continue;
      const u=x1===x0?0:(x-x0)/(x1-x0),w=y1===y0?0:(y-y0)/(y1-y0);
      if(v.some(a=>a<0)){const s=sample(x,y);if(s<0){unknown++;mask[y*W+x]=-1;}else mask[y*W+x]=s;}
      else mask[y*W+x]=(1-u)*(1-w)*v[0]+u*(1-w)*v[1]+(1-u)*w*v[2]+u*w*v[3];
    }
  }
  for(let y=0;y<H;y+=16)for(let x=0;x<W;x+=16)visit(x,y,Math.min(W-1,x+15),Math.min(H-1,y+15));
  let total=0,fp=0,fn=0,uvError=0;
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){const q=projection(x,y,eye);if(!q)continue;total++;
    const truth=visible(q,thin),m=mask[y*W+x];if(m>=.5&&!truth)fp++;if(m>=0&&m<.5&&truth)fn++;
    // Closed form ray-plane intersection vs independently mirrored eye ray.
    const mirror=[eye[0],-eye[1],eye[2]],dir=sub(q.p,mirror),t=(14-mirror[2])/dir[2],s=add(mirror,mul(dir,t));
    uvError=Math.max(uvError,Math.abs((s[0]+5)/10-q.u),Math.abs(s[1]/6-q.v));
  }
  assert(uvError<1e-10);
  return {eye,thin,limit,rays,total,fp,fn,unknown,uvError,knownErrorPercent:100*(fp+fn)/total};
}
const cases=[];for(const eye of [[0,2,0],[.075,2,0],[.5,2,0]])for(const thin of [false,true])for(const budget of [256,1024,4096])cases.push(adapt(eye,thin,budget));
const report={note:'Synthetic geometry, not measured renderer FPS. Unknown is not zero. Thin occluder can be missed.',cases};
console.log(JSON.stringify(report,null,2));if(process.argv[2])fs.writeFileSync(process.argv[2],JSON.stringify(report,null,2));
