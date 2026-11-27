# CSE WARP Beta X — V17

**Révision moteur : V17.031 — Z Material 001 et profil de démarrage piscine V17.634 · scène CI V17.631.** [Z Material](docs/ZMATERIAL_001.md). [Validation L01/L02](docs/L01_SOLAR_PCF_V17029.md). Les fenêtres moteur et rendu dédié affichent les deux révisions. Le paquet Windows contient le moteur et ses outils ; la scène est versionnée et distribuée séparément. LumaTrace utilise maintenant les surfaces lumineuses de la scène comme sources locales et réutilise leur éclairage calculé par triangle.

**V17 réunit l’exploration temps réel, les backends CPU/GPU et le noyau partagé avec V16.** DX11 Raster est le choix temps réel général ; Vulkan et LumaTrace sont expérimentaux. Le chemin de présentation GPU directe est disponible sous Windows, avec Qt/QImage comme solution de repli. La CI WARP ne mesure pas les performances du GPU matériel.

V16 et V17 utilisent le même manifeste de modules de rendu : [`engine/render_modules.json`](../engine/render_modules.json). Il donne une révision par module ; CMake contrôle les entrées et place le manifeste dans le paquet. Une correction ciblée doit faire évoluer la révision du seul module concerné.

## Modules de rendu

Le registre commun garde une organisation courte par responsabilité :

- **DX11 Raster** : construction des maillages CPU (`d3d11_mesh`), shaders et orchestration/presentation réparties entre les modules communs et raster.
- **Vulkan Raster** : maillage, politique de mise à jour, mipmaps, lecture d’image et cycle de vie des buffers/images (`vulkan_resources`).
- **LumaTrace CPU** : module `lumatrace_cpu`, sans dépendance graphique. Il convertit le BVH partagé en BVH4, valide les primitives et prépare la correspondance et l’ordre des triangles transmis au calcul.
- **LumaTrace GPU Compute DX11** : module `lumatrace_gpu_compute`, distinct du CPU. Il possède les buffers GPU, le shader, le cache des radiances et le dispatch. Le raster DX11 affiche ensuite la géométrie éclairée.
- Le rendu CPU PBR complet existe dans `cpu_renderer`, `cpu_intersection` et `cpu_lighting`, mais ce n’est pas un traceur LumaTrace CPU : LumaTrace reste un prototype d’éclairage GPU et n’a pas encore de noyau CPU de parcours et de calcul de radiance.

Les dépendances et révisions sont inscrites dans le même manifeste. Les modules optionnels Vulkan et LumaTrace Compute ne sont ajoutés au build que lorsque leur backend est disponible. V16 et V17 assemblent les mêmes modules partagés ; ils ne maintiennent pas deux copies indépendantes du moteur.

## Validation historique V17.600

