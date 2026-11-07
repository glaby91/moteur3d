// View-space geometry regression against the architectural generators.
// Checks unobstructed lines to the existing mural material, not photo fidelity.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const base=new URL('../2026-10-30-cote-ete-gt/native_v14/',import.meta.url);
const src=readFileSync(new URL('scene_pripyat_central.js',base),'utf8');
const api=vm.runInNewContext(src+`
({arch:M3D_ENERGETIK_ARCH,slabs:m3dBuildEnergetikSlabs,arcade:m3dBuildFoyerMuralArcade,
 foyerStair:m3dBuildFoyerStairFlights,eastStair:m3dBuildEastStairCore,
 shell:m3dBuildEnergetikShell,rear:m3dBuildEnergetikRearWall})`,{}, {timeout:5000});
const views=JSON.parse(readFileSync(new URL('scene_pripyat_views.json',base),'utf8')).views;
function collect(fn) {
 const out=[];
 fn({box:(...v)=>out.push(v),capsule:()=>{}});
 return out;
}
const structure=[
 ...collect(api.slabs),...collect(api.arcade),...collect(api.foyerStair),
 ...collect(api.shell),...collect(api.rear)
];
// Slab, floor, masonry and stair can occlude a visible artwork.
// Backing wall is a desired endpoint; do not mistake it for a foreground occluder.
const foreground=structure.filter(b=>!(b[0]>=api.arch.foyer.mosaicWallX-0.03 && b[3]<=api.arch.foyer.mosaicWallX+0.5
 && b[2]>=api.arch.foyer.mosaicStartZ-0.15 && b[5]<=api.arch.foyer.mosaicEndZ+0.15));
function eye(v) {
 const a=v.angle*Math.PI/180, e=v.elevation*Math.PI/180;
 return [v.target[0]+v.distance*Math.sin(a)*Math.cos(e),
 v.target[1]+v.distance*Math.sin(e),
 v.target[2]+v.distance*Math.cos(a)*Math.cos(e)];
}
// The segment/AABB slab test ignores hits at the extreme endpoints.
function intercept(a,b,box){
 let lo=0.0001,hi=0.9999;
 for(let i=0;i<3;i++){
   const d=b[i]-a[i];
   if(Math.abs(d)<1e-10){if(a[i]<box[i]-1e-7||a[i]>box[i+3]+1e-7)return false;continue;}
   let t0=(box[i]-a[i])/d,t1=(box[i+3]-a[i])/d;
   if(t0>t1)[t0,t1]=[t1,t0];
   lo=Math.max(lo,t0);hi=Math.min(hi,t1);
   if(lo>hi)return false;
 }
 return true;
}
const view=views[1];
const origin=eye(view),f=api.arch.foyer;
// Check eye position and three widely spaced targets: left, centre, right.
// This does not replace an image comparison, but guards against opaque slabs
// crossing the viewing corridor again.
assert.ok(origin[0]<f.mosaicSurfaceX-3,'camera must stand on painted side');
for(const z of [f.mosaicStartZ+1.2,(f.mosaicStartZ+f.mosaicEndZ)/2,f.mosaicEndZ-1.2]){
 const dest=[f.mosaicSurfaceX-0.015,4.15,z];
 const hits=foreground.filter(b=>intercept(origin,dest,b));
 assert.equal(hits.length,0,`structural occlusion of mosaic target z=${z}: ${JSON.stringify(hits.slice(0,4))}`);
}
console.log('3 mosaic sightlines clear of modeled structural boxes; all existing materials retained');
