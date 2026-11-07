/**
 * Geometry-level regression probes using the actual source generators.
 * These are NOT image-space/photogrammetric validation.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const src=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(src+`
({
 arch:M3D_ENERGETIK_ARCH,
 validate:m3dCheckEnergetikDatum,
 arcade:m3dBuildFoyerMuralArcade,
 stair:m3dBuildEastStairCore,
 foyerStair:m3dBuildFoyerStairFlights,
 rear:m3dBuildEnergetikRearWall,
 shell:m3dBuildEnergetikShell,
 connector:m3dBuildConnector,
 polissya:m3dBuildPolissya
})`, {}, {timeout:5000});
assert.equal(api.validate(),true);
assert.equal(api.arch.envelope.x1-api.arch.envelope.x0,81);
assert.ok(Math.abs(api.arch.envelope.z1-api.arch.envelope.z0-24.6)<1e-8);

function geometry(builder){
  const boxes=[];
  builder({box:(...v)=>boxes.push(v),capsule:()=>{}});
  assert.ok(boxes.length>0,'geometry generator emitted nothing');
  for(const v of boxes){
    assert.equal(v.length,7);
    for(const n of v.slice(0,6))assert.ok(Number.isFinite(n),'nonfinite box coordinate');
    assert.ok(v[3]>v[0] && v[4]>v[1] && v[5]>v[2],`inverted box: ${v.join(',')}`);
  }
  return boxes;
}
function hits(boxes,x,y,z){
  return boxes.filter(v=>x>=v[0] && x<=v[3] &&
                         y>=v[1] && y<=v[4] &&
                         z>=v[2] && z<=v[5]);
}
const slabs=geometry(api.slabs);
assert.equal(hits(slabs,2,3.30,-37.0).length,0,'mezzanine blocks the mural atrium at first floor');
assert.equal(hits(slabs,2,6.40,-37.0).length,0,'second slab blocks the double-height mural atrium');
assert.ok(hits(slabs,10,3.30,-37.0).length>0,'east-side upper gallery missing');
assert.ok(hits(slabs,5,3.30,-41.5).length>0,'rear connecting strip missing');
assert.equal(hits(slabs,-40,3.30,-37).length,0,'first-floor slab blocks double-height auditorium');
assert.ok(hits(slabs,-40,6.40,-37).length>0,'upper auditorium ceiling slab missing');
const arcade=geometry(api.arcade);
const muralProbeX=api.arch.foyer.mosaicWallX+api.arch.foyer.arcadeDepth/2;
assert.equal(hits(arcade,muralProbeX,1.2,-40.5).length,0,'lower foyer bay must be open');
assert.equal(hits(arcade,muralProbeX,1.2,-36.9).length,0,'other lower foyer bay must be open');
assert.ok(hits(arcade,muralProbeX,3.9,-36.9).length>0,'upper painted wall must have backing');
assert.ok(arcade.some(v=>v[6]==='stone' && v[4]>2.0),'stone piers missing');
assert.ok(api.arch.foyer.mosaicWallX-api.arch.foyer.mosaicSurfaceX<0.05,'mural is visibly detached from backing');

const rear=geometry(api.rear);
assert.equal(hits(rear,-12.2,1.8,-42.8).length,0,'rear glazing must have opening');
assert.ok(hits(rear,-10.2,1.8,-42.8).length>0,'rear window jamb missing');
const shell=geometry(api.shell);
assert.equal(hits(shell,0,10.4,-31).length,0,'former solid rooftop occluder returned');

const connector=geometry(api.connector);
assert.equal(hits(connector,35,2.0,-21).length,0,'arch pedestrian way blocked');
assert.ok(hits(connector,35,5.1,-21).length>0,'arch superstructure missing');

const foyerStair=geometry(api.foyerStair);
assert.equal(foyerStair.filter(v=>v[6]==='stone').length,18,'17 broad foyer steps plus landing required');
const f=api.arch.foyer;
assert.ok(foyerStair.some(v=>Math.abs(v[4]-f.firstLandingY)<0.00001),
  'top foyer tread must match the finished landing elevation');
assert.equal(f.firstLandingY,api.arch.levels.firstSlab+api.arch.levels.slabThickness,
  'landing uses underside instead of walking surface');
const stair=geometry(api.stair);
assert.equal(stair.filter(v=>v[6]==='stone').length,51,'expected 17 treads over 3 flights');
assert.equal(stair.filter(v=>v[6]==='concrete').length,6,'three stair landings and galleries required');
const l=api.arch.levels;
for(const y of [l.firstSlab+l.slabThickness,l.secondSlab+l.slabThickness,l.roof]){
  assert.ok(stair.some(v=>v[6]==='concrete' && Math.abs(v[4]-(y+0.02))<0.00001),
    'stair landing does not meet floor elevation '+y);
}

const hotel=geometry(api.polissya);
assert.equal(hits(hotel,51.5,2.1,-32).length,0,'Polissya core must be hollow');
assert.equal(hits(hotel,40.25,2.2,-23).length,0,'Polissya western entry blocked');
assert.equal(hits(hotel,52,2.4,-20.30).length,0,'Polissya front window is an opaque wall');
assert.ok(hotel.some(v=>v[6]==='rust' && v[4]>15.9),'rusty hotel rooftop sign missing');
assert.ok(hotel.some(v=>v[6]==='stone' && v[4]>18.5),'hotel roof terrace missing');
console.log('Energetik/Polissya generator geometry probes passed:',{
  slabs:slabs.length, arcade:arcade.length, rear:rear.length, shell:shell.length,
  foyerStair:foyerStair.length,
  stair:stair.length, connector:connector.length, hotel:hotel.length
});
