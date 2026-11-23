# CSE WARP — moteur 3D V17

**Release Candidate V17.003 — 23 novembre 2026**  
Cette version est une candidate à l’évaluation, pas une version stable. Elle réunit le moteur V17 avec LumaTrace et la scène de référence publiée séparément.

## Version et scène livrée

- Moteur : **V17.003**
- Révisions des composants : PBR **001**, éclairage CPU **003**, DirectX 11 **003**, Vulkan **003**, LumaTrace **009**.
- Dernière scène publiée : **V17.613**, depuis le dépôt [moteur3d-scene](https://github.com/glaby91/moteur3d-scene) et sa [release V17.613](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.613-19).
- Le build officiel #587 a chargé, validé et inclus V17.613 dans son package Windows.
- Les fichiers de scène restent dans le dépôt séparé. Le workflow du moteur récupère la dernière release et l’intègre au package, qui peut ensuite être lancé hors ligne.

## Télécharger et lancer

Le [build Windows x64 officiel #587](https://github.com/glaby91/moteur3d/actions/runs/45080722276) a terminé avec succès.

Télécharger l’artefact **CSE-WARP-V17.003-GitHub-587-Windows-x64**, extraire l’archive, puis lancer `LANCER_CSE_WARP.bat`. Le package contient la scène V17.613.

[Artefact Windows x64 #587](https://github.com/glaby91/moteur3d/actions/runs/45080722276/artifacts/14777487153) · [Source de la scène](https://github.com/glaby91/moteur3d-scene) · [PR de promotion V17.003](https://github.com/glaby91/moteur3d/pull/82)

## Moteurs de rendu

| Backend | Rôle et état |
|---|---|
| **DirectX 11 Raster** | Rendu temps réel recommandé pour explorer la scène. Le chemin GPU natif est disponible ; l’éclairage reste une approximation raster. |
| **Vulkan Raster** | Rendu GPU expérimental, dépendant du pilote et du matériel. |
| **D3D11 Compute** | Calcul GPU par compute shader, conservé pour les comparaisons et les expériences d’éclairage. |
| **LumaTrace BVH4** | Parcours BVH4 sur GPU pour l’éclairage ; chemin expérimental à comparer sur le GPU cible. |
| **CPU Ray Tracing PBR** | Référence CPU plus lente pour les ombres et l’éclairage calculés par ray tracing. |
| **CPU GI** | Calcul d’éclairage indirect et cuisson de tables GI, destiné aux rendus de qualité et aux comparaisons. |

La V17 réutilise le noyau partagé V16 pour les fonctions communes et les scènes. Le backend DirectX livré est **Direct3D 11** ; un backend **DXR / DirectX 12 n’est pas inclus** dans cette version.

## Compiler depuis les sources

La compilation de référence est le workflow [CSE WARP V17 — Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v17.yml). Il utilise Visual Studio 2022, CMake, Qt 6.8.3 et le SDK Vulkan. Il construit l’application, les outils de benchmark et les ressources, puis importe la dernière scène publiée dans le package.

Pour une compilation locale Windows, installer Visual Studio 2022 C++ x64, CMake, Qt 6.8.3 et le SDK Vulkan 1.3 ou ultérieur, puis configurer et compiler :

```powershell
cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="<chemin-vers-Qt>" -DM3D_REQUIRE_VULKAN=ON
cmake --build build-v17 --config Release --parallel 4
```

La compilation locale ne télécharge pas automatiquement la scène distante. Pour obtenir le package prêt à lancer avec la scène la plus récente, utiliser le workflow GitHub Actions.

## Benchmarks

Le package inclut `LANCER_BENCHMARK.bat` et les exécutables de benchmark. Exemple depuis le dossier extrait :

```text
LANCER_BENCHMARK.bat "benchmark-civic\scene.json" "results\run1" --full-benchmark --all-scenes --include-cpu
```

Les rapports distinguent les temps de chargement, de préparation (dont la construction BVH) et de rendu. Pour comparer deux versions, conserver le même ordinateur, la même scène, la caméra, le backend, la résolution et les réglages ; comparer plusieurs passages et les percentiles, pas une image isolée.

## Validation et limites de la Release Candidate

Le run 587 sur `master` a terminé avec succès la compilation Windows x64, le chargement de la release V17.613, les rendus et les régressions V16/V17. La CI utilise WARP, un adaptateur logiciel : ses résultats valident les chemins testés, mais ne mesurent pas les performances sur Intel HD 620, NVIDIA ou un autre GPU matériel.

La fluidité et les performances de LumaTrace/Vulkan restent à évaluer sur les cartes cibles. La Release Candidate ne devient pas une version stable avant cette validation sur matériel réel.

## Organisation du dépôt

- `v17/` : interface, intégration V17, backends, shaders et outils V17.
- `v16/` : noyau partagé et fonctions historiques utilisées par V17.
- `scenes/v16/benchmark-civic/` : fixtures internes de benchmark ; la scène d’ouverture livrée provient du dépôt séparé `moteur3d-scene`.
- `.github/workflows/` : compilation Windows, contrôles de régression et publication des artefacts.
- `docs/` et `v17/docs/` : architecture, versions, performances et protocoles de benchmark.
- `experiments/` : prototypes et versions historiques conservés pour référence.

Détails du benchmark : [v17/docs/FULL_BENCHMARK.md](v17/docs/FULL_BENCHMARK.md). Version moteur : [v17/version.json](v17/version.json).