# CSE WARP — moteur 3D V17

**Moteur : V17.024 · scène principale : V17.631**
CSE WARP V17 réunit l’interface d’exploration, plusieurs backends de rendu, le noyau moteur partagé avec V16, les outils de benchmark et un mécanisme de mise à jour de scène. La V17 reste une version expérimentale : les performances et la présentation native doivent être mesurées sur le GPU visé.

> La branche principale du moteur est `master`. [État des branches](BRANCH_STATUS.md)

[Audit historique et registre des optimisations (HTML → V17)](#audit-historique-et-registre-des-optimisations--26-novembre-2026)

## Scène de référence et paquet Windows

**[Compilations Windows x64 de master](https://github.com/glaby91/moteur3d/actions/workflows/build-main-v17.yml)**

Extraire le ZIP dans son dossier de build nommé, puis lancer `LANCER_CSE_WARP.bat`. Le paquet contient le moteur et les outils, **pas la scène**, qui conserve son propre dépôt et son versioning. Au démarrage, le moteur vérifie la publication officielle avant de charger la scène locale ; en cas d'indisponibilité réseau, la scène locale reste utilisable. En l'absence de scène locale, accepter son téléchargement. Les vérifications suivantes sont asynchrones et proposent une mise à jour sans redémarrage forcé. Ctrl maintenu : accroupissement ; Espace : saut ; Tab ou < brièvement : Pip-Boy ; F2 : HUD.

- [Scène principale V17.631](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-reflect-V17.631) : géométrie V17.629, politique optique et caches BVH4/index schema 2 précalculés.
- [Dernière scène officielle](https://github.com/glaby91/moteur3d-scene/releases/latest)
- [Validation matérielle HD620 — V17.022/023](v17/docs/CSP001_ZREFLECT_V17023_VALIDATION.md).

Pour retrouver une ancienne compilation, consulter [V17.006, build 683 avec scène V17.623](https://github.com/glaby91/moteur3d/actions/runs/45493998985) ou [V17.003, build 587](https://github.com/glaby91/moteur3d/actions/runs/45080722276).

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
| **LumaTrace BVH4 — éclairage GPU + Z Reflect** | Raster DX11 + Compute GPU BVH4, éclairage direct/indirect par sondes, réflexions planaires | Backend hybride temps réel, pas un path tracer multi-rebonds ni DXR. Z Reflect activé par défaut. |

Sur Windows, l’option **Affichage GPU direct natif** permet aux backends DX11, Vulkan et LumaTrace de présenter dans leur propre fenêtre. Désactivez-la pour utiliser la sortie Qt/QImage. Vulkan peut rester indisponible si le SDK n’était pas présent à la compilation ou si le pilote ne permet pas son initialisation. Le menu indique l’état du backend.

## LumaTrace — architecture hybride GPU

LumaTrace s'appuie sur le pipeline raster Direct3D 11 pour la visibilité principale, les matériaux PBR, les textures, la profondeur et la présentation native. Les calculs massifs restent sur le GPU : compute HLSL, parcours BVH4, radiance directe par triangle et éclairage indirect approximé par sondes spatiales. La voie de sondes utilise deux passes et n'est pas un path tracer à rebonds arbitraires. Les données de scène et buffers indexés restent autant que possible résidents sur GPU ; en présentation native, aucune lecture des pixels vers le CPU n'est nécessaire. Le CPU prépare la géométrie, les index, les sélections et commandes ; il ne recalcule pas une image GPU. Les caches précalculés de la scène réduisent le travail au chargement. Le module adaptatif de budget 0/1 reste optionnel (`M3D_V17_LUMA_ADAPTIVE=1`), pas activé arbitrairement.

### Z Reflect — réflexions planaires activées par défaut

1. Le catalogue optique et son index spatial identifient les surfaces autorisées par le créateur de scène. La politique distingue AUTO et exclusions ; l'ascenseur est exclu de la réflexion planaire. La sonde partagée de l'ascenseur reste une déclaration, pas un rendu actif.
2. Le CPU interroge les plans visibles, calcule leur surface projetée réelle (triangles découpés), réutilise le résultat à caméra strictement identique et retient les plans admissibles. Le HUD affiche `ZR ON actifs/total` ; les plans différés sont instrumentés. V17.631 comporte 5 plans, 121 objets candidats, 29 vues de référence ; 2 cibles partagées à demi-résolution sont disponibles à l'exécution. Ce plafond de ressources ne limite pas le catalogue.
3. Une caméra miroir est calculée pour chaque plan actif. Eau et vitrage utilisent les mêmes cibles réutilisables. Frustum et filtrage CPU conservatif des blocs entièrement derrière le plan réduisent la géométrie soumise. Les passes secondaires sont opaques, sans récursion. Le Z Prepass principal reste actif ; le prépass miroir est désactivé par défaut (configuration C validée).
4. La texture est composée dans le shader selon Fresnel, matériau et, pour l'eau, distorsion. Ce sont des reflets planaires, pas de vraie réfraction multi-interface ; les miroirs non planaires et sondes partagées ne sont pas implémentés.

Le coût n'est pas nul : une caméra miroir rasterise et éclaire une partie de la scène une seconde fois. En 720p HD620, la campagne V17.022 a retenu C : 21,71 FPS moyens sur quatre vues contre 19,63 pour B ; 1,77 ms GPU de reflet piscine et 18,07 ms façade. Les valeurs ne garantissent pas les FPS de V17.024. Objectif à terme : conserver la crédibilité visuelle (notamment silhouettes d'arbres dans les vitrages) pour un coût marginal, idéalement 5–10 % du budget image. Z Warp (rayons ciblés en escargot, matrices adaptatives) est une piste différée [#94](https://github.com/glaby91/moteur3d/issues/94), non implémentée.

| Option | Défaut / effet |
|---|---|
| `M3D_V17_ZREFLECT` | ON en LumaTrace ; 0 désactive les vitrages planaires, l'eau reste active |
| `M3D_V17_ZREFLECT_CPU_CULL` | 1, filtrage conservatif miroir |
| `M3D_V17_ZREFLECT_PREPASS` | 0, prépass miroir seul |
| `M3D_V17_DISABLE_Z_PREPASS` | 0, contrôle global (ne pas confondre avec le prépass miroir) |
| `M3D_V17_ZREFLECT_GPU_TIMING` | 1 pour activer les timestamps GPU non bloquants de réflexion native |

Benchmarks : `LANCER_PARCOURS_30S.bat` (DX11/Vulkan/LumaTrace), `LANCER_ZREFLECT_AB.bat` (16 mesures), `LANCER_ZREFLECT_MATRIX.bat` (32 mesures A/B/C/D), `LANCER_LUMATRACE_01_AB.bat` (circuit complet), `LANCER_LUMATRACE_01_PISCINE.bat`. Les scripts téléchargent la scène officielle et vérifient ses empreintes ; le CSV mesure les FPS, la sélection/soumission CPU, le temps GPU cumulé des reflets et les triangles par plan. SYSTEM_INFO.json identifie le matériel, la compilation et les options.

Sources : [validation C](v17/docs/CSP001_ZREFLECT_V17023_VALIDATION.md), [matrice A/B/C/D](v17/docs/CSP001_ZREFLECT_V17022_MATRIX.md), [module adaptatif](v17/docs/LUMATRACE_ADAPTIVE_001.md). La CI principale vérifie les caches de la scène V17.631, les sélections, les quatre configurations et la parité pixel par pixel sur quatre vues WARP, ainsi que les plans de benchmark et les commandes d'exploration. L'ancien workflow exhaustif demeure disponible manuellement ; les tests matériels restent indispensables.

## DirectX 11 — R001 validé sur HD620 (V17.025)

Branche de développement validée, moteur principal master V17.024 inchangé. R001 reste ON par défaut pour DX11 Raster (A/B : `M3D_V17_DX11_R001=0`). AABB conservatif, masques partagés profondeur/couleur, filtrage des lumières hors champ ou sans énergie, suppression des lectures d'ombre à daylight nul, contrôles des limites. LumaTrace et la scène V17.631 ne sont pas modifiés.

Benchmark HD620, 1366×768, MSAA 2x, 4 × 90 images OFF/ON/ON/OFF, trajectoire déterministe, preview matériel avec readback : **14,3576 → 16,7388 FPS (+16,59 %)** ; GPU draw 55,94 → 44,20 ms ; 12 captures PNG identiques par indice. CI Windows et huit tests WARP jour/nuit réussis. [Rapport R001](v17/docs/DX11_R001.md) · [Données comparatives](v17/docs/benchmarks/DX11_R001_COMPARAISON.csv) · [Compilation validée](https://github.com/glaby91/moteur3d/actions/runs/46008251282).

**Pause décidée le 27/11/2026 :** pas de R002 ni d'autre chantier engagé. Conserver Geometry Prep, ordre des triangles, rendu indexé, instancing et Z Prepass ; aucune réorganisation de géométrie sans preuve préalable. Ombres GPU, backface sélectif, Z Light par tuiles, compression et transparences restent des pistes différées. Simulations et 3–5 vérifications avant toute compilation d'une future modification. Ne pas fusionner automatiquement vers master.

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

La compilation de référence est le workflow [CSE WARP V17 — Main Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-main-v17.yml). Il construit l’application, les backends disponibles et les outils, vérifie la scène de référence épinglée et produit un artefact moteur sans scène.

Pour compiler localement, installer Visual Studio 2022 avec C++ x64, CMake, Qt 6.8.3 (module Multimedia) et Vulkan SDK 1.3 ou ultérieur. Depuis la racine du dépôt :

```powershell
cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DM3D_REQUIRE_VULKAN=ON
cmake --build build-v17 --config Release --parallel 4
```

Le workflow principal télécharge la scène de référence validée pour les tests, sans la copier dans le paquet final. Une compilation locale seule n’exécute pas automatiquement toutes les étapes de contrôle et de packaging Windows de la CI.

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

La version **LumaTrace BVH4** est un backend hybride d’éclairage GPU et de réflexions planaires. Elle ne remplace pas le moteur CPU complet et ne constitue pas encore un path tracer multi-rebonds. Les résultats de benchmark doivent donc être lus comme des chemins distincts, pas comme une comparaison de qualité physique universelle.

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

1. Placer `LANCER_PARCOURS_30S.bat` à côté de `moteur3d_v17.exe`, dans le package Windows V17 décompressé avec ses DLL et ses plugins Qt. Le même BAT se réutilise à chaque campagne. [Télécharger le lanceur](https://raw.githubusercontent.com/glaby91/moteur3d/master/v17/tools/LANCER_PARCOURS_30S.bat).
2. Double-cliquer sur le BAT. Le paquet V17.009 inclut le script PowerShell, `route30.exe` et le noyau partagé utilisé par l’exploration. Un BAT téléchargé seul récupère le script sur GitHub. Le script consulte la dernière publication de `glaby91/moteur3d-scene`, télécharge `V17-scene.zip` au premier lancement et affiche la révision chargée. La scène du package peut être absente ou incomplète. Le benchmark extrait sa propre copie avec les textures. Il conserve les archives dans `benchmark-tools/archives`, en contrôle la taille et le SHA-256 à chaque lancement et télécharge à nouveau toute archive incomplète. [Publications de la scène](https://github.com/glaby91/moteur3d-scene/releases/latest).
3. Pendant la campagne, garder la fenêtre de rendu visible. À la fin, récupérer `benchmark-results/parcours30-<date>.zip`. Il contient `SCENE_VERSION.json` et `resultats.csv`. Si un backend échoue, sa ligne indique l’erreur, les autres parcours continuent et le ZIP conserve les mesures réussies.

DX11 Raster, Vulkan Raster et LumaTrace présentent directement les images dans une fenêtre de **1280 × 720 pixels**, via DXGI ou la swapchain Vulkan. La synchronisation verticale est activée sur les trois backends (intervalle 1 pour DXGI, FIFO pour Vulkan). Le benchmark contrôle à chaque image que le moteur rapporte zéro tampon de relecture. Les trois passages suivent le même mouvement latéral sinusoïdal devant la piscine et la fresque, à 14 h 45. Chacun commence par deux images d’échauffement avant les 30 secondes chronométrées.

Dans le CSV, `frames_completed_30s` compte les appels de présentation réussis avant la fin des 30 secondes et `fps_30s` vaut ce nombre divisé par 30. `mean_render_ms` mesure le temps écoulé côté application, y compris l’attente éventuelle de présentation et la dernière image commencée. Les mesures valides portent `presentation=native_swapchain`, `vsync=on` et `readback_buffers=0`. L’ancien parcours utilisait 1000 × 680 avec relecture GPU vers CPU. Le changement simultané de résolution et de présentation empêche d’en déduire le coût de la copie seule.

La préparation des triangles statiques est **activée par défaut** dans l’exploration et dans ce parcours pour DX11, Vulkan et LumaTrace. Elle a lieu avant les 30 secondes chronométrées. Dans `resultats.csv`, `geometry_prep_requested=on` indique la demande et `geometry_prep_enabled=true` confirme son application par le backend. `geometry_source_vertices` et `geometry_unique_vertices` comptent les sommets avant et après préparation. `geometry_index_count` donne le nombre d’indices du maillage préparé. Les compteurs restent vides si le backend échoue.

**Validation du 26 novembre 2026.** Sur la scène V17.629, le parcours piscine de 30 s en 720p a confirmé `geometry_prep_enabled=true` et `readback_buffers=0` pour les trois backends. DX11 a présenté 691 images (23,03 FPS), Vulkan 884 (29,47 FPS) et LumaTrace 815 (27,17 FPS). Ces valeurs correspondent à un passage par backend sur la machine de test. [Révision du parcours validé](https://github.com/glaby91/moteur3d/commit/900bf11df4833e881141682741ae814202cf2403).

Pour comparer avec la préparation désactivée, enregistrer le script à côté du BAT et lancer `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\route30.ps1 -Root . -GeometryPrep off` depuis ce dossier. Les deux modes utilisent le même exécutable et le même parcours. [Script du parcours](https://raw.githubusercontent.com/glaby91/moteur3d/master/v17/tools/route30.ps1).

## Audit historique et registre des optimisations — 26 novembre 2026

**Périmètre :** origines BASIC (2019), premiers moteurs HTML/JavaScript d’octobre 2026, V9–V13.16, portage natif V14–V15, V16 et V17 jusqu’à V17.010. Audit documentaire et statique, sans nouvelle compilation ni mesure. Les résultats anciens restent liés à leur scène, leur matériel et leurs paramètres. Ce registre distingue les acquis, les prototypes, les essais décevants et les propositions ; il ne certifie pas la présence de chaque optimisation dans la tête actuelle.

### Historique algorithmique

| Génération | Travaux et enseignements |
|---|---|
| BASIC 2019 | Triangles, couleurs, éclairage, lecture BMP ; patrimoine conservé. |
| HTML initial (18 octobre) | Canvas, tri par profondeur moyenne, 32 buffers RGB565, prévisions N+1 à N+5, collisions simplifiées, patrouille. Les buffers futurs ne sont pas des rendus complets validés. |
| Anti-stuttering | Tuiles 16×16, réutilisation des régions stables lorsque caméra et lumière sont fixes ; pré-rendu concurrent et reprojection Z désactivés après artefacts. Le compteur de copie Canvas (~40 ms) ne mesure pas la reprojection seule. |
| V3–V4 | Carte Z lumineuse unique, cache statique, PCF, interpolation de deux cartes d'ombre pour stabiliser le mouvement. |
| V5 | Z-buffer caméra par pixel, clipping proche, framebuffer RGBA ; test d'inversion de 2 310 triangles : 0 pixel différent sur 76 800 pour la pose testée. |
| V6–V7 | Stress de tessellation ×10 (24 100 triangles), arbre détaillé, PCF 4×4, lissage de contours optionnel ; réutilisation du maillage natif pour les ombres. |
| V8–V8.1 | RGB565, mipmaps 8–4096, UV perspective-correctes, filtrage, LOD par tuile et face ; suppression de subdivisions coplanaires ; Z puis texturage des seuls pixels visibles. |
| V9–V9.2 | Ray casting analytique, BVH, candidats par tuile, un rayon par pixel interne ; arrêt anticipé PCF, cache matériaux, suppression d'allocations, LUT de brouillard ; ciel interpolé et résolution interne optionnelle. |
| V9.3 | Banc N+1 Web Workers synthétique, étude du rendu par frontières de visibilité, cache de surfaces et reprojection contrôlée. |
| V11–V13.16 | Scènes Côte été GT/restomod, nombreuses révisions HTML, modes FHD/upscale, contrat de backend CPU V13.15, workers V13.16 ; détail de chaque diff intermédiaire restant à auditer. |
| V14–V15 | C++/Qt/Win32, BVH, tuiles multicœurs, SIMD AVX2/FMA3, AABB, PBR/HDR, débruitage, Compute HLSL, DX11 raster, détection DXR. |
| V16 | Scènes modulaires, matériaux PBR, GGX/Schlick, espace linéaire, ACES/sRGB, GI CPU/GPU, cache indirect à demi-résolution, comparaison des backends. |
| V17 | Exploration, instancing, LOD, Z-prepass, light culling, sondes et tables GI, Vulkan, présentation GPU native, LumaTrace BVH4, cache de radiance, préparation géométrique indexée, scène séparée et versioning modulaire. |

Archives : [HTML et expérimentations](experiments/), [V9.2 archivée](https://github.com/glaby91/moteur3d/tree/archive/master-v9-before-warp-v17-261114), [V11–V13.16](experiments/2026-10-30-cote-ete-gt/), [audit V15](docs/audits/AUDIT_GLOBAL_MOTEUR3D_V15_2026-11-06.md), [audit V17 du 14 novembre](docs/audits/AUDIT_GLOBAL_CSE_WARP_FONCTIONNALITES_OPTIMISATIONS_2026-11-14.md), [audit du 18 novembre](docs/AUDIT_GLOBAL_MOTEUR3D_2026-11-18.md).

### Résultats historiques à préserver

| Expérience | Résultat | Limite / décision |
|---|---|---|
| V8.1 Z puis textures | 1 911 → 925 ms à 800×600 ; Z identique, erreur couleur max 2/255 | DOM/Canvas simulé ; conserver séparation visibilité/shading. |
| V9.1 shading | 5 824 → 5 039 ms | Banc simulé ; conserver les courts-circuits exacts et caches. |
| V9.2 demi-résolution | 10 932 → 2 915 ms à 1920×1200 affiché | Banc simulé, perte de résolution interne assumée. |
| N+1, i5-7200U | 3 758 ms sans worker ; 4 428 / 4 904 / 5 733 ms avec 1/2/3 workers | Charge synthétique concurrente, pas de vraies images candidates ; ne pas multiplier les workers aveuglément. |
| Rapture CPU FHD | 21,14 / 11,33 / 6,10 / 5,98 s avec 1/2/4/8 threads | Seulement ~2 % gagnés de 4 à 8 ; pool adapté au matériel. |
| AABB rejet anticipé | 12–16 % sur un relevé | Reprofiler le chemin actuel. |
| Feuilles AVX2 de 8 triangles | ~33 % de tests d'objets supplémentaires, gain non convaincant | Ne pas généraliser ; comparer feuilles sœurs et scalaire. |
| CPU GI Pavillon FHD | 4,03 s à 1 spp ; 15,87 s à 4 spp ; 94,91 s à 16 spp/2 rebonds | Le grain à 64 spp restait signalé ; débruitage guidé à étudier. |
| Triple buffering | +3,84 % débit moyen ; latence P50 35,60 → 91,58 ms | Ne pas imposer en exploration. |
| Z-prepass HD620, 1366×768 MSAA2 | GPU draw 64,41 → 39,94 / 41,18 ms ; débit calcul +50–52 % | Trois PNG identiques par campagne ; trajet ciblé, pas toutes caméras. |
| Multirésolution périphérique | ~12 % sur WARP dans une variante avec artefacts | Sous le seuil historique de 15 %, pas de généralisation. |
| Parcours 30 s, 720p, scène V17.629 | DX11 23,03 FPS ; Vulkan 29,47 FPS ; LumaTrace 27,17 FPS | Mesure documentée le 26/11, un passage par backend, préparation ON et zéro readback ; pas une comparaison ON/OFF. |

Sources : [V8.1](experiments/2026-10-19-v8-1-optimized-raster/README.md), [V9.2](experiments/2026-10-19-v9-2-fast-sky-16x10/README.md), [N+1](experiments/2026-10-29-v92-workers-4k-target/README.md), [Z-prepass HD620](docs/performance/V17.432-Z-PREPASS-RESULTATS-HD620-20261113.md), [campagne historique](docs/audits/AUDIT_GLOBAL_CSE_WARP_FONCTIONNALITES_OPTIMISATIONS_2026-11-14.md).

### Registre des pistes d'amélioration

Priorités proposées : P0 = fidélité/robustesse bloquante ; P1 = optimisation prioritaire sans perte visuelle ; P2 = expérimentation ciblée ; R = recherche séparée. Les statuts historiques ne valent pas certification de la V17.010.

| Domaine | Piste | État / action |
|---|---|---|
| Visibilité | Z-buffer, clipping, Z-prepass | Acquis ; conserver test d'invariance à l'ordre des faces opaques, vérifier tous backends. |
| Visibilité | Frustum/AABB, rejet spatial exact, tri opaque front-to-back | P1 : mesurer CPU/GPU, préserver les objets et ombres hors champ utiles. |
| Visibilité | Occlusion hiérarchique (Hi-Z) | P1 : prototype conservateur, repli pour cas ambigus, mesurer le coût. |
| Géométrie | Indexation, préparation statique, buffers persistants | P1 : préparation documentée active DX11/Vulkan/LumaTrace ; A/B à scène et parcours identiques. |
| Géométrie | Instancing, regroupement matériaux, LOD projeté | P1 : vérifier présence et efficacité, sans modifier silhouettes ni transparences. |
| Géométrie | Suppression subdivisions coplanaires redondantes | P1 : préserver normales, UV, matériaux, frontières, collisions et identifiants. |
| Géométrie | Tessellation adaptative près des sources | P2 : mesurer le compromis radiance/coût ; ne pas densifier partout. |
| Lumières | Boucle globale, limite de sources, rejet spatial | P1 : conserver les contributions réelles ; ne pas supprimer des lampes pour afficher un gain artificiel. |
| Lumières | Frustum des sphères d'influence (PR 44) | Prototype ; benchmark sur master actuel. |
| Lumières | Masques par tuile 32×32 (PR 45) | Prototype ; capacité historique 256 sources, mesurer coût CPU/upload. |
| Lumières | Z-light culling / tiled ou clustered lighting | P1 : exploiter profondeur min/max du Z-prepass, vérifier transparences et absence de lumière perdue. |
| Ombres | Cache carte Z, PCF anticipé, interpolation temporelle | P1 : réévaluer les optimisations V4/V9.1, mesurer lectures et stabilité. |
| Matériaux | RGB565 historique, mipmaps, UV, cache PBR disque | P1 : contrôler formats, bande passante, intégrité et cache sans baisser la qualité. |
| Matériaux | Normal mapping, GGX, Fresnel, transmission | P0 : parité et classification eau/verre, normales GLB, texture source, orientation et énergie. |
| Matériaux | Reflet piscine / caméra miroir | P0 : vérifier plan, matrice miroir, projection, clipping, coordonnées et dépendance caméra ; cause non confirmée. |
| Éclairage | Sondes 3D (16×8×16), 25 états horaires, table GI CPU | P2 : vérifier cache et fuites lumineuses, interpolation temporelle et cohérence d'énergie. |
| Éclairage | GI GPU à demi-résolution | P2 : reconstruction guidée par profondeur/normales ; comparer au rendu complet. |
| Éclairage | Débruitage CPU/GPU | P2 : guides albédo/normales/profondeur en HDR avant tone mapping ; préserver les détails. |
| LumaTrace | BVH4, cache de radiance par triangle, quatre rayons diffus déterministes | P1 : profiler premier dispatch et réutilisation ; comparer au CPU de référence. |
| LumaTrace | Regroupement des émissifs en cellules de 4 m, jusqu'à 16 sources d'appoint | P1 : valider projection des lampes, ombres, énergie, éviter double comptage. |
| LumaTrace | Invalidation locale / progressive du cache, jour-nuit | P1 : distinguer géométrie, matériaux, lumière locale et soleil ; cohérence temporelle. |
| LumaTrace | Interpolation de radiance, échantillonnage près des lampes, ombres de contact | P2 : vérifier fuites et discontinuités, budget de rayons fixe. |
| LumaTrace | Transparences, spéculaire, deuxième rebond sélectif | P0 pour transparence ; P2 pour transport additionnel. Pas un path tracer complet. |
| LumaTrace | BVH compact/quantifié, cache disque, calcul asynchrone | P2 : mesurer précision, mémoire, synchronisation et latence. |
| CPU | Pool persistant, ordonnancement des tuiles, localité mémoire | P1 : adapter le nombre de workers au matériel. |
| CPU | SIMD, SoA, AVX2/FMA3, feuilles sœurs, sélection adaptative | P2 : comparer au scalaire, ne pas présumer un gain. |
| GPU | Ressources/shaders persistants, atlas, formats compacts | P1 : mesurer allocations et transferts, éviter recompilation par frame. |
| GPU | Présentation DXGI / Vulkan WSI, zéro readback | Acquis documenté ; vérifier fallback Qt, resize, shutdown, temps GPU et latence écran. |
| GPU | 1/2/3 buffers, staging, UMA, frames in flight | P2 : privilégier fluidité et latence, pas seulement débit. |
| GPU | Vulkan vs DX11, durée de vie des ressources | P1 : profiler les mouvements et le cycle jour/nuit, vérifier absence de fuite. |
| GPU | MSAA, resolve, copy, anisotropie | P1 : instrumenter séparément ; conserver qualité de référence. |
| GPU | Multirésolution périphérique, résolution interne | P2 : options explicites, pas d'activation silencieuse. |
| Temporel | 32 buffers et prédiction N+1–N+5 | R : préférer anticipation ciblée des ressources plutôt que calcul spéculatif concurrent massif. |
| Temporel | Reprojection, régions stables, cache de surfaces | R : validation profondeur/identifiants, désocclusion et repli exact ; éviter les artefacts historiques. |
| Recherche | Rendu par frontières de visibilité | R : prototype analytique réduit, mesurer coût des frontières et pixels ambigus avant extension. |
| Recherche | PHOTON-RT adaptatif | R : le prototype PNG et le mode natif uniforme 25 % sont distincts ; véritable ordonnanceur adaptatif à valider. |
| Recherche | OpenGL, DXR, FPGA/Z80 | R : conserver les prototypes ; DXR détecté mais pas de backend matériel validé ; FPGA nécessite étude des transferts. |
| Scène | Photo2Scene, multivues, Gaussian Splatting | R : reconstruction d'assets distincte du rendu ; vérifier métrique/provenance. |
| Robustesse | Jour/nuit, ciel interpolé, radio asynchrone | P1 : profiler les saccades, découpler les tâches longues et les invalidations. |
| Robustesse | Scènes/versioning/manifestes, tests Release, packaging | P1 : tester réellement le démarrage, les contrôles et les transitions ; ne pas dépendre d'assertions désactivables. |
| Robustesse | Linux, licences et provenance des assets | P2 : qualifier avant distribution. |

Références de recherche : [rendu par frontières](experiments/2026-10-29-v92-workers-4k-target/CONCEPTION_RENDERING_PAR_FRONTIERES.md), [PHOTON-RT](docs/PHOTON_RT_ADAPTIVE.md), [CPU/FPGA](docs/CPU_FPGA_RENDER_BACKEND.md), [roadmap raster](v17/docs/RASTER_PERFORMANCE_ROADMAP.md), [LumaTrace one-bounce](v17/docs/V17_604_LUMATRACE_ONE_BOUNCE.md), [émissifs V17.008](v17/docs/V17_008_LUMATRACE_EMISSIVE_LIGHTS.md).

### Inventaire des PR historiques (77 recensées)

| Famille | Numéros |
|---|---|
| Temporel | 1 |
| CPU/PBR/Compute | 2, 3, 5, 6, 7 |
| Architecture V16 | 8 |
| Photo2Scene | 9–15 |
| Parité CPU/GPU | 16–20 |
| Exploration DX11/OpenGL | 21, 22 |
| Cache PBR/ombres | 23 |
| Présentation/versioning | 26 |
| GI et énergie | 27, 28, 30, 31 |
| Navigation/distribution | 29, 32 |
| Scène | 33, 35–38, 40, 43, 46, 49 |
| Raster/lumières | 39, 41, 42, 44, 45 |
| Z-prepass | 47, 48 |
| HUD/audio/navigation | 51, 52, 53, 55 |
| Anomalies | 56–60 |
| Profilage/multirésolution | 61–63 |
| Promotion master/enseigne | 64, 65 |
| Vulkan/présentation native | 69, 71–73 |
| Scène/GI/matériaux | 74–77 |
| Versioning/HUD/radio | 78 |
| LumaTrace et consolidation | 82–88 |
| Préparation géométrique | 89, 90 |

[Liste complète des pull requests](https://github.com/glaby91/moteur3d/pulls?q=is%3Apr). Une PR ouverte peut avoir été reprise dans une branche cumulative : vérifier son diff et son ascendance avant réintégration. L'inventaire n'est pas une revue ligne par ligne des 77 diffs.

### Feuille de route proposée

1. Geler la référence moteur/scène/build et reproduire les anomalies de lumière, réflexion piscine et transparence.
2. Comparer préparation géométrique ON/OFF et Z-prepass ON/OFF sur le même parcours de 30 s, puis élargir à plusieurs caméras.
3. Mesurer le Z-light culling et les masques de lumières par tuile avec contrôle strict de fidélité.
4. Profiler l'invalidation LumaTrace au cycle jour/nuit ; expérimenter la mise à jour locale sans blocage.
5. Réduire le coût de shading et de bande passante à qualité constante ; contrôler les temps GPU réels, P50/P95/P99 et latence de présentation.
6. Reprendre PHOTON-RT, rendu par frontières et reprojection dans des branches de recherche isolées ; ne les promouvoir qu'après validation visuelle et matérielle.

Objectif HD620 : 40–45 FPS (budget ~25–22,2 ms par image), non acquis. Objectif historique 4K/100 FPS : 10 ms par image, recherche à long terme sur matériel adapté. Aucun gain futur ne doit être annoncé avant mesure. Ne pas additionner les gains de campagnes hétérogènes ni assimiler WARP logiciel à un GPU matériel.

État relevé le 26/11 : [code V17.010](https://github.com/glaby91/moteur3d/commit/ef5461e06a9b4326620f451575b1599f70426066), [PR 90 / V17.009](https://github.com/glaby91/moteur3d/pull/90), scène [V17.629 build 42](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.629-42). Le code le plus récent ne constitue pas à lui seul une preuve de publication d'un package validé.

### V17.026 — correctif matériaux émissifs DX11/LumaTrace
Les déclarations d'émission de la scène sont désormais transmises au shader raster partagé. [Rapport et tests](v17/docs/DX11_EMISSION_V17026.md). R001 reste actif ; autres développements suspendus.
