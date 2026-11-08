/** Regression: Energetik main facade in 1986 (historical photo study).
 * Checks object placement and era switches, NOT photographic authenticity.
 * IMG-022 is explicitly excluded as an arcade roof reference.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const src=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const exporter=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/export_scene.js',import.meta.url),'utf8');
const variants=(period,display)=>{
 const env={};
 if(period!==undefined)env.M3D_PRIPYAT_FACADE_PERIOD=period;
 if(display!==undefined)env.M3D_ENERGETIK_FACADE_DISPLAY=display;
 return vm.runInNewContext(src+`({
  period:M3D_PRIPYAT_FACADE_PERIOD,display:M3D_ENERGETIK_FACADE_DISPLAY,
  config:M3D_ENERGETIK_1986_FRONT,
  congress:m3dBuildEnergetikCongressFacade,glass:m3dBuildEnergetik1986FacadeGlazing,
  entry:m3dBuildEnergetikHistoricEntry,
  steps:m3dBuildEnergetikHistoricSteps
 })`,{process:{env}}, {timeout:5000});
};
const historic=variants(undefined,undefined);
assert.equal(historic.period,'pre1986','Default facade must be the inhabited city, not the ruins');
assert.equal(historic.display,'congress1986','Default historical year should present the XXVII Congress');
assert.equal(historic.config.portraitCount,12);
function draw(api,kind){
 const box=[],triangles=[],capsules=[];
 const fns={box:(...x)=>box.push(x),meshTriangle:(...x)=>triangles.push(x),
  capsule:(...x)=>capsules.push(x)};
 const manifest=api[kind](fns);
 for(const b of box){
   assert.ok(b.length===7 && b.slice(0,6).every(Number.isFinite));
   assert.ok(b[3]>b[0] && b[4]>b[1] && b[5]>b[2], `Degenerate box ${b.join(',')}`);
 }
 return {box,triangles,capsules,manifest};
}
const cp=draw(historic,'congress');
assert.equal(cp.manifest.portraits,12);
assert.equal(cp.triangles.length,24,'Twelve portrait panels need UV-mapped double triangles');
assert.ok(cp.box.filter(b=>b[6]==='congressRed').length>=2,'Red congress banner is missing');
assert.ok(cp.box.filter(b=>b[6]==='congressIvory').length>60,'Cyrillic text ink is missing');
assert.ok(cp.box.some(b=>b[6]==='congressCrimson'),'Folded red backdrop missing');
for(const tri of cp.triangles){
 assert.equal(tri[3],'propagandaPortraitAtlas');
 for(const vertex of tri.slice(0,3)){
  assert.ok(vertex.p.every(Number.isFinite) && vertex.uv.every(Number.isFinite));
  assert.ok(vertex.uv.every(v=>v>=0 && v<=1),'UV beyond atlas');
 }
}
assert.ok(draw(historic,'glass').box.filter(b=>b[6]==='glass').length>=10);
assert.ok(draw(historic,'entry').box.filter(b=>b[6]==='glass').length>=6);
const steps=draw(historic,'steps');
assert.ok(steps.box.filter(b=>b[6]==='stone').length>=7,'Broad historic entrance stairs missing');
assert.ok(steps.capsules.length>=10,'Historic entrance balustrades missing');
const victory=draw(variants('pre1986','victory1985'),'congress');
assert.equal(victory.manifest.portraits,0,'Victory anniversary should have no congress portrait row');
assert.ok(victory.box.some(b=>b[6]==='congressCharcoal'));
const emblem=draw(variants('pre1986','soviet_emblem'),'congress');
assert.equal(emblem.manifest.portraits,0);
const ruins=variants('abandoned','congress1986');
assert.equal(draw(ruins,'congress').box.length,0,'Do not display congress signs in ruins mode');
assert.equal(draw(ruins,'glass').box.length,0,'Do not fill abandoned upper facade windows');
assert.equal(draw(ruins,'entry').box.length,0,'Do not fill abandoned foyer windows');
assert.equal(draw(ruins,'steps').box.length,0,'Do not duplicate contemporary steps');
for(const kind of ['congressRed','congressCrimson','congressCharcoal','congressIvory','congressFrame','congressGold','propagandaPortraitAtlas'])
 assert.ok(exporter.includes("'"+kind+"'"), 'Exporter must generate PBR maps for '+kind);
console.log('Energetik 1986: default Congress, 12 UV portraits, upper/lower glass, broad stairs, PBR and alternate eras pass:',{boxes:cp.box.length,portraits:cp.manifest.portraits,triangles:cp.triangles.length,stairs:steps.box.length});
