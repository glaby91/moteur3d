# V16 — Pavillon 01 only

The V16 development scene is now **scenes/v16/pavillon01/scene.json**.
Pripyat remains archived and unchanged; do not migrate or load it in V16 acceptance tests.

The first Pavillon 01 files contain *conceptual box volumes*, scalar materials, instance transforms, daylight and camera definitions. They are not yet a full architectural mesh or a rendered/compiled scene.

Next:
1. Wire a native C++17 JSON manifest/resource loader into m3d_scene.cpp and Qt open-scene.
2. Build actual walls, apertures, stairs, rooms, roof and exterior geometry with real UVs and normals.
3. Produce 2048px PBR texture assets and map material IDs to texture sets.
4. Implement identical scene interpretation in CPU and Compute backend.
5. Add validator tests, CPU/GPU reference renders and Windows Qt6 build checks.

Validation command:
python tools/v16_scene_dispatch.py scenes/v16/pavillon01/scene.json --sector pavillon01
