# CSE WARP V17 — campagnes de benchmark

Ce document décrit les exécutables livrés avec le moteur et leurs options. La compilation des outils se fait avec `cmake --build ... --config Release`. Le package Windows contient `LANCER_BENCHMARK.bat` et `moteur3d_v17_campaign.exe`.

## Campagne multi-backend

Exemple complet, depuis le dossier du package :

```bat
LANCER_BENCHMARK.bat "benchmark-civic\scene.json" "results\full" --full-benchmark --all-scenes --include-cpu --rounds 6 --frames 12 --warmup 2
```

`LANCER_BENCHMARK.bat` transmet ses paramètres à `moteur3d_v17_campaign.exe`. La syntaxe est :

```text
moteur3d_v17_campaign scene.json outdir [--full-benchmark] [--rounds N] [--frames N] [--warmup N] [--all-scenes] [--include-cpu] [--smoke] [--simulation] [--raster-msaa 1|2|4|8] [--raster-anisotropy 1|2|4|8|16] [--raster-lod-percent 50..200] [--raster-culling 0|1] [--raster-lod-enabled 0|1]
```

| Option | Description |
|---|---|
| `--full-benchmark` | D3D11 Raster à profondeurs de lecture 1/2/3, Vulkan Raster, LumaTrace BVH4 GPU, D3D11 Compute, CPU Ray Tracing PBR direct et CPU GI à 2/4 rebonds. |
| `--rounds N` | Séries mesurées, 1–30, défaut 6. |
| `--frames N` | Images par série, 2–240, défaut 12. |
| `--warmup N` | Échauffement par série, 0–20, défaut 2. |
| `--all-scenes` | Ajoute les scènes de benchmark livrées. |
| `--include-cpu` | Profil de temps CPU par nombre de threads et diagnostic Compute DX11 hors Full Benchmark ; le Full Benchmark ajoute déjà les cas CPU/Compute. |
| `--smoke` | Contrôle court, 320×180, 2 séries × 2 images et 1 échauffement ; ignore scènes et profil CPU. Incompatible avec `--simulation`. |
| `--simulation` | Contrôle à textures 256 px, raster 256×144 et 480×270, diagnostic 96×54, 2 séries × 2 images et 1 échauffement. C’est une vérification fonctionnelle, pas un score FPS matériel. |
| `--raster-msaa N` | 1, 2, 4 ou 8 échantillons MSAA. |
| `--raster-anisotropy N` | 1, 2, 4, 8 ou 16. |
| `--raster-lod-percent N` | Pourcentage de qualité/distance LOD, 50–200. |
| `--raster-culling 0|1` | Désactive/active le culling de frustum. |
| `--raster-lod-enabled 0|1` | Désactive/active le LOD dynamique. |

Le Full Benchmark applique les mêmes scènes, caméras et résolutions (1366×768 et 1920×1080) à DX11/Vulkan/LumaTrace. Le groupe Compute et les deux modes GI CPU s’exécute à une résolution basse distincte ; le ray tracing direct CPU est à 320×180 et 1 spp. Ces groupes ne doivent pas être comparés comme des FPS de même charge.

LumaTrace mesure la préparation du BVH4 et le rendu GPU. Si une géométrie ne peut pas être préparée pour LumaTrace, le cas est écrit `skipped` avec la raison ; il ne transforme pas en échec le rendu des autres backends.

Chaque dossier horodaté contient les fichiers JSON, CSV, rapport HTML et images générées. Les champs `medianMs`, `p95Ms`, `p99Ms` et les mesures par bloc servent à distinguer latence typique et pointes.

## Benchmark de rendu V17

```bat
moteur3d_v17_benchmark.exe "benchmark-civic\scene.json" "results\raster" --all-cameras --compare-readback --raster-msaa 2 --raster-anisotropy 8 --raster-lod-percent 85 --raster-culling 1 --raster-lod-enabled 1
```

