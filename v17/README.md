# CSE WARP Beta X — V17

**Référence de développement (14/11/2026) : V17.493, scène БАССЕЙН dans le paquet Windows après restauration de la scène approuvée.** Le raster DX11 est le moteur principal, le CPU le comparateur ; [audit master](../docs/V17_493_MASTER_AUDIT_2026-11-14.md).

**V17 = architecture temps réel en construction, sans abandonner les moteurs CPU et GPU V16.**

> **Historique V17-1 (ancien état)** : premier exécutable expérimental V17 — interface exploration, caméra WASD/souris, affichage continu, backend raster Direct3D11 réutilisé. Les 30/40 FPS et la qualité Call of Pripyat **ne sont pas encore validés**.

## Prototype V17-1 — déjà implémenté

- `v17/moteur3d_v17_qt.cpp` : fenêtre V17, mode Exploration, clic gauche + souris pour regarder, ZQSD/flèches (AZERTY) pour se déplacer, Maj pour accélérer, espace/Ctrl pour monter/descendre, Échap pour arrêter.
- `v17/CMakeLists.txt` : **réutilise directement le moteur V16 complet** (CPU ray tracing, CPU GI, D3D11 Compute, D3D11 Raster et scènes), sans recoder de moteur.
- Rasterisation GPU existante ajoutée comme cinquième méthode, avec caches des ombres et scène chargée. Éclairage raster encore de première génération : l'amélioration de la chaleur, du verre et du PBR est une **étape ultérieure**.
- FPS mesurés à l'écran pendant l'exploration et durée de rendu disponible.
- **Limitation temporaire essentielle :** raster DirectX11 renvoie encore les pixels vers le CPU pour affichage dans Qt. Donc l'affichage direct d'une texture GPU, le vsync et les 25 ms **ne sont pas encore obtenus**. Il faudra remplacer ce readback et profiler la GPU HD620.
- Build Windows sur GitHub Actions avec `actions/checkout@v5` et `actions/upload-artifact@v6`, tests de non-régression V16 conservés.

## Tester sur le Dell

Ouvrir `moteur3d_v17.exe`, choisir le preset **Dell 1366×768**, et cliquer sur **Démarrer exploration (V17)**. Tester la caméra 02, noter la fluidité et les FPS. Comparer le backend **V17 GPU D3D11 Raster** avec le **V16 GPU D3D11 Compute** en mode image fixe, puis les modes CPU. Les chiffres Windows CI WARP ne prédisent pas le temps réel du HD620.


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
