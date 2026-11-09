/** Energetik photo-study lattice: deliberately distinct from Polissya arcade.
 * This validates geometry construction, NOT survey-grade alignment.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');

function api(mode){
  const scope={process:{env:mode?{M3D_ENERGETIK_FORECOURT_LATTICE:mode}:{}}};
  return vm.runInNewContext(source+';({lattice:m3dBuildEnergetikForecourtLattice,arcade:m3dPolissyaArcadePlan,connector:m3dBuildConnector,env:M3D_PRIPYAT_BLOCKOUT,latticeMode:M3D_ENERGETIK_FORECOURT_LATTICE_MODE})',scope,{timeout:5000});
}
const disabled=api();
const noGeometry={box:()=>{throw Error('disabled mode generated boxes');},
 meshTriangle:()=>{throw Error('disabled mode generated triangles');}};
assert.equal(disabled.lattice(noGeometry).enabled,false,'default scene must stay unchanged');
assert.equal(disabled.latticeMode,'off');
const enabled=api('photo-study');
assert.equal(enabled.latticeMode,'photo-study');
const boxes=[],triangles=[];
const lattice=enabled.lattice({
  box:(...values)=>boxes.push(values),
  meshTriangle:(...values)=>triangles.push(values)
});
assert.equal(lattice.enabled,true);
assert.equal(lattice.subject,'Energetik');
assert.equal(lattice.independentFrom,'Polissya arcade');
assert.equal(lattice.roofCladding,false,'not a solid roof');
assert.equal(lattice.crossRibs,12);
assert.equal(lattice.postCount,10);
assert.equal(boxes.length,20,'five pairs of supports with footings');
assert.equal(triangles.length,1008,'24 segments x 3 rails plus twelve cross ribs, twelve tris per beam');
for(const b of boxes){
  assert.equal(b.length,7);
  assert.ok(b.slice(0,6).every(Number.isFinite));
  assert.ok(b[0]<b[3]&&b[1]<b[4]&&b[2]<b[5],'solid with inverted bounds');
}
for(const [a,b,c,material] of triangles){
  assert.ok(['concrete','metal'].includes(material));
  for(const v of [a,b,c]){
    assert.equal(v.p.length,3);
    assert.equal(v.n.length,3);
    assert.ok(v.p.every(Number.isFinite));
    assert.ok(v.n.every(Number.isFinite));
    assert.ok(Math.abs(Math.hypot(...v.n)-1)<1e-4,'face normal not normalized');
  }
}
const palace=enabled.env.energetik,hotel=enabled.env.polissya;
for(let i=0;i<=250;i++){
  const t=i/250;
  for(const side of [-1,0,1]){
    const [x,z]=lattice.point(t,side*lattice.halfWidth);
    assert.ok(z>palace.z1+0.4,'lattice study must remain outside the palace front wall');
    assert.ok(x<hotel.x0-10,'Energetik canopy cannot be mislocated at Polissya');
  }
}
const a1=disabled.arcade(),a2=enabled.arcade();
for(const t of [0,.2,.5,.8,1]){
  const p=a1.at(t),q=a2.at(t);
  assert.ok(Math.abs(p[0]-q[0])<1e-9 && Math.abs(p[1]-q[1])<1e-9,
    'old independent Polissya arcade changed');
}
const connectorBefore=[],connectorAfter=[];
disabled.connector({box:()=>{},meshTriangle:(...args)=>connectorBefore.push(args)});
enabled.connector({box:()=>{},meshTriangle:(...args)=>connectorAfter.push(args)});
assert.equal(connectorAfter.length,connectorBefore.length,
 'Energetik study changed the established Polissya arcade generator');
console.log('Energetik curved forecourt lattice: '+triangles.length+
 ' triangles, '+boxes.length+' box primitives, no roof slab, Polissya untouched');
