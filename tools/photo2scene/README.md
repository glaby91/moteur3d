# Photo2Scene WARP — reconstruction architecturale fondée sur des calculs

## Origine du projet

CSE WARP Beta X vient du programme QBasic initial et est devenu un moteur C++ / Qt6 : BVH, CPU multithread, PBR/normal maps, GPU D3D11 Compute et GI CPU. Photo2Scene WARP est un module géométrique distinct, pas un nouveau moteur graphique. Les calculs restent reproductibles et indépendants des prompts.

## Architecture recommandée : intelligence visuelle + géométrie métrique

1. IA et humain : délimiter bâtiments, plans de sol, façades, fenêtres, piliers, routes et arcades; proposer les points correspondants. Chaque proposition est contrôlée et possède une source.
2. Vision classique : extraction de segments et de contours dans les pixels de l'image originale, filtres de distorsion, RANSAC, calibration de caméra, points de fuite, homographies et triangulation.
3. Documentation : associer les cotes, dimensions d'étages, trames, plans d'archive et coordonnées cartographiques aux sources authentifiées et incertitudes. Aucun plan générique ne prouve les dimensions d'Energetik.
4. Reconstruction multi-photo : optimiser simultanément caméras, points et surfaces avec un graphe de contraintes (bundle adjustment), pertes robustes et validation sur photographies non utilisées pour l'ajustement.
5. Contrôle qualité : conserver résidus en pixels et incertitudes, distinguer mesuré / déduit / hypothétique / indéterminé. Si plusieurs solutions sont compatibles, les signaler explicitement.
6. Moteur CSE WARP : importer les coordonnées et la géométrie paramétrique pour comparer photos et rendu. La ressemblance visuelle seule n'est pas une mesure.

## Ce qui est implémenté (MVP reproductible)

Le script tools/photo2scene/photo2scene.py (Photo2Scene WARP), sans bibliothèque externe, calcule :

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

## Essai Energetik sur deux photographies réelles (9 novembre 2026)