- `--quick` : une caméra, 1366×768, 6 images mesurées.
- `--smoke` : vérification courte CI avec textures 256 px.
- `--fhd-stills` : capture fixe 1920×1080.
- `--compare-v16` : ajoute le Compute HLSL et le rendu CPU direct V16.
- `--readback-depth 1|2|3` : profondeur de tampon GPU/CPU.
- `--compare-readback` : compare pixels et timings pour 1, 2 et 3 tampons.
- `--all-cameras` : parcourt toutes les caméras ; `--camera-id ID` en sélectionne une seule.
- `--hour H` : heure solaire de 0 inclus à 24 exclu.
- `--cpu-direct-still` : rend le rayon CPU direct pour la caméra choisie ; nécessite `--camera-id`.
- `--cpu-direct-width N` / `--cpu-direct-height N` : dimension CPU de 320 à 1920 ; valeurs par défaut 960×540.
- `--raster-msaa`, `--raster-anisotropy`, `--raster-lod-percent`, `--raster-culling` et `--raster-lod-enabled` : règlent le raster comme dans le tableau campagne.

Les résultats sont écrits en `results.csv`, `summary.json` et PNG, dans un sous-dossier avec date/heure.

## Trajectoire DX11 sur GPU matériel

```bat
moteur3d_v17_gpu_trajectory.exe "benchmark-civic\scene.json" "results\trajectory"
```

L’outil Windows requiert un adaptateur GPU matériel (sauf `--ci-smoke`) et utilise la caméra de scène `v8_glazing_raccords`. Par défaut, il mesure 90 images sur une trajectoire de six secondes en 1366×768 ; `--fixed-camera` garde le même point de vue. Options de contrôle : `--ci-smoke` (640×360, 4 images), `--z-prepass=on|off` et `--light-aabb=on|off`. Le rapport `frames.csv` détaille latence, occupation GPU disponible, mémoire processus, attente CPU et temps de dessin/transfert GPU lorsqu’ils sont instrumentés.

## Cuisson GI hors ligne

```bat
moteur3d_v17_gibake.exe "benchmark-civic\scene.json" "results\scene.m3dgi" --bench "results\gi.json"
```

La cuisson écrit une table GI CPU dans le fichier `.m3dgi` ; elle ne recalcule pas la table à chaque image. Options : `--smoke` réduit le volume, `--full-gi` conserve le volume GI complet même avec `--smoke`, `--bench report.json` exporte les médianes de cuisson, chargement disque et interpolation.

## Benchmark V16

```bat
moteur3d_v16_benchmark.exe "pavillon01\scene.json" "results\v16"
moteur3d_v16_benchmark.exe "pavillon01\scene.json" "results\v16-cpu" --cpu-only
```

`--quick` active le chemin CPU rapide et équivaut à `--cpu-only`. Le mode normal garde les cas CPU et GPU Compute HLSL lorsque le matériel le permet.

## Méthode de comparaison et limites

Les rapports séparent chargement, construction de scène, PBR, géométrie/instanciation, construction BVH, préparation des ombres et rendu. Il faut comparer la construction BVH séparément du parcours BVH et du temps par image.

Les campagnes Raster/Vulkan/LumaTrace mesurent un rendu hors écran et le readback des pixels ; elles ne chronomètrent ni la présentation native swapchain ni le rafraîchissement de l’écran. Un temps CPU de soumission en présentation directe ne mesure pas la fin GPU. Les runners Windows peuvent employer WARP, qui valide les chemins sans prédire le débit d’une Intel HD 620 ou d’une autre carte graphique.

Pour une comparaison attribuable, conserver la même machine, scène, caméra, résolution, backend, qualité, heure, nombre d’images et échauffement ; répéter les séries et comparer P50/P95/P99. Garder ensemble tous les rapports d’une exécution et inspecter les PNG pour confirmer que les images correspondent au même contenu.
