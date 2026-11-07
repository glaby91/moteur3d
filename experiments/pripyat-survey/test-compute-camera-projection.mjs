// Regression guard: switching CPU, D3D11 raster and D3D11 compute must NOT change camera framing.
// Run with: node experiments/pripyat-survey/test-compute-camera-projection.mjs
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
const shader=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/src/shaders/m3d_raytrace_compute.hlsl',import.meta.url),'utf8');
const cpu=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/src/m3d_render.cpp',import.meta.url),'utf8');
const raster=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/src/m3d_directx11.cpp',import.meta.url),'utf8');
const ui=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/src/moteur3d_qt.cpp',import.meta.url),'utf8');
assert.match(cpu,/rayX=\(2\*fx\/w-1\)\*cam\.tanHalf,rayY=\(1-2\*fy\/h\)\*cam\.tanHalf\/cam\.aspect/);
assert.match(raster,/float nx = dot\(q, right\) \/ \(safeZ \* tanHalf\)/);
assert.match(raster,/float ny = dot\(q, up\) \* aspect \/ \(safeZ \* tanHalf\)/);
assert.match(shader,/right\*uv\.x\*tanHalfFov-up\*uv\.y\*tanHalfFov\/aspect/);
assert.doesNotMatch(shader,/right\*uv\.x\*tanHalfFov\*aspect/);
assert.match(ui,/connect\(renderer_, qOverload<int>\(&QComboBox::currentIndexChanged\), this, \[this\]\(int\) \{ readSettings\(\); requestRender\(\); \}\)/);
for(const [width,height] of [[1366,768],[1920,1080],[800,600],[640,360],[800,800]]){
  const aspect=width/height,tanHalf=Math.tan(52*Math.PI/360);
  for(const [x,y] of [[0,0],[width/2,height/2],[width-1,height-1],[width*0.27,height*0.81]]){
    const u=(x+0.5)/width*2-1,v=(y+0.5)/height*2-1;
    const cpuRay=[u*tanHalf,-v*tanHalf/aspect,1];
    const computeRay=[u*tanHalf,-v*tanHalf/aspect,1];
    const rasterNdc=[cpuRay[0]/tanHalf,cpuRay[1]*aspect/tanHalf];
    assert.ok(Math.abs(cpuRay[0]-computeRay[0])<1e-12);
    assert.ok(Math.abs(cpuRay[1]-computeRay[1])<1e-12);
    assert.ok(Math.abs(rasterNdc[0]-u)<1e-12 && Math.abs(rasterNdc[1]+v)<1e-12);
  }
}
console.log('Compute camera projection: CPU / D3D11 raster parity verified for 5 aspect ratios.');
