# SIMD du moteur

Les noyaux AVX2 sont séparés sous `src/simd/avx2/`. Le détecteur x64 valide les capacités CPU et l’état OS avant dispatch. Le chemin standard reste compilé et sélectionnable.

- `post.cpp` : AA contours.
- `ray_generation.cpp` : rayon caméra par groupes de huit, hors DOF.
- `triangle_intersection.cpp` : intersections de huit triangles homogènes d’une feuille BVH.
- `bvh_bounds.cpp` : tests simultanés des deux enfants BVH; actif pour chaque nœud interne sur le chemin AVX2.
- Les matériaux, textures, réflexions, autres primitives et le DOF conservent leur chemin scalaire; les rayons secondaires et d’ombres utilisent les noyaux BVH AVX2 lorsqu’ils sont compatibles.

Les réflexions spéculaires de proximité restent scalaires, tandis que leur éclairage diffus est accumulé par paquets de huit. La lecture des mipmaps couleur, normal et rugosité utilise le noyau d’interpolation AVX2. Les performances doivent être mesurées par scène et qualité avant d’étendre le SIMD aux autres étapes.


Build 261103-0012 : le mode x64 standard est sélectionné par défaut après une comparaison utilisateur sur i5-7200U (4 threads, mêmes compteurs de rayons et de parcours BVH) : 8 066,9 ms en x64 standard contre 11 139,9 ms avec AVX2, soit environ 38 % de temps en plus pour AVX2. Ces captures correspondent à une mesure par mode; il faut confirmer par plusieurs répétitions médianes avant de conclure sur la performance générale. Les options automatique et AVX2 forcé restent disponibles pour les mesures et le profilage.
