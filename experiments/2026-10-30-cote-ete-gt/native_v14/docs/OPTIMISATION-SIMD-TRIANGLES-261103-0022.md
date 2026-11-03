# Feuilles triangles SIMD — build 261103-0022

## Constat

Le benchmark du build 261103-0021 rapporte zéro paquet SIMD triangles. Dans le constructeur BVH V13.14, les feuilles étaient arrêtées systématiquement à quatre objets, alors que le noyau AVX2 consomme huit triangles à la fois. Le dispatch existait, mais sa condition ne pouvait donc jamais être satisfaite.

## Changement

Le constructeur garde maintenant ensemble les sous-ensembles de 5 à 8 objets lorsqu’ils ne contiennent que des triangles. Les autres types conservent l’ancienne limite de quatre objets; la règle SAH existante reste appliquée au reste de l’arbre. L’exporteur imprime le nombre de feuilles exactement de huit triangles dans les journaux de compilation afin de confirmer que la scène contient des candidats.

## Mesure requise

Le noyau AVX2 utilise des gathers depuis les objets et son coût peut dépasser celui des tests scalaires. Une feuille plus large échange aussi moins de nœuds AABB contre davantage de tests de primitives. Le changement est donc évalué sur le processeur cible : comparer x64 et AVX2/FMA3 à 1, 2 et 4 threads, vérifier que simd_triangle_packets est non nul, puis comparer rendu, total, tests d’objets, nœuds et AABB avec le build 0021. Ce build ne doit pas être considéré plus rapide tant que ces mesures ne le montrent pas.
