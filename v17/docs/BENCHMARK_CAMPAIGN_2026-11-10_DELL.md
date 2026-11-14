# CSE WARP V17 — bilan technique du 10 novembre 2026

## Objet et traçabilité

Travail GitHubisé sur la branche `feature/v17-realtime-architecture-roadmap-261110` :

- V17.265 : API de qualité Raster commune à l’application hôte et à Qt, MSAA adaptable (jusqu’à 8×), anisotropie, LOD et culling ; mesures de temps GPU D3D11 et pipeline à 1/2/3 tampons.
- V17.271 : optimisation expérimentale du pixel shader (rejet de lumières hors de portée, dérivées conditionnelles) et conversion RGBA→ARGB CPU plus rapide ; chemin A/B de référence conservé. Tests WARP de parité pixels exacts.
- V17.281 : campagne multi-scènes dans le même build, mesures répétées et ordre 1/2/3 buffers équilibré ; exports CSV/JSON/HTML/PNG ; vérification CI sur Windows.
- Ce rapport archive les résultats utilisateurs et les anomalies détectées ; le code de la campagne a reçu un correctif de sélection de scène CPU et un journal de progression explicite.

**Source des mesures :** `CSE-WARP-V17.281-CAMPAGNE-20261110_213942_910.zip`, SHA-256 `fcc6a79018482313c596997e5a239d5ea6820afb939fcd239a683265252b192b`. L’archive brute de ~61 Mo et les chemins locaux de l’utilisateur ne sont **pas** publiés dans ce dépôt. Les chiffres ci-dessous sont des valeurs agrégées anonymisées.

## Configuration et contrôle qualité

- PC cible : Windows 10 22H2, processeur à **4 threads logiques**, GPU Intel HD 620 (test matériel utilisateur ; contrairement au banc CI WARP). Qt 6.8.3.
- Build déclaré : **V17.281**, source revision embarquée `4be9698b67a8`. Environnement : MSAA demandé 4×, anisotropie 16×, LOD 100 %, culling activé.
- **6 scènes**, **2 caméras**, **2 résolutions** (1366×768, 1920×1080), **3 profondeurs de lecture** = **72 cas** ; **6 séries de 12 images** par cas = **5 184 images mesurées**.
- Tous les **72/72 cas Raster** ont le statut `ok`, `gpuTimingValid=true` et `pixelIdentical=true`. Les **72 PNG**, rassemblés en 24 triplets, ont été vérifiés indépendamment : chaque triplet est identique par SHA-256.
- Statistiques par cas : temps GPU Draw/Transfer, CPU submit/Map/conversion, FPS de débit, latence moyenne, P50/P95/P99, écart-type, coefficient de variation.

## Résultats consolidés

- En moyenne arithmétique sur les 24 couples scène/caméra/résolution, **3 buffers gagne ~3,84 % de débit** face à 1 buffer ; gain médian ~4,13 %. Le débit à 3 buffers dépasse 1 buffer **dans 21/24 cas** ; 3 buffers est premier dans **18/24 cas**, 2 buffers dans 4 et 1 buffer dans 2.
- Médiane du débit sur les 24 cas : **26,79 FPS** (1 buffer), **27,65 FPS** (2 buffers), **28,02 FPS** (3 buffers). Attention : moyenne de scénarios disparates, et ce sont des FPS de pipeline, **pas** le FPS affiché par Qt.
- La médiane des latences P50 par cas passe de **35,60 ms** (1 buffer) à **69,90 ms** (2) et **91,58 ms** (3). L’intérêt du débit se paie donc en délai de réaction ; **garder 1 buffer en exploration par défaut**, 2/3 en option ou export hors ligne.
- Dispersion relative médiane des latences par cas : **12,84 %** (1), **10,87 %** (2), **31,08 %** (3). Ne pas surestimer le léger gain sans répéter l’expérience dans des conditions thermiques et mémoire comparables.

### Détail des 24 couples (FPS du pipeline)

