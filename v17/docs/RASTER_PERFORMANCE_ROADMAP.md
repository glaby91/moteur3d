# CSE WARP V17 — Raster DirectX 11 : qualité et performance

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
- Il n'y a **pas encore d'instancing GPU natif**. Les répétitions de
  fenêtres, dalles et sièges sont encore développées en triangles.
- Le gain de temps n'est pas garanti : le culling réduit le nombre de
  triangles soumis, mais ajoute une faible charge CPU et peut ajouter des
  appels de dessin. Un benchmark comparatif est obligatoire.

## Prochaines étapes — non encore implémentées

| Priorité | Chantier | Bénéfice recherché | Validation |
|---|---|---|---|
| P1 | Instancing GPU des objets identiques (dalles, vitres, bancs, arbres) | Réduire mémoire géométrique, transferts et draw calls | Comparer VRAM, FPS, image de référence |
| P1 | Regroupement spatial des maillages + frustum culling par groupes | Réduire les triangles réellement visibles | Mesurer triangles soumis et coût CPU de sélection |
| P2 | Niveaux de détail LOD sur la végétation et les façades éloignées | Gain GPU sans détériorer la vue rapprochée | Captures près/loin, comparaisons |
| P2 | Traitement PBR et éclairage sur Raster (normales, roughness, contraste, ombres) | Approcher la qualité visuelle du Compute | Images de référence du même cadrage |
| P3 | Profilage du GPU et de la lecture CPU des frames | Identifier les coûts réellement limitants | Durées upload/draw/readback séparées |
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
