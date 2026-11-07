/** Regression for rear-wing placement of the documented Energetik sports hall.
 * It is part of the palace. No model-file scraping or external copyrighted assets.
 * Historical dimensions remain provisional until georeferenced plan survey.
 */
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(source+`({
  a:M3D_ENERGETIK_ARCH,b:M3D_PRIPYAT_BLOCKOUT,
  wing:m3dBuildEnergetikSportsWing,rear:m3dBuildEnergetikRearWall,
  datum:m3dCheckEnergetikDatum,urban:m3dCheckUrbanFootprints
})`,{}, {timeout:5000});
assert.ok(api.datum());
assert.ok(api.urban());
const g=api.a.sports,e=api.a.envelope;
assert.ok(g.z0<e.z0 && g.z1>e.z0 && g.z1<e.z0+1.2,'gym must join existing north wall');
assert.ok(g.x0>=e.x0 && g.x1<=e.x1,'gym must fit the palace rear wing span');
assert.ok(!Object.hasOwn(api.b,'gym'),'gym must not be an independent urban building');
function capture(fn){const boxes=[],triangles=[],capsules=[];
 fn({box:(...v)=>boxes.push(v),meshTriangle:(...v)=>triangles.push(v),capsule:(...v)=>capsules.push(v)});
 for(const b of boxes)assert.ok(b[3]>b[0]&&b[4]>b[1]&&b[5]>b[2],`non-positive volume ${b}`);
 return {boxes,triangles,capsules};
}
const wing=capture(api.wing),rear=capture(api.rear);
function inside(b,x,y,z){return b.some(v=>x>=v[0]&&x<=v[3]&&y>=v[1]&&y<=v[4]&&z>=v[2]&&z<=v[5]);}
const doorX=(g.doorwayLeft+g.doorwayRight)/2,doorY=1.6;
assert.ok(!inside(rear.boxes,doorX,doorY,e.z0+0.5),'palace rear wall blocks internal passage');
assert.ok(!inside(wing.boxes,doorX,doorY,g.z1-0.2),'sports wing wall blocks passage');
assert.ok(!inside(wing.boxes,(g.x0+g.x1)/2,5,g.z0+0.2),'park-side window gallery must be open');
assert.equal(wing.triangles.filter(t=>t[3]==='gymFloor').length,2,'entire PBR sports atlas missing');
assert.ok(wing.boxes.some(b=>b[6]==='gymConcrete'&&b[4]>11),'high-beam sports ceiling missing');
assert.ok(wing.capsules.some(c=>c[3]==='gymMetal'),'rusted sporting fixtures missing');
console.log('Energetik gym verified as physically connected rear wing, with park glazing and clear corridor');
