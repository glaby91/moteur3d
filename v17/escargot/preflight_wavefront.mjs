function simulate(){
 let seed=40119;const rand=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/4294967296};
 let cases=0,paths=0,queries=0,maxWaves=0,unknown=0,complete=0;
 for(let c=0;c<10000;c++){
  const budget=2+Math.floor(rand()*4095),n=1+Math.floor(rand()*Math.max(1,Math.floor(budget/4))),base=Math.floor(budget/n),rem=budget%n;
  const jobs=Array.from({length:n},(_,i)=>({quota:base+(i<rem?1:0),primary:1+Math.floor(rand()*8),secondary:rand()<.2?0:1+Math.floor(rand()*8),used:0,done:false}));
  let rounds=0,used=0;
  while(jobs.some(p=>!p.done&&p.used<p.quota)){
   rounds++;for(const p of jobs)if(!p.done&&p.used<p.quota){p.used++;used++;if(p.used===p.primary+p.secondary)p.done=true}
  }
  if(used>budget||rounds>16||jobs.some(p=>p.used!==Math.min(p.quota,p.primary+p.secondary)||p.done!==(p.quota>=p.primary+p.secondary)))throw Error("quota or termination");
  paths+=n;queries+=used;complete+=jobs.filter(p=>p.done).length;unknown+=jobs.filter(p=>!p.done).length;maxWaves=Math.max(maxWaves,rounds);cases++;
 }
 return {cases,paths,queries,complete,unknown,maxWaves};
}
console.log(JSON.stringify(simulate(),null,2));
