# Moteur3D V16 — Windows / Pavillon 01

## Scope
- **Windows x64 only:** Qt 6.8.3, MSVC 2022, CMake, GitHub Actions.
- Pavillon 01 is the **only V16 development scene**.
- Legacy binary loader is removed from the V16 compilation target. Previous sources and branches remain in Git history, and any later Pripyat conversion will be a separate utility.
- No Linux build or packaging is targeted for V16 at this stage.

## Implemented (committed, not yet build-confirmed)
- Modular JSON manifest, resource loader, traversal checks.
- External geometry/instances/material/PBR/cameras/lights/environment resource files.
- Hierarchical scene nodes and Qt6 scene/resource browser, including material PBR fields.
- JSON box primitives -> **84 render triangles** with bounding volume hierarchy (BVH).
- CPU render scene startup and scene switching from V16 JSON.
- Triangle-only Compute backend can consume the same geometric BVH (materials/lighting are not yet equivalent to CPU).
- Procedural PBR *preview recipes*, separate from objects, generate albedo/normal/roughness textures at 2048 px with full 12-level mip chains. **External PBR image files and true material authoring still remain.**
- Windows-only GitHub Actions workflow on `feature/v16-modular-scene-dispatch`, including native Qt tests and packaging.
- CTest assertions for geometry, material IDs, valid BVH, 2048->1 mip chains and path traversal rejection.

## Remaining
1. Confirm MSVC GitHub Actions build is successful; inspect CI logs for compiler/runtime errors.
2. Build detailed architectural geometry: rooms, actual wall thicknesses, door/window cutouts, stairs, furniture, landscape and water geometry.
3. Add actual image import for albedo, normal, roughness, metallic, ambient occlusion, height and emissive; implement persistent compressed texture cache.
4. Add physically coherent material shading, proper texture color-space handling and tangent-space mapping in CPU and Compute.
5. Confirm Windows packaged executable opens Pavillon 01, displays triangles and materials; collect reference screenshots and benchmark metrics.
6. Add scene edits through Qt controls, dirty tracking and independent resource reload.

## Development links
- Branch: https://github.com/glaby91/moteur3d/tree/feature/v16-modular-scene-dispatch
- Windows Actions workflow: https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v14-qt.yml

## Local Windows build
```powershell
python tools/v16_scene_dispatch.py scenes/v16/pavillon01/scene.json
cd experiments/2026-10-30-cote-ete-gt/native_v14
cmake -S . -B build-v16 -G "Visual Studio 17 2022" -A x64
cmake --build build-v16 --config Release --parallel
ctest --test-dir build-v16 --build-config Release --output-on-failure
cmake --install build-v16 --config Release --prefix dist-v16
windeployqt --release --compiler-runtime --dir dist-v16 dist-v16/moteur3d_qt.exe
.`dist-v16/moteur3d_qt.exe`
```

Build and runtime are *not yet independently verified* in this session.
