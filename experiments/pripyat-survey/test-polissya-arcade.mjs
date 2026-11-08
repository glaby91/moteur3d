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
  hotel:m3dBuildPolissya,
  annex:m3dBuildPolissyaAttachedAnnex
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
assert.ok(p.roofCrownRise>.10&&p.roofCrownRise<.45,
 'Canopy cross section must be cambered, never a flat slab (IMG-022 excluded)');
assert.ok(p.halfWidth>=1.6&&p.halfWidth<=2.3,'canopy width implausible');
const start=p.at(0),mid=p.at(.5),end=p.at(1);
const front=p.at(.02),last=p.at(.98);
const departSlope=Math.abs((front[1]-start[1])/(front[0]-start[0]));
const arrivalSlope=Math.abs((end[0]-last[0])/(end[1]-last[1]));
assert.ok(departSlope<.04,'sketch: arcade must leave palace nearly horizontally');
assert.ok(arrivalSlope<.04,'sketch: arcade must finish nearly vertically along hotel');
assert.ok(end[1]>5&&end[1]<10,'sketch: arcade end must stop around middle of hotel frontage');
assert.ok(start[1]>palace.z0+7&&start[1]<palace.z1-7,
  'sketch: arcade start must align with middle portion of the palace side facade');
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
assert.ok(meshes.some(tri=>tri.slice(0,3).some(v=>v.p[1]>p.roofTop+p.roofCrownRise-.001)),
 'Roof triangles lack a raised transverse crown: flat roof regression');
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
assert.ok(hb.some(x=>x[6]==='metal'&&x[4]>18),'restored 1986 hotel sign missing');
assert.ok(hb.some(x=>x[6]==='glass'),'historical 1986 hotel windows must be glazed');
assert.ok(hb.some(x=>x[6]==='plaster'),'pale roof cladding missing');
const annex=api.block.polissyaAnnex;
assert.ok(annex,'Low hotel-attached wing omitted from city plan');
assert.equal(api.block.housing.some(b=>b.id==='east-south'||b.id==='east-north'),false,
 'Unverified multi-storey blocks appear on the right of the Hotel / gallery');
assert.ok(annex.z0 < hotel.z0-12 && annex.z1>hotel.z0 && annex.z1<hotel.z0+.6,
 'One-storey wing must touch the hotel SOUTH end');
const attachedParts=[];
const manifest=api.annex({box:(...b)=>attachedParts.push(b)});
assert.equal(manifest.glazedBays,6,'Low wing should have its own storefront');
assert.ok(attachedParts.some(b=>b[6]==='glass'),'1986 restaurant wing needs intact glazing');
assert.ok(attachedParts.some(b=>b[6]==='concrete' && b[4]>=annex.height-.001 &&
  b[0]===annex.x0 && b[2]===annex.z0),'low wing roof needs a physical volume');
assert.ok(manifest.doorWorldX0>=hotel.x0+.8 && manifest.doorWorldX1<=hotel.x0+5.6,
 'Annex passage must line up with the already existing hotel side entrance');
// The annex is separate from the curved arcade. Check each roof-edge sample,
// not only the two plan endpoints.
let nearest=Infinity;
const rectPointDist=(x,z)=>
 Math.hypot(Math.max(annex.x0-x,0,x-annex.x1),
            Math.max(annex.z0-z,0,z-annex.z1));
for(let i=0;i<=1000;i++){
 const t=i/1000;
 for(const side of [-1,0,1]){
  const [x,z]=p.at(t,side*p.halfWidth);
  nearest=Math.min(nearest,rectPointDist(x,z));
 }
}
assert.ok(nearest>0.8,
 'Arcade roof clips the attached hotel low wing (closest distance: '+nearest.toFixed(2)+' m)');
console.log('Attached low Polissya wing valid; minimum free arcade / wing gap: '+nearest.toFixed(2)+' m');
console.log('Polissya hotel + curved freestanding arcade: '+meshes.length+' roof triangles, '+boxes.length+' pillar pieces, '+hb.length+' hotel parts');
