# SIMD par paires de feuilles BVH — build 261103-0023

Le build 0022 a rendu le noyau d’intersection AVX2 accessible, mais sa politique de feuilles larges a ajouté environ 10,2 millions de tests d’objets sur Rapture, contre environ 6,1 millions de tests AABB évités. Les temps de rendu n’ont donc pas progressé de façon convaincante.

Le build 0023 restaure la limite de petites feuilles. Lorsqu’un nœud BVH a deux enfants feuilles dont les objets sont huit triangles, le chargeur le marque une fois. En mode AVX2, et seulement quand les deux boîtes sont touchées et qu’aucun triangle n’est exclu par les filtres de transparence/objet ignoré, l’intersection teste les huit triangles en paquet avant d’éviter la visite des feuilles. Sinon le parcours existant reste utilisé. Le chemin x64 n’emploie pas cette optimisation.

Le workflow affiche le nombre de paires candidates pour Rapture. La comparaison complète doit vérifier le compteur simd_triangle_packets et le rendu médian. Aucun gain n’est affirmé avant le relevé sur i5-7200U.
