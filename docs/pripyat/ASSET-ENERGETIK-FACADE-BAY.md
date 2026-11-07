# Energetik façade bay — asset quality pass 01

A reusable, open façade bay for the Palace of Culture Energetik. This is a focused first asset from the indexed catalogue, built from the existing photo mapping and material definitions. It is not a photogrammetric scan: the bay width and height are approximate dimensions carried over from the existing scene blockout.

## Delivered files

- `assets/pripyat/energetik_facade_bay.js` — shared native geometry builder; 26 concrete relief pieces.
- `scene_pripyat_facade_bay.bin` — isolated scene exported through the current V14 scene exporter, with the concrete PBR mip chain embedded.
- `assets/pripyat/energetik_facade_bay.glb` — GLB 2.0 static mesh for Unreal Engine, in metres, 4,984 triangles, with embedded base color, tangent normal, and metallic-roughness maps.
- `assets/pripyat/textures/` — source and processed 512px maps, including the native compressed mip chain `.m3dpbr.gz`.
- `assets/pripyat/energetik_facade_bay.asset.json` — dimensions, stable material ID, texture provenance, and validation state.

## Material mapping

The catalogue ID remains `prp_concrete_panel` in the GLB and manifest. The existing native engine does not yet accept arbitrary `prp_*` IDs, so the scene adapter maps it to the already-defined native `concrete` material (2% reflectivity). The material is non-metallic. No metallic value is inferred from reflectivity. Target roughness is 0.78–0.94; the map average is approximately 0.87.

The tangent normal is a subtle approximation derived from the generated albedo luminance. The roughness and normal maps are authored maps, not measured scan data. The 512px tile is suitable for this asset scale and current native scene format.

## Unreal import

Import `assets/pripyat/energetik_facade_bay.glb` as a static mesh. Enable material and texture import; the GLB embeds its PBR maps. Set the asset scale to 1.0 because coordinates are metres. The manifest preserves `prp_concrete_panel` as the project material ID.

## Rebuild

From `experiments/2026-10-30-cote-ete-gt/native_v14`:

```sh
M3D_SKIP_IMPORTED_CAR=1 M3D_FACADE_PBR=1 M3D_TEXTURE_SIZE=512 node export_scene.js source_v1314.html scene_pripyat_facade_bay.bin scene_pripyat_facade_bay.js
```

To regenerate texture maps, use Node.js with `sharp` installed and run `node tools/generate_concrete_maps.js`. Then run `node tools/export_facade_glb.js`; the writer has no runtime dependencies and writes the GLB under `assets/pripyat/`.

## Scope and validation

This pass delivers one high-detail architectural asset instead of filling all 20 catalogue families with rough proxies. GLB structure, embedded images, winding and normals were checked. The native scene exported successfully. Unreal Editor and the native Qt executable are not available in this build environment, so imports and final in-engine image comparison remain open validations.
