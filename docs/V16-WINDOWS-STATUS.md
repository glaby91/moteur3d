# CSE WARP Beta X — Moteur3D V16 Windows, Pavillon 01 et modules séparés

**Nom public du moteur : CSE WARP Beta X.** Les exécutables conservent temporairement leurs noms techniques `moteur3d_v16*.exe` pour préserver les scripts de build et les outils existants.

## Périmètre
- Windows x64 uniquement : MSVC 2022, Qt 6.8.3 et CMake.
- Pavillon 01 est la seule scène V16 active ; Pripiat sera converti séparément plus tard.
- Aucun lecteur de scène V3/V4/V5 dans la cible V16.

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
- CPU ray tracing : 6 caméras × 3 résolutions (400×225, 800×450, 1280×720) × 5 réglages de threads (1/2/4/8/16).
- CPU qualité : 6 caméras × 800×450, 4 échantillons/pixel.
- GPU D3D11 Compute : 6 caméras × 800×450, aperçu de la géométrie ; marqué `skipped` si indisponible.
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
- La génération d'un PNG n'est pas une validation de qualité architecturale.
- Les cartes PBR sont encore majoritairement procédurales ; les images PBR externes et le rendu GPU PBR cohérent restent à faire.
- L'exécutable Windows n'a pas été manipulé manuellement sur un PC physique dans cette session.

## Liens
- Branche : https://github.com/glaby91/moteur3d/tree/feature/v16-modular-scene-dispatch
- Workflow Windows : https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v16.yml
