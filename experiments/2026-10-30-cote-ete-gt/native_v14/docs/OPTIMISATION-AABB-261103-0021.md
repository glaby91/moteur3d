# Optimisation AABB — build 261103-0021

## Motivation

Le benchmark du build 261103-0020 sur Rapture (Full HD, i5-7200U, 3 répétitions) mesure environ 4,57 millions de rayons et 435 millions de tests AABB. Les paquets SIMD de triangles restent à zéro. Le temps de rendu ne s’améliore pas de façon stable avec AVX2/FMA3.

## Changement

Le test rayon-boîte calcule maintenant l’intervalle [entrée, sortie] axe par axe. Il arrête le test dès que l’intervalle devient vide, ou si une direction parallèle est hors des limites de la boîte. Cela évite les calculs sur les axes suivants dans les cas de rejet.

Aucun format de scène, paramètre de rendu, ordre BVH ou calcul d’intersection de primitive n’est modifié. Le chemin reste scalaire, ce qui permet d’évaluer précisément cette optimisation avant de tenter une traversée BVH vectorisée plus invasive.

## Validation

Le workflow CI Windows/Linux vérifie la compilation. Le gain en temps de rendu doit être mesuré sur la machine cible avec le même CSV, la même scène, les mêmes réglages et au moins 5 répétitions. Comparer x64 standard et AVX2/FMA3 à 1, 2 et 4 threads; 8 threads ne devrait pas être retenu comme réglage utile si la machine n’a que 4 processeurs logiques.
