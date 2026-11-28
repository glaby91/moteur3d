# Z Escargot 006 Resident (experimental)

Base: Profile005 3d22f9f, exact scene V17.634. No changes to main. Limits unchanged: 4 receivers, 768 BVH queries, 120000 triangles, 160 sources per receiver. Authored permissions, source ranking, overflow rollback, transparency ordering and optical shader material policy preserved.

Six-pass preflight:
1. Inspect CPU/GPU hot path; cache exact reflected AABBs and plane-side extrema by receiver patch/source, capped at 16 MiB payload per receiver; oversized sources use the old path.
2. Group 32 consecutive triangles. Reject only if the whole union is outside one padded footprint halfspace; keep the per-triangle projected AABB test and original order. Synthetic axis-aligned camera simulation: 20000 groups, 640000 triangles, 18311 groups rejected, 0 false rejections (2064 near-plane groups, 9 camera-inside groups). This is a model of the math, not a compiled engine test.
3. Move the camera clip projection to VS; retain the original near/far constants and clip-distance sign. Opaque depth calculation removed; transparency still sorts by original depth.
4. Reuse CPU vertex payload only when receiver patch, lighting epoch, side and exact ordered triangle IDs match. Reuse frame vector allocation; rebuild selection each frame to avoid stale visibility.
5. Keep one GPU vertex buffer per layer; exact byte comparison, upload changed layers only; invalidate on scene rebuild or device change. No cached optical image reused when camera changes. This is not yet a fully persistent per-object GPU index buffer; changed layers still upload. GPU format remains 40 bytes/vertex.
6. Fix process-wide Z Reflect toggle (Win32 environment); profile verifies disabled modes and logs flag, exterior prefers named garden/facade preset, hardware samples increased to 16 measured frames. Exterior optical activity must still be checked in report. WARP is not hardware performance.

Remaining: full visual parity under motion, fine detail temporal stability (source selection policy intentionally unchanged in this first parity-focused optimization), actual GPU/CPU performance and compilation must be verified on CI and hardware. No new FPS claims.

Follow-up guard: camera-inside union cannot be culled (legacy full-screen behavior). Added compiled C++ parity test for 320000 random AABBs using real scene camera; optical integration smoke exercises water geometry signal, static cache, moving view, native presentation and restoration before profiling. Results pending CI. The first preliminary run is superseded by this revision.

## Profiling failure 46151753009
Compiled engine and optical smoke passed: 320000 actual C++ footprint tests, 6011 water geometry signal pixels, 73 rays and 7218 triangles; native swapchain zero readback. Report contains 432 completed renders, 72 flagged solely because base-off retained one active water plane. RuntimeOptics::select(..., glassEnabled=false) intentionally retains water; do not change the production toggle. Add separate process-local CSE_WARP_ESCARGOT_PROFILE_BASE_OFF flag, bypass selector only for true all-optics-off benchmark; strict telemetry gate retained, smoke covers isolation and restores the normal toggle. Profiler stderr logging made visible. No changes to optical geometry path.
Preliminary WARP 960x540 moving MSAA1, same curtain camera: planner 100.45 to 39.39 ms, cull 74.78 to 22.15 ms, triangles 119998 to 119998, total wall 696.34 to 626.09 ms. This is not hardware FPS. Failed baseline must be rerun; changed exterior camera prevents a direct exterior comparison.
