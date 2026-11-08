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
  roadPlan:m3dPolissyaSideRoadPlan, sideRoad:m3dBuildPolissyaSideRoad
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
const junctionForecourt={x0:53,x1:82.5,z0:15,z1:25};
for(const h of api.block.housing){
  const intersects=h.x0<junctionForecourt.x1 && h.x1>junctionForecourt.x0 &&
                   h.z0<junctionForecourt.z1 && h.z1>junctionForecourt.z0;
  assert.ok(!intersects, 'unverified residential context occludes Polissya wing/arcade: '+h.id);
}

const start=p.at(0),mid=p.at(.5),end=p.at(1);
const front=p.at(.02),last=p.at(.98);
const departSlope=Math.abs((front[1]-start[1])/(front[0]-start[0]));
const arrivalSlope=Math.abs((end[0]-last[0])/(end[1]-last[1]));
assert.ok(departSlope<.04,'sketch: arcade must leave palace nearly horizontally');
assert.ok(arrivalSlope<.04,'sketch: arcade must finish nearly vertically along hotel');
assert.ok(end[1]>p.awningEndZ && end[1]<hotel.z1,
  'arcade must pass in front of the full wing width before terminating near its northern side');
assert.ok(start[1]>palace.z0+7&&start[1]<palace.z1-7,
  'sketch: arcade start must align with middle portion of the palace side facade');
assert.equal(p.endShape,'straight','photographs: flat transverse ends, no semicircular caps');
assert.equal(p.stepCount,4,'four shallow curved steps from the square');
assert.equal(p.platformTop,0.40,'arcade deck elevation');
assert.ok(Math.abs(end[1]-(p.awningEndZ+p.frontPassBeyondWing))<1e-6,
  'curved gallery must pass in FRONT of the hotel wing, beyond its far edge');
assert.ok(p.frontPassBeyondWing>=.8&&p.frontPassBeyondWing<=2.0,
  'arcade must not terminate abruptly at the wing corner');
assert.ok(Math.abs(p.awningStartZ-(hotel.z1-3.10-5.40))<1e-6,
 'wing side must be set back from right hotel corner by one bay plus wing width');
assert.ok(p.arrivalGapZ>0&&p.arrivalGapZ<=.5,'no structural connection permitted at roof seam');
assert.ok(Math.abs(p.awningEndZ-(hotel.z1-3.10))<1e-6,
  'photo 2: wing outer side is one window bay short of hotel corner');
assert.ok(Math.abs(p.hotelCornerZ-hotel.z1)<1e-6,'hotel corner landmark must be independent');
assert.ok(p.awningBackX-p.awningFrontX>20,
  'wing MUST project outward from the hotel, not run along the facade');
assert.ok(p.awningEndZ-p.awningStartZ>4&&p.awningEndZ-p.awningStartZ<7,
  'wing cross-section must be ~5m wide, not 20m along the hotel facade');
assert.ok(Math.abs(end[0]+p.halfWidth+p.hotelClearance-p.awningFrontX)<.001,
  'arcade free end must meet the outer wing end at its corner, without contact');
assert.ok(hotel.x0-p.awningFrontX>20,
  'wing must extend perpendicularly towards the esplanade');
// The gallery passes the ENTIRE free end of the wing, with the concrete
// roof staying in front of (not penetrating) the low hotel's roof slab.
const frontCrossings=Array.from({length:257},(_,i)=>p.at(i/256))
  .filter(pt=>pt[1]>=p.awningStartZ+.3 && pt[1]<=p.awningEndZ-.3);
