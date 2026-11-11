# CSE WARP V17.build — Raster DirectX 11 : qualité et performance

Date de référence : 2026-11-10. Branche de travail :
`feature/v17-realtime-architecture-roadmap-261110`.

## Objectif retenu

**Améliorer la finesse et la crédibilité du rendu Raster tout en diminuant son temps par image.**
Ne pas remplacer le Raster par le GPU Compute : le Compute reste un **comparateur
visuel** et un mode autonome. Ne pas reconstruire le moteur ni les scènes Pripiat
déjà créées.

Cible de capacité du moteur : **jusqu'à 2 millions de triangles en scène**.
Ce n'est ni une promesse de 40 FPS, ni une limite de primitives BVH, ni une
garantie que deux millions de triangles seront soumis au GPU à chaque image.

## Références réellement observées, Dell Intel HD 620

| Scène | Mode | Premier rendu | Rendu suivant | FPS théoriques |
|---|---|---:|---:|---:|
| Complexe civique extrême (500k+ triangles) | V17 DX11 Raster, 1366×768 | 3467,6 ms | 65,3 ms | 15,3 |
| Même scène, exploration | V17 DX11 Raster, 1366×768 | — | 73 ms (capture) | 13,7 |
| Même scène | V16 DX11 Compute | — | 341,5 ms | 2,9 |

Ces mesures proviennent de captures de l'application sur le Dell :
elles ne sont **pas** des mesures automatisées, ni des moyennes sur 100 images.
Le gestionnaire des tâches affichait environ 27 % CPU, 77 % GPU et 82 %
de RAM totale ; ces pourcentages couvrent l'ensemble du système et ne prouvent
pas seuls quel élément limite V17.

Le benchmark V17 inclut le calcul et la lecture GPU→CPU ; son FPS est une
estimation et **n'est pas** un FPS de présentation écran avec VSync.

## Modifications appliquées dans cette branche

1. **Worker DirectX 11 dédié déjà introduit** : contexte, shaders et ressources
   restent sur le même thread entre les rendus. Le second rendu ne repart
   plus automatiquement sur un autre worker. Deux rendus consécutifs ont été
   testés sous Windows avec WARP ; le Dell a également montré 65,3 ms
   après 3467,6 ms au premier rendu.
2. **Culling de lots opaques** : à la première préparation du GPU, le
   vertex buffer est découpé en lots de 4096 triangles maximum. Les bornes
   3D sont extraites des vrais sommets, y compris les surfaces non cubiques
   et le terrain. Chaque image élimine uniquement les lots dont la sphère
   englobante est *entièrement* hors du frustum. Les lots visibles contigus
   sont fusionnés en un appel `Draw`, pour conserver un très faible nombre
   de draw calls quand tout est visible.
3. **PBR/mipmaps** : suppression d'un `frac(uv)` superflu avant les échantillons
   albédo, normales et rugosité. Le sampler DirectX est déjà en mode WRAP :
   la suppression évite des discontinuités artificielles de dérivées au
   bord des répétitions qui dégradaient le choix des mipmaps.
4. **Benchmark** : `summary.json` et `results.csv` exportent le nombre
   de triangles **effectivement soumis** à DirectX 11 pour les tests Raster.
5. **Réversibilité** : variable `M3D_V17_DISABLE_FRUSTUM_CULLING=1`
   pour désactiver uniquement la sélection des lots et comparer avec le
   rendu intégral. Le fichier de scène et les matériaux PBR restent identiques.

### Contraintes et limites du premier culling

- Il s'agit bien d'un **culling coarse-grained** des lots opaques, et
  **pas encore** d'un frustum culling par objet, ni de GPU occlusion culling.
- Les lots sont constitués selon l'ordre actuel des triangles : s'ils
  mélangent des zones très éloignées, le rejet sera moins efficace.
- Les surfaces transparentes restent triées et soumises séparément comme
  auparavant, pour ne pas altérer leur ordre de composition.
- **Instancing GPU natif introduit en développement** pour les boîtes
  opaques non tournées. Le chargeur V16 conserve leurs 12 triangles pour CPU,
  Compute et BVH et transmet leurs plages au Raster, qui peut les remplacer
  par un maillage cube commun et un buffer d'instances D3D11. Les objets
  tournés et les vitrages transparents gardent leur triangulation existante.
- Les canopées d'arbres V16 conservent leur géométrie source pour CPU/Compute ;
  un chemin GPU partagé dispose de trois maillages LOD (8×4, 12×6, 18×9).
  La silhouette proche retient les ondulations et les facettes de feuilles.
- Les sphères/ellipsoïdes natives bénéficient également de trois LOD.
- Ces nouveaux chemins sont soumis aux tests WARP avant validation sur HD 620 ;
  leur gain FPS n'est **pas encore garanti**.
- Le gain de temps n'est pas garanti : le culling réduit le nombre de
  triangles soumis, mais ajoute une faible charge CPU et peut ajouter des
  appels de dessin. Un benchmark comparatif est obligatoire.

## Travail intégré depuis la première V17

