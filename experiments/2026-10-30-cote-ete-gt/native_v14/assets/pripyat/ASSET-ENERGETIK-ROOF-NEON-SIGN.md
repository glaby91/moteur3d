# Energetik rooftop neon sign — H05

Reusable source: `energetik_roof_neon_sign.js`  
Unreal interchange: `energetik_roof_neon_sign.glb`  
Manifest: `energetik_roof_neon_sign.asset.json`

## Construction

The source builds the Cyrillic title **ЭНЕРГЕТИК**, an open rusted-steel truss, painted rails, ceramic insulators, wiring and two transformer housings. The tubes face `+Z`; dimensions and coordinates are in metres.

Each letter is an independent lighting circuit. Glass envelopes and emissive neon cores are both cut into short sections along their strokes. At a 0.28 m section length the current assembly exports 518 separately named glTF nodes: 171 emissive core pieces, 171 glass sections, plus the support, wiring and hardware. The scene instance is placed over the central Energetik entrance and adds one cyan point light above each letter.

## Use

The same `buildEnergetikRoofNeonSign` function feeds the native Pripyat scene and the GLB exporter. The GLB retains one node per authored part, material assignments, metre scale, letter IDs and emissive material strength. A downstream renderer can hide, dim or animate a single letter or tube section without rebuilding the sign.

The façade bay and padded cinema seat row remain separate reusable assets. `tools/export_energetik_hero_showcase_json.js` assembles those assets with the sign for the reproducible preview; `tools/render_energetik_hero_showcase.py` draws the geometry without changing the model.

## Accuracy

This is a first geometric reconstruction from the H05 reference. Letter spacing, frame dimensions, tube path and support details are approximations for the current scene scale. Emissive rendering and import behavior should be reviewed in the native application and Unreal Editor before treating the result as a measured replica.
