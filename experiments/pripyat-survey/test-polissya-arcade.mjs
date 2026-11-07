/** Polissya hotel + freestanding curved promenade: photoguided proportions.
 * These tests certify internal scene consistency, NOT historic measured accuracy. */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const scene=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(scene+`({
  block:M3D_PRIPYAT_BLOCKOUT,
  plan:m3dPolissyaArcadePlan,
  arcade:m3dBuildConnector,
  hotel:m3dBuildPolissya
})`,{}, {timeout:5000});
const p=api.plan(),palace=api.block.energetik,hotel=api.block.polissya;
assert.ok(p.x0>palace.x1+2.5,'gallery must start clear of Energetik');
assert.ok(hotel.x1-hotel.x0>=35,'hotel needs elongated main facade');
assert.ok(p.roofBottom>3.1&&p.roofBottom<4.1,'canopy headroom inconsistent');
assert.ok(p.halfWidth>=1.6&&p.halfWidth<=2.3,'canopy width implausible');
const start=p.at(0),mid=p.at(.5),end=p.at(1);
assert.ok(mid[1]>start[1]+4&&mid[1]>end[1]+4,'gallery is not visibly curved');
for(let i=0;i<=128;i++){
 const t=i/128;
 for(const side of [-1,0,1]){
  const [x,z]=p.at(t,side*p.halfWidth);
  assert.ok(x>palace.x1+2,'arcade touches palace');
  // Entire colonnade stands in FRONT of the hotel, outside its +Z face.
  assert.ok(z>hotel.z1+2.0,'colonnade enters the hotel facade');
 }
}
const meshes=[],boxes=[];
api.arcade({box:(...args)=>boxes.push(args),meshTriangle:(...args)=>meshes.push(args)});
assert.ok(meshes.length>=250,'bent roof mesh should use a continuous multi-segment surface');
assert.ok(boxes.length>=34,'regular paired arcade pillars missing');
for(const [a,b,c,mat] of meshes){
 assert.ok(['stone','concrete'].includes(mat),'unexpected arcade material');
 for(const v of [a,b,c]){
  assert.equal(v.p.length,3);assert.equal(v.uv.length,2);
  assert.ok(v.p.every(Number.isFinite),'nonfinite mesh vertex');
 }
}
const hb=[];
api.hotel({box:(...args)=>hb.push(args),capsule:()=>{}});
assert.ok(hb.some(x=>x[4]>19),'hotel rooftop terrace missing');
assert.ok(hb.some(x=>x[6]==='rust'&&x[4]>18),'oxidised roof sign missing');
assert.ok(hb.some(x=>x[6]==='windowdark'),'hotel windows missing');
assert.ok(hb.some(x=>x[6]==='plaster'),'pale roof cladding missing');
console.log('Polissya hotel + curved freestanding arcade: '+meshes.length+' roof triangles, '+boxes.length+' pillar pieces, '+hb.length+' hotel parts');
