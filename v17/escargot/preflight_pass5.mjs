function preflight(){
    function spiral(w,h,clip){let out=[],visits=0,cx=Math.floor((w-1)/2),cy=Math.floor((h-1)/2);function add(x,y){visits++;if(x>=0&&y>=0&&x<w&&y<h)out.push(y*w+x)}add(cx,cy);for(let r=1;out.length<w*h;r++){if(!clip||cy-r>=0)for(let x=clip?Math.max(-r+1,-cx):-r+1;x<=(clip?Math.min(r,w-1-cx):r);x++)add(cx+x,cy-r);if(!clip||cx+r<w)for(let y=clip?Math.max(-r+1,-cy):-r+1;y<=(clip?Math.min(r,h-1-cy):r);y++)add(cx+r,cy+y);if(!clip||cy+r<h)for(let x=clip?Math.min(r-1,w-1-cx):r-1;x>=(clip?Math.max(-r,-cx):-r);x--)add(cx+x,cy+r);if(!clip||cx-r>=0)for(let y=clip?Math.min(r-1,h-1-cy):r-1;y>=(clip?Math.max(-r,-cy):-r);y--)add(cx-r,cy+y)}return {out,visits}}
    let spiralCases=0;for(let w=1;w<=64;w++)for(let h=1;h<=64;h++){let a=spiral(w,h,false),b=spiral(w,h,true);if(a.out.length!==b.out.length||a.out.some((x,i)=>x!==b.out[i]))throw Error("spiral parity");spiralCases++}
    const thinOld=spiral(4096,1,false),thinNew=spiral(4096,1,true);if(thinOld.out.some((x,i)=>x!==thinNew.out[i]))throw Error("thin");
    let seed=7123,gridCases=0,oldChecks=0,newChecks=0,hybridChecks=0;function rnd(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296}
    for(let t=0;t<30000;t++){const w=1+Math.floor(rnd()*4096),h=1+Math.floor(rnd()*4096),target=1+Math.floor(rnd()*1024);const count=s=>Math.ceil(w/s)*Math.ceil(h/s);let a=Math.max(1,Math.ceil(Math.sqrt(w*h/target))),lo=a,hi=Math.max(w,h);while(true){oldChecks++;if(count(a)<=target)break;a++}while(lo<hi){let m=lo+Math.floor((hi-lo)/2);newChecks++;if(count(m)>target)lo=m+1;else hi=m}if(a!==lo)throw Error("grid parity");lo=Math.max(1,Math.ceil(Math.sqrt(w*h/target)));hi=Math.max(w,h);for(let k=0;k<8&&lo<hi;k++){hybridChecks++;if(count(lo)<=target){hi=lo;break}lo++}while(lo<hi){let m=lo+Math.floor((hi-lo)/2);hybridChecks++;if(count(m)>target)lo=m+1;else hi=m}if(a!==lo)throw Error("hybrid parity");gridCases++}
    // An actual planar optical mapping test: mirror z=0, camera (0,0,2),
    // source plane z=4. A ray through mirror (x,y,0) intersects source
    // at (3x,3y,4); UV normalized over source footprint [-3,3]^2.
    let n=0,mae=0,max=0,exact=0;const known=[];for(let y=0;y<=16;y+=4)for(let x=0;x<=16;x+=4)known.push([x,y]);
    for(let y=0;y<=16;y++)for(let x=0;x<=16;x++){
        const k=known.find(p=>p[0]===x&&p[1]===y);
        const uv=(a,b)=>[(a/8-1)*3/6+.5,(b/8-1)*3/6+.5];
        const truth=uv(x,y);
        const neighbors=known.map((p,i)=>({p,i,d:(p[0]-x)**2+(p[1]-y)**2})).sort((a,b)=>a.d-b.d||a.i-b.i).slice(0,4);
        const pred=k?uv(...k):neighbors.reduce((a,b)=>{let u=uv(...b.p);return [a[0]+u[0]/4,a[1]+u[1]/4]},[0,0]);
        const e=Math.hypot(pred[0]-truth[0],pred[1]-truth[1]);mae+=e;max=Math.max(max,e);n++;if(k){exact++;if(e>1e-12)throw Error("measured invariant")}
    }
    return {spiralCases,thin:{oldVisits:thinOld.visits,newVisits:thinNew.visits},gridCases,oldChecks,newChecks,hybridChecks,planar:{pixels:n,measured:exact,meanUvError:mae/n,maxUvError:max}};
}
console.log(JSON.stringify(preflight(),null,2));
