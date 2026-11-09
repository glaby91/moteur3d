# Photo2Scene — reconstruction architecturale fondée sur des calculs

## Origine du projet

CSE WARP Beta X vient du programme QBasic initial et est devenu un moteur C++ / Qt6 : BVH, CPU multithread, PBR/normal maps, GPU D3D11 Compute et GI CPU. Photo2Scene est un module géométrique distinct, pas un nouveau moteur graphique. Les calculs restent reproductibles et indépendants des prompts.

## Architecture recommandée : intelligence visuelle + géométrie métrique

1. IA et humain : délimiter bâtiments, plans de sol, façades, fenêtres, piliers, routes et arcades; proposer les points correspondants. Chaque proposition est contrôlée et possède une source.
2. Vision classique : extraction de segments et de contours dans les pixels de l'image originale, filtres de distorsion, RANSAC, calibration de caméra, points de fuite, homographies et triangulation.
3. Documentation : associer les cotes, dimensions d'étages, trames, plans d'archive et coordonnées cartographiques aux sources authentifiées et incertitudes. Aucun plan générique ne prouve les dimensions d'Energetik.
4. Reconstruction multi-photo : optimiser simultanément caméras, points et surfaces avec un graphe de contraintes (bundle adjustment), pertes robustes et validation sur photographies non utilisées pour l'ajustement.
5. Contrôle qualité : conserver résidus en pixels et incertitudes, distinguer mesuré / déduit / hypothétique / indéterminé. Si plusieurs solutions sont compatibles, les signaler explicitement.
6. Moteur CSE WARP : importer les coordonnées et la géométrie paramétrique pour comparer photos et rendu. La ressemblance visuelle seule n'est pas une mesure.

## Ce qui est implémenté (MVP reproductible)

Le script tools/photo2scene/photo2scene.py, sans bibliothèque externe, calcule :

- l'intersection aux moindres carrés de droites annotées avec erreur RMS en pixels ;
- un FOV à partir de DEUX familles de lignes dont les directions 3D sont orthogonales, avec hypothèses caméra sténopé, pixels carrés, distorsion nulle et point principal connu ou centré ;
- une homographie sur UN plan (sol, mur ou façade) défini par quatre ancres non collinéaires ou plus, exprimées en unités de scène ;
- des coordonnées relatives pour des points présumés situés sur ce même plan ;
- les erreurs de reprojection sur les ancres et des points de validation séparés ;
- des refus explicites pour les systèmes dégénérés ou un FOV impossible.

Le programme NE sait PAS encore déduire une profondeur hors plan avec une seule vue, reconstruire tout un objet, calibrer la distorsion, exécuter la reconnaissance d'objets IA, trianguler plusieurs images ou effectuer un bundle adjustment. Un FOV et un point de fuite seuls ne suffisent pas à connaître les distances.

## Exécuter les tests et les jeux synthétiques

Depuis la racine du dépôt :

    python -m unittest discover -s tools/photo2scene/tests -v
    python tools/photo2scene/photo2scene.py analyse tools/photo2scene/examples/synthetic-ground-anchors.json -o ground-result.json
    python tools/photo2scene/photo2scene.py analyse tools/photo2scene/examples/synthetic-orthogonal-vanishing.json -o fov-result.json

Ces données sont SYNTHÉTIQUES. Elles ne mesurent pas Pripiat.

Entrées :
- image : id, width, height.
- vanishing.directions : segments {a:[x,y], b:[x,y]} organisés par directions réelles parallèles.
- vanishing.orthogonal_pair : deux directions déclarées orthogonales, pas simplement deux familles quelconques.
- vanishing.principal_point : optionnel, centre de la photo par défaut.
- planes : plans avec anchors {pixel:[x,y], scene_uv:[u,v]}, query et holdout.
- unités scene_uv : unités arbitraires du plan, convertibles en mètres uniquement si une contrainte externe authentifiée établit l'échelle.

## Données sur Git, pas des milliers de lignes dans le prompt

Chaque bâtiment aura idéalement :

    data/pripyat/
      sources/         Provenance, URL, publication, auteur et droits
      photos/          Métadonnées et références des originaux, hashes, rognages
      observations/    Points et régions annotés, fiabilité, auteur ou IA
      dimensions/      Cotes et preuves, incertitudes et conflits
      cameras/         Pose, focale, distorsion, solutions alternatives
      solutions/       Paramètres 3D, transformations, qualité et incertitudes
      validation/      Contrôles sur photos tenues à l'écart et projections

On ne duplique pas les photographies sous droit d'auteur sans autorisation. Le prompt ne contient que le résumé des sources consultées et les corrections à faire.

Le registre tools/photo2scene/data/pripyat/energetik-dimensions-registry.json conserve des cotes de travail (trame 6 m, murs 510/380 mm, poteaux 400 mm, fresque estimée). Ces cotes sont NON VÉRIFIÉES pour l'édifice réel : priors souples ou valeurs à ne pas employer, jamais contraintes fermes.

## Méthode meilleure pour notre jeu de photos

L'étape suivante n'est pas le Gaussian Splatting comme remplacement de l'architecture, mais un GRAPHE DE FACTEURS STRUCTURÉ avec :
- nœuds : caméras, points 3D, plans, lignes, volumes et paramètres architecturaux ;
- facteurs : observations en pixels, parallélisme, orthogonalité, fenêtres répétées, raccordement de façades, cotes d'archives authentifiées ;
- initialisation : OpenCV (détection, appariement, RANSAC, correction de distorsion) ;
- résolution : solveur multi-vues robuste, par exemple SciPy least_squares ou Ceres, avec report des paramètres non identifiables ;
- validation : erreurs en pixels et intervalles d'incertitude, jeux de photographies non utilisés pendant l'ajustement.

La géométrie obtenue reste éditable et portable dans toutes les scènes du moteur CSE WARP.

## Critères d'acceptation

Les paramètres historiques sont identifiables par leurs sources. Les mesures ont une erreur de reprojection, les hypothèses sont explicites, et aucune distance d'un objet hors d'un plan connu ne peut être inventée. Une reconstruction seulement plausible visuellement n'est PAS une reconstruction mesurée.
