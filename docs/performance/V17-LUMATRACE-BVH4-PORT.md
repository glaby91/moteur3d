# V17 / LumaTrace BVH4 backend test

Branch: `codex/v17-lumatrace-backend-test`  
Base: V17.603 (`d50ba673187c80ddb41f78674575ae079bcbab4b`)

## Current integration

`m3d_lumatrace_bvh4` collapses V17's already-built binary BVH into the same
56-byte four-child node layout consumed by LumaTrace's HLSL. It runs once at
scene load and is stored on `v16::Runtime` for later backend selection. BVH
leaves retain their ranges into `Scene::order`. This slice does not create
`GpuTriangle` records or copy vertex positions, normals, UVs, or materials.

The renderer now has a selectable `LumaTrace BVH4 — test (éclairage GPU)` path.
It reuses the V17 D3D11 device, scene raster path, swapchain or QImage output,
and the existing V17 material textures. A compute shader traverses the BVH4
for two opposing, orbiting colored point lights and writes one radiance value
per opaque triangle. The lights stay within two metres of the scene center.
The pixel shader adds that light to the normal V17 raster result. The compute
pass runs again when the frame is rendered; the BVH is still built only once
when the scene runtime is created.

Triangle-only scenes are still required. The backend returns an explicit
error for analytical primitives instead of silently dropping them. On first
selection, the cloud checkbox is switched off; the user can re-enable it to
compare. It remains the same V17 checkbox and does not change the LumaTrace
renderer or its BVH.

## Memory model

The additional CPU build workspace is one `uint64_t` subtree-count and one
byte traversal state per V17 BVH node. The output is one 56-byte BVH4 node per
collapsed node. Triangle storage and the CPU primitive-order array remain in
the V17 `Scene`; GPU integration may upload the existing order references, but
must not upload LumaTrace's 128-byte-per-triangle `GpuTriangle` structure.

## Current limits and validation

This implementation keeps the original vertex stream rather than building
LumaTrace's 128-byte `GpuTriangle` records. The compute pass and raster pass
share that stream through a D3D11 structured-buffer view. The BVH node layout
is 56 bytes and leaves refer through the uploaded primitive-order indirection.

The branch remains experimental pending a Windows D3D11 build and a visual
comparison on the Porsche GLB and a larger V17 scene. This Linux workspace has
no CMake executable or Windows D3D11/HLSL compiler, so those runtime checks
cannot be claimed here. The V17 campaign benchmark still reports BVH4 build
time and scene support independently from frame time.
