# CSE WARP Beta X — Moteur3D V16 Windows, Pavillon 01 et modules séparés

**Nom public du moteur : CSE WARP Beta X.** Les exécutables conservent temporairement leurs noms techniques `moteur3d_v16*.exe` pour préserver les scripts de build et les outils existants.

## Périmètre
- Windows x64 uniquement : MSVC 2022, Qt 6.8.3 et CMake.
- Pavillon 01 est la seule scène V16 active ; Pripiat sera converti séparément plus tard.
- Aucun lecteur de scène V3/V4/V5 dans la cible V16.

## Optimisations conservatrices (V16 — sans baisse de qualité)
- **GPU :** les scènes V16 construites par `buildRuntime()` portent un identifiant de contenu unique et une révision. Les changements de caméra ne rescannent plus les mipmaps PBR 2048² ; les modifications de matériaux dans Qt appellent `markGpuContentChanged()`. Les scènes non suivies continuent à utiliser l'empreinte intégrale, sans risque de cache périmé pour les appels historiques.
- **CPU :** une carte d'ombres calculée pour Pavillon 01 est réutilisée entre les caméras d'une même scène, tant que l'heure d'éclairage et la scène ne changent pas.
- Aucun échantillon supprimé, aucun changement de résolution, aucune réduction de la géométrie. La validation automatique vérifie la stricte identité des rendus GPU avant/après réutilisation du cache et l'invalidation en cas de modification de matériau.
- Les chiffres de performance sous WARP logiciel ne préjugent pas des gains sur le GPU matériel.

## Détail architectural — scène Pavillon 01 enrichie
- 311 instances (contre 172) et 26 matériaux PBR distincts (contre 11). Les nouveaux objets correspondent à de vrais détails : étanchéité de toiture, couvertines zinc, évacuations/ventilations, bardage bois à joints, soubassements minéraux, bandeaux et meneaux de fenêtres, seuils, terrasse en lames étroites, sols terrazzo et équipements intérieurs.
- Bois de terrasse, bardage, écorce et feuillages ont leurs matériaux distincts. Les textures secondaires utilisent des chaînes mipmap PBR de 512 à 1024 px afin de limiter le coût mémoire ; les matériaux d'origine conservent leur définition.
- La forme générale, les positions des caméras et la transparence vitrée de Pavillon 01 sont conservées.

## Éclairage global CPU — mode facultatif
- Le moteur disposait déjà d'un path tracer CPU avec rebonds indirects et encodage HDR. La V16 propose désormais dans la liste des backends : « CPU Éclairage global — 2 rebonds, 4 échantillons » et « CPU Éclairage global — 4 rebonds, 16 échantillons ».
- Ces modes sont **CPU uniquement** et plus coûteux que l'aperçu direct. Le GPU D3D11 Compute conserve son mode PBR simplifié rapide ; aucun calcul GI GPU n'est revendiqué.
- Les tests Windows conservent CPU/GPU Full HD et ajoutent une image GI Full HD distincte. Le test GI technique emploie 1 échantillon pour couvrir la voie d'exécution, les préréglages d'utilisation en emploient 4 ou 16.

## Architecture livrée
- `moteur3d_v16_core.dll` : moteur C++ (ray tracing CPU, aperçu Compute D3D11, BVH, matériaux).
- `moteur3d_v16.exe` : visualisation Qt6, catalogue des matériaux, explorateur et six raccourcis caméra **1 à 6**.
- `moteur3d_v16_benchui.exe` : **fenêtre autonome** de configuration des benchmarks.
- `moteur3d_v16_benchmark.exe` : calculateur de benchmark indépendant, sans fenêtre GUI.

L'interface et les benchmarks se compilent séparément de la DLL. Les modifications de l'interface ne nécessitent pas de recompiler le cœur tant que son code et son ABI ne changent pas.

## Scène compacte
- Maison : environ 18 × 12 m ; sol : **30 × 26 m** (centre en z=1), environ 5 à 6 m autour.
- Terrasse rapprochée et bassin resserré pour tenir dans les limites du terrain.
- **3 arbres** (12 instances tronc/houppier), **6 massifs**, et accès en dalles.
- **6 caméras** (façade complète, jardin, entrée, salon, étage, aérienne) accessibles par boutons et touches 1 à 6.
- Les anciennes sphères de feuillage restent des primitives : l'objectif de qualité visuelle n'est pas encore atteint.

## Benchmark complet — exports automatiques
Dans le programme principal : **Outils > Benchmark complet…**.
La fenêtre de benchmark propose un dossier de destination et les modes complet, rapide, ou CPU seul.

Mode complet :
- CPU ray tracing : 6 caméras × 3 résolutions (1920×1080, 2560×1440, 3840×2160) × 5 réglages de threads (1/2/4/8/16).
- CPU qualité : 6 caméras × 1920×1080, 4 échantillons/pixel.
- GPU D3D11 Compute : 6 caméras × 1920×1080, textures PBR simplifiées ; marqué `skipped` si indisponible.
- Dossier horodaté `Pavillon01-V16-AAAAmmJJ_HHMMSS_mmm`.
- Exports `results.csv`, `summary.json`, `system.json`, `progress.jsonl` et `images/*.png`.
- Les essais GPU sont des aperçus géométriques ; **aucune parité PBR CPU/GPU n'est revendiquée**.

Le test rapide automatique de GitHub Actions vérifie un parcours CPU et la production de CSV, JSON et PNG. Le mode complet n'est pas exécuté en CI.

## Construction incrémentale
Depuis la racine du dépôt :
```powershell
.\v16\build-windows.ps1 -Target ui
.\v16\build-windows.ps1 -Target engine
.\v16\build-windows.ps1 -Target benchmark
.\v16\build-windows.ps1 -Target benchmark-ui
.\v16\build-windows.ps1 -Target all
```
La compilation utilise un seul cache `build-v16` : un changement de source GUI seul ne provoque pas la recompilation des fichiers moteur.

## Tests et limitations
- Build Windows, tests de chargement, rendu CPU et export rapide benchmark **validés sur GitHub Actions n°85**.
- L'application utilise le Full HD (1920×1080) par défaut, avec options Full HD (4 échantillons CPU) et 4K. Tous les PNG de référence CPU, GPU et benchmark sont en Full HD minimum ; la CI contrôle les dimensions réelles des fichiers.
- La génération d'un PNG n'est pas une validation de qualité architecturale.
- Les cartes PBR sont encore majoritairement procédurales ; les images PBR externes et le rendu GPU PBR cohérent restent à faire.
- L'exécutable Windows n'a pas été manipulé manuellement sur un PC physique dans cette session.

## Liens
- Branche : https://github.com/glaby91/moteur3d/tree/feature/v16-modular-scene-dispatch
- Workflow Windows : https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v16.yml
