/** Separate the typological 1970 design from the Pripyat modification,
 * and keep the sport floor at 40 x 20 m with spectator clearance.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const dir=new URL('../2026-10-30-cote-ete-gt/native_v14/', import.meta.url);
const scene=readFileSync(new URL('scene_pripyat_central.js', dir),'utf8');
const exporter=readFileSync(new URL('export_scene.js',dir),'utf8');
const a=vm.runInNewContext(scene+`({
  p:M3D_ENERGETIK_TYPE_1970,a:M3D_ENERGETIK_ARCH,
  gym:m3dBuildEnergetikSportsWing,validate:m3dCheckEnergetikDatum
})`,{},{timeout:5000});
assert.equal(a.p.project,'2C-06-9/70');
assert.equal(a.p.planAxesMetres.length,81.3);
assert.equal(a.p.planAxesMetres.depth,30.4);
assert.equal(a.p.auditorium.clearHeight,11.6);
assert.equal(a.p.foyerAreaSqM,540.6);
assert.equal(a.validate(),true);
assert.notEqual(a.a.envelope.z1-a.a.envelope.z0,a.p.planAxesMetres.depth,
 'historical standard design must not silently rescale the Pripyat variant');
const g=a.a.sports;
assert.equal(g.courtLength,40);
assert.equal(g.courtWidth,20);
assert.ok(g.x1-g.x0 >= g.courtLength+2*g.clearEnd, 'missing end clearance');
assert.ok(g.z1-g.z0 >= g.courtWidth+2*g.clearSide, 'missing side clearance');
assert.equal(g.x1-g.x0,44);
assert.equal(g.z1-g.z0,26.2);
const triangles=[];
a.gym({box:()=>{},capsule:()=>{},meshTriangle:(...v)=>triangles.push(v)});
assert.equal(triangles.filter(t=>t[3]==='gymFloor').length,2);
assert.ok(exporter.includes('X=(U-.5)*43.6,Z=(V-.5)*25.8'),
 'floor atlas must map precisely to the new hall mesh');
assert.ok(exporter.includes('near(aX-20.0,.075)'),
 'handball goal lines must be 40 metres apart');
assert.ok(exporter.includes('near(aZ-10.0,.075)'),
 'handball side lines must be 20 metres apart');
console.log('Reference 2C-06-9/70 separated from Pripyat geometry; gym clearance and 40x20 atlas verified');
