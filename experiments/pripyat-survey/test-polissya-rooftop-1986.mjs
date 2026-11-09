/** Polissya 1986: genuinely OPEN panoramic roof, not a solid 3D cube.
 * Rear windows must have real glazing, and the roof sign must be above the
 * terrace cornice. No assumption about whether the café opened before 1986.
 */
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
const src=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const api=vm.runInNewContext(src+`({
  model:M3D_POLISSYA_MODEL,period:M3D_PRIPYAT_FACADE_PERIOD,
  build:m3dBuildPolissyaLocal,block:M3D_PRIPYAT_BLOCKOUT
})`,{process:{env:{M3D_PRIPYAT_FACADE_PERIOD:'pre1986'}}},{timeout:5000});
assert.equal(api.period,'pre1986');
const boxes=[],capsules=[];
api.build({box:(...x)=>boxes.push(x),capsule:(...x)=>capsules.push(x)});
const m=api.model, terraceX0=m.x1-14.0,roofY=m.height,terraceY=roofY+3.4;
const within=(b,x,y,z)=>b[0]<x && x<b[3] && b[1]<y && y<b[4] && b[2]<z && z<b[5];
const solid=boxes.filter(b=>['concrete','plaster','stone','brick','wood'].includes(b[6]));
assert.ok(boxes.some(b=>b[4]>terraceY+.20 && b[3]-b[0]>12 && b[5]>m.z1),
 'Full-width loggia canopy missing');
assert.ok(!solid.some(b=>b[1]<=roofY+.05&&b[4]>=terraceY-.1&&b[3]-b[0]>5&&b[5]-b[2]>2),
 'Large solid 3-m-tall roof box reintroduced');
const centers=Array.from({length:4},(_,i)=>terraceX0+(i+.5)*14/4);
for(const x of centers){
 for(const z of [m.z0+1.20,m.z1-.19]){
  const y=roofY+2.15;
  assert.equal(solid.filter(b=>within(b,x,y,z)).length,0,
    `Roof loggia opening blocked at x=${x}, z=${z}`);
 }
}
const posts=boxes.filter(b=>b[6]==='concrete' && b[1]>=roofY+.79 &&
 b[4]>=terraceY-.001 && b[4]<=terraceY+.01 && b[3]-b[0]<.55);
assert.ok(posts.length>=10,'Roof needs 5 piers on both open sides');
const highMetal=boxes.filter(b=>b[6]==='metal' && b[1]>terraceY+.3);
assert.ok(highMetal.length>=30,'Actual roof-edge sign must sit above terrace roof');
const rearWindows=boxes.filter(b=>b[6]==='glass' && Math.abs(b[2]-(m.z0-.1))<.001);
assert.ok(rearWindows.length>=16,'Rear facade not restored across floors');
const rearTransoms=boxes.filter(b=>b[6]==='metal' && b[2]<m.z0 && b[3]-b[0]>1.3);
assert.ok(rearTransoms.length>=16,'Rear glazing lacks horizontal metal frames');
assert.equal(api.block.housing.some(b=>b.id==='east-south'||b.id==='east-north'),false,
 'Imaginary high-rise appears next to Polissya');
console.log('Polissya 1986 panoramic open roof + rear glazing validated:',{
  openingsChecked:centers.length*2,piers:posts.length,roofSignStrokes:highMetal.length,
  rearGlass:rearWindows.length,rearTransoms:rearTransoms.length
});