- **Photo A :** 516 x 387 px, vue oblique et retour latéral ; estimation de focale/FOV seulement conditionnelle (FOV horizontal ~63,61 degrés) avec orthogonalité des deux façades, distorsion négligée et point principal centré.
- **Photo B :** 1024 x 680 px, vue plus frontale. Un relevé OpenCV LSD a détecté 2 497 segments ; 13 lignes candidates de toiture, 14 de bandeau supérieur, 12 de balcon et 18 montants verticaux. Un **sous-ensemble** de segments non interprétés en 3D est stocké dans `data/pripyat/energetik-20261109-second-view-sample.json`.
- Le calcul `facade_linefit.py` ajuste indépendamment les trois niveaux, puis teste s'ils convergent vers un point de fuite commun : le critère échoue sur cette photo. Sorties volontairement **FOV=null** et **distance3D=null**. Un VP vertical loin hors champ ne suffit pas à rendre cette calibration identifiable.
- Un premier SIFT + RANSAC entre les deux originaux donne des associations de piliers visuellement **fausses** malgré des candidats géométriques. Aucune correspondance 2D-2D n'est validée. Ne pas exécuter de triangulation ou annoncer une position de caméra sur ces appariements.
- Prochaine étape : repères **uniques** (jonction particulière de volumes, angles, lettres distinctives de l'enseigne ou intersections spécifiques), vérification manuelle des identifiants 1:1, rectification du plan principal et résidu sur points indépendants, puis seulement la reconstruction multi-vue.
- Les photographies et les versions nettoyées par IA ne sont pas ajoutées au dépôt. Leurs empreintes SHA-256 et les coordonnées de segments permettent d'associer les annotations aux originaux sans substituer une image inventée à la géométrie.

Commande reproductible :

    python tools/photo2scene/facade_linefit.py tools/photo2scene/data/pripyat/energetik-20261109-second-view-sample.json -o energetik-second-linefit.json

Cette étape confirme notre règle : un algorithme qui refuse de conclure est préférable à une mesure fictive.

## Troisième photographie Energetik — vue distante 1920 × 1080 (9 novembre 2026)

- Photographie originale fournie par l'utilisateur, avec façade principale, angle supérieur droit du bâtiment, retour latéral, enseigne et place au premier plan. Peupliers occultant de nombreux piliers ; le nettoyage IA ne constitue **pas** une source géométrique.
- Détection classique OpenCV LSD : **5 234 segments**. Des sous-ensembles candidats sont enregistrés dans `data/pripyat/energetik-20261109-third-view-sample.json` avec SHA-256 de l'original et noms de régions. Ils n'ont **pas** valeur de correspondances architecturales validées.
- Trois familles de segments visibles s'ajustent chacune à une droite, mais les directions ne forment pas encore un jeu de contraintes suffisant pour un FOV fiable. Le programme partagé `facade_linefit.py` accepte désormais trois familles structurales nommées dans le JSON ; il refuse explicitement une profondeur 3D non déterminée.
- Test automatique de matching strict SIFT sur les trois originaux : **2 correspondances candidates** entre photo n° 1 et n° 3 et **1** entre photo n° 2 et n° 3 (ratio 0,65, association mutuelle), **aucune validée**. Ce résultat n'autorise ni triangulation ni estimation de positions relatives.
- Pour progresser : identifier des points **architecturaux uniques** (angle droit supérieur, raccords spécifiques, enseignes avec support physique localisable, détails asymétriques), rechercher leurs correspondants en image originale, puis optimiser les poses et surfaces dans un solveur multi-vues avec contrôles de reprojection. Éviter de traiter toutes les baies répétées comme des repères uniques.

Commande :

    python tools/photo2scene/facade_linefit.py tools/photo2scene/data/pripyat/energetik-20261109-third-view-sample.json -o energetik-third-linefit.json

Les unités de scène restent non résolues tant que les vues ne sont pas recalées par des repères vérifiés et des contraintes de plan/dimensions indépendantes.


## PREMIER AJUSTEMENT FILAIRE 3D — TROIS PHOTOS (NON CERTIFIÉ)

Le programme multiview_facade.py applique un *bundle adjustment* aux 68 candidats de points étiquetés manuellement dans data/pripyat/energetik-20261109-threeview-facade-anchors.json. 34 observations servent à ajuster les caméras ; les 34 autres sont réservées aux contrôles en pixels.

| Original | RMS contrôle (pixels) | FOV conditionnel |
|---|---:|---:|
| Photo 1 oblique (516 × 387) | 3,44 | 47,05° |
| Photo 2 frontale (1024 × 680) | 6,36 | 26,89° |
| Photo 3 éloignée (1920 × 1080) | 2,58 | 26,37° |

Le plan de façade est normalisé avec une largeur de travée = 1 unité horizontale. Une hauteur de colonnade de 2,253 unités par travée est ajustée. Il ne s'agit PAS d'une mesure historique certifiée.

Une estimation précédente du FOV de la photo 1 par deux points de fuite donnait 63,61°, tandis que le bundle actuel donne 47,05°. Un ajustement qui contraint la focale de la photo 1 vers cette première valeur conserve des erreurs de contrôle assez proches. Cette instabilité rend les paramètres intrinsèques encore incertains.

Ni la profondeur de l'édifice, ni la façade latérale, ni les dimensions métriques ne sont résolues. Le fichier OBJ produit est seulement une façade plane Z=0 avec 14 montants filaires ; le modèle ne prétend pas représenter le bâtiment entier.

Pour reproduire :

    python -m pip install numpy scipy opencv-python-headless
    python tools/photo2scene/multiview_facade.py tools/photo2scene/data/pripyat/energetik-20261109-threeview-facade-anchors.json -o resultats-3d/

Pour produire les PNG de contrôle, fournir --originals-dir avec les véritables images d'origine, identifiées par leur SHA-256. L'outil vérifie ces empreintes. Les images nettoyées par IA ne sont pas des données de mesure.

La scène d'essai CSE WARP Beta X se trouve à scenes/v16/pripyat-photo2scene-proof/scene.json : 16 éléments filaires approximés par de très fines boîtes pour être affichables par le moteur, avec trois caméras conditionnelles. L'épaisseur technique de ces boîtes n'est pas mesurée. La scène d'origine Energetik reste distincte et intacte.

## Prototype géométrique à trois caméras — façade plane (9 novembre 2026)

Un **premier recalage simultané** a été exécuté et rendu reproductible. Sources :
- `data/pripyat/energetik-three-view-front-plane-annotations.json` : trois originaux et leurs points de colonnade proposés manuellement, avec SHA-256 et statut **UNVERIFIED** ;
- `three_view_front_plane.py` : `cv2.solvePnP` pour initialiser trois caméras, puis optimisation robuste `scipy.optimize.least_squares` des focales, poses et d'une hauteur commune ;
- `tests/test_three_view_front_plane.py` : contrôle numérique sur les points mis de côté (travées impaires), test d'export filaire strictement plan ;
- CI `photo2scene-geometry.yml` : test et export d'un OBJ filaire en unités de travée et d'un JSON contenant les paramètres conditionnels des caméras.

**Résultats reproductibles, non certifiés historiquement :** hauteur entre deux lignes de façade = environ **2,2533 travées** ; RMS validation photo 1 : **3,44 pixels**, photo 2 : **6,36 pixels**, photo 3 : **2,58 pixels**. Les focales calculées correspondent à des FOV horizontaux conditionnels d'environ **47,05° / 26,89° / 26,37°**. La première valeur diffère de l'estimation aux points de fuite (~63,61°) : la focale n'est pas démontrée sans contraintes indépendantes.

Le modèle exporté est **un seul plan Z=0**, avec une unité arbitraire par travée. Il ne calcule ni profondeur du retour droit, ni géométrie de toiture, ni longueurs historiques. Une faible erreur de reprojection ne suffit pas à valider la correspondance de piliers répétitifs. Les poses 3D produites sont conditionnelles aux identifiants de travée annotés et aux hypothèses du modèle sténopé ; **ne pas les intégrer comme données archéologiques définitives**.

Reproduire :

    python -m pip install numpy scipy opencv-python-headless
    python tools/photo2scene/three_view_front_plane.py tools/photo2scene/data/pripyat/energetik-three-view-front-plane-annotations.json --output front-plane.json --obj front-plane.obj

**Prochaine priorité :** identification de repères asymétriques réellement communs aux vues et contrôles indépendants des hypothèses de travées, puis ajout de plans perpendiculaires et de contraintes dimensionnelles authentifiées.
