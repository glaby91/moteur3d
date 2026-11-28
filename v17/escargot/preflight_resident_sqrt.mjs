function sqrtCheck(){
 let seed=0x8bdbbc12;const rnd=()=>seed=(Math.imul(seed,1664525)+1013904223)>>>0;
 const f=new Float32Array(1),u=new Uint32Array(f.buffer);
 const bits=x=>{f[0]=x;return u[0]},num=x=>{u[0]=x>>>0;return f[0]};
 const mul=(a,b)=>{const a0=a&65535,a1=a>>>16,b0=b&65535,b1=b>>>16;const low0=a0*b0,mid=a0*b1+a1*b0,lo=(low0+(mid&65535)*65536)>>>0;return [lo,(a1*b1+Math.floor(mid/65536)+(lo<(low0>>>0)?1:0))>>>0]};
 const cmp=(a,b)=>a[1]===b[1]?Math.sign(a[0]-b[0]):Math.sign(a[1]-b[1]);
 function solve(x,approx){
  const aa=bits(x),ea=(aa>>>23)&255,e=ea-127,parity=e&1,E=(e-parity)/2+127,ma=(aa&0x7fffff)|0x800000,shift=25+parity,N=[(ma<<shift)>>>0,ma>>>(32-shift)];
  const qb=bits(approx),qe=(qb>>>23)&255,qm=(qb&0x7fffff)|0x800000;
  let m=qe===E?qm:qe===E+1?0x1000000:qe===E-1?qm>>>1:0;
  const finish=m=>{let exponent=E;if(m>=0x1000000){m>>>=1;exponent++}return ((exponent<<23)|(m&0x7fffff))>>>0};
  if(m>=0x7ffffe&&m<=0x1000002){for(let i=0;i<3;i++){const lo=cmp(N,mul(2*m-1,2*m-1)),hi=cmp(N,mul(2*m+1,2*m+1));if(lo<0||(lo===0&&(m&1))){m--;continue}if(hi>0||(hi===0&&(m&1))){m++;continue}return finish(m)}}
  let l=0x800000,h=0x1000000;for(let i=0;i<25&&l<h;i++){const mid=l+Math.floor((h-l+1)/2);if(cmp(mul(2*mid,2*mid),N)<=0)l=mid;else h=mid-1}
  m=l;const c=cmp(N,mul(2*m+1,2*m+1));if(c>0||(c===0&&(m&1)))m++;return finish(m);
 }
 let checked=0,errors=0;
 for(let i=0;i<250000;i++){const x=num(((1+rnd()%254)<<23)|(rnd()&0x7fffff)),expected=bits(Math.sqrt(x)),approx=num((expected+(rnd()%17)-8)>>>0);if(solve(x,approx)!==expected)errors++;checked++}
 if(errors)throw Error("sqrt parity "+errors);return {checked,errors};
}
console.log(JSON.stringify(sqrtCheck(),null,2));
