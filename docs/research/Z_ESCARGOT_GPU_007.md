# Escargot 007 GPU block residency (experimental)
Parent 006: 1967b91a11ed92ed4bdebe9d3949cd60de839f95. Do not merge to main. No automatic compilation on this branch.
## Implementation
CPU retains receiver discovery, object ranking and conservative block AABB checks. Fixed discovery phase, deterministic ranking; no timed refresh at identical camera. Build contiguous opaque blocks of up to 32 triangles at scene load; transparent blocks of one triangle keep exact CPU depth sorting. Preserve whole-source budget admission from 006, with 120000 actual submitted triangles max. GPU immutable original vertex stream (40 bytes per vertex, additional copy for phase 1); compact 8-byte block descriptors uploaded only when changed. VS performs reflection, original-plane clip and previous approximate daylight/shadow lighting; rasterizer performs frustum/scissor/depth clipping. No per-frame CPU reconstruction of opaque geometry; no CPU readback added. 75% per dimension target cap and normalized sampling; adaptive LOD and actual reuse of main mesh are future work. Padded opaque instances may submit extra rejected vertices; new telemetry tracks this. Dynamic illumination cache invalidation tracks shadow upload revision, transform and daylight/sun. Local-light shading remains the 006 approximation, not new physical lighting. Object selection deterministic at identical configuration, but strict persistence across changing camera and adaptive LOD not yet implemented; budgets may omit objects. Moving objects require geometry revision. Test 006 vs 007 with same scene/view; 007 uses 75% target, so not a pure same-resolution A/B.
## Five preflight passes (synthetic JS model / static inspection, not compiled C++ or HLSL)
- 1 - block packing and material exclusions: {"objects":10000,"triangles":1847222,"blocks":503298,"submittedWithPadding":9895368}
- 2 - mirror/plane clip and projection: {"samples":100000,"maxError":1.4210854715202004e-13}
- 3 - budget, alpha ordering and deterministic admission: {"cases":10000,"rollbacks":921966}
- 4 - reduced target, scissor and pixel address: {"rectangles":100000}
- 5 - integration contracts, cache and buffer bounds: {"staticContracts":12,"manualWorkflow":true}
## Pending
Compile actual DX11 shaders and smoke on WARP, then hardware HD620 A/B. No proof of frame-rate gain or image fidelity until those tests. The workflow is manual only on this branch; no build requested in this step.

## Follow-up preflight: bound real GPU work, not just useful triangles
Short opaque runs were initially padded to 32 triangles, causing a 5.36x expansion on mixed synthetic data. Bucket runs by next power of two (1/2/4/8/16/32), at most six opaque draws per receiver, preserving transparent per-triangle depth order. Charge padded cost against the 120000 total submission cap; runtime rejects excess. Stable sorting by bucket can change equal-depth opaque tie ordering; validate on actual scene. Metrics expose planner_submitted_triangles and resident_submitted_vertices. Repeated simulation after correction:
- {"pass":"1","objects":10000,"actual":1847222,"blocks":503298,"oldPadded":9895368,"newPadded":2360815,"overhead":1.2780353417185373}
- {"pass":"2","points":100000,"maxError":1.4210854715202004e-13}
- {"pass":"3","cases":10000,"rollbacks":3614219,"maxWork":120000}
- {"pass":"4","rectangles":100000}
- {"pass":"5","cases":10000,"instances":2509370,"staticChecks":3}
No real DX11 execution performed yet.