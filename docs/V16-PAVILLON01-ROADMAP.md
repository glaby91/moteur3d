# V16 — Pavillon 01 : état d'intégration

## Livré sur la branche feature/v16-modular-scene-dispatch

- Manifest `scene.json`, ressources indépendantes, fichiers de géométrie / instances / matériaux / PBR.
- 203 instances de la maison, de l'intérieur et de l'extérieur (11 matériaux, 5 caméras).
- Nouveau loader C++17/Qt6 transactionnel avec validation des chemins.
- Compilation d'objets primitifs en triangles, BVH équilibré, chaînes de mipmaps (albedo sRGB, normal, roughness).
- Exécutable distinct `moteur3d_v16` sans accès au chargeur V3/V4/V5.
- Arbre de scène et catalogue PBR, édition et sauvegarde des paramètres des matériaux, export PNG.
- Rendu CPU et aperçu D3D11 Compute avec repli CPU.
- CMake et GitHub Actions Windows/Linux ; test de rendu CPU et artefact PNG.

## Vérifications à réaliser et limites

- La CI prouve la compilation et l'exécution du rendu CPU, mais un test interactif sur PC Windows reste souhaitable.
- Le GPU Compute reste une prévisualisation géométrique, sans textures PBR ni transparence équivalentes au CPU.
- Les textures sont générées procéduralement à l'ouverture; aucun lot de PNG/PBR externes n'est encore livré.
- La géométrie architecturale reste une première version procédurale simplifiée. Affiner ouvertures, escaliers, terrain et végétation.
- Ajouter une vraie bibliothèque de maillages génériques et des caches de textures persistants après stabilisation.

Pripiat et ses anciens formats ne font pas partie de la V16. Conversion externe ultérieure uniquement.
