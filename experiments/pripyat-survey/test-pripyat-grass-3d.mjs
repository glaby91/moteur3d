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
  roadSegments:M3D_PRIPYAT_EXPANSION_DATA.routes.length,
  terrainQuad:m3dBuildTiledGrassTerrainQuad
})`,{process:{env:{}}},{timeout:5000});
const sites=context.sites();
assert.ok(sites.length>=500,'needs substantial 3D grass, not isolated proxy objects');
assert.ok(sites.length<=context.config.patchMax,'grass triangle budget exceeded');
assert.equal(context.config.bladesPerPatch,3);
assert.equal(context.config.patchMax,1450);
const kinds={};
for(const s of sites){
  kinds[s.kind]=(kinds[s.kind]??0)+1;
  assert.ok(Number.isFinite(s.x+s.y+s.z+s.h) && s.h>0,'invalid world-space grass');
  const maxHeight={joint:.16,curb:.28,planter:.30,lawn:.27}[s.kind];
  assert.ok(s.h<=maxHeight,`grass too tall for ${s.kind}: ${s.h.toFixed(3)} m`);
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
// Verify that every 100m SRTM cell repeats grass PBR every ~4m, not once.
const tiled=[];
const textured=vm.runInNewContext(code+`({tiler:m3dBuildTiledGrassTerrainQuad})`,{
  process:{env:{}},
  meshTriangle:(a,b,c,mat)=>tiled.push({a,b,c,mat})
},{timeout:5000});
textured.tiler([[0,0,0],[100,0,0],[100,0,100],[0,0,100]],4);
assert.equal(tiled.length,2,'one SRTM quad must remain two triangles');
assert.equal(tiled[0].mat,'grass');
assert.equal(tiled[0].b.uv[0]-tiled[0].a.uv[0],25,'100m terrain stretched: expected 25 PBR repeats');
assert.equal(tiled[0].c.uv[1]-tiled[0].a.uv[1],25,'grass tiling must repeat in Z');
const tris=[];
const report=context.build({meshTriangle:(a,b,c,m)=>tris.push({a,b,c,m})},sites);
assert.equal(report.patches,sites.length);
assert.equal(report.triangles,sites.length*context.config.bladesPerPatch*6);
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
// Alignment with true plaza 3.2m grouting: each tuft is clustered
// at the intersection of existing expansion joints, not on slab centres.
const plaza={x0:-53,z0:-15};
let matched=0,total=0;
for(const v of sites.filter(v=>v.kind==='joint')){
  const gx=Math.round((v.x-plaza.x0-1)/3.2),gz=Math.round((v.z-plaza.z0-1)/3.2);
  if(gx>=0&&gx<32&&gz>=0&&gz<23){
    total++;if(Math.hypot(v.x-(plaza.x0+1+gx*3.2),v.z-(plaza.z0+1+gz*3.2))<.18)matched++;
  }
}
assert.ok(total>100&&matched/total>.95,'grass does not follow actual paving joints');
assert.ok(code.includes('m3dBuildPripyatGroundGrass({meshTriangle});'),
          'grass builder must be called in actual makeScene');
console.log('Grass 3D OK: '+sites.length+' tufts, '+tris.length+' actual double-sided triangles; groups: '+JSON.stringify(kinds));
