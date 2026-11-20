# V17 / LumaTrace BVH4 backend test

Branch: `codex/v17-lumatrace-backend-test`  
Base: V17.603 (`d50ba673187c80ddb41f78674575ae079bcbab4b`)

## First integration slice

`m3d_lumatrace_bvh4` collapses V17's already-built binary BVH into the same
56-byte four-child node layout consumed by LumaTrace's HLSL. BVH leaves retain
their ranges into `Scene::order`. This slice does not create `GpuTriangle`
records or copy vertex positions, normals, UVs, or materials.

The adapter currently accepts triangle-only scenes. V17 analytic primitives
must first be tessellated by the existing scene-to-raster path, or the backend
must reject the scene with a clear status. It must never silently omit them.

## Memory model

The additional CPU build workspace is one `uint64_t` subtree-count and one
byte traversal state per V17 BVH node. The output is one 56-byte BVH4 node per
collapsed node. Triangle storage and the CPU primitive-order array remain in
the V17 `Scene`; GPU integration may upload the existing order references, but
must not upload LumaTrace's 128-byte-per-triangle `GpuTriangle` structure.

## Remaining renderer work

1. Add a D3D11 compute pass that reads the V17 raster geometry buffer and BVH4
   nodes, then stores per-triangle light results.
2. Bind those results in the V17 pixel shader and keep the existing raster,
   textures, window, and input path.
3. Add the selectable `LumaTrace BVH4 — test` backend and report its build,
   dispatch, and frame timing separately.
4. On first selection, turn off the cloud option for this backend; leave the
   existing checkbox enabled so the user can turn clouds back on to compare.

This branch is experimental. The adapter has not yet been exercised by the
renderer because the D3D11/HLSL integration is a separate step.
