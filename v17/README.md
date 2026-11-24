# CSE WARP Beta X — V17

**Révision moteur : V17.004.** La révision de la scène reste propre à son manifeste et est affichée séparément. La scène de référence conserve le palais Energetik, la piscine avec sa coupole vitrée basse, la fresque continue et le jardin retravaillé. La coupole ferme ses deux pignons par une verrière encadrée de cinq meneaux radiaux par extrémité ; une costière inox assure le raccord continu sur les quatre côtés. Trois caméras dédiées permettent d’en contrôler les raccords et l’intérieur. Les images de contrôle incluses dans le paquet sont produites par le moteur CPU Ray Tracing PBR direct à 960 × 540. DX11 et Vulkan peuvent présenter directement dans leur fenêtre native ou passer par une image Qt. Le chemin de présentation directe reste à valider sur le Dell HD620 ; les résultats CI WARP ne prédisent pas son comportement matériel.

**V17 = architecture temps réel en construction, sans abandonner les moteurs CPU et GPU V16.**

V16 et V17 utilisent le même manifeste de modules de rendu : [`engine/render_modules.json`](../engine/render_modules.json). Il donne une révision par module ; CMake contrôle les entrées et place le manifeste dans le paquet. Une correction ciblée doit faire évoluer la révision du seul module concerné.

## État de validation V17.600

Le commit moteur `ceaa6a6` (V17.600) a passé la [CI Windows x64, run 44437496051](https://github.com/glaby91/moteur3d/actions/runs/44437496051) : compilation Release avec Vulkan, contrôles du paquet, validations du chargeur de scènes, rendus de contrôle WARP, audit de rayons de la fresque et suite de régression V16/V17. L’artefact Windows est produit par GitHub Actions.

Cette validation porte sur la compilation, les contrôles automatisés et les chemins de rendu exécutés en CI. Les rendus DX11 concernés utilisent WARP, l’adaptateur logiciel de Microsoft. Ils ne valident pas les performances sur une carte Intel ou NVIDIA, la fluidité en mouvement sur le Dell HD620, ni le rythme d’affichage du swapchain natif. Les images sont contrôlées automatiquement pour leur présence et leur intégrité ; cela ne remplace pas une inspection visuelle.

Aucun gain de performances BVH n’est établi par le seul statut « CI verte ». Pour comparer deux versions, utiliser le même ordinateur, la même scène, la même caméra, la même résolution, le même backend et les mêmes réglages ; répéter les mesures et comparer les médianes et percentiles. Séparer le temps de construction du BVH du temps de rendu. Pour DX11/Vulkan en présentation directe, le compteur CPU de soumission ne constitue pas une mesure de fin d’exécution GPU ni du temps de balayage écran. Voir [Full Benchmark](docs/FULL_BENCHMARK.md).

## Prototype V17-1 — déjà implémenté

- `v17/moteur3d_v17_qt.cpp` : fenêtre V17, mode Exploration, clic gauche + souris pour regarder, ZQSD/flèches (AZERTY) pour se déplacer, Maj pour accélérer, espace/Ctrl pour monter/descendre, Échap pour arrêter.
- `v17/CMakeLists.txt` : **réutilise directement le moteur V16 complet** (CPU ray tracing, CPU GI, D3D11 Compute, D3D11 Raster, Vulkan Raster et scènes), sans recoder de moteur.
- Le raster DX11 est la cinquième méthode et Vulkan la sixième ; les deux utilisent la scène chargée et le cache d’ombres. L’éclairage raster reste de première génération : l’amélioration de la chaleur, du verre et du PBR est une **étape ultérieure**.
- FPS mesurés à l'écran pendant l'exploration et durée de rendu disponible. Leur interprétation dépend du backend et du chemin de présentation ; voir les limites de mesure ci-dessus.
- **Présentation directe expérimentale :** le raster DX11 et le raster Vulkan proposent un chemin swapchain dans des fenêtres natives distinctes, sans readback par image. Le mode Qt/QImage reste disponible. Le fonctionnement de la présentation directe, du mouvement caméra et des animations doit encore être confirmé sur le Dell HD620.
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

Chaque phase nécessite un accord explicite avant modification du moteur, compilation ou fusion. Voir le document de cadrage pour les **portes de validation V17-0 à V17-7**.

**Base V16** : `feature/v16-dell-native-1366x768-fhd-261110`. La V17 ne doit pas être fusionnée dans `master` sans décision explicite.


## Tableau de bord de diagnostic

La branche `feature/v17-diagnostic-dashboard-tab-20261123` ajoute un Pip-Boy compact au poignet et un panneau plein écran pendant l’exploration. Un appui bref sur `Tab` affiche ou masque le Pip-Boy compact au poignet. Maintenir `Tab` environ 550 ms ouvre l’interface plein écran ; un appui bref la ferme et revient au Pip-Boy. `Échap` garde son rôle habituel et arrête le mode caméra (les deux interfaces se ferment avec lui). `F11` conserve son comportement de plein écran du viewport.

Le panneau sépare les pages **Aperçu**, **Scène**, **Lumières**, **Ressources** et **Diagnostic**. Il affiche la version et la révision Git du moteur, la révision fournie par le manifeste chargé, les objets source, triangles compilés, matériaux, albedos image, lumières, caméra, résolution, cadence, dernier temps moteur et charge mémoire système. Les valeurs non instrumentées (mémoire du processus et VRAM) sont explicitement signalées.

Le comptage des triangles provient de la géométrie triangulée de la scène chargée. La révision moteur est la version produit compilée plus la révision Git empaquetée ; la révision de scène vient des métadonnées du manifeste. En présentation GPU directe, le temps affiché peut correspondre à la soumission CPU et ne constitue pas un temps GPU de fin d’exécution. Voir [la fiche de reprise et les limites](docs/DIAGNOSTIC_DASHBOARD.md).

La modification est enregistrée sur une branche de travail et n’a pas encore été compilée.



### Exploration : accroupissement et saut

En mode « Marcher au sol », maintenir **Ctrl** permet de s’accroupir. La hauteur de vue et le gabarit de collision diminuent ; le personnage ne se redresse pas tant que le plafond est trop bas. **Espace** déclenche un saut uniquement depuis le sol. En vol libre, Espace et Ctrl gardent leurs commandes verticales.

### Radio de scène

La radio tente d’abord le flux Internet de la station, avec délai et repli automatique vers le WAV local si la playlist, le réseau ou la lecture échoue. Le paquet Windows contient le WAV de secours et l’implantation des quatre haut-parleurs ; la compilation échoue si l’un des deux fichiers manque.
