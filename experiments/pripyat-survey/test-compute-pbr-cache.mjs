// Static integration guards; hardware rendering is validated separately.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const root=new URL('../2026-10-30-cote-ete-gt/native_v14/',import.meta.url);
const cpp=readFileSync(new URL('src/m3d_compute11.cpp',root),'utf8');
const shader=readFileSync(new URL('src/shaders/m3d_raytrace_compute.hlsl',root),'utf8');
assert.match(cpp,/static thread_local ComputeCache cache/,'Compute must persist scene/device resources');
assert.match(cpp,/sceneKey==scene\.obj\.data\(\)/,'GPU cache invalidation absent');
assert.match(cpp,/DXGI_FORMAT_R8G8B8A8_UNORM/,'readback must use 8-bit target');
assert.match(cpp,/DXGI_FORMAT_R8_UNORM/,'roughness atlas format missing');
assert.match(cpp,/atlas\(DXGI_FORMAT_R8G8B8A8_UNORM,subColors,srv\[4\]\)/);
assert.match(cpp,/atlas\(DXGI_FORMAT_R8G8B8A8_UNORM,subNormals,srv\[5\]\)/);
assert.match(cpp,/atlas\(DXGI_FORMAT_R8_UNORM,subRoughs,srv\[6\]\)/);
assert.match(shader,/Texture2DArray<float4> AlbedoAtlas : register\(t4\)/);
assert.match(shader,/Texture2DArray<float4> NormalAtlas : register\(t5\)/);
assert.match(shader,/Texture2DArray<float4> RoughAtlas : register\(t6\)/);
assert.match(shader,/hitUV=o\.uv0\.xy/); // prevent interpolating from uninitialised UV
assert.match(shader,/AlbedoAtlas\.SampleLevel/);
assert.match(shader,/NormalAtlas\.SampleLevel/);
assert.match(shader,/RoughAtlas\.SampleLevel/);
assert.match(shader,/float3 sky=lerp\(skyHorizon,skyZenith,t\)/);
assert.match(shader,/right\*uv\.x\*tanHalfFov-up\*uv\.y\*tanHalfFov\/aspect/);
console.log('Compute D3D11 PBR atlas + cached GPU resource source guards passed');
