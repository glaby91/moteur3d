# Energetik — prototype Photo2Scene WARP (facade normalisée, 3 vues)

Cette scène est **un instrument de vérification**, et non une reconstruction historique validée du Palais Energetik.

## Ce qui a été effectivement calculé

- 3 photographies originales d'Energetik, avec observations manuelles de travées numérotées depuis l'angle droit de la façade.
- 68 observations de points (34 réservées au réglage des caméras, 34 conservées en contrôle indépendant). Les labels de travée restent **hypothétiques**, même si le solveur est mathématique.
- Une grille de façade **commune en 3D**, limitée aux 14 travées observées ou interpolées, avec une unité horizontale = 1 largeur de travée.
- Une hauteur de colonnade ajustée à 2,2533 unités de largeur de travée, sous les hypothèses du solveur, **sans aucune conversion en mètres**.
- 3 poses de caméra conditionnelles issues d'un ajustement global robuste (SciPy least_squares / PnP OpenCV).
- Écarts de reprojection RMS sur les repères réservés à la validation : photo 1 = 3,44 px ; photo 2 = 6,36 px ; photo 3 = 2,58 px.
- Les épaisseurs des lignes dans la scène (0,035 unité) servent seulement à les visualiser dans le moteur CSE WARP V16. Elles ne sont pas des dimensions mesurées.

## Ce qui n'est PAS résolu

- Les positions des piliers sont appariées par l'ordre et l'hypothèse de régularité de la colonnade, **pas encore confirmées** par des points uniques indépendants.
- Seul le plan de façade avant Z=0 est contraint. Retour latéral, arrière, profondeurs du bâtiment, toiture, décalage de l'entrée et épaisseurs : **non mesurés**.
- Le FOV d'une vue seule est ambigu : une première estimation de la photo 1 donnait 63,61° sous des hypothèses de points de fuite ; le calage planaire 3-vues donne ~47,05° sous d'autres hypothèses. Ces valeurs divergent. En imposant à la photo 1 une focale proche de 416 px, un autre calage conserve un contrôle photo comparable. **Ne pas considérer ~47° comme vérité**.
- Les résidus sont une preuve de cohérence *avec nos annotations*, pas une preuve de conformité aux bâtiments historiques. Les annotations doivent être revérifiées avec des repères uniques.

## Pourquoi intégrer la scène V16 ?

Le moteur doit pouvoir afficher toutes nos géométries avec le même BVH, les mêmes matériaux, lumières et projections, indépendamment de la scène. Cette scène contient donc des **boîtes très fines uniquement pour la visualisation** de la grille filaire. Les plans non observés sont laissés vides : pas de géométrie inventée.

Pour l'ouvrir dans l'exécutable CSE WARP Beta X, sélectionner le fichier \`pripyat-photo2scene-proof/scene.json\`. Trois caméras sont proposées : photos 1, 2 et 3.

Les coordonnées et contrôles détaillés sont calculés par le module \`tools/photo2scene/multiview_facade.py\`; le fichier OBJ de référence est fourni par les artefacts GitHub Actions Photo2Scene WARP.

**Règle qualité :** les lignes vertes/orangées correspondent à des *observations de candidats* et des paramètres de scène ; ce ne sont pas des relevés certifiés.