| Scène | Caméra | Largeur | 1 buffer | 2 buffers | 3 buffers | GPU Draw ms (1) | P50 ms (1) | CV % (1) |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| scene-extreme-0 | facade | 1366 | 21.76 | 20.45 | 22.8 | 32.9 | 45.9 | 15.9 |
| scene-extreme-0 | facade | 1920 | 16.38 | 15.84 | 17.03 | 43.05 | 62.17 | 8.2 |
| scene-extreme-0 | escalier | 1366 | 20.47 | 20.13 | 20.64 | 33.6 | 48.7 | 5.9 |
| scene-extreme-0 | escalier | 1920 | 14.12 | 14 | 14.44 | 53.88 | 70.2 | 7.6 |
| scene-1 | facade | 1366 | 46.03 | 45.92 | 48.01 | 15.9 | 20.5 | 12.8 |
| scene-1 | facade | 1920 | 30.46 | 31.34 | 31.79 | 22.78 | 29.6 | 22.7 |
| scene-1 | jardin | 1366 | 46.31 | 46.64 | 49.01 | 13.92 | 20.08 | 23.7 |
| scene-1 | jardin | 1920 | 31.79 | 32.07 | 34.2 | 21.86 | 29.66 | 15.9 |
| scene-2 | facade | 1366 | 35.04 | 36.07 | 36.36 | 21.72 | 26.68 | 12.9 |
| scene-2 | facade | 1920 | 22.31 | 22.25 | 22.15 | 33.36 | 43.53 | 8.5 |
| scene-2 | escalier | 1366 | 28.67 | 28.95 | 29.58 | 26.81 | 33.72 | 11.2 |
| scene-2 | escalier | 1920 | 17.11 | 17.49 | 17.46 | 45.98 | 55.72 | 8.9 |
| scene-lourd-3 | facade | 1366 | 24.64 | 24.71 | 26.46 | 28.64 | 37.48 | 24.6 |
| scene-lourd-3 | facade | 1920 | 17.44 | 17.67 | 18.42 | 38.9 | 57.61 | 8 |
| scene-lourd-3 | escalier | 1366 | 23.26 | 24.31 | 24.89 | 30.34 | 41.62 | 8.2 |
| scene-lourd-3 | escalier | 1920 | 13.79 | 14.88 | 15.29 | 53.09 | 67.09 | 19.7 |
| scene-4 | main | 1366 | 39.04 | 40.25 | 39.82 | 14.69 | 25.41 | 7.5 |
| scene-4 | main | 1920 | 24.91 | 23 | 24.63 | 24.07 | 41.5 | 15.2 |
| scene-4 | brick | 1366 | 36.74 | 36.84 | 37.82 | 17.61 | 27.3 | 9.6 |
| scene-4 | brick | 1920 | 24.8 | 26.36 | 26.46 | 24.75 | 40.05 | 13.1 |
| scene-5 | oblique_01 | 1366 | 60.34 | 62.67 | 63.53 | 5.97 | 16.7 | 11.1 |
| scene-5 | oblique_01 | 1920 | 36.1 | 37.86 | 35.52 | 9.7 | 28.43 | 15.8 |
| scene-5 | frontal_02 | 1366 | 66.5 | 68.62 | 69.49 | 4.84 | 15.17 | 13.2 |
| scene-5 | frontal_02 | 1920 | 37.94 | 41.96 | 38.06 | 8.64 | 27.64 | 15.5 |

Les résolutions de cette table ont les hauteurs respectives 768 et 1080 pixels. Le mapping des scènes suit l’ordre du benchmark : `scene-extreme-0` complexe extrême sélectionné ; `scene-1` pavillon ; `scene-2` complexe standard ; `scene-lourd-3` complexe lourd ; `scene-4` laboratoire matériaux ; `scene-5` preuve Photo2Scene.

## Préparation des scènes, mémoire et défauts

