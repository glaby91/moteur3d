// Regression: four Pripyat historical photograph studies, 2026-11-09.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const root=new URL('../2026-10-30-cote-ete-gt/native_v14/',import.meta.url);
const scene=readFileSync(new URL('scene_pripyat_central.js',root),'utf8');
const exporter=readFileSync(new URL('export_scene.js',root),'utf8');
const sign=require(new URL('assets/pripyat/energetik_roof_neon_sign.js',root).pathname);
function variant(period,display){
 return vm.runInNewContext(scene+`({roof:m3dBuildEnergetikHistoricalRoofTitle,
   facade:m3dBuildEnergetikCongressFacade,lights:M3D_SCENE_LIGHTS})`,
 {process:{env:{M3D_PRIPYAT_FACADE_PERIOD:period,M3D_ENERGETIK_FACADE_DISPLAY:display}}});
}
function build(fn){
 const boxes=[],capsules=[],triangles=[];
 const args={box:(...a)=>boxes.push(a),capsule:(...a)=>capsules.push(a),meshTriangle:(...a)=>triangles.push(a)};
 const result=fn(args);
 for(const b of boxes)assert.ok(b.length===7&&b[3]>b[0]&&b[4]>b[1]&&b[5]>b[2]);
 return {boxes,capsules,triangles,result};
}
const v85=variant('pre1986','victory1985'),study85=build(v85.facade);
assert.equal(study85.result.portraits,0);
assert.ok(study85.boxes.some(b=>b[0]<=-35&&b[3]>=-30&&b[6]==='congressIvory'));
assert.ok(study85.capsules.length>=5,'Anonymous silhouette missing');
const v86=variant('pre1986','congress1986'),study86=build(v86.facade);
assert.equal(study86.result.portraits,12);
assert.equal(study86.capsules.length,0,'1985 relief leaked into 1986 variant');
const ruins=variant('abandoned','victory1985');
assert.equal(build(ruins.facade).boxes.length,0);
assert.ok(build(v86.roof).boxes.every(b=>b[6]==='neon'));
assert.ok(build(ruins.roof).boxes.every(b=>b[6]==='rust'));
assert.ok(v86.lights.length>=8);
const objects=[];const add=(...a)=>objects.push(a);
const lit=sign.buildEnergetikRoofNeonSign({objects,box:add,roundBox:add,capsule:add},0,9.43,-19.34,{coreMaterial:'neonRed'});
assert.equal(lit.lighting.nativeMaterial,'neonRed');
assert.ok(objects.some(o=>o.at(-1)==='neonRed'));
assert.ok(exporter.includes('neonRed:[249,48,36]'));
const views=JSON.parse(readFileSync(new URL('scene_pripyat_views.json',root),'utf8')).views;
assert.equal(views.length,18);
assert.equal(new Set(views.map(v=>v.name)).size,18);
for(const [i,v] of views.entries())
 assert.ok(v.distance>0&&v.fov>=35&&v.fov<=90&&v.target.length===3&&
 v.target.every(Number.isFinite),'Invalid viewpoint '+i);
for(let i=14;i<18;i++)assert.match(views[i].match_status,/not-registered/);
assert.equal(views[15].hour_step,32);
console.log('Archive 1985/1986 panel, neon signage, variants and four camera studies: OK');