- **PBR Raster :** correction du filtrage des UV répétés, éclairage indirect
  hémisphérique peu coûteux, prise en compte de la métallicité, rugosité et
  transmission pour les matériaux historiques génériques.
- **Instancing :** boîtes opaques non tournées des scènes V16/JSON et
  ellipsoïdes avec buffers d'instances partagés. Les triangles originaux
  restent disponibles pour les autres moteurs.
- **LOD :** sélection par taille projetée des ellipsoïdes et des canopées.
  Les niveaux de qualité sont préconstruits et réutilisés sur le GPU.
- **Profilage :** export du nombre de draw calls, des boîtes et arbres
  instanciés, des triangles soumis et de la durée du readback GPU→CPU.
- **Compatibilité de test :** variables d'environnement
  `M3D_V17_DISABLE_FRUSTUM_CULLING=1`,
  `M3D_V17_DISABLE_GPU_INSTANCING=1`,
  `M3D_V17_DISABLE_BOX_INSTANCING=1`,
  `M3D_V17_DISABLE_FOLIAGE_INSTANCING=1`,
  `M3D_V17_DISABLE_ELLIPSE_INSTANCING=1` et
  `M3D_V17_DISABLE_LOD=1` pour mesurer des variantes à scène identique.
- **Version Windows :** `V17.<build GitHub Actions>`, affichée par les
  programmes, les propriétés Windows et les rapports du benchmark.
  Ex. `V17.208`, sans suffixe artificiel `17.3`.

**État de validation :** ces changements sont du code intégré dans la
branche de travail ; tant que le pipeline Windows correspondant n'a pas
réussi, ils ne doivent pas être considérés comme une build stable.

## Navigation libre — prochaine version V17.build (demande utilisateur)

**Objectif :** pouvoir se déplacer en hauteur naturellement, selon la
direction exacte de la caméra, et accéder immédiatement à quatre angles
extérieurs prédéfinis de la scène, sans perdre la navigation AZERTY.

### Commandes de déplacement

- **Z / flèche haut :** avance dans le vecteur de regard complet
  `forward={sin(yaw)cos(pitch),sin(pitch),-cos(yaw)cos(pitch)}`.
  La composante Y est **conservée** : caméra vers le ciel = montée,
  caméra vers le sol = descente. **S / flèche bas :** recule dans
  l'axe inverse.
- **Q / flèche gauche** et **D / flèche droite :** déplacements latéraux
  horizontaux, pour ne pas faire monter ou descendre accidentellement
  lors d'une translation.
- **Espace** monte verticalement et **Ctrl** descend verticalement,
  indépendamment du regard. Ces touches existent déjà dans le code V17 ;
  les rappeler explicitement dans l'aide, et vérifier les événements
  clavier quand le focus quitte la vue.
- **Maj** accélère le mouvement. Normaliser les mouvements simultanés
  pour éviter d'aller plus vite en diagonale. Survol libre, sans collision
  artificielle, avec possibilité d'observer les étages et les toits.
- **Souris + clic gauche :** orienter la caméra, y compris le pitch,
  sans déplacement automatique. **Échap :** arrêter l'exploration.
- **Compatibilité :** ZQSD AZERTY et flèches ; conserver les raccourcis
  préexistants et ne pas imposer WASD.

**Constat dans le code actuel :** `frameTick()` calcule déjà un
`forward` avec le pitch, mais utilise `horizontal` pour Z/S :
les avances/reculs annulent ainsi systématiquement Y. Il faudra
remplacer uniquement le vecteur utilisé pour Z/S, en conservant
`horizontal` pour les déplacements latéraux.

### Quatre boutons cliquables — vues aux quatre angles

Placer une rangée de **quatre vrais boutons de caméra** clairement
identifiés à proximité des raccourcis 1–6 :

| Bouton | Position géométrique | Direction initiale |
|---|---|---|
| **Angle 1 — ↖** | coin min X / min Z | vers le centre de la zone |
| **Angle 2 — ↗** | coin max X / min Z | vers le centre de la zone |
| **Angle 3 — ↙** | coin min X / max Z | vers le centre de la zone |
| **Angle 4 — ↘** | coin max X / max Z | vers le centre de la zone |

*Ces repères sont relatifs à l'axe X/Z de la scène ; les flèches ne
prétendent pas désigner des points cardinaux réels.*

Pour chaque angle, déterminer la zone de vue à partir de la géométrie
utile du décor / groupe architectural. Exclure ou pondérer les très
grands sols, terrains, routes et objets isolés pour éviter qu'ils
élargissent excessivement les bornes. Définir un recul et une hauteur
proportionnels au gabarit, de manière que l'ensemble principal rentre
dans le champ à la résolution et au FOV courants. Viser le centre
du volume visible, pas l'origine arbitraire (0,0,0).

