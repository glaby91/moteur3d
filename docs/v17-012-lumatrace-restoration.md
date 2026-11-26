# V17.012 — LumaTrace restoration candidate

Base: 23a313192086f54f8c810464f56df6b31caf2790; historical algorithm: ecff1714711b364ad233f9204dc36bf75a37645d (V17.002 / LumaTrace-008). The successful user route30 ZIP does not contain the executable SHA, so the precise identity of its separate benchmark binary is not independently proven.

## Three static review passes
1. Geometry: preserve static preparation and original primitive ordering, raw packed vertices (32 bytes), optional R32_UINT index SRV at compute t7, BVH4 unchanged. Constant layout 3184 bytes (HLSL/C++). CPU preparation untouched.
2. Lighting: restore direct triangle cache plus 12x5x12 two-stage probe volume; 2 coarse and up to 4 refinement rays per probe. Clear non-refined cells by writing zero; separate coarse/final UAVs, unbind compute SRVs/UAVs between passes; invalidate on scene/geometry replacement or light-state change. Raster uses sun and adds probe radiance only in LumaTrace hybrid mode. Existing DX11/Vulkan paths retain their previous behavior. Historical light selection is restored, including authored lights and day/night scaling; prior synthetic emitter proxies are not carried over in this stabilization candidate.
3. Regression: GUI and walk-controller sources untouched (Ctrl, Space, Tab / Less). Existing packaged controls test and new WARP indexed/non-indexed cold/warm probe parity test. No new threads, no dispatch batching, no change to route30. Physical GPU confirmation still required.

## Validation
CI must pass before hardware testing. The historical route30 protocol remains unchanged and may run with hybrid lighting disabled; also check interactive LumaTrace with the 3D indirect lighting option enabled. Verify the pool at day/night, refraction and reflected light, native presentation and all three backends. No automatic merge.

## Backlog — after LumaTrace is stable
- CPU ray tracing: BVH4/BVH8 comparison, SIMD wide-node traversal, exact intersections and opaque any-hit shadow queries; assess TLAS/BLAS and instancing. Keep original CPU reference, compare images and performance first. Later adaptive secondary rays, diffuse radiance cache and multi-bounce quality.
- LumaTrace: optional central ray (default in future triangle mode), 2 (center + deterministic vertex) or 4 (center + 3 vertices) sample origins per triangle. Future local cache across neighboring coplanar triangles, based on position, geometric normal, ray direction and light state; separate incident radiance from material response. No neighbor reuse or new per-triangle secondary-ray mode enabled in this restoration candidate.
