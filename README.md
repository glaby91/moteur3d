# CSE WARP — moteur 3D V17

**Release Candidate V17.006 — 24 novembre 2026**  
Cette version est une candidate à l’évaluation, pas une version stable. Elle réunit le moteur V17 avec LumaTrace et la scène de référence publiée séparément.

## Version et scène livrée

- Moteur : **V17.006**
- Révisions des composants : PBR **001**, éclairage CPU **003**, DirectX 11 **003**, Vulkan **003**, LumaTrace **009**.
- Dernière scène publiée : **V17.613**, depuis le dépôt [moteur3d-scene](https://github.com/glaby91/moteur3d-scene) et sa [release V17.613](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.613-19).
- Le build officiel #587 (moteur V17.003) a chargé, validé et inclus V17.613 dans son package Windows. Il sert de référence historique ; le build V17.006 de cette branche doit repasser par la CI.
- Les fichiers de scène restent dans le dépôt séparé. Le workflow du moteur récupère la dernière release et l’intègre au package, qui peut ensuite être lancé hors ligne.

## Dernier paquet validé avant V17.006

Le dernier paquet dont la compilation est validée est le [build Windows x64 #587 — V17.003](https://github.com/glaby91/moteur3d/actions/runs/45080722276). Il ne correspond pas à la présente révision V17.006.

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

Pour compiler localement sous Windows avec la dernière scène, installer Visual Studio 2022 C++ x64, CMake, Qt 6.8.3 et le SDK Vulkan 1.3 ou ultérieur. Depuis la racine du dépôt moteur, enregistrer le script ci-dessous sous `COMPILER_V17_AVEC_DERNIERE_SCENE.bat`, ajuster `QT_ROOT` au chemin d’installation de Qt, puis lancer le fichier batch. Il télécharge les deux fichiers de la dernière release de scène, vérifie le manifeste et fournit le dossier extrait à CMake.

```bat
@echo off
setlocal
cd /d "%~dp0"

rem Adapter ce chemin à l'installation locale de Qt 6.8.3.
set "QT_ROOT=C:\Qt\6.8.3\msvc2022_64"
set "SCENE_ROOT=%TEMP%\cse-warp-v17-scene-latest"

if not exist "%QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
  echo Qt 6.8.3 introuvable dans "%QT_ROOT%".
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $root=$env:SCENE_ROOT; if(Test-Path $root){Remove-Item $root -Recurse -Force}; New-Item -ItemType Directory -Force $root | Out-Null; $zip=Join-Path $root 'V17-scene.zip'; $manifestPath=Join-Path $root 'SCENE_VERSION.json'; Invoke-WebRequest 'https://github.com/glaby91/moteur3d-scene/releases/latest/download/SCENE_VERSION.json' -OutFile $manifestPath; Invoke-WebRequest 'https://github.com/glaby91/moteur3d-scene/releases/latest/download/V17-scene.zip' -OutFile $zip; $manifest=Get-Content $manifestPath -Raw | ConvertFrom-Json; if($manifest.repository -ne 'glaby91/moteur3d-scene' -or $manifest.scene -ne 'benchmark-civic/scene.json' -or $manifest.revision -notmatch '^V17\.\d{3}$'){throw 'Manifeste de scène invalide'}; Expand-Archive -LiteralPath $zip -DestinationPath $root -Force; $scene=Join-Path $root 'benchmark-civic'; if(!(Test-Path (Join-Path $scene 'scene.json'))){throw 'benchmark-civic/scene.json absent de l’archive'}; Copy-Item $manifestPath (Join-Path $scene 'SCENE_VERSION.json') -Force; Write-Host ('Scène récupérée : ' + $manifest.revision)"
if errorlevel 1 exit /b 1

cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=%QT_ROOT%" -DM3D_REQUIRE_VULKAN=ON "-DV17_SCENE_SOURCE_DIR=%SCENE_ROOT%\benchmark-civic"
if errorlevel 1 exit /b 1

cmake --build build-v17 --config Release --parallel 4
if errorlevel 1 exit /b 1

echo Compilation terminée. Exécutable : build-v17\Release\moteur3d_v17.exe
endlocal
```

Le moteur copie la scène indiquée par `V17_SCENE_SOURCE_DIR` dans le dossier de l’application lors de la compilation. Le binaire et la scène se trouvent donc sous `build-v17\Release`. La compilation locale construit le moteur et ses cibles CMake ; elle ne reproduit pas toutes les étapes de validation, de finition et de packaging exécutées par le workflow GitHub Actions.

## Évolutions futures

La liste ci-dessous est **non exhaustive**. Ce sont des pistes et des objectifs à étudier, implémenter et valider ; ils ne sont pas tous réalisés.

- **Actualisation de la scène au démarrage :** consulter le dépôt `moteur3d-scene`, comparer la révision locale à la dernière release et afficher un message avec les deux versions et le lien de téléchargement si la scène locale est dépassée. Cette vérification reste à implémenter.
- **Qualité et coût de calcul :** viser le meilleur rendu possible avec le moins de calculs possible pour les modes interactifs. Le CPU Ray Tracing doit continuer à progresser comme référence de qualité visuelle.
- **Raster DX11 :** poursuivre l’instancing, le regroupement spatial et le culling, ajouter du LOD sur les façades et le mobilier, puis améliorer le réglage PBR, de l’éclairage et des ombres sans dégrader l’image. La capacité cible de la roadmap va jusqu’à 2 millions de triangles ; elle reste à valider.
- **Mesures et présentation DX11 :** valider le Z-prepass sur plusieurs vues, étudier la sélection des lumières locales par zones et une présentation native DXGI afin de réduire les copies GPU→CPU. Les gains attendus restent à mesurer sur le matériel cible.
- **LumaTrace :** étendre la gestion des lumières aux rebonds secondaires afin qu’elles contribuent aussi à l’éclairage indirect ; mesurer la qualité obtenue et le coût de calcul.
- **LumaTrace — précision près des lumières :** comparer deux pistes locales autour des sources lumineuses : augmenter la densité jusqu’à 16 sondes, ou raffiner le maillage par subdivision adaptative. Mesurer séparément leur effet sur les anomalies visuelles, la qualité et les performances face à DX11 et Vulkan.
- **Eau DX11/Vulkan :** étudier des normales animées, des reflets d’environnement à faible coût et éventuellement des ombres raster, en vérifiant la qualité et la fluidité.
- **Physique :** préparer l’intégration d’un moteur de physique pour gérer les interactions et collisions de façon plus complète.
- **Backend DXR / DirectX 12 :** étudier comme évolution possible un backend de ray tracing matériel DXR, avec détection des GPU compatibles et un chemin de repli pour les autres configurations.
- **Chargement et diagnostic :** étudier un cache PBR persistant et une télémétrie GPU plus détaillée pour réduire les temps de chargement et mieux localiser les coûts.

Références : [roadmap Raster V17](v17/docs/RASTER_PERFORMANCE_ROADMAP.md), [plan Z-prepass, lumières et DXGI](docs/performance/V17.433-ZPREPASS-LOCAL-LIGHT-DXGI-PLAN.md), [piste d’amélioration de l’eau](docs/IDEES_FUTURES_EAU_DX11.md) et [audit global des optimisations](docs/audits/AUDIT_GLOBAL_CSE_WARP_FONCTIONNALITES_OPTIMISATIONS_2026-11-14.md).

## Benchmarks

Le package inclut `LANCER_BENCHMARK.bat` et les exécutables de benchmark. Exemple depuis le dossier extrait :

```text
LANCER_BENCHMARK.bat "benchmark-civic\\scene.json" "results\\run1" --full-benchmark --all-scenes --include-cpu
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