**Comportement :** clic = téléportation instantanée de la caméra et
orientation vers le centre, lancement d'un rendu Raster, puis
déplacement libre immédiat. Ne pas quitter l'exploration lorsqu'elle
est active. Conserver les **six caméras historiques** et ne pas
réorganiser leurs raccourcis clavier. Les quatre boutons restent
désactivés jusqu'à la fin du chargement ; les calculs de cadrage sont
effectués une fois par scène et rafraîchis lors d'un changement de scène.

**Validation de cette version :** tests clavier automatisés pour
le déplacement avec pitch positif/négatif, Espace/Ctrl, Maj et
diagonales ; test des quatre boutons avec vérification du quadrant
XZ, de la hauteur, de la direction vers le centre, et de l'absence
de curseur obsolète lors d'un changement de caméra. Test manuel
sur le Dell (AZERTY, 1366×768), y compris scène extrême.

**État : spécification enregistrée, pas encore implémentée.**
Cette navigation ne doit pas être annoncée comme fonctionnelle tant que
la compilation Windows et le test utilisateur n'ont pas été réalisés.

## Prochaines étapes — restant à implémenter ou valider

| Priorité | Chantier | Bénéfice recherché | Validation |
|---|---|---|---|
| P1 | Étendre l'instancing aux boîtes tournées, vitrages et éléments composites | Moins de vertex buffers et de draw calls | Comparaison avec le chemin d'origine |
| P1 | Regroupement spatial réel des lots et culling plus fin | Éviter les bounds trop larges sur une place immense | Mesurer triangles soumis et coût de sélection |
| P2 | LOD sur les façades et le mobilier, puis validation des canopées | Réduire les détails invisibles loin de la caméra | Captures près/loin, pop-in, différences d'images |
| P2 | Calibrer PBR/éclairage/ombres améliorés | Approcher la qualité du Compute sans surcoût GPU | Comparer images et FPS sur HD620 |
| P3 | Chronométrage GPU natif et détail upload/shaders/draw | Distinguer latence GPU, driver et lecture CPU | D3D11 timestamp queries et télémétrie séparée |
| P3 | Streaming / cache PBR persistant | Réduire les temps d'ouverture des scènes | Premier et deuxième lancement, RAM |
| P3 | Préparation d'une scène de 2 millions de triangles | Valider capacité, pas uniquement FPS | Tests de mémoire, stabilité, fermeture |

## Protocole de test

1. Fermer les anciennes instances de `moteur3d_v17.exe`, garder le
   même cadrage et la même résolution.
2. Mesurer le Pavillon 01 puis le complexe léger, lourd, extrême.
3. Sous Windows, lancer le benchmark V17 normalement puis, pour un
   test A/B, ouvrir **une nouvelle session** avec
   `set M3D_V17_DISABLE_FRUSTUM_CULLING=1` dans `cmd.exe` avant le
   lancement de l'exécutable.
4. Comparer les fichiers `summary.json`, `results.csv` et les PNG :
   temps du premier rendu, P50, P95, triangles soumis, RAM et stabilité.
5. Examiner les artefacts sur les objets en bord de caméra et à distance :
   l'optimisation ne doit ni masquer des objets visibles ni dégrader les textures.
6. Confirmer le comportement de fermeture du processus après les tests.

## Validation CI

Le test `v17_raster_warp_smoke` sous Microsoft WARP compare pixel par
pixel des images avec et sans culling (caméras distinctes). Il contrôle
la cohérence de l'image, pas les FPS sur Intel HD 620.

**Ne pas annoncer les cibles 40 ms (25 FPS) et 25 ms (40 FPS) comme atteintes.**
Elles restent nos jalons d'optimisation pour le complexe extrême.

## Consignes futures — Direct GPU et versioning strict (11 novembre 2026)

**Statut : spécification uniquement, non implémentée.** Ne pas engager de modification
du moteur ou du rendu sans nouvelle demande explicite.
Détails et critères : [Consignes V17 — Direct GPU et versioning](V17_CONSIGNES_DIRECT_GPU_VERSIONING.md).
Suivi : [issue GitHub #25](https://github.com/glaby91/moteur3d/issues/25).

- **Direct GPU Present** : étudier une swap chain DXGI Flip Model D3D11/Windows
  dans une fenêtre native Qt. Éviter le détour permanent GPU → RAM CPU → QImage.
  Le resolve MSAA doit rester sur GPU. DirectFlip/Independent Flip dépend du
  pilote, du compositeur Windows et du mode fenêtre.
- **Qualité inchangée** : PBR 2048, géométrie, lumières, ombres, transparence,
  MSAA 4×, anisotropie 16×, LOD 100 % et rendu CPU/Compute conservé.
- **Validation** : comparer les pixels avant présentation, les FPS réellement
  affichés, la latence, la cadence et les saccades sur Intel HD 620.
- **Versioning obligatoire** : toute modification, y compris code, shaders,
  scène, ressources, tests, CI, benchmark et documentation, est enregistrée
  dans Git avec commit, SHA, historique des changements et statut de validation.
  Chaque binaire distribué conserve son identité V17.<build GitHub>, sa révision
  source et les artefacts de tests. Ne jamais modifier silencieusement une
  build publiée ; préserver la dernière version stable et une marche arrière.