Le commit moteur `ceaa6a6` (V17.600) a passé la [CI Windows x64, run 44437496051](https://github.com/glaby91/moteur3d/actions/runs/44437496051) : compilation Release avec Vulkan, contrôles du paquet, validations du chargeur de scènes, rendus de contrôle WARP, audit de rayons de la fresque et suite de régression V16/V17. L’artefact Windows est produit par GitHub Actions.

Cette validation porte sur la compilation, les contrôles automatisés et les chemins de rendu exécutés en CI. Les rendus DX11 concernés utilisent WARP, l’adaptateur logiciel de Microsoft. Ils ne valident pas les performances sur une carte Intel ou NVIDIA, la fluidité en mouvement sur le Dell HD620, ni le rythme d’affichage du swapchain natif. Les images sont contrôlées automatiquement pour leur présence et leur intégrité ; cela ne remplace pas une inspection visuelle.

Aucun gain de performances BVH n’est établi par le seul statut « CI verte ». Pour comparer deux versions, utiliser le même ordinateur, la même scène, la même caméra, la même résolution, le même backend et les mêmes réglages ; répéter les mesures et comparer les médianes et percentiles. Séparer le temps de construction du BVH du temps de rendu. Pour DX11/Vulkan en présentation directe, le compteur CPU de soumission ne constitue pas une mesure de fin d’exécution GPU ni du temps de balayage écran. Voir [Full Benchmark](docs/FULL_BENCHMARK.md) et le README principal pour les commandes et options complètes.

## Exploration et backends disponibles

- `v17/moteur3d_v17_qt.cpp` : fenêtre V17, mode Exploration, ZQSD/flèches, souris, Maj, accroupissement, saut, échelles de piscine, Pip-Boy, radio et sélection du backend. Voir le README principal pour la table complète des touches.
- `v17/CMakeLists.txt` : **réutilise directement le moteur V16 complet** (CPU ray tracing, CPU GI, D3D11 Compute, D3D11 Raster, Vulkan Raster et scènes), sans recoder de moteur.
- Les sept choix visibles sont CPU Ray Tracing PBR, D3D11 Compute, GI CPU à 2 et 4 rebonds, D3D11 Raster, Vulkan Raster et LumaTrace BVH4. Ils réutilisent la scène chargée ; DX11/Vulkan/LumaTrace disposent de fenêtres de rendu GPU dédiées selon la plateforme.
- FPS mesurés à l'écran pendant l'exploration et durée de rendu disponible. Leur interprétation dépend du backend et du chemin de présentation ; voir les limites de mesure ci-dessus.
- **Présentation GPU directe :** DX11, Vulkan et LumaTrace peuvent utiliser une swapchain native dédiée sous Windows ; Qt/QImage reste un chemin de repli. Une mesure CPU de soumission ne donne pas à elle seule le temps de fin GPU.
- Build Windows sur GitHub Actions avec `actions/checkout@v5` et `actions/upload-artifact@v6`, tests de non-régression V16 conservés.

## Tester sur le Dell

Ouvrir `moteur3d_v17.exe`, choisir le preset **Dell 1366×768**, et cliquer sur **Démarrer exploration (V17)**. Tester la caméra 02, noter la fluidité et les FPS. Comparer le backend **V17 GPU D3D11 Raster** avec le **V16 GPU D3D11 Compute** en mode image fixe, puis les modes CPU. Noter le backend, la présentation directe ou Qt, la résolution, la caméra, le nombre d’échantillons et les temps de chargement/construction/rendu. Les chiffres Windows CI WARP ne prédisent pas le temps réel du HD620.

Le document de référence, avec objectifs vérifiables, risques, conditions de validation et étapes de développement, est : **[Objectifs V17 — temps réel, DirectX 11 / OpenGL et qualité lumineuse](../docs/V17-OBJECTIFS-TEMPS-REEL-QUALITE-261110.md)**.

## Priorités

1. **Navigation fluide** : caméra libre et images continues sur Intel HD Graphics 620, en commençant à **1366 × 768**. 30 FPS premier palier, 40 FPS / **25 ms** cible à mesurer, 60 FPS optionnel.
2. **Qualité d'atmosphère** : la géométrie V16 existe, mais l'image est encore trop froide/plate. Viser une finesse d'objets, des contrastes, des textures PBR et des effets de lumière dans l'esprit de **S.T.A.L.K.E.R.: Call of Pripyat**, **sans DXR** ni copie de ses ressources.
3. **Pipeline GPU hybride** : rasterisation matérielle + shaders PBR + GI ciblée, réutilisation temporelle, ombres et reflets spécialisés ; analyser d'abord DirectX 11 déjà présent, puis OpenGL/GLSL pour Qt et Linux.
4. **CPU intégral conservé** : ray tracing + path tracing GI, ombres, verre/eau, tous les matériaux et export ; référence visuelle et calcul sans GPU.
5. **Aucune régression V16** : conservez Full HD, QHD, 4K, scènes, assets et backend GPU Compute HLSL pour comparaisons.
6. **Mesures sur la vraie machine** : distinguer CI WARP, rendu hors écran, soumission CPU et temps GPU ; mesurer sur le matériel cible avant de déclarer une cible de FPS atteinte.

## Règle de développement

Les évolutions de V17 sont versionnées dans GitHub. Les modules partagés sont consignés dans [`engine/render_modules.json`](../engine/render_modules.json) et les builds Windows sont produits par GitHub Actions. La scène publiée séparément est contrôlée et empaquetée avec le moteur ; son gestionnaire peut proposer une version plus récente au lancement.


## Tableau de bord de diagnostic

Le moteur comprend un Pip-Boy compact au poignet et un panneau plein écran pendant l’exploration. Un appui bref sur `Tab` affiche ou masque le Pip-Boy compact au poignet. Maintenir `Tab` environ 550 ms ouvre l’interface plein écran ; un appui bref la ferme et revient au Pip-Boy. `Échap` garde son rôle habituel et arrête le mode caméra (les deux interfaces se ferment avec lui). `F11` conserve son comportement de plein écran du viewport.

Le panneau sépare les pages **Aperçu**, **Scène**, **Lumières**, **Ressources** et **Diagnostic**. Il affiche la version et la révision Git du moteur, la révision fournie par le manifeste chargé, les objets source, triangles compilés, matériaux, albedos image, lumières, caméra, résolution, cadence, dernier temps moteur et charge mémoire système. Les valeurs non instrumentées (mémoire du processus et VRAM) sont explicitement signalées.

Le comptage des triangles provient de la géométrie triangulée de la scène chargée. La révision moteur est la version produit compilée plus la révision Git empaquetée ; la révision de scène vient des métadonnées du manifeste. En présentation GPU directe, le temps affiché peut correspondre à la soumission CPU et ne constitue pas un temps GPU de fin d’exécution. Voir [la fiche de reprise et les limites](docs/DIAGNOSTIC_DASHBOARD.md).


### Exploration : accroupissement et saut

En mode « Marcher au sol », maintenir **Ctrl** permet de s’accroupir. La hauteur de vue et le gabarit de collision diminuent ; le personnage ne se redresse pas tant que le plafond est trop bas. **Espace** déclenche un saut uniquement depuis le sol. En vol libre, Espace et Ctrl gardent leurs commandes verticales.

### Radio de scène

La radio tente d’abord le flux Internet de la station, avec délai et repli automatique vers le WAV local si la playlist, le réseau ou la lecture échoue. Le paquet Windows contient le WAV de secours et l’implantation des quatre haut-parleurs ; la compilation échoue si l’un des deux fichiers manque.

## Chargement et mise à jour de la scène

La scène civique est maintenue séparément dans `moteur3d-scene`, puis empaquetée avec le moteur à la compilation. Au démarrage, CSE WARP cherche d’abord une scène déjà installée dans les données utilisateur, puis `benchmark-civic/scene.json` dans le paquet. Il charge immédiatement la scène locale trouvée ; la vérification GitHub ne retarde pas ce chargement.

Avec une scène locale, la première vérification réseau asynchrone attend 10 minutes après le chargement ; les suivantes sont espacées de 30 minutes. Si une révision plus récente est publiée, le moteur propose de la télécharger et de redémarrer. La scène courante reste active jusqu’à l’acceptation ; le moteur ne charge pas une seconde scène par-dessus la première. Si aucune scène civique n’est installée, le moteur vérifie immédiatement le dépôt, propose le téléchargement et ne charge la scène qu’une fois. « Plus tard » mémorise la révision proposée et évite de répéter la même alerte ; une nouvelle publication sera signalée.

## Z Atmosphere 002 (V17.028 expérimental)
Grain lumineux additif très discret pour DirectX 11, LumaTrace et Vulkan, sans brouillard ni atténuation. [Architecture, paramètres, limites et essais](docs/ZATMOSPHERE_001.md).

## V17.031 — profil de démarrage

LumaTrace BVH4, Ultra 8x MSAA, anisotropie 16x, LOD 200%, LOD adaptatif désactivé, exposition 0,85. Pose piscine exacte ajoutée en mémoire pour V17.634 ; pas de changement du dépôt scène. Voir le [README principal](../README.md#v17031--profil-de-démarrage-et-philosophie-warp) pour les options et l'état L03–L05. Les noyaux LumaTrace restent 010/003 ; DX11 raster 018, shader 009, Vulkan 010 et Z Material 001.
