# CSE WARP — moteur 3D V17

**Release Candidate V17.003 — 22 novembre 2026**  
Cette version est une candidate à l’évaluation, pas une version stable. Elle réunit le moteur V17 avec LumaTrace et la scène de référence publiée séparément.

## Version et scène livrée

- Moteur : **V17.003**
- Révisions des composants : PBR **001**, éclairage CPU **003**, DirectX 11 **003**, Vulkan **003**, LumaTrace **009**.
- Scène d’ouverture : dernière release du dépôt [moteur3d-scene](https://github.com/glaby91/moteur3d-scene). La compilation du 22 novembre intègre la révision **V17.605**, indiquée par le manifeste de scène.
- Le package Windows contient la scène localement : il peut être lancé hors ligne. Une nouvelle compilation récupère la dernière release de scène et vérifie sa révision avant de publier le package.

## Télécharger et lancer

Le [build Windows x64 V17.003 — run 585](https://github.com/glaby91/moteur3d/actions/runs/45075108806) a compilé le moteur, validé le chargement et les rendus de la scène, terminé les régressions V16/V17 et publié le package.

Dans la page du run, télécharger l’artefact **CSE-WARP-V17.003-GitHub-585-Windows-x64**, extraire l’archive, puis lancer `LANCER_CSE_WARP.bat`. L’artefact est conservé par GitHub Actions pour une durée limitée.

[Source de la scène](https://github.com/glaby91/moteur3d-scene) · [PR de promotion V17.003](https://github.com/glaby91/moteur3d/pull/82)

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

La compilation de référence est le workflow [CSE WARP V17 — Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v17.yml). Il utilise Windows Server, Visual Studio 2022, CMake, Qt 6.8.3 et le SDK Vulkan. Il construit l’application, les outils de benchmark et les ressources, puis assemble la dernière scène publiée dans le package.

Pour une compilation locale Windows, installer Visual Studio 2022 C++ x64, CMake, Qt 6.8.3 et le SDK Vulkan 1.3 ou ultérieur, puis configurer et compiler :

```powershell
cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="<chemin-vers-Qt>" -DM3D_REQUIRE_VULKAN=ON
cmake --build build-v17 --config Release --parallel 4
```

La compilation locale du moteur ne télécharge pas automatiquement une scène distante : pour obtenir le package prêt à lancer avec la scène la plus récente, utiliser le workflow GitHub Actions.

## Benchmarks

Le package inclut `LANCER_BENCHMARK.bat` et les exécutables de benchmark. Exemple depuis le dossier extrait :

```text
LANCER_BENCHMARK.bat "benchmark-civic\\scene.json" "results\\run1" --full-benchmark --all-scenes --include-cpu
```

Les rapports distinguent les temps de chargement, de préparation (dont la construction BVH) et de rendu. Pour comparer deux versions, conserver le même ordinateur, la même scène, la caméra, le backend, la résolution et les réglages ; comparer plusieurs passages et les percentiles, pas une image isolée.

## Validation et limites de la Release Candidate

Le run 585 a terminé avec succès la compilation Windows x64, la validation de la scène publiée, des rendus DX11 WARP, les comparaisons de backend et la suite de régression V16/V17. Les rapports de campagne et de simulation sont aussi joints aux artefacts du run.

**WARP est un adaptateur logiciel Windows.** Les résultats CI démontrent que les chemins testés compilent et fonctionnent sur le runner ; ils ne mesurent pas les performances sur Intel HD 620, NVIDIA ou autre GPU matériel. La fluidité, la qualité visuelle et les performances de LumaTrace/Vulkan doivent encore être évaluées sur les cartes cibles. Le statut Release Candidate reste provisoire jusqu’à cette validation sur matériel réel.

## Organisation du dépôt

- `v17/` : interface, intégration V17, backends, shaders et outils V17.
- `v16/` : noyau partagé et fonctions historiques utilisées par V17.
- `scenes/v16/benchmark-civic/` : fixtures et scènes de benchmark internes ; la scène d’ouverture livrée provient du dépôt séparé `moteur3d-scene`.
- `.github/workflows/` : compilation Windows, contrôles de régression et publication des artefacts.
- `docs/` et `v17/docs/` : architecture, versions, performances et protocoles de benchmark.
- `experiments/` : prototypes et versions historiques conservés pour référence.

Les détails du benchmark sont dans [v17/docs/FULL_BENCHMARK.md](v17/docs/FULL_BENCHMARK.md), la version moteur dans [v17/version.json](v17/version.json) et le suivi des changements dans [la PR V17.003](https://github.com/glaby91/moteur3d/pull/82).
