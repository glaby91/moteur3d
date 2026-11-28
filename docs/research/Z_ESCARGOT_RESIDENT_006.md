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
