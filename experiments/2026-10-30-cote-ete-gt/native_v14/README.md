# Moteur 3D natif x64 — build 261101-0857

## Repères de version

- **Date/heure de compilation :** `261101-0857` (format `AAMMJJ-HHMM`).
- **Révision GitHub :** SHA court du commit, injecté à la compilation.
- **Branche GitHub :** nom exact de la branche, injecté à la compilation.
- **Scène et géométrie source :** V13.14.
- **Exécution GitHub Actions :** numéro de run CI indépendant de la révision et de la branche.

La révision et la branche apparaissent dans l’interface et les propriétés Windows de l’exécutable. `build_info.h` fournit des valeurs de secours pour une compilation locale; le workflow GitHub le régénère à chaque build.

Le contrat bilingue des entrées/sorties, module par module, est documenté dans [CONTRAT-ENTREES-SORTIES-261101-0857.md](docs/CONTRAT-ENTREES-SORTIES-261101-0857.md).

## Modules du moteur

Les sources de travail sont séparées dans `src/` afin que chaque partie puisse évoluer indépendamment :

- `src/m3d_scene.cpp` : lecture et validation de l’export V13.14.
- `src/m3d_lighting.cpp` : soleil, ciel et sources lumineuses pilotés par l’heure.
- `src/m3d_intersection.cpp` : intersections des primitives, calcul des normales et parcours BVH.
- `src/m3d_shading.cpp` : carte d’ombres, textures, éclairage de surface et réflexions.
- `src/m3d_render.cpp` : caméra, lancer de rayons et rendu parallèle par tuiles.
- `src/m3d_post.cpp` : mise à l’échelle bilinéaire et réduction 2× du résultat.
- `src/moteur3d_win32.cpp` : interface native, réglages et orchestration.
- `m3d_engine.h` : types et fonctions partagés entre les modules.

`assemble_native.js` reconstruit, dans un ordre déterministe, le fichier monolithique `moteur3d_v14_native.cpp`. C’est ce fichier reconstruit que MSVC compile : les sources modulaires restent la référence de travail et l’exécutable utilise le même parcours de rendu.

## Ordonnancement

À l’exécution, l’application charge et valide d’abord la scène, met à jour l’éclairage selon l’heure, puis construit la carte d’ombres. Le rendu calcule ensuite les pixels par tuiles en s’appuyant sur les intersections BVH et l’ombrage des matériaux. Enfin, le post-traitement adapte l’image à la résolution choisie avant l’affichage.

L’ordre de reconstruction/compilation est : scène → éclairage → intersections → ombrage → rendu → post-traitement → interface. Les interfaces partagées sont déclarées dans `m3d_engine.h`.

## Téléchargement et compilation

L’archive `Moteur3D-Natif-x64-261101-0857.zip` contient l’exécutable x64 et `moteur3d_v1314_scene.bin`. Extraire les deux fichiers dans le même dossier, puis lancer le fichier EXE.

Le workflow GitHub injecte la révision et la branche, exporte la scène depuis `source_v1314.html`, reconstruit le fichier source et compile avec MSVC. Le mode **Ultra** conserve les ombres adoucies et les réflexions à un rebond sur l’eau, la peinture et les vitrages.
