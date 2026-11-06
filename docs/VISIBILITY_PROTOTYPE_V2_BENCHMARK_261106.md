# Benchmark prototype visibilité v2 vs ray tracing BVH (simulation)

Date : 2026-11-06. Statut : prototype autonome compilé localement, non intégré au moteur Pripyat.

Le premier prototype comparait une rasterisation tuilée à une recherche naïve de triangles par pixel. Pour rendre le comparatif plus représentatif, le **rendu classique** utilise désormais une BVH binaire construite avant le chronométrage. Le prototype emploie une rasterisation CPU par tuiles 16x16 avec Z-buffer en 1/Z, UV implicites via position monde et shading identique.

## Mesures locales (C++17, GCC -O3, un thread)

| Triangles | Résolution | Classique rayons + BVH | Raster tuilé | Ratio | Différences |
|---:|---|---:|---:|---:|---|
| 98 | 640x360 | 78.068 ms | 9.274 ms | 8.42x | 8810 pixels, erreur RGB 0.745/255 |
| 498 | 640x360 | 118.280 ms | 14.771 ms | 8.01x | 9287 pixels, erreur RGB 0.777/255 |
| 498 | 1280x720 | 446.078 ms | 41.639 ms | 10.71x | 49576 pixels, erreur RGB 1.059/255 |

Ce sont des mesures d'une **scène synthétique simplifiée**, pas une comparaison de performance exécutée sur le moteur réel. Le vrai moteur dispose de BVH, AVX2, multithreading, matériaux, ombres et réflexions, donc **aucun ratio ne peut être extrapolé à Pripyat**. La qualité n'est pas encore strictement équivalente (différences de couverture des triangles et des bords).

## Prochaines étapes avant intégration
1. Rendre les couvertures et tests de profondeur pixel-identiques à la référence sur scènes opaques, avec clipping near-plane et règles top-left.
2. Remplacer le parcours triangle-par-triangle du prototype par un binning des triangles en tuiles (actuellement les triangles itèrent sur les tuiles).
3. Ajouter une véritable occlusion hiérarchique Hi-Z conservatrice et mesurer les objets/tuiles rejetés.
4. Brancher le prototype sur les mêmes géométries, matériaux, caméra et éclairage que m3d_render.cpp ; comparaison A/B à résolution et paramètres identiques.
5. Tester les cas transparence, objets fins, surfaces courbes, déplacement de caméra et 1080p/4K ; ensuite profiler en multithread.

Le code source autonome et les images comparatives ont été produits en archive téléchargeable dans la conversation. Cette documentation ne constitue pas un commit du moteur intégré.
