# V16 — Native Qt6 modular scene loader

Implemented: `m3d_scene_v16.h/.cpp` reads `scene.json` and its independent geometry, instance, material, PBR, lighting, camera and environment JSON resources. It validates the manifest version, duplicate sectors, malformed resources and paths escaping the scene root. It returns a staged `SceneDatabase` and an explicit error, leaving replacement of the active scene to the caller.

**Scope:** Pavillon 01 only. The original binary scene loader is not called by this module. It remains in the existing executable until the new runtime geometry and render dispatch are complete. No claim of compiled or rendered V16.

Next: convert JSON primitives into render meshes; connect menu open to this loader; build material/texture cache with mipmap filtering; replace legacy entrypoint after parity tests.
