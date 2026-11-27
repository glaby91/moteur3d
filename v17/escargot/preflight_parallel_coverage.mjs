function simulate(){
 let seed=791029;const rand=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
 const F=Math.fround,eps=F(4*2**-23);let slabCases=0,acceptedOld=0,acceptedNew=0,falseNegative=0,extra=0;
 function box(o,d,lo,hi,low,high,fast){let min=F(low),max=F(high);for(let k=0;k<3;k++){if(Math.abs(d[k])<1e-12){if(o[k]<lo[k]||o[k]>hi[k])return false;continue;}let a=fast?F(F(lo[k]-o[k])*F(1/d[k])):F(F(lo[k]-o[k])/d[k]),b=fast?F(F(hi[k]-o[k])*F(1/d[k])):F(F(hi[k]-o[k])/d[k]);if(a>b)[a,b]=[b,a];if(fast){a=F(a-F(eps*Math.max(1,Math.abs(a))));b=F(b+F(eps*Math.max(1,Math.abs(b))))}min=Math.max(min,a);max=Math.min(max,b);if(max<min)return false;}return true}
 for(let j=0;j<100000;j++){
  let o=Array.from({length:3},()=>F((rand()-.5)*200)),d=Array.from({length:3},()=>F((rand()-.5)*2));if(j%17===0)d[j%3]=0;
  const t=F(rand()*100+1),p=o.map((v,k)=>F(v+F(t*d[k]))),lo=p.map(v=>F(v-rand()*4)),hi=p.map(v=>F(v+rand()*4));
  if(j%3===0)hi[j%3]=lo[j%3];if(j%7===0)o[0]=F(o[0]+100);
  const a=box(o,d,lo,hi,.001,200,false),b=box(o,d,lo,hi,.001,200,true);slabCases++;acceptedOld+=a;acceptedNew+=b;if(a&&!b)falseNegative++;if(b&&!a)extra++;
 }
 if(falseNegative)throw Error("slab false negative");
 // Independent model of coverage weights. UV/ID values are untouched.
 function weights(d){if(d.length===1||d[0]===0)return d.map((_,i)=>i===0?1:0);let far=d[d.length-1],radius=far+Math.max(1,.125*far),w=d.map(x=>Math.max(0,radius-x)/(1+x)),s=w.reduce((a,b)=>a+b,0);return w.map(x=>x/s)}
 let cases=0,exactZeros=0,maxNormError=0;
 for(let i=0;i<50000;i++){const n=1+Math.floor(rand()*4),d=Array.from({length:n},()=>Math.floor(rand()*8192)).sort((a,b)=>a-b);if(i%5===0)d[0]=0;const mask=Array.from({length:n},()=>rand()>.5?1:0),w=weights(d),coverage=w.reduce((s,v,j)=>s+v*mask[j],0);if(coverage< -1e-12||coverage>1+1e-12)throw Error("coverage");if(d[0]===0){if(coverage!==mask[0])throw Error("exact");if(!mask[0])exactZeros++;}maxNormError=Math.max(maxNormError,Math.abs(w.reduce((s,v)=>s+v,0)-1));cases++}
 // One straight edge on a lattice, compare adjacent pixel coverage jumps;
 // not a rendered image or a general proof of smoothing.
 const pts=[];for(let y=0;y<=96;y+=12)for(let x=0;x<=96;x+=12)pts.push({x,y,id:pts.length,hit:x>=48?1:0});
 const old=[],now=[];for(let y=0;y<=96;y++)for(let x=0;x<=96;x++){
  const near=pts.map(p=>({...p,d:(p.x-x)**2+(p.y-y)**2})).sort((a,b)=>a.d-b.d||a.id-b.id).slice(0,4);
  const exact=near[0].d===0,ns=exact?near.slice(0,1):near;
  old.push(ns.reduce((a,p)=>a+p.hit,0)/ns.length);now.push(weights(ns.map(p=>p.d)).reduce((s,v,j)=>s+v*ns[j].hit,0));
 }
 function jumps(a){let max=0,sum=0,n=0,large=0;for(let y=0;y<=96;y++)for(let x=1;x<=96;x++){let j=Math.abs(a[y*97+x]-a[y*97+x-1]);max=Math.max(max,j);sum+=j;n++;if(j>.2)large++}return {max,mean:sum/n,large}}
 return {slabCases,acceptedOld,acceptedNew,falseNegative,extra,cases,exactZeros,maxNormError,edge:{before:jumps(old),after:jumps(now)}};
}
console.log(JSON.stringify(simulate(),null,2));
