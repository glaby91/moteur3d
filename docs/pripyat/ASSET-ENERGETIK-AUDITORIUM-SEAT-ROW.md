# Energetik auditorium seat row — first asset pass

## Delivered

- `experiments/2026-10-30-cote-ete-gt/native_v14/assets/pripyat/energetik_cinema_seat.js` — reusable single-seat builder with separable frame, backrest, hinged seat pan and side panels.
- `scene_energetik_seat_preview.bin` — native Moteur3D V14 scene with three seat instances and one continuous painted-steel mounting rail.
- `assets/pripyat/energetik_cinema_seat_row.glb` — GLB 2.0 static row for Unreal Engine, 125 authored primitives and four material groups.
- `scene_energetik_seat_preview.assets.json` — seat components, pivots, poses and physics-profile placeholders.
- `assets/pripyat/textures/seats/` — procedural, non-photographic 512 px base-color, tangent-normal and metallic-roughness maps for brown vinyl, painted support steel, dark steel and wood trim.

## Three reference-supported states

The row retains the same padded-side construction across all three seats. The left seat has a missing seat pan, the center seat has its pan raised, and the right seat has its pan lowered. Their steel mountings connect to one continuous cross rail. This follows the supplied three-seat reference and avoids presenting unverified alternative furniture designs as documented Energetik objects.

The builder can represent other side-panel constructions through an explicit option for later, separately referenced assets. Those alternatives are not instantiated in this row.

## Materials and scale

All dimensions are in metres. Brown vinyl is authored with subdued scuffs and shallow wear; the green support-paint tile has sparse chips exposing a dark substrate. The steel, wood and vinyl tiles are procedural and are not derived from documentary photographs. The GLB contains the PBR maps; the native scene uses the engine's existing named materials and embedded texture path.

Base color, tangent normal and metallic-roughness maps are embedded in the GLB. The exposed steel hardware uses a metallic factor; painted support steel, upholstery and wood remain dielectric. Material colors, roughness and the assumed wood/vinyl identification still require comparison against the original high-resolution H03/user seat photographs.

## Validation state

- Native scene exporter completed: 126 scene objects including the neutral floor (125 model objects) and two preview lights; the scene file is about 40 MiB.
- GLB generation completed with four materials and embedded maps. It is a static mesh: the native scene format does not yet animate the seat-pan hinge.
- The GLB has not been imported into Unreal Editor, and the Windows Qt preview has not been captured in this environment. Final visual match and material response remain open.

## Rebuild

From `experiments/2026-10-30-cote-ete-gt/native_v14`:

```sh
python3 tools/generate_seat_maps.py
node tools/export_energetik_seat_row_glb.js
M3D_SKIP_IMPORTED_CAR=1 node export_scene.js source_v1314.html scene_energetik_seat_preview.bin scene_energetik_seat_preview.js
node export_energetik_seat_manifest.js scene_energetik_seat_preview.assets.json
```