| Scène | Parse ms | Géométrie/PBR ms | Ombres ms | Objets triangulés | RAM processus après build MiB |
|---|---:|---:|---:|---:|---:|
| Extrême (`scene-extreme-0`) | 185,88 | 19 018,60 | 3 589,36 | 592 308 | 349,32 |
| Pavillon (`scene-1`) | 15,26 | **66 248,46** | 826,35 | 8 412 | **1 176,09** |
| Complexe standard (`scene-2`) | 60,35 | 7 523,39 | 3 309,42 | 36 204 | **1 676,44** |
| Complexe lourd (`scene-lourd-3`) | 164,18 | 15 516,60 | 3 076,39 | 172 512 | 603,18 |
| Matériaux (`scene-4`) | 94,02 | 6 396,22 | 190,84 | 672 | 767,88 |
| Photo2Scene (`scene-5`) | 92,76 | 10,96 | 93,80 | 192 | 581,17 |

- **Pic RAM du processus : 2 307,47 MiB** ; la capture utilisateur montrait **94 % de RAM système** utilisée. La valeur de pic dans les cas Raster est cumulative sur le processus, pas une mesure par scène indépendante.
- Le pavillon demande **66,25 s** de reconstruction malgré 8 412 triangles. Priorité : profiler chargement/génération PBR, cache des textures, allocations, coût des chemins source, et mesurer séparément les sous-étapes avant de changer les algorithmes.
- Le journal V17.281 ne montrait pas les étapes avant le premier rendu. Le correctif émet désormais des messages et durées distincts pour parsing, préparation PBR, ombres, RAM et Compute.
- Le diagnostic CPU avait été mal sélectionné : il s’est déclenché sur `scene-5` car `scene-1` n’incluait pas la chaîne « pavillon01 ». Correctif : sélectionner le dossier parent `pavillon01` et fallback uniquement si aucune scène Pavillon n’est incluse.
- Diagnostic CPU **ancien** sur la scène Photo2Scene, 320×180, 1 spp, **deux exécutions seulement** : 1 thread **25,09 ms**, 2 threads **14,12 ms**, 4 threads **9,48 ms**. Ces chiffres ne représentent pas un rendu CPU Full HD. Correctif : l’outil réalise maintenant autant de répétitions CPU que de séries GPU et enregistre P95 et dispersion.
- Compute : essai séparé 320×180, échec après **60 406,72 ms** avec message **« Chaîne mip couleur manquante sur un matériau »**. Ce défaut ne concerne pas le Raster (72/72 succès) ; reproduire et corriger la validation des mipmaps Compute, sur la scène réellement concernée.

## Actions ordonnées — sans sacrifier la qualité

1. **P0 préparation/mémoire** : instrumenter phases de build PBR et caches ; réduire les 66 secondes du pavillon et les pics RAM, sans diminuer la résolution des textures.
2. **P1 coût du pixel shader** : profil ciblé ombrage 9 taps, PBR, lumières et transparence à qualité strictement constante. Le Raster GPU Draw reste dominant. Mesurer sur le Dell réel.
3. **P1 sûreté Compute** : réparer les chaînes mip de couleur manquantes ; surveiller les échecs silencieux et faire des tests de scènes croisées.
4. **P2 lecture/présentation** : étudier texture D3D11 partagée avec l’UI Qt, sans introduire de copies ou de latence. Le chemin 1 buffer reste la référence interactive.
5. **P2 maturité benchmark** : mesures VRAM, FPS affichés/vsync, GPU par passe/shader, contexte énergétique et mémoire ; A/B intercalés pour confirmer les gains inférieurs à 5 %.
6. **Fonctions prévues** : navigation caméra dans l’axe complet du regard, montée/descente, boutons vers les quatre coins ; spécifiés séparément mais non validés dans l’exécutable actuel.

## Validation, limites et reproductibilité

La compilation V17.281 et la suite CI V16/V17 ont réussi : https://github.com/glaby91/moteur3d/actions/runs/42896166331 . La CI emploie **Microsoft WARP**, différent de l’Intel HD 620. Les tendances ci-dessus proviennent bien de l’archive du PC utilisateur, mais ne caractérisent pas un GPU haut de gamme.

Une campagne **terminée** atteste seulement de ce qu’elle a mesuré : ne pas annoncer l’optimisation CPU/Compute comme valide tant que son échec n’a pas été corrigé et re-testé.