assert.ok(frontCrossings.length>=10,'arcade must physically traverse the full wing front');
assert.ok(frontCrossings.every(pt=>pt[0]+p.halfWidth<=p.awningFrontX-p.hotelClearance+.001),
  'arcade curved roof would penetrate the hotel's wing');
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
assert.ok(meshes.length>=350,'curved, square-ended arcade roof missing');
assert.ok(meshes.filter(t=>t[3]==='rust').length>=p.pierCount*24,'oxidised post bases missing');
assert.ok(meshes.filter(t=>t[3]==='stone' && t.slice(0,3).some(v=>v.p[1]<=.41)).length>=p.segments*p.stepCount*2,'four arc-following shallow steps missing');
const postTriangles=meshes.filter(x=>x[3]==='plaster');
assert.ok(postTriangles.length>=p.pierCount*2*12,'tall ceramic-clad arcade pillars missing');
assert.ok(p.halfWidth>=2.0,'roof slab must overhang rebuilt posts');
assert.ok(postTriangles.some(t=>t.slice(0,3).some(v=>v.p[1]>=p.platformTop+.1&&v.p[1]<=p.platformTop+.15)),'supports must meet the raised plinth');
assert.ok(postTriangles.some(t=>t.slice(0,3).some(v=>v.p[1]>=p.roofBottom)),'posts must reach soffit');
for(const [a,b,c,mat] of meshes){
 assert.ok(['stone','concrete','plaster','rust'].includes(mat),'unexpected arcade material');
 for(const v of [a,b,c]){
  assert.equal(v.p.length,3);assert.equal(v.uv.length,2);
  assert.ok(v.p.every(Number.isFinite),'nonfinite mesh vertex');
 }
}
// Right-hand access roadway runs along X beside the short side-wall of
// Polissya. It must stay OUTSIDE the building and OUTSIDE the wing stairs.
const street=api.roadPlan(),rb=[];
api.sideRoad({box:(...args)=>rb.push(args)});
assert.ok(street.z0>hotel.z1+1.0,'side road intrudes into Polissya');
assert.ok(street.x0<hotel.x0 && street.x1>hotel.x1,
  'side road must run past the hotel, perpendicular to its long facade');
assert.ok(street.width>=5.5&&street.width<=7.5,'side street must be usable by vehicles');
assert.ok(rb.some(v=>v[6]==='asphalt' &&
  Math.abs(v[0]-street.x0)<.001&&Math.abs(v[2]-street.z0)<.001&&
  Math.abs(v[3]-street.x1)<.001&&Math.abs(v[5]-street.z1)<.001),
  'missing long side road asphalt surface');
assert.ok(rb.some(v=>v[6]==='concrete'&&v[4]>.12),
  'side road must have a visible curb');
assert.ok(rb.every(v=>v[2]>hotel.z1),'road elements cross the hotel wall');
const hb=[],hotelMeshes=[],hotelCapsules=[];
api.hotel({box:(...args)=>hb.push(args),capsule:(...args)=>hotelCapsules.push(args),
           meshTriangle:(...args)=>hotelMeshes.push(args)});
assert.ok(hotelMeshes.length>=24,'pitched rooftop summer cafe slab missing');
const rooftop=hotelMeshes.filter(tr=>tr[3]==='concrete'||tr[3]==='stone');
const heights=rooftop.flatMap(tr=>tr.slice(0,3).map(v=>v.p[1]));
assert.ok(Math.max(...heights)-Math.min(...heights)>.5,'photographic rooftop slope is not visible');
assert.ok(hb.filter(x=>x[6]==='brick').length>=100,'exposed masonry brick courses missing from hotel');
// The LONG low extension must be a deep gallery, not the obsolete 10 m plate.
// Actual rotation check: the FREE END of the long wing is ~22m west of
// Polissya, while the wing's width in the world-Z direction is ~5.4m.
const projectingAwning=hb.filter(v=>v[6]==='concrete' &&
                         Math.abs(v[1]-p.roofBottom)<1e-6 &&
                         Math.abs(v[4]-p.roofTop)<1e-6 &&
                         v[0]<hotel.x0-15);
assert.ok(projectingAwning.some(v=>Math.abs(v[0]-p.awningFrontX)<.001 &&
                                      Math.abs(v[3]-p.awningBackX)<.001 &&
                                      Math.abs(v[2]-p.awningStartZ)<.001 &&
                                      Math.abs(v[5]-p.awningEndZ)<.001),
  'wing roof must project WEST from Polissya perpendicular to its frontage');
assert.ok(projectingAwning.every(v=>v[5]<=p.awningEndZ+.001),
  'wing must end before the main hotel corner by one window bay');
const supports=hb.filter(v=>v[6]==='plaster' && v[1]>.35 && v[1]<.5 &&
                                  v[4]>p.roofBottom && v[0]<hotel.x0-1);
assert.ok(supports.length>=6,'long extension must have repeated structural pillars');
const fascia=hb.filter(v=>v[6]==='concrete' && v[0]<hotel.x0-2 &&
                               v[1]>2.7 && v[1]<3.1 && v[4]>=p.roofBottom);
assert.ok(fascia.length>=1,'continuous deep upper fascia missing');
const sideSteps=hb.filter(v=>v[6]==='stone' && Math.abs(v[1]-.02)<.001 &&
                                [0.125,0.250,0.375].some(y=>Math.abs(v[4]-y)<.001));
assert.ok(sideSteps.length>=3,'three small side access treads to hotel are missing');
const handrails=hotelCapsules.filter(x=>x[3]==='rust' && x[0]?.[1]>.9 && x[1]?.[1]>.9 &&
                                                x[0]?.[1]<1.6 && x[1]?.[1]<1.6);
assert.ok(handrails.length>=2,'side access needs a visible rusted guardrail');
assert.ok(hb.some(x=>x[4]>19),'hotel rooftop terrace missing');
assert.ok(hb.some(x=>x[6]==='rust'&&x[4]>18),'oxidised roof sign missing');
assert.ok(hb.some(x=>x[6]==='windowdark'),'hotel windows missing');
assert.ok(hb.some(x=>x[6]==='plaster'),'pale roof cladding missing');
console.log('Polissya hotel + curved freestanding arcade: '+meshes.length+' total triangles, '+postTriangles.length+' post-surface triangles, '+hb.length+' hotel parts');
