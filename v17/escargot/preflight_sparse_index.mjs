function simulate(){
let seed=319831;const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296;};
let cases=0,oldTests=0,newTests=0,errors=0;
for(let t=0;t<500;t++){
 const w=16+Math.floor(rnd()*112),h=16+Math.floor(rnd()*112),step=1+Math.floor(rnd()*5),points=[];
 for(let y=0;y<h;y+=step)for(let x=0;x<w;x+=step)if(rnd()<.10)points.push({x:x+Math.floor(Math.min(step,w-x)/2),y:y+Math.floor(Math.min(step,h-y)/2),id:points.length});
 if(points.length<16)continue;
 function build(p,axis){if(!p.length)return null;const k=axis?"y":"x";p.sort((a,b)=>a[k]-b[k]||a.id-b.id);const m=p.length>>1,a=p[m],l=build(p.slice(0,m),1-axis),r=build(p.slice(m+1),1-axis);
 return {a,l,r,minX:Math.min(a.x,l?.minX??a.x,r?.minX??a.x),maxX:Math.max(a.x,l?.maxX??a.x,r?.maxX??a.x),minY:Math.min(a.y,l?.minY??a.y,r?.minY??a.y),maxY:Math.max(a.y,l?.maxY??a.y,r?.maxY??a.y)};}
 const root=build([...points],0);
 for(let j=0;j<50;j++){
  const x=Math.floor(rnd()*w),y=Math.floor(rnd()*h),dist=p=>(p.x-x)**2+(p.y-y)**2,cmp=(a,b)=>a.d-b.d||a.id-b.id;
  const expected=points.map(p=>({id:p.id,d:dist(p)})).sort(cmp).slice(0,4);oldTests+=points.length;
  const best=[];const lower=n=>n?Math.max(0,n.minX-x,x-n.maxX)**2+Math.max(0,n.minY-y,y-n.maxY)**2:Infinity;
  function visit(n){if(!n||best.length===4&&lower(n)>best[3].d)return;newTests++;best.push({id:n.a.id,d:dist(n.a)});best.sort(cmp);if(best.length>4)best.pop();let l=n.l,r=n.r;if(lower(r)<lower(l))[l,r]=[r,l];visit(l);visit(r);}
  visit(root);if(JSON.stringify(best)!==JSON.stringify(expected))errors++;cases++;
 }
}
if(errors)throw Error("nearest parity");
return {cases,oldTests,newTests,errors};
}
console.log(JSON.stringify(simulate(),null,2));
