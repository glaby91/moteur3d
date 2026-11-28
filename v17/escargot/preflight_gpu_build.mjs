function preflight(){
 let seed=0x7fae3d2;const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
 let tiles=0,queries=0,known=0,kept=0,exactZeros=0;
 for(let t=0;t<5000;t++){
  const width=32+Math.floor(rnd()*280),height=32+Math.floor(rnd()*200),ts=[8,16,32][t%3],x=Math.floor(rnd()*(width-ts+1)),y=Math.floor(rnd()*(height-ts+1)),r=[x,y,x+ts-1,y+ts-1];
  const pts=Array.from({length:1+Math.floor(rnd()*160)},(_,id)=>({id,x:Math.floor(rnd()*width),y:Math.floor(rnd()*height),positive:rnd()>.4}));
  const complete=pts.filter(()=>rnd()>.25),partial=pts.filter(p=>!complete.includes(p)&&p.positive);
  const min=p=>Math.max(0,r[0]-p.x,p.x-r[2])**2+Math.max(0,r[1]-p.y,p.y-r[3])**2;
  const max=p=>Math.max(Math.abs(r[0]-p.x),Math.abs(r[2]-p.x))**2+Math.max(Math.abs(r[1]-p.y),Math.abs(r[3]-p.y))**2;
  const short=a=>a.length<=4?a:a.filter(p=>min(p)<=a.map(max).sort((a,b)=>a-b)[3]);
  function gpuShort(a){
    if(a.length<=4)return a;
    let best=[Infinity,Infinity,Infinity,Infinity];
    for(const p of a){let v=max(p);if(v<best[0])best=[v,...best.slice(0,3)];else if(v<best[1])best=[best[0],v,best[1],best[2]];else if(v<best[2])best=[best[0],best[1],v,best[2]];else if(v<best[3])best[3]=v}
    return a.filter(p=>min(p)<=best[3]);
  }
  const old=short([...short(complete),...partial]),now=gpuShort([...complete,...partial]);
  if(old.map(p=>p.id).join()!=now.map(p=>p.id).join())throw Error("tile shortlist record mismatch");
  tiles++;known+=complete.length+partial.length;kept+=now.length;
  for(let j=0;j<10;j++){
   const px=x+Math.floor(rnd()*ts),py=y+Math.floor(rnd()*ts),d=p=>(p.x-px)**2+(p.y-py)**2,less=(a,b)=>d(a)<d(b)||(d(a)===d(b)&&a.id<b.id);
   const truth=[...complete,...partial].sort((a,b)=>d(a)-d(b)||a.id-b.id).slice(0,4),best=[];
   for(const p of now){if(best.length===4&&!less(p,best[3]))continue;let pos=best.findIndex(q=>less(p,q));if(pos<0)pos=best.length;best.splice(pos,0,p);if(best.length>4)best.pop()}
   if(truth.map(p=>p.id).join()!=best.map(p=>p.id).join())throw Error("stencil mismatch");
   if(best.length&&d(best[0])===0&&!best[0].positive)exactZeros++;
   queries++;
  }
 }
 return {tiles,queries,known,kept,exactZeros,errors:0};
}
console.log(JSON.stringify(preflight(),null,2));
