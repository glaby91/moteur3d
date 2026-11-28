function simulate(){
 let seed=0x5e5ca601;const rand=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296;};
 let frames=0,hits=0,misses=0,mismatch=0;
 // Model all 18 scalar camera/light keys + 9 other scene/option fields.
 for(let s=0;s<1000;s++){
  let key=Array.from({length:27},()=>Math.floor(rand()*1000000)),cache=null;
  for(let i=0;i<32;i++){
   let next=key.slice();const mutate=i%3!==0;
   if(mutate){const field=Math.floor(rand()*27);next[field]++;}
   const reused=cache!==null&&next.every((v,j)=>v===cache[j]);
   const expected=cache!==null&&!mutate&&key.every((v,j)=>v===cache[j]);
   if(reused!==expected)mismatch++;
   if(reused)hits++;else misses++;
   cache=next;key=next;frames++;
  }
 }
 // Native kernel intersection math model: two-sided Möller–Trumbore,
 // deterministic nearest t, tie lowest original primitive ID.
 const sub=(a,b)=>a.map((v,i)=>v-b[i]),dot=(a,b)=>a.reduce((s,v,i)=>s+v*b[i],0),cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
 function intersect(o,d,a,b,c,low){const e=sub(b,a),f=sub(c,a),p=cross(d,f),det=dot(e,p);if(Math.abs(det)<1e-10)return null;const q=sub(o,a),u=dot(q,p)/det;if(u<0||u>1)return null;const z=cross(q,e),v=dot(d,z)/det;if(v<0||u+v>1)return null;const t=dot(f,z)/det;return t>low?[t,u,v]:null}
 let triangleCases=0,triangleErrors=0;
 for(let j=0;j<30000;j++){const z=.5+rand()*100,x=(rand()-.5)*20,y=(rand()-.5)*20;let u=rand(),v=rand();if(u+v>1){u=1-u;v=1-v}const a=[x,y,z],b=[x+2,y,z],c=[x,y+3,z],o=[x+2*u,y+3*v,0],d=[0,0,1];const low=j%5===0?z+.001:0;
  const q=intersect(o,d,a,b,c,low),expected=z>low;if((q!==null)!==expected||q&&Math.max(Math.abs(q[0]-z),Math.abs(q[1]-u),Math.abs(q[2]-v))>1e-10)triangleErrors++;triangleCases++;}
 if(mismatch||triangleErrors)throw Error("preflight failed");
 return {frames,hits,misses,mismatch,triangleCases,triangleErrors};
}
console.log(JSON.stringify(simulate(),null,2));
