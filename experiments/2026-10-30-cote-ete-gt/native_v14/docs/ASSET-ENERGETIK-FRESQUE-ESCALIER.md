# Mosaïque du hall d’escalier Energetik

## Asset

- Texture panoramique 2D : `assets/pripyat/textures/energetik_fresco_mosaic.jpg` (2048 × 930, sRGB, ratio 2,20:1).
- GLB 2.0 réutilisable : `assets/pripyat/energetik_fresco_mosaic.glb`, plan texturé de 9,5 × 4,3 m, matériau diélectrique, rugosité 0,72.
- Scène native : un quad à UV explicites utilise l’albédo RGB888 avec mipmaps. Une normale plane et une rugosité élevée évitent de transformer l’image en métal brillant; le relief des tesselles reste visible dans l’albédo.
- Texture native produite par `tools/generate_energetik_fresco_map.py` au format `M3DPBR2`, chaîne 2048 → 8 puis complétée jusqu’à 1 × 1 par l’exporteur.

## Position dans la scène

Le panneau est placé sur le côté hall de la paroi près de l’escalier principal :

- position du quad : X = 8,12 m, Y = 2,00 m, Z = −41,55 m ;
- étendue : Z = −41,55 à −32,05 m et Y = 2,00 à 6,30 m ;
- normale : −X ; largeur orientée vers +Z ;
- identifiant de matériau : `energetikFresco`.

La position et les dimensions reprennent le blockout existant et l’estimation communiquée (9–10 m × 4–4,5 m). Elles restent à confirmer visuellement dans la scène. Le GLB local est orienté dans XY, face +Z ; pour la scène Pripyat, le placer à la position ci-dessus avec une rotation Y de −90°.

## Références et limites

L’image est une reconstitution générée à partir des quatre vues fournies dans cette conversation. Elle reprend les personnages, les formes industrielles, la palette lumineuse et l’apparence en tesselles; les zones sans photographie sont interprétées. Ce n’est ni une photogrammétrie, ni une mosaïque documentaire exacte. Les photos de référence ne sont pas copiées dans le dépôt.

## Rebuild

Depuis `experiments/2026-10-30-cote-ete-gt/native_v14` :

```sh
python tools/generate_energetik_fresco_map.py
node tools/export_energetik_fresco_glb.js
M3D_SKIP_IMPORTED_CAR=1 M3D_TEXTURE_SIZE=2048 node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
```

L’exporteur Windows lance ces étapes avant l’export de la scène et ajoute le GLB au paquet Unreal.
