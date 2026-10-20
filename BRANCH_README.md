# Branche raster — sauvegarde indépendante

- **Moteur HTML figé :** [engine/index.html](engine/index.html), copie de V8.1.
- **Programme QB64 :** [qb64/raster/RasterEngine.bas](qb64/raster/RasterEngine.bas).
- **Chronologie :** [VERSIONING.txt](VERSIONING.txt).
- **Mode :** triangles, Z-buffer caméra, UV et textures, et optimisations en tuiles.
- **QB64 :** ouvrir le fichier `.bas` dans QB64-PE, compiler puis exécuter. A/D = tourner ; +/- = zoom ; H = basculer 320×200 / 640×400 interne ; ESC = quitter.
- **Portée :** adaptation QB64 pédagogique, pas un port intégral pixel à pixel du simulateur HTML.
- **Tests :** absence de compilateur QB64-PE dans cet environnement ; source à compiler et valider localement.

L'autre famille est isolée sur la branche `raycasting`.

## CPC6128 — raster Mode 0 couleur pleine

- **[Source assembleur Z80](retro/cpc6128-z80/CPC_RASTER_MODE0.asm)** : 24 triangles, huit poses 3D préprojetées, Mode 0 (160×200 / 16 encres), aucune texture.
- **[Guide de conception, limites et mémoire](retro/cpc6128-z80/README.md)**.
- **[Versioning autonome CPC6128](retro/cpc6128-z80/VERSIONING.txt)**.

Cette version CPC est une démonstration expérimentale ; elle doit encore être assemblée et mesurée sous émulateur ou sur CPC6128 réel. Le chiffre de 8–12 FPS est une **cible à vérifier**, pas une mesure obtenue.

**Conception suivante :** [optimisation Z80 V0.2 avec cache de registres secondaires](retro/cpc6128-z80/OPTIMISATION_V02_FAST_REGISTERS.md) — spécification non encore implémentée.


## Raster Z80 V0.2 Fast Registers — implémenté

- [Nouveau moteur assembleur CPC6128 V0.2](retro/cpc6128-z80/CPC_RASTER_V02_FAST_REGISTERS.asm) : 12 triangles visibles, registres secondaires via `EXX`, remplissage par `PUSH BC`, fond vidéo partiellement restauré.
- [Tests statiques, mémoire et différences d'image](retro/cpc6128-z80/README_V02_FAST_REGISTERS.md).

La V0.1 est conservée pour comparaison. **Pas de FPS mesuré ni de validation assembleur/matériel**.
