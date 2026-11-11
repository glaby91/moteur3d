# CSE WARP V17 — simulation totale intégrée à GitHub Actions

**Objectif :** valider tout le parcours disponible du moteur (scènes, géométrie, matériaux, Raster, CPU, Compute, réglages de qualité et restitution) dans une seule compilation Windows et un seul processus de simulation multi-scènes.

## Couverture exacte

Le workflow `.github/workflows/build-moteur3d-v17.yml` compile une seule fois V17, exécute les tests GPU et de l’interface déjà présents, puis lance `v17/tools/total_simulation.ps1`.

- **Six scènes de référence** : complexe civique extrême, pavillon, complexe standard et lourd, laboratoire matériaux et reconstruction Photo2Scene.
- **Deux caméras et deux résolutions par scène** : 256 × 144 et 480 × 270.
- **Trois profondeurs de lecture GPU** : 1, 2 et 3 buffers. Deux tours, deux images mesurées par tour, échauffement séparé.
- **72 cas Raster, 288 images mesurées, 72 PNG**. Les 24 triplets de PNG sont comparés par SHA-256.
- **Six séries de mesures** des étapes : lecture, création des textures PBR, géométrie, BVH, ombres et mémoire.
- **CPU** : contrôle multithread sur une scène légère, mêmes opérations testées avec 1/2/4/8 threads lorsqu’ils existent.
- **Compute D3D11** : exécution distincte avec vérification d’un résultat effectif et rejet des erreurs.
- **Tests complémentaires de la même compilation** : raster/référence shader, instancing et culling, profils MSAA et filtrage, scène changée pendant un rendu, déterminisme de l’affichage, mipmaps PBR séquentielles/parallèles.

Le workflow exige un verdict PASS pour toutes les configurations, et refuse notamment tout écart pixel entre modes de lecture, perte de scène, échec Compute, absence de métriques ou pic anormal de RAM.

## Résultats consultables

Dans GitHub Actions, l’archive `CSE-WARP-V17.<build>-Total-Simulation-WARP` contient :

- `total_simulation_verdict.json` : verdict détaillé et compteurs, seulement si toutes les validations ont réussi.
- `summary.json` et `campaign_report.html` : données et tableau lisible.
- `campaign_results.csv` / `campaign_frames.csv` : mesures par scène et par frame.
- `images/` : les 72 PNG enregistrés.

En cas d’échec, les fichiers partiels de la simulation sont également conservés si disponibles.

## Limites explicites

Le CI utilise **Microsoft WARP**, un GPU logiciel. Les textures sont volontairement bornées à 256 px pour cette **simulation fonctionnelle exhaustive** et les images sont plus petites que celles du produit. Cela ne modifie **en aucun cas** le mode haute qualité 2048 px ni les réglages de l’application utilisateur.

Cette simulation ne confirme pas la fluidité de 30/60 FPS, le rendu natif de l’Intel HD 620, le partage de texture Qt/DirectX, la navigation verticale, les collisions ni la physique ; ces points nécessitent leurs propres tests de matériel et de fonctionnalités. Le test longue durée de performance reste le benchmark complet sur le Dell.

Exécution manuelle dans un environnement Windows configuré (après compilation) :

```powershell
./v17/tools/total_simulation.ps1 -BinaryDirectory 'build-v17/Release' -OutputDirectory 'build-v17/total-simulation'
```