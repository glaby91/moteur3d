function simulation(){
  let seed=0x0e5ca601;const rnd=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
  const avg=(a)=>a.reduce((s,x)=>s+x,0)/a.length;
  const blank=()=>[0,0,0,0,0,0]; // premultiplied RGB, alpha, receiver weight, depth
  const top=(s,x,y,pred)=>s.map((v,i)=>({id:i,d:(v.x-x)**2+(v.y-y)**2,v})).filter(q=>pred(q.v)).sort((a,b)=>a.d-b.d||a.id-b.id).slice(0,4);
  const key=n=>`${n[0]?.d===0?1:0}:${n.map(v=>v.id).join(",")}`;
  const inside=(r,x,y)=>x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;
  const newStats=()=>({searches:0,composed:0,reused:0,empty:0,layerVisits:0,textureSamples:0});
  function frame(w,h,step,kind,layersN){
    const s=[],layers=[];for(let j=0;j<layersN;j++)layers.push({id:j,opacity:j%3===0?1:(.2+.6*rnd()),hits:new Map()});
    for(let y=0;y<h;y+=step)for(let x=0;x<w;x+=step){
      const a=x+Math.floor(Math.min(step,w-x)/2),b=y+Math.floor(Math.min(step,h-y)/2);
      const complete=kind!=="partial"||rnd()>.24,receiver=kind==="zero"&&rnd()<.25?0:(!complete&&rnd()<.25?-1:1);
      const i=s.length;s.push({x:a,y:b,complete:receiver===0||complete,receiver,weight:.2+.7*rnd(),depth:1+3*rnd()});
      if(receiver!==1)continue;
      for(const l of layers){
        const yes=kind==="sparse"?a<(w*.2+step)&&b<h*.25&&rnd()<.5:kind==="zero"?rnd()<.45:rnd()<.65;
        if(yes)l.hits.set(i,{u:rnd(),v:rnd(),depth:kind==="ties"?2:1+8*rnd(),opacity:l.opacity});
      }
    }
    for(const l of layers){
      const pts=[...l.hits.keys()].map(i=>s[i]);if(!pts.length)continue;
      const x=Math.max(0,Math.min(...pts.map(p=>p.x))-step),y=Math.max(0,Math.min(...pts.map(p=>p.y))-step);
      const right=Math.min(w,Math.max(...pts.map(p=>p.x))+1+step),bottom=Math.min(h,Math.max(...pts.map(p=>p.y))+1+step);
      l.r={x,y,w:right-x,h:bottom-y};
    }
    return {s,layers:layers.filter(l=>l.r),w,h};
  }
  function run(f,opt){
    const {s,layers,w,h}=f,stats=newStats(),pixels=[];
    const same=s.every(v=>v.complete===(v.receiver!==-1)),noPartial=layers.every(l=>[...l.hits.keys()].every(i=>s[i].complete));
    const can=same&&noPartial;
    function evaluate(x,y,active,n){
      stats.composed++;
      const nr=same?n:top(s,x,y,v=>v.receiver!==-1);
      if(!nr.length)return blank();
      const r=nr[0].d===0?nr.slice(0,1):nr.filter(v=>v.v.receiver===1);
      if(!r.length||r[0].v.receiver!==1)return blank();
      const rw=avg(r.map(v=>v.v.weight)),rd=avg(r.map(v=>v.v.depth));
      const coverage=can&&n.length>0,lanes=coverage?(n[0].d===0?1:n.length):1,values=[];
      for(const l of active){
        stats.layerVisits++;
        const ns=noPartial?n:top(s,x,y,v=>v.complete||l.hits.has(s.indexOf(v)));
        if(!ns.length)continue;
        const exact=ns[0].d===0,selected=exact?ns.slice(0,1):ns;
        const hit=selected.filter(q=>l.hits.has(q.id));
        if(!hit.length)continue;
        const a=hit.map(q=>l.hits.get(q.id));
        const mask=coverage?hit.reduce((m,q)=>m|(1<<selected.findIndex(z=>z.id===q.id)),0):1;
        values.push({id:l.id,mask,depth:avg(a.map(v=>v.depth)),u:avg(a.map(v=>v.u)),v:avg(a.map(v=>v.v)),alpha:avg(a.map(v=>v.opacity))*(coverage?1:hit.length/selected.length)});
      }
      values.sort((a,b)=>b.depth-a.depth||a.id-b.id);
      const acc=Array.from({length:lanes},()=>[0,0,0,0]);
      for(const v of values){
        stats.textureSamples++;
        const rgb=[v.u,v.v,.5+.5*Math.cos(v.u*6+v.v*4+v.id)];
        for(let k=0;k<lanes;k++)if(v.mask&(1<<k)){
          for(let z=0;z<3;z++)acc[k][z]=rgb[z]*v.alpha+acc[k][z]*(1-v.alpha);
          acc[k][3]=v.alpha+acc[k][3]*(1-v.alpha);
        }
      }
      const out=[0,1,2,3].map(k=>avg(acc.map(v=>v[k])));
      // A transparent cell carries no observable optical contribution.
      return out[3]>0?[...out,rw,rd]:blank();
    }
    for(let y=0;y<h;y++){
      const row=layers.filter(l=>y>=l.r.y&&y<l.r.y+l.r.h);
      if(!opt){for(let x=0;x<w;x++){if(!row.length){pixels.push(blank());continue}const active=row.filter(l=>inside(l.r,x,y));const n=top(s,x,y,v=>v.complete);stats.searches++;pixels.push(evaluate(x,y,active,n));}continue;}
      const events=row.flatMap((l,i)=>[{x:l.r.x,i,on:true},{x:l.r.x+l.r.w,i,on:false}]).sort((a,b)=>a.x-b.x||a.i-b.i||Number(a.on)-Number(b.on));
      const member=new Array(row.length).fill(false);let x=0,e=0;
      while(x<w){while(e<events.length&&events[e].x<=x){member[events[e].i]=events[e].on;e++}
        const end=e<events.length?Math.min(w,events[e].x):w,active=row.filter((_,i)=>member[i]);
        if(end<=x)throw Error("non advancing span");
        if(!active.length){for(;x<end;x++){pixels.push(blank());stats.empty++}continue}
        let prev=null,previous;
        for(;x<end;x++){const n=top(s,x,y,v=>v.complete);stats.searches++;const k=key(n);if(can&&prev===k){pixels.push(previous);stats.reused++;continue}
          const p=evaluate(x,y,active,n);pixels.push(p);if(can){prev=k;previous=p;}
        }
      }
    }return {pixels,stats,can};
  }
  const groups={};let totalCells=0,frames=0,maxError=0;
  function test(w,h,step,kind,n){
    const f=frame(w,h,step,kind,n),a=run(f,false),b=run(f,true);
    if(a.pixels.length!==b.pixels.length)throw Error("length");
    if(!b.can&&b.stats.reused)throw Error("partial reuse");
    for(let i=0;i<a.pixels.length;i++)for(let j=0;j<6;j++){
      const e=Math.abs(a.pixels[i][j]-b.pixels[i][j]);maxError=Math.max(maxError,e);if(e!==0)throw Error(`parity ${kind} ${i} ${j}: ${e}`);
    }
    const g=groups[kind]??={frames:0,cells:0,before:newStats(),after:newStats()};
    g.frames++;g.cells+=w*h;for(const k in a.stats){g.before[k]+=a.stats[k];g.after[k]+=b.stats[k]}
    totalCells+=w*h;frames++;
  }
  for(const kind of ["complete","zero","partial","sparse","ties"])for(let i=0;i<40;i++)test(12+Math.floor(rnd()*24),9+Math.floor(rnd()*18),1+Math.floor(rnd()*9),kind,1+Math.floor(rnd()*8));
  test(128,96,12,"complete",12);test(128,96,12,"sparse",12);
  test(96,64,1,"dense",3);test(1,256,16,"thin",4);
  const before=newStats(),after=newStats();for(const g of Object.values(groups))for(const k in before){before[k]+=g.before[k];after[k]+=g.after[k]}
  return {frames,totalCells,maxError,before,after,groups};
}
console.log(JSON.stringify(simulation(),null,2));
