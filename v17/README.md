# CSE WARP Beta X — V17

**V17 = exploration 3D réellement fluide ET qualité d'image convaincante.**

> État : **cadrage technique uniquement** — aucun changement de moteur ni compilation dans cette branche V17.

Le document de référence, avec objectifs vérifiables, risques, conditions de validation et étapes de développement, est :

**[Objectifs V17 — temps réel, DirectX 11 / OpenGL et qualité lumineuse](../docs/V17-OBJECTIFS-TEMPS-REEL-QUALITE-261110.md)**

## Priorités

1. **Navigation fluide** : caméra libre et images continues sur Intel HD Graphics 620, en commençant à **1366 × 768**. 30 FPS premier palier, 40 FPS / **25 ms** cible à mesurer, 60 FPS optionnel.
2. **Qualité d'atmosphère** : la géométrie V16 existe, mais l'image est encore trop froide/plate. Viser une finesse d'objets, des contrastes, des textures PBR et des effets de lumière dans l'esprit de **S.T.A.L.K.E.R.: Call of Pripyat**, **sans DXR** ni copie de ses ressources.
3. **Pipeline GPU hybride** : rasterisation matérielle + shaders PBR + GI ciblée, réutilisation temporelle, ombres et reflets spécialisés ; analyser d'abord DirectX 11 déjà présent, puis OpenGL/GLSL pour Qt et Linux.
4. **CPU intégral conservé** : ray tracing + path tracing GI, ombres, verre/eau, tous les matériaux et export ; référence visuelle et calcul sans GPU.
5. **Aucune régression V16** : conservez Full HD, QHD, 4K, scènes, assets et backend GPU Compute HLSL pour comparaisons.
6. **Mesures sur la vraie machine** : ne pas assimiler timings WARP de CI et performances Intel HD620.

## Règle de développement

Chaque phase nécessite un accord explicite avant modification du moteur, compilation ou fusion. Voir le document de cadrage pour les **portes de validation V17-0 à V17-7**.

**Base V16** : `feature/v16-dell-native-1366x768-fhd-261110`. La V17 ne doit pas être fusionnée dans `master` sans décision explicite.
