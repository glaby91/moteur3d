/** Photo-guided checks for user's 08/11/2026 Pripyat archive references.
 * Internal geometry validation only: NOT a photogrammetric certificate.
 * Run: node experiments/pripyat-survey/test-photo-references-261108.mjs
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
function builders(era){
 const context={process:{env:{M3D_PRIPYAT_FACADE_PERIOD:era}}};
 return vm.runInNewContext(source+`;({
   block:M3D_PRIPYAT_BLOCKOUT,
   arch:M3D_ENERGETIK_ARCH,
   model:M3D_POLISSYA_MODEL,
   awning:M3D_POLISSYA_ENTRANCE_AWNING,
   arc:m3dPolissyaArcadePlan,
   hotel:m3dBuildPolissya,
   glazing:m3dBuildEnergetik1986FacadeGlazing,
   roofTitle:m3dBuildEnergetikHistoricalRoofTitle,
   period:M3D_PRIPYAT_FACADE_PERIOD
 })`,context,{timeout:5000});
}
const api=builders('abandoned');
assert.equal(api.period,'abandoned');
const arc=api.arc();
const westEdge=api.block.polissya.x0;
const canopyWorldEdge=westEdge-api.awning.projection;
const lastArcadeEdge=arc.at(1)[0]+arc.halfWidth;
const gap=canopyWorldEdge-lastArcadeEdge;
assert.ok(gap>=.65 && gap<=2.4,'No credible open pedestrian gap between separate roofs: '+gap);
assert.ok(arc.at(1)[0]<westEdge-5.0,'Gallery must pass in front of the projecting canopy');
assert.ok(arc.at(0)[0]>api.arch.envelope.x1+2.0,'Arcade detached from palace');
assert.ok(api.awning.endLocalX-api.awning.startLocalX>24,
 'Hotel entrance frontage should span most of building, unlike previous token canopy');
const hotel=[];
api.hotel({box:(...args)=>hotel.push(args),capsule:()=>{}});
const lipX=canopyWorldEdge;
assert.ok(hotel.some(([x0,y0,z0,x1,y1,z1,m])=>
  m==='concrete' && y0>=3 && x0<=lipX+.1 && x1>=westEdge-.25 && z1-z0>23),
  'Large canopy missing in generated world-space hotel geometry');
const oldGlazing=[];
api.glazing({box:(...args)=>oldGlazing.push(args)});
assert.equal(oldGlazing.length,0,'Abandoned facade must remain open');
const historic=builders('pre1986');
assert.equal(historic.period,'pre1986');
const glass=[];
historic.glazing({box:(...args)=>glass.push(args)});
assert.ok(glass.filter(x=>x[6]==='glass').length>=10,'Historical window bays missing');
const beforeRoof=[],afterRoof=[];
api.roofTitle({box:(...args)=>afterRoof.push(args)});
historic.roofTitle({box:(...args)=>beforeRoof.push(args)});
assert.ok(beforeRoof.length>120 && beforeRoof.length===afterRoof.length,
  'Full archival phrase should consist of geometric letter strokes');
assert.ok(beforeRoof.every(v=>v[6]==='neon'),'Historic blue roof title should be emissive');
assert.ok(afterRoof.every(v=>v[6]==='rust'));
console.log('Photo-guided arcade/hotel/Energetik study geometry checks passed', {
 openPedestrianGapM:+gap.toFixed(2),
 entranceWidthM:+(api.awning.endLocalX-api.awning.startLocalX).toFixed(2),
 glazingBays:glass.filter(x=>x[6]==='glass').length,
 roofLetterStrokes:beforeRoof.length
});
