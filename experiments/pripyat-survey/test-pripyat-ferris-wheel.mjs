/** Pripyat mechanical Ferris wheel regression: geodesy, hub, rotor, cabin motion.
 * The source defines 3 separate assemblies; current V14 BIN is a pose snapshot.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const scene=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(scene+`({
 spec:M3D_FERRIS_WHEEL,site:m3dWheelSite,pose:m3dWheelPose,
 fixed:m3dBuildWheelStatic,rotor:m3dBuildWheelRotor,
 wing:m3dBuildEnergetikSportsWing,
 gondolas:m3dBuildWheelGondolas,build:m3dBuildPripyatWheel
})`,{},{timeout:5000});
const {spec}=api,site=api.site();
assert.equal(spec.numberOfGondolas,20);
assert.equal(spec.diameter,24.6);
assert.ok(Math.abs(site.x+14.5663904)<.05,'easting not projected to provisional scene reference');
assert.ok(Math.abs(site.z+122.6577414)<.05,'northing not projected to provisional scene reference');
const poses=[0,90,180,270,360].map(t=>api.pose(t));
function near(a,b,msg,tol=1e-8){assert.ok(Math.abs(a-b)<tol,msg+` (${a} vs ${b})`);}
near(poses[0].phaseRad,0,'start');
near(poses[1].phaseRad,Math.PI/2,'quarter rotation');
near(poses[2].phaseRad,Math.PI,'half rotation');
near(poses[3].phaseRad,3*Math.PI/2,'three-quarter rotation');
near(poses[4].phaseRad,0,'complete revolution');
for(const p of poses){
 assert.equal(p.gondolas.length,spec.numberOfGondolas);
 for(const g of p.gondolas){
   near(Math.hypot(g.pivot[0]-p.hub[0],g.pivot[1]-p.hub[1]),spec.radius,'rim radius');
   near(g.pivot[2],site.z,'pin on wheel plane');
   near(p.phaseRad+g.localCounterRotationRad,0,'parent rotor phase cancelled by each upright gondola');
   near(g.worldCabinPitchRad,0,'independent cabin must remain vertical');
 }
}
near(poses[0].gondolas[0].pivot[0],poses[4].gondolas[0].pivot[0],'wrap at 360');
near(poses[0].gondolas[0].pivot[1],poses[4].gondolas[0].pivot[1],'wrap at 360');
function collect(fn,pose){const boxes=[],capsules=[],ellipsoids=[];
 const ops={box:(...v)=>boxes.push(v),capsule:(...v)=>capsules.push(v),ellipsoidMesh:(...v)=>ellipsoids.push(v)};
 if(pose)fn(ops,pose);else fn(ops);
 boxes.forEach(b=>assert.ok(b[3]>b[0]&&b[4]>b[1]&&b[5]>b[2],'inverted box'));
 return {boxes,capsules,ellipsoids};
}
const fixed=collect(api.fixed);
const rotor=collect(api.rotor,poses[0]);
const cabins=collect(api.gondolas,poses[0]);
assert.ok(fixed.capsules.length>=10,'static A-frame/supports missing');
assert.ok(rotor.capsules.length>100,'rotating double rim/spokes incomplete');
assert.equal(cabins.ellipsoids.length,40,'20 separate yellow carriages with 2 shells each');
const rotor90=collect(api.rotor,poses[1]);
assert.notDeepEqual(JSON.stringify(rotor.capsules.slice(0,2)),JSON.stringify(rotor90.capsules.slice(0,2)),
 'wheel must rotate when pose phase changes');
assert.throws(()=>api.pose(NaN),/invalid/);
assert.throws(()=>api.pose(0,Infinity),/invalid/);
console.log('Pripyat wheel pose solver: site, 360s revolution, 20 upright cabins, rotating hub and support rig OK');

// The wheel must actually be visible from inside the sports hall through its
// park-facing glazing, not merely placed in the correct direction.
const views=JSON.parse(readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_views.json',import.meta.url),'utf8')).views;
const v=views[5],rad=Math.PI/180;
const eye=[v.target[0]+v.distance*Math.sin(v.angle*rad)*Math.cos(v.elevation*rad),
 v.target[1]+v.distance*Math.sin(v.elevation*rad),
 v.target[2]+v.distance*Math.cos(v.angle*rad)*Math.cos(v.elevation*rad)];
const hub=poses[0].hub;
const boxes=[];
api.wing({box:(...p)=>boxes.push(p),capsule:()=>{},meshTriangle:()=>{}});
function intersects(from,to,b){
 let lo=.001,hi=.999;
 for(let i=0;i<3;i++){
  const d=to[i]-from[i];
  if(Math.abs(d)<1e-12){if(from[i]<b[i]||from[i]>b[i+3])return false;continue;}
  let a=(b[i]-from[i])/d,c=(b[i+3]-from[i])/d;
  if(a>c)[a,c]=[c,a];
  lo=Math.max(lo,a);hi=Math.min(hi,c);
  if(lo>hi)return false;
 }
 return true;
}
assert.ok(eye[2]>-68.9&&eye[2]<-42.7,'camera 06 must stand inside gym');
assert.ok(hub[2]<-68.9,'ferris wheel must be behind the park-facing gym windows');
const occluders=boxes.filter(b=>intersects(eye,hub,b));
assert.equal(occluders.length,0,'gym architecture blocks the wheel view from camera 06');
console.log('Photographic view 06 clear through gym glazing to Ferris wheel hub');
