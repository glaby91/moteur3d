# SIMD du moteur

Les noyaux AVX2 sont séparés sous `src/simd/avx2/`. Le détecteur x64 valide les capacités CPU et l’état OS avant de les appeler. Le chemin standard reste compilé et sélectionnable.

- `post.cpp` : AA des contours.
- `ray_generation.cpp` : rayons caméra normalisés par paquets de huit, hors DOF.
- `triangle_intersection.cpp` : Möller–Trumbore sur huit triangles d’une même feuille BVH.
- Le parcours BVH, les matériaux, textures, réflexions et le DOF gardent leurs branches scalaires. Les requêtes d’ombres et de réflexion profitent du test AVX2 des triangles, car elles partagent `closest()`.

Les optimisations seront mesurées séparément; la présence d’AVX2 n’implique pas que chaque opération divergente soit vectorisée.
