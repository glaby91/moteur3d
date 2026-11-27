# CSP001 / Z-Reflect R001 — stage 1 (uncompiled)
Base V17.016 build 772. Staged source, no CI requested. Adaptive 0/1 unchanged (OFF by default). 
## Implemented
- Shared scene loader reads an optional portable CSP001 binary BVH2 + primitive order + BVH4 sidecar, otherwise rebuilds normally. Exact generated geometry key, payload checksum, bounded lengths, permutation and both tree coverage/bounds checks, transactional commit. This does not bypass geometry generation or texture preparation. Same producer/consumer toolchain recommended; different floating-point results safely miss.
- Offline CLI uses the real scene loader and writes the BVH sidecar and an optical candidate inventory, grouping triangles by approximate plane, verifying membership and assigning logical dormant camera/buffer slots. Candidate roughnessScale is a multiplier, not actual roughness; F0 is dielectric only. No GPU buffers allocated. Inventory is diagnostic, may split coplanar patches or omit single-triangle surfaces, not yet a render input.
- Existing horizontal water reflection: conservative AABB frustum rejection, preserving the actual mirror rendering and material shader. No occlusion query, no brightness/size rejection, no new reflection for other materials. Unknown bounds fall back to ON. Bounds cover every water triangle in the raster mesh. Reflection may be computed unnecessarily for multiple water bodies; this is intentional.
## Next stages (not implemented)
Production-grade planar clustering and zone dependency tables, multi-plane shader and dedicated/pool GPU buffers, async lighting snapshots and cache invalidation, indexed geometry/compact albedo/group bounds/probe tables. These must be versioned independently and verified for actual image parity. Do not confuse camera reflection with refraction. A separate scene packaging script will invoke the offline CLI after final scene patches, before publishing the ZIP.
## Review
1. Keep BVH2, BVH4 and primitive order together; no raw C++ structure dumps.
2. Hash exact positions/bounds in chunks, avoid per-triangle heap allocation; reject malformed files before allocation; transactionally preserve fallback.
3. Check permutation, tree connectivity, leaf coverage, bounds and depth; no recursive untrusted traversal.
4. Water culling uses conservative frustum only, expanded sides; no occlusion or material threshold guesses.
5. Preserve existing shaders, backends and default adaptive state. No GPU resources on worker thread; generic inventory is diagnostic only. Real Windows compile, scene load, cache hit/miss and image parity still required.

## A/B and verification
Set M3D_CSP001=0 to force local BVH reconstruction. Cache-hit progress label at 99%; bvhBuildMs includes read/validation, lumaTraceBvhBuildMs is zero on hit. Cache cap 128 MiB; oversized or incompatible scenes fall back. Added C++ smoke test for roundtrip, geometry mismatch, corruption and portrait projection (not compiled). Second review corrected horizontal FOV convention to exactly match HLSL, default dielectric candidate flood and legacy water sentinel. No claims of GPU timings or real-scene coverage.
