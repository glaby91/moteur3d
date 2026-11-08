'use strict';
const assert=require('node:assert/strict');
const {buildHelicopterWreckReference}=require('./helicopter_wreck_reference');
const objects=[], point=p=>{assert(p.length===3&&p.every(Number.isFinite));};
const api={
 box(...a){assert(a.length===7);assert(a.slice(0,6).every(Number.isFinite));objects.push('box');},
 capsule(a,b,r,m){point(a);point(b);assert(r>0&&m);objects.push('capsule');},
 ellipsoidMesh(...a){assert(a.slice(0,6).every(Number.isFinite));objects.push('ellipse');},
 cone(a,b,r0,r1,m){point(a);point(b);assert(r0>r1&&r1>0&&m);objects.push('cone');}
};
const result=buildHelicopterWreckReference(api);
assert.equal(result.placedInPripyat,false);
assert(objects.length>70,'Reference is insufficiently detailed');
assert(objects.includes('cone'));
assert.throws(()=>buildHelicopterWreckReference(api,{scale:-1}),/Invalid scale/);
console.log(JSON.stringify({asset:result.id,placedInPripyat:result.placedInPripyat,primitiveCount:objects.length,counts:objects.reduce((m,x)=>(m[x]=(m[x]||0)+1,m),{})}));
