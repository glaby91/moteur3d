# CSE WARP — moteur 3D V17

**Moteur : V17.008 · scène de référence courante : V17.627**
CSE WARP V17 réunit l’interface d’exploration, plusieurs backends de rendu, le noyau moteur partagé avec V16, les outils de benchmark et un mécanisme de mise à jour de scène. La V17 reste une version expérimentale : les performances et la présentation native doivent être mesurées sur le GPU visé.

> **Branche par défaut :** `master`. La livraison V17.008 avec scène V17.627 est fusionnée dans cette branche; voir [BRANCH_STATUS.md](BRANCH_STATUS.md) pour le repère des branches moteur conservées.

## Scène de référence et paquet Windows

La scène « Palais et coupole vitrée de la piscine » est maintenue séparément dans [`glaby91/moteur3d-scene`](https://github.com/glaby91/moteur3d-scene). La release courante est [V17.627](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.627-40). Le workflow moteur récupère la dernière release au début de la compilation, vérifie le manifeste de l’archive et son numéro de révision, charge la scène avec le moteur, puis place **la scène complète et son manifeste dans le même paquet Windows que le moteur**. Le paquet est ainsi utilisable hors ligne dès son extraction.

Le programme vérifie ensuite les nouvelles releases en tâche de fond. Il ouvre immédiatement la scène locale trouvée ; la vérification réseau ne retarde pas le rendu. Une nouvelle version est proposée sans remplacer la scène en cours. Si l’utilisateur accepte, la nouvelle scène est installée dans les données utilisateur et un redémarrage est proposé. En l’absence de scène locale, le moteur propose de télécharger la release avant de démarrer. Une vérification indisponible laisse la scène locale utilisable.

- Dépôt de la scène : [moteur3d-scene](https://github.com/glaby91/moteur3d-scene)
- Release actuelle : [V17.623 — build 36](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.623-36)
- Workflow de compilation du moteur : [CSE WARP V17 — Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v17.yml)
- Révision emballée : `SCENE_REVISION.txt` et `SCENE_VERSION.json`, à la racine du paquet ainsi que dans `benchmark-civic/`.

Le build historique [V17.003 #587](https://github.com/glaby91/moteur3d/actions/runs/45080722276) ne contient pas cette révision. Le paquet Windows V17.006 — build GitHub 683, avec la scène V17.623 — est disponible dans les artefacts du [workflow de compilation](https://github.com/glaby91/moteur3d/actions/runs/45493998985).

## Format de description des scènes

`scene.json` relie les ressources d’une scène par secteurs :

- **Géométrie** : sommets/triangles ou formes de base réutilisables.
- **Instances** : géométrie, matériau, position, taille et rotation d’un objet.
- **Matériaux et textures** : couleur, texture PBR, rugosité, aspect métallique et transparence.
- **Éclairage** : sources visibles et lumières locales sont définies séparément, avec position, couleur, intensité, portée et groupe.
- **Caméras** : vues prédéfinies, position, cible et champ de vision.

Exemple schématique d’une instance d’arbre :

```json
{
  "id": "arbre_01",
  "parent": "Place/Arbres",
  "geometry": "mesh_arbre_feuillu",
  "material": "materiau_arbre",
  "position": [12.0, 0.0, 35.0],
  "size": [1.0, 1.0, 1.0],
  "rotationY": 25
}
```

L’origine de la géométrie compte autant que la position : si le pivot du modèle d’arbre est au milieu du tronc, `Y = 0` enterre une partie de l’arbre ; si le pivot est à la base, `Y = 0` peut poser le tronc au sol. Une géométrie source peut être réutilisée par de nombreuses instances. Les corrections de scène sont versionnées dans le dépôt de scène et republiées sous forme d’archive ; elles ne demandent pas de reconstruire l’application moteur.

### Scène historique de Pripiat — place centrale

La place centrale de Pripiat avec le Palais de la Culture Energetik et l’hôtel Polissya est un package séparé, compatible avec V16/V17. Il n’est pas fusionné dans la scène civique par défaut. Après extraction, l’ouvrir avec **Fichier → Ouvrir une scène V16 (scene.json)…**, puis sélectionner `Pripyat-V16/pripyat/scene.json`.

[Télécharger le package de la place centrale de Pripiat](https://github.com/glaby91/moteur3d-scene/releases/download/pripyat-scene-v16-compat-20261124.1/Pripyat-place-centrale-V16-V17-compatible.zip) · [Détails et version](https://github.com/glaby91/moteur3d-scene/releases/tag/pripyat-scene-v16-compat-20261124.1)

## Fonctionnalités

- Exploration interactive de la scène depuis ses caméras, avec compteur FPS et informations caméra.
- Choix du moteur de rendu depuis l’interface ; modes CPU et GPU conservés ensemble pour comparer qualité, coût et compatibilité.
- Fenêtres de rendu natives dédiées aux backends GPU. DX11, Vulkan et LumaTrace peuvent présenter par swapchain GPU directe sur Windows ; le chemin Qt/QImage reste disponible en solution de repli. Le chemin direct évite le readback d’image systématique, mais le temps de soumission CPU affiché ne garantit pas la durée GPU réelle.
- Navigation au sol avec collisions approximatives, escaliers, accroupissement, saut et échelles de piscine ; option de vol libre.
- Pip-Boy de diagnostic avec données moteur/scène, éclairage, ressources et état du backend.
- Radio 3D spatialisée à quatre haut-parleurs, flux Internet et fichier WAV local de secours ; bouton et volume dans l’interface.
- Mise à jour asynchrone de la scène civique, découplée du chargement initial.
- Benchmarks autonomes V16/V17 et cuisson hors ligne de la table d’éclairage global CPU.

## Commandes et touches

| Touche / action | Fonction |
|---|---|
| `Z Q S D` ou flèches | Marcher dans la direction regardée (AZERTY) |
| Souris | Orienter la caméra pendant l’exploration |
| `Maj` | Accélérer |
| `Ctrl` maintenu | S’accroupir en mode marche ; la caméra ne se redresse pas sous un plafond bas |
| `Espace` | Sauter depuis le sol en mode marche ; monter en vol libre |
| `Ctrl` en vol libre | Descendre |
| `E` près d’une échelle de piscine | Monter ou descendre l’échelle si le personnage est à proximité |
| Clic droit | Activer le zoom/lunette ×3 |
| `Tab` ou `<` brièvement | Afficher/masquer le Pip-Boy compact au poignet |
| `Tab` ou `<` maintenu environ 550 ms | Ouvrir le tableau de bord plein écran |
| `Échap` | Fermer le tableau de bord et arrêter l’exploration |
| `F11` | Basculer le viewport en plein écran |
| `F2` | Afficher/masquer le HUD de la fenêtre de rendu |
| `Ctrl + Maj + A` | Capturer l’anomalie courante en PNG et JSON |

La case **Marcher au sol** active le déplacement à hauteur d’homme (yeux à 1,70 m), les collisions et le saut. La décocher rend les commandes verticales de vol libre à `Espace` et `Ctrl`. Les collisions sont une approximation de navigation par rayons, pas une simulation physique complète.

Le Pip-Boy compact donne accès au tableau de bord plein écran. Celui-ci regroupe les pages **Aperçu**, **Scène**, **Lumières**, **Ressources** et **Diagnostic** : révision moteur/scène, objets et triangles, matériaux, lumières, caméra, résolution, cadence et signalements du backend. Une valeur non instrumentée est marquée comme indisponible plutôt que devinée.

La radio se pilote depuis le panneau latéral **SON AMBIANT** : démarrer/arrêter et régler le volume. Le lecteur tente le flux en ligne puis bascule sur `audio/pripyat_radio.wav` si le flux ne peut pas être lu. Le son spatial suit la caméra ; il ne bloque pas la boucle de rendu.

## Backends disponibles

| Choix dans l’interface | Calcul | Usage / limites |
|---|---|---|
| **CPU Ray Tracing PBR — direct** | Rayon primaire et éclairage direct CPU | Référence lente, utile pour comparer les rendus et les matériaux. |
| **GPU D3D11 Compute — rapide, ombres + verre** | Compute shader HLSL partagé avec V16 | Voie historique GPU, conservée pour compatibilité et diagnostics. |
| **CPU Éclairage global — 2 rebonds** | Ray tracing CPU et rebonds GI | Qualité réglable, coût élevé. |
| **CPU Éclairage global — 4 rebonds** | Ray tracing CPU et davantage de rebonds GI | Calcul plus lourd, destiné aux rendus de qualité. |
| **V17 GPU D3D11 Raster** | Rasterisation matérielle, profondeur et shaders | Backend recommandé pour explorer en temps réel. Éclairage raster approximatif. |
| **V17 Vulkan Raster** | Rasterisation Vulkan | Expérimental ; dépend du pilote et de la disponibilité du SDK à la compilation. |
| **LumaTrace BVH4 — éclairage GPU** | Préparation d’un BVH4 CPU puis parcours/calcul GPU | Mode expérimental d’éclairage direct, distinct d’un path tracer multi-rebonds. Géométrie incompatible : le benchmark l’enregistre comme ignorée avec motif. |

Sur Windows, l’option **Affichage GPU direct natif** permet aux backends DX11, Vulkan et LumaTrace de présenter dans leur propre fenêtre. Désactivez-la pour utiliser la sortie Qt/QImage. Vulkan peut rester indisponible si le SDK n’était pas présent à la compilation ou si le pilote ne permet pas son initialisation. Le menu indique l’état du backend.

## Architecture modulaire

V16 et V17 assemblent les mêmes modules partagés ; la V17 n’est pas une deuxième copie indépendante du noyau. [`engine/render_modules.json`](engine/render_modules.json) sert de registre commun : identifiant, révision, sources, dépendances et caractère optionnel de chaque module. Une correction ciblée doit incrémenter le module concerné plutôt que modifier un numéro global sans traçabilité.

Le découpage récent place l’orchestration de l’interface dans `v17/moteur3d_v17_qt.cpp` et les chemins de rendu dans des fichiers distincts :

- `v17/moteur3d_v17_render.cpp` : orchestration partagée et préparation du rendu ;
- `v17/moteur3d_v17_render_cpu.cpp` : ray tracing CPU et GI ;
- `v17/moteur3d_v17_render_compute.cpp` : dispatch du backend Compute ;
- `v17/moteur3d_v17_render_d3d11.cpp` : raster DX11 ;
- `v17/moteur3d_v17_render_vulkan.cpp` : raster Vulkan ;
- `v17/moteur3d_v17_render_lumatrace.cpp` : interface de rendu LumaTrace ;
- `v17/scene_update_manager.h` : vérification, téléchargement et installation de scène ;
- `v17/diagnostic_dashboard.h` : interface Pip-Boy et pages de diagnostic ;
- `v17/vulkan/m3d_vulkan_resources.*` : durée de vie des buffers, images et mémoires Vulkan.

Le moteur partagé se trouve principalement dans `experiments/2026-10-30-cote-ete-gt/native_v14/src/`. Les modules CPU couvrent intersection, éclairage, matériaux, SIMD, post-traitement et GI. Les backends GPU restent spécialisés. Le registre et les révisions se trouvent dans `engine/render_modules.json` ; les configurations CMake V16/V17 assemblent les mêmes composants.

Détails du découpage : [`v17/README.md`](v17/README.md) · [registre des modules](engine/render_modules.json) · [architecture et validation Vulkan](v17/vulkan/README.md).

## Compiler

La compilation de référence est le workflow [CSE WARP V17 — Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v17.yml). Il construit l’application, les backends optionnels disponibles, les outils, copie la dernière scène publiée après validation et produit un artefact Windows autonome.

Pour compiler localement, installer Visual Studio 2022 avec C++ x64, CMake, Qt 6.8.3 (module Multimedia) et Vulkan SDK 1.3 ou ultérieur. Depuis la racine du dépôt :

```powershell
cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DM3D_REQUIRE_VULKAN=ON
cmake --build build-v17 --config Release --parallel 4
```

Le workflow GitHub télécharge et contrôle la release de scène avant le packaging ; une compilation locale seule n’exécute pas automatiquement toutes les étapes de finition, d’audit et de packaging Windows de la CI.

## Benchmarks en ligne de commande

Le paquet Windows fournit `LANCER_BENCHMARK.bat`, qui appelle `moteur3d_v17_campaign.exe` sans ouvrir l’interface. Les commandes s’exécutent depuis le dossier extrait. Les chemins d’entrée/sortie peuvent être relatifs à ce dossier.

### Campagne Full Benchmark multi-backend

```bat
LANCER_BENCHMARK.bat "benchmark-civic\scene.json" "results\full" --full-benchmark --all-scenes --include-cpu --rounds 6 --frames 12 --warmup 2
```

La matrice compare DX11 Raster (pipelines de lecture à 1/2/3 tampons), Vulkan Raster et LumaTrace aux mêmes tailles raster, puis Compute DX11, CPU ray tracing PBR direct et GI CPU à 2 et 4 rebonds dans un groupe de diagnostic basse résolution. Les cas CPU/Compute ne sont pas présentés comme comparables en FPS aux images raster 1366×768 et 1920×1080.

### Options de campagne (`moteur3d_v17_campaign.exe`)

La syntaxe complète est :

```text
moteur3d_v17_campaign scene.json outdir [--full-benchmark] [--rounds N] [--frames N] [--warmup N] [--all-scenes] [--include-cpu] [--smoke] [--simulation] [options raster]
```

| Option | Effet / valeurs |
|---|---|
| `--full-benchmark` | Lance la matrice étendue des backends ci-dessus. |
| `--rounds N` | Nombre de séries indépendantes, de 1 à 30 (défaut 6). |
| `--frames N` | Images mesurées par série, de 2 à 240 (défaut 12). |
| `--warmup N` | Images d’échauffement par série, de 0 à 20 (défaut 2). |
| `--all-scenes` | Parcourt les scènes de benchmark distribuées. Sans cela, utilise seulement le fichier scène fourni. |
| `--include-cpu` | Ajoute le profil de threads CPU et le diagnostic Compute DX11 hors Full Benchmark. |
| `--smoke` | Contrôle court : une scène, 320×180, 2 séries de 2 images et 1 image d’échauffement. Ignore `--all-scenes` et `--include-cpu`. Incompatible avec `--simulation`. |
| `--simulation` | Réduit les textures à 256 px, utilise 256×144 / 480×270 et fixe 2 séries, 2 images, 1 échauffement. Contrôle fonctionnel, pas une mesure de FPS matérielle. |
| `--raster-msaa N` | MSAA 1, 2, 4 ou 8. |
| `--raster-anisotropy N` | Filtrage anisotrope 1, 2, 4, 8 ou 16. |
| `--raster-lod-percent N` | Qualité/distance LOD, 50 à 200 %. |
| `--raster-culling 0\|1` | Désactive/active le culling de frustum. |
| `--raster-lod-enabled 0\|1` | Désactive/active le LOD dynamique. |

Chaque exécution crée un sous-dossier horodaté avec `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et les PNG de référence disponibles. Le cas LumaTrace non pris en charge est marqué `skipped` plutôt que compté comme échec de toute la scène.

### Benchmark DX11 par caméras et captures fixes

```bat
moteur3d_v17_benchmark.exe "benchmark-civic\scene.json" "results\raster" --all-cameras --compare-readback --raster-msaa 2 --raster-anisotropy 8 --raster-lod-percent 85 --raster-culling 1 --raster-lod-enabled 1
```

Options utiles : `--quick` mesure une caméra à 1366×768 (6 images) ; `--smoke` fait un contrôle CI court avec textures réduites ; `--all-cameras` parcourt toutes les caméras ; `--camera-id ID` sélectionne une seule caméra ; `--readback-depth 1|2|3` règle le nombre de tampons GPU/CPU ; `--compare-readback` compare les pixels en 1/2/3 tampons ; `--compare-v16` ajoute les voies V16 Compute HLSL et CPU direct ; `--fhd-stills` produit une capture 1920×1080 ; `--hour H` choisit l’heure entre 0 inclus et 24 exclu. Les cinq options raster sont celles décrites dans le tableau précédent.

Pour un rendu CPU fixe, préciser la caméra et la résolution :

```bat
moteur3d_v17_benchmark.exe "benchmark-civic\scene.json" "results\cpu" --camera-id v7_pool_fresco --hour 14 --cpu-direct-still --cpu-direct-width 960 --cpu-direct-height 540
```

`--cpu-direct-width` et `--cpu-direct-height` acceptent 320 à 1920 pixels. `--cpu-direct-still` nécessite `--camera-id`.

### Trajectoire GPU matérielle DX11

```bat
moteur3d_v17_gpu_trajectory.exe "benchmark-civic\scene.json" "results\trajectory"
```

Ce benchmark Windows cible le GPU matériel, la scène et la caméra de vitrage `v8_glazing_raccords`. Par défaut il mesure une trajectoire de six secondes à 1366×768 sur 90 images ; `--fixed-camera` garde une vue fixe. `--ci-smoke` réduit le test à 640×360 et 4 images pour la CI. Les interrupteurs expérimentaux `--z-prepass=on|off` et `--light-aabb=on|off` permettent une comparaison A/B. Un adaptateur GPU matériel est requis hors `--ci-smoke`.

### Cuisson GI et benchmark V16

```bat
moteur3d_v17_gibake.exe "benchmark-civic\scene.json" "results\scene.m3dgi" --bench "results\gi.json"
moteur3d_v16_benchmark.exe "pavillon01\scene.json" "results\v16" --cpu-only
```

`moteur3d_v17_gibake.exe scene.json cache.m3dgi` calcule puis écrit une table GI CPU. `--smoke` utilise un volume réduit ; `--full-gi` garde la table complète même en smoke ; `--bench report.json` ajoute les médianes de cuisson, relecture disque et interpolation. Le benchmark V16 accepte `--quick` (équivaut à CPU-only rapide) ou `--cpu-only` (désactive le profil GPU Compute HLSL).

### Interpréter les mesures

Les rapports séparent chargement, compilation de scène, textures/PBR, construction BVH et rendu. Ils produisent médiane/P50, P95/P99, dispersion et FPS estimés lorsque le protocole s’y prête. Une construction BVH plus rapide ne signifie pas nécessairement un rendu plus rapide.

Les benchmarks d’aperçu lisent les pixels hors écran : ils ne mesurent ni la présentation native swapchain ni le rafraîchissement de l’écran. Le temps de soumission CPU en affichage direct n’est pas le temps réel d’exécution GPU. La CI Windows peut utiliser WARP, un GPU logiciel ; ses résultats ne prédisent pas les FPS d’une Intel HD 620 ou d’une carte matérielle. Pour comparer deux versions, garder ordinateur, scène, caméra, résolution, backend, qualité et protocole identiques, puis répéter les mesures.

## Validation et limites

Le workflow Windows x64 compile V16 et V17, vérifie le paquet, charge et rend la scène publiée, puis produit les benchmarks et images de contrôle. Une CI verte valide les chemins exécutés sur le runner, pas la fluidité sur le GPU de l’utilisateur. Vulkan et LumaTrace restent expérimentaux ; les performances matérielles, la parité visuelle complète et les coûts de présentation native doivent être vérifiés séparément.

La version **LumaTrace BVH4** est un backend d’éclairage GPU expérimental. Elle ne remplace pas le moteur CPU complet et ne constitue pas encore un path tracer multi-rebonds. Les résultats de benchmark doivent donc être lus comme des chemins distincts, pas comme une comparaison de qualité physique universelle.

## Organisation du dépôt

- `v17/` : interface, intégration V17, backends, shaders, gestionnaire de scène et outils.
- `v16/` : exécutable historique, benchmark et configuration du noyau partagé.
- `engine/render_modules.json` : registre des modules partagés, versions et dépendances.
- `scenes/v16/` : scènes/fixtures internes de test et benchmark ; la scène de référence actuelle est publiée séparément puis copiée dans le paquet compilé.
- `experiments/` : code moteur partagé et prototypes conservés pour comparaison.
- `.github/workflows/` : compilation, tests de chargement/rendu, packaging et publication des artefacts.
- `docs/` et `v17/docs/` : architecture, validation, outils et protocoles de benchmark.

Références : [Guide moteur V17](v17/README.md) · [Full Benchmark](v17/docs/FULL_BENCHMARK.md) · [Version moteur](v17/version.json) · [Pistes d’optimisation raster](v17/docs/RASTER_PERFORMANCE_ROADMAP.md).

## Benchmark de déplacement de 30 secondes

1. Placer `LANCER_PARCOURS_30S.bat` à côté de `moteur3d_v17.exe`, dans le package Windows V17 décompressé avec ses DLL et ses plugins Qt. Le même BAT se réutilise à chaque campagne. [Télécharger le lanceur](https://raw.githubusercontent.com/glaby91/moteur3d/benchmark/luminosity-audit-v1/v17/tools/LANCER_PARCOURS_30S.bat).
2. Double-cliquer sur le BAT. Il récupère le script PowerShell sur GitHub. Le script consulte la dernière publication de `glaby91/moteur3d-scene`, télécharge `V17-scene.zip` au premier lancement et affiche la révision chargée. La scène du package peut être absente ou incomplète. Le benchmark extrait sa propre copie avec les textures. Il conserve les archives dans `benchmark-tools/archives`, en contrôle la taille et le SHA-256 à chaque lancement et télécharge à nouveau toute archive incomplète. [Publications de la scène](https://github.com/glaby91/moteur3d-scene/releases/latest).
3. À la fin des trois parcours, récupérer `benchmark-results/parcours30-<date>.zip`. Il contient `SCENE_VERSION.json` et `resultats.csv` avec le nombre d’images terminées en 30 secondes, les FPS et le temps moyen par rendu, dernière image commencée comprise. Si un backend échoue, la console donne le dossier des résultats partiels.

DX11 Raster, Vulkan Raster et LumaTrace suivent successivement le même déplacement latéral sinusoïdal depuis la caméra piscine/fresque, à 14 h 45 et en 1000 × 680. La position dépend du temps écoulé. Chaque backend dispose de deux images d’échauffement puis de 30 secondes mesurées. La mesure inclut la relecture GPU vers CPU. Prévoir environ 90 secondes plus le chargement et les échauffements.
