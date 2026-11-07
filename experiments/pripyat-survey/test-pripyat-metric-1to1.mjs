/** Verify 2026 OSM/SRTM import conversion into the native metre-based scene.
 * Historic appearance and source mapping quality are separate questions.
 */
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

const scene=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const {d,geo,road}=vm.runInNewContext(scene+`({
  d:M3D_PRIPYAT_EXPANSION_DATA,
  geo:m3dExpansionGeoMetres,
  road:m3dExpansionLegacyRoadMetres
})`,{}, {timeout:5000});
const eps=1e-7,close=(a,b,label)=>assert.ok(Math.abs(a-b)<eps,`${label}: ${a} vs ${b}`);
assert.ok(d.scale>.48&&d.scale<.50,'the archived OSM data scale must be understood and detected');
assert.equal(d.step,100,'SRTM samples are on a 100 metre grid');
assert.equal(d.radius,2000,'geographic radius must remain 2000 m');
assert.equal(d.grid.length,41,'SRTM source grid should be 41 by 41');
assert.equal(d.grid[20].length,41);
const [cx,cz]=geo(0,0);close(cx,d.anchorX,'centre X');close(cz,d.anchorZ,'centre Z');
for(const east of [-2000,-700,-100,0,100,700,2000]){
 const [x,z]=geo(east,0);
 close(Math.hypot(x-cx,z-cz),Math.abs(east),'eastward metric 1:1');
}
for(const north of [-2000,-700,-100,100,700,2000]){
 const [x,z]=geo(0,north);
 close(Math.hypot(x-cx,z-cz),Math.abs(north),'northward metric 1:1');
}
const sample=d.routes.find(x=>x.w>4&&x.p.some(y=>y.length>1));
assert.ok(sample,'missing OSM source roads');
const p=sample.p.find(x=>x.length>1);
const p0=road(...p[0]),p1=road(...p[1]);
close(Math.hypot(p0[0]-cx,p0[1]-cz),
      Math.hypot(p[0][0]-cx,p[0][1]-cz)/d.scale,'legacy road radial scale');
close(Math.hypot(p1[0]-p0[0],p1[1]-p0[1]),
      Math.hypot(p[1][0]-p[0][0],p[1][1]-p[0][1])/d.scale,
      'legacy road segment length');
assert.ok(sample.w/d.scale>8&&sample.w/d.scale<14,
  'road width must also be rescaled from archived units');
const roadMid=scene.slice(scene.indexOf('function addPripyatExpansion()'),scene.indexOf('function makeScene()'));
assert.ok(roadMid.includes('m3dExpansionLegacyRoadMetres(...path[k],d)'),
    'OSM source routes must be corrected at export');
assert.ok(roadMid.includes('const realWidth=route.w/d.scale'),
    'OSM road width must be corrected at export');
assert.ok(roadMid.includes('return (getElevation(e,n)-d.base)*fade(e,n)+offset'),
    'topography must preserve real elevation metre differences');
assert.ok(!roadMid.includes('d.scale*(e*Math.cos'), 'SRTM terrain cannot remain compressed');
const sampleCount=d.routes.reduce((s,r)=>s+r.p.reduce((a,p)=>a+p.length-1,0),0);
console.log('Pripyat metric 1:1 OK — '+sampleCount+
  ' road segments; OSM source factor '+d.scale+' reverted; terrain radius '+d.radius+' m');
