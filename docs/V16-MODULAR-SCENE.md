# V16 — modular scene dispatch (initial implementation)

Branch: feature/v16-modular-scene-dispatch, based on integration/moteur3d-unified-v15.

## Current scope
- A versioned scene.json manifest lists sectors and separately stored geometry, instances, materials, PBR, lights, cameras and environment resources.
- tools/v16_scene_dispatch.py resolves references relative to the scene root, rejects escaping paths, duplicate sector identifiers, missing files and malformed JSON.
- Run: python tools/v16_scene_dispatch.py scenes/v16/pripyat/scene.json
- Optional sector selection: --sector energetik
- The Energetik example intentionally contains **empty placeholders**, not reconstructed assets.

## Next integration work (not yet implemented)
1. Export real geometry, material IDs, UVs, normals, texture paths and transforms from scene_pripyat_central.js / export_scene.js without changing the existing scene.
2. Add C++17 manifest loader with a JSON parser dependency and typed runtime resources, integrated into m3d_scene.cpp and the Qt scene-open menu.
3. Map material IDs to separate PBR assets and supply identical resolved scene data to CPU and D3D11 Compute.
4. Add sector streaming, caching, dirty flags and resource lifetime handling.
5. Preserve .bin V3/V4/V5 loading as legacy fallback.
6. Add golden-image parity tests and Windows Qt6 CI build.

This commit is a **first functional validator**, not a completed V16 engine or compiled Windows binary.
