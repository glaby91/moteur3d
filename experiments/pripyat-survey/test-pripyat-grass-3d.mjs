/** Deterministic verification of actual 3D weed placement & CPU/Compute-compatible geometry. */
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

const code=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const context=vm.runInNewContext(code+`({
  config:M3D_PRIPYAT_GRASS_LAYER,
  sites:m3dPripyatGroundGrassSites,
  build:m3dBuildPripyatGroundGrass,
  overlaps:m3dPripyatInsideUrbanBuilding,
  nearRoad:m3dPripyatNearVerifiedRoad,
  roadSegments:M3D_PRIPYAT_EXPANSION_DATA.routes.length
})`,{process:{env:{}}},{timeout:5000});
const sites=context.sites();
assert.ok(sites.length>=500,'needs substantial 3D grass, not isolated proxy objects');
assert.ok(sites.length<=context.config.patchMax,'grass triangle budget exceeded');
assert.equal(context.config.bladesPerPatch,3);
const kinds={};
for(const s of sites){
  kinds[s.kind]=(kinds[s.kind]??0)+1;
  assert.ok(Number.isFinite(s.x+s.y+s.z+s.h) && s.h>0,'invalid world-space grass');
  if(s.kind==='lawn'){
    assert.ok(Math.hypot(s.x+10,s.z+31.1)<context.config.nearRadius+1e-8,'outside flat SRTM ground');
    assert.ok(!(s.x>-65&&s.x<112&&s.z>-89&&s.z<105),'lawn on urban pavement');
    assert.ok(!context.overlaps(s.x,s.z,1.4),'lawn collides with building');
    assert.ok(!context.nearRoad(s.x,s.z,.8),'lawn collides with mapped road');
    assert.equal(s.y,context.config.peripheryBaseY,'wrong lawn height');
  }else if(s.kind==='planter'){
    assert.equal(s.y,context.config.planterTopY,'plant growing inside planter');
  }else{
    assert.ok(s.kind==='joint'||s.kind==='curb','unexpected central vegetation kind');
    assert.equal(s.y,context.config.plazaBaseY,'weed does not start at paved surface');
    assert.ok(!context.overlaps(s.x,s.z,.35),'weeds inside a building');
  }
}
for(const kind of ['joint','curb','planter','lawn'])
 assert.ok(kinds[kind]>20,`missing planted type ${kind}`);
assert.equal(JSON.stringify(context.sites()),JSON.stringify(sites),'nondeterministic export');
const tris=[];
const report=context.build({meshTriangle:(a,b,c,m)=>tris.push({a,b,c,m})},sites);
assert.equal(report.patches,sites.length);
assert.equal(report.triangles,sites.length*context.config.bladesPerPatch*2);
assert.equal(tris.length,report.triangles);
for(let i=0;i<tris.length;i+=2){
 const a=tris[i],b=tris[i+1];
 assert.equal(a.m,b.m,'sided material mismatch');
 assert.equal(a.a.p[0],b.c.p[0],'reverse-wound back face not matching');
 assert.equal(a.a.p[1],b.c.p[1],'reverse-wound back face not matching');
 assert.ok(a.a.n.every(Number.isFinite),'triangle normal invalid');
}
assert.ok(!code.includes("ellipsoidMesh(x+dx,0.22,z+dz,0.38,0.28,0.34,'grass'"),
          'old green sphere proxies remain');
assert.ok(code.includes('m3dBuildPripyatGroundGrass({meshTriangle});'),
          'grass builder must be called in actual makeScene');
console.log('Grass 3D OK: '+sites.length+' tufts, '+tris.length+' actual double-sided triangles; groups: '+JSON.stringify(kinds));
