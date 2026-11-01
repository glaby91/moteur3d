# 1970s Coupe

- Original model: [Grab3D 1970s Coupe](https://grab3d.com/models/vehicles/1970s-coupe/)
- License: CC0 1.0 (the source model page identifies the model as CC0; no attribution is required).
- Original GLB: `1970s-coupe.glb` (18,978 triangles, 2K PBR source textures).
- Native export uses the model's positions, vertex normals, UV0, body base-colour texture and tire base-colour texture. Metallic/roughness/normal maps from the GLB are retained in the original file but the current native scene format does not consume them.
- Placement and scale are set in `scene_coastal_v1314.js`; conversion to native triangles is handled by `gltf_scene_import.js`.

Run `python prepare_textures.py` with Pillow installed to regenerate the RGB565 mip chains used by the Windows build.
