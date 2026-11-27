function simulate(){
 let seed=78134;const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
 let cases=0,all=0,kept=0,exactZero=0,affineMax=0;
 for(let t=0;t<500;t++){
  const w=8+Math.floor(rnd()*120),h=8+Math.floor(rnd()*120),tw=Math.min(w,32),th=Math.min(h,32),x0=Math.floor(rnd()*(w-tw+1)),y0=Math.floor(rnd()*(h-th+1)),x1=x0+tw-1,y1=y0+th-1;
  const samples=Array.from({length:4+Math.floor(rnd()*160)},(_,id)=>({x:Math.floor(rnd()*w),y:Math.floor(rnd()*h),id,hit:rnd()>.3})); const complete=samples.filter(()=>rnd()>.2),partial=samples.filter(p=>!complete.includes(p)&&p.hit);
  function shortlist(a){if(a.length<=4)return a;const max=p=>Math.max(Math.abs(p.x-x0),Math.abs(p.x-x1))**2+Math.max(Math.abs(p.y-y0),Math.abs(p.y-y1))**2,min=p=>Math.max(0,x0-p.x,p.x-x1)**2+Math.max(0,y0-p.y,p.y-y1)**2;const lim=a.map(max).sort((a,b)=>a-b)[3];return a.filter(p=>min(p)<=lim);}
  const shared=shortlist(complete),sparse=shortlist([...shared,...partial]);
  for(let j=0;j<50;j++){
   const x=x0+Math.floor(rnd()*tw),y=y0+Math.floor(rnd()*th),dist=p=>(p.x-x)**2+(p.y-y)**2,top=a=>a.slice().sort((a,b)=>dist(a)-dist(b)||a.id-b.id).slice(0,4);
   for(const [a,b] of [[complete,shared],[[...complete,...partial],sparse]]){const aa=top(a),bb=top(b);if(aa.map(p=>p.id).join(",")!==bb.map(p=>p.id).join(","))throw Error("top4 parity");if(aa[0]&&dist(aa[0])===0&&!aa[0].hit)exactZero++;all+=a.length;kept+=b.length;cases++;}
  }
 }
 // Analytic plane mirror: ray from camera (0,0,2) to receiver (x,y,0)
 // then to source plane z=4. UV source coordinates are affine in x,y.
 let n=0,sumMean=0,sumPlane=0;
 const known=[];for(let y=0;y<=16;y+=4)for(let x=0;x<=16;x+=4)known.push({x,y,u:x/16,v:y/16});
 for(let y=0;y<=16;y++)for(let x=0;x<=16;x++){
  const nn=known.slice().sort((a,b)=>(a.x-x)**2+(a.y-y)**2-((b.x-x)**2+(b.y-y)**2)||a.y-b.y||a.x-b.x).slice(0,4);
  const exact=nn[0].x===x&&nn[0].y===y;
  const u=exact?nn[0].u:nn.reduce((s,p)=>s+p.u,0)/4,v=exact?nn[0].v:nn.reduce((s,p)=>s+p.v,0)/4;
  sumMean+=Math.hypot(u-x/16,v-y/16);
  const px=(x/8-1),py=(y/8-1),t=2;const au=(px+px*t+3)/6,av=(py+py*t+3)/6;
  const marginU=Math.max((Math.max(...nn.map(p=>p.u))-Math.min(...nn.map(p=>p.u)))*.1,.002),marginV=Math.max((Math.max(...nn.map(p=>p.v))-Math.min(...nn.map(p=>p.v)))*.1,.002);
  const allowed=!exact&&au>=Math.min(...nn.map(p=>p.u))-marginU&&au<=Math.max(...nn.map(p=>p.u))+marginU&&av>=Math.min(...nn.map(p=>p.v))-marginV&&av<=Math.max(...nn.map(p=>p.v))+marginV;
  const err=Math.hypot((allowed?au:u)-x/16,(allowed?av:v)-y/16);sumPlane+=err;affineMax=Math.max(affineMax,err);n++;
 }
 return {cases,all,kept,exactZero,planar:{n,meanUvError:sumMean/n,continuousUvError:sumPlane/n,maxContinuousError:affineMax}};
}
console.log(JSON.stringify(simulate(),null,2));
