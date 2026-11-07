/** Polissya hotel + freestanding curved promenade: photoguided proportions.
 * These tests certify internal scene consistency, NOT historic measured accuracy. */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const scene=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(scene+`({
  block:M3D_PRIPYAT_BLOCKOUT,
  plan:m3dPolissyaArcadePlan,world:m3dPolissyaLocalToWorld,model:M3D_POLISSYA_MODEL,angle:M3D_POLISSYA_ORIENTATION_DEGREES,
  arcade:m3dBuildConnector,
  hotel:m3dBuildPolissya
})`,{}, {timeout:5000});
const p=api.plan(),palace=api.block.energetik,hotel=api.block.polissya;
assert.equal(api.angle,90,'Polissya should be perpendicular to palace');
assert.equal(hotel.x1-hotel.x0,22,'rotated Polissya depth');
assert.equal(hotel.z1-hotel.z0,38,'rotated Polissya length');
const local=api.model;
const westFront=api.world(local.x0,local.z1),eastBack=api.world(local.x0,local.z0);
assert.ok(westFront[0]<eastBack[0],'hotel local +Z frontage must rotate toward square (west)');
const eastAlong=api.world(local.x1,local.z1);
assert.ok(Math.abs(westFront[0]-eastAlong[0])<.001,'long hotel axis must be world Z');
assert.ok(Math.abs(eastAlong[1]-westFront[1]-(local.x1-local.x0))<.001,'90-degree rotation must preserve longitudinal dimensions');
const galleryStart=p.at(0),galleryEnd=p.at(1);
assert.ok(galleryStart[0]>palace.x1+2.0,'gallery must start clear of Energetik');
assert.ok(galleryEnd[0]<hotel.x0-2.0,'gallery must end before hotel frontage');
assert.ok(hotel.z1-hotel.z0>=35,'hotel needs elongated long facade after rotation');
assert.ok(p.roofBottom>3.1&&p.roofBottom<4.1,'canopy headroom inconsistent');
assert.ok(p.halfWidth>=1.6&&p.halfWidth<=2.3,'canopy width implausible');
const start=p.at(0),mid=p.at(.5),end=p.at(1);
const front=p.at(.02),last=p.at(.98);
const departSlope=Math.abs((front[1]-start[1])/(front[0]-start[0]));
const arrivalSlope=Math.abs((end[0]-last[0])/(end[1]-last[1]));
assert.ok(departSlope<.04,'sketch: arcade must leave palace nearly horizontally');
assert.ok(arrivalSlope<.04,'sketch: arcade must finish nearly vertically along hotel');
assert.ok(end[1]>8&&end[1]<16,'sketch: arcade end must stop around middle of hotel frontage');
assert.equal(p.endCapSegments,12,'sketch: both free roof ends must be rounded');
const chordZ=(start[1]+end[1])/2;
assert.ok(Math.abs(mid[1]-chordZ)>3,'gallery is not visibly curved');
for(let i=0;i<=128;i++){
 const t=i/128;
 for(const side of [-1,0,1]){
  const [x,z]=p.at(t,side*p.halfWidth);
  assert.ok(x>palace.x1+1.0,'arcade touches palace');
  // All portions remain west of the world-oriented hotel's western frontage.
  assert.ok(x<hotel.x0-1.0,'colonnade enters the hotel west facade');
 }
}
const meshes=[],boxes=[];
api.arcade({box:(...args)=>boxes.push(args),meshTriangle:(...args)=>meshes.push(args)});
assert.ok(meshes.length>=350+4*p.endCapSegments*2,'bent roof and semicircular ends missing');
assert.ok(boxes.length>=40,'regular paired arcade pillars missing');
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
