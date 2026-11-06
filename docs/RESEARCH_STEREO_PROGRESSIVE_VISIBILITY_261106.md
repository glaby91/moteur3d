# Recherche future : rendu stéréoscopique et visibilité progressive CPU

Date : 2026-11-06. Statut : proposition d'architecture, NON implémentée.

## 1. Rendu à deux yeux (à faire ensuite)
- Deux caméras parallèles séparées par une distance interoculaire configurable (valeur initiale indicative : 0,063 m à l'échelle réelle).
- Projections asymétriques convergeant vers un plan de confort, sans rotation forcée des caméras.
- Export gauche/droite et comparaison avec la vue monoculaire.
- Ne pas fusionner les deux vues par moyenne : cela produit des images fantômes et ne simule pas la fusion perceptuelle du cerveau.
- Mesurer le coût CPU et les différences de visibilité avant toute intégration VR.

## 2. Idée utilisateur : surface concave avançant avec l'œil
Hypothèse : une surface virtuelle concave/une enveloppe de visibilité suit la caméra. Quand la géométrie l'intersecte, elle fournit des candidats visibles ; les coordonnées de texture et les normales sont évaluées directement, puis l'éclairage est calculé. Les points résolus cessent d'être recherchés lorsque leur visibilité est certaine.

Interprétation technique : culling spatial + projection/rasterisation CPU par tuiles + Z-buffer + shading différé ; la surface concave peut être une représentation de la fenêtre de vue, mais ne suffit pas seule à prouver la visibilité. Une intersection avec l'enveloppe ne garantit ni le pixel projeté ni l'absence d'occlusion.

### Pipeline expérimental proposé
1. Découper l'image en tuiles de 16x16 pixels, avec frustum/hiérarchie spatiale BVH existante.
2. Transformer et clipper les triangles candidats, puis déterminer leurs pixels couverts.
3. Conserver pour chaque pixel la profondeur minimale, l'ID de primitive, les UV interpolées avec correction de perspective, et la normale (G-buffer).
4. Rejeter les fragments plus éloignés par test de profondeur ; ne jamais considérer un pixel définitivement clos avant d'avoir éliminé les candidats plus proches.
5. Exécuter les matériaux, normal/bump mapping et lumières seulement sur les pixels finalement visibles.
6. Réutiliser conditionnellement les fragments précédents (reprojection temporelle + validation de profondeur, ID objet, normales et caméra) ; invalider sur déplacement, dévoilement, mouvement d'objet ou changement de scène.
7. Garder le ray tracing CPU pour ombres, réflexions et transparences lorsque nécessaires.

### Limites importantes
- Le bump mapping ne modifie que la normale apparente, pas la géométrie ni la silhouette ; la méthode décrite est plutôt un rendu hybride visibilité/rasterisation + shading.
- Les objets translucides, fins, courbes, les surfaces procédurales et les réflexions exigent des passes spécialisées.
- Un simple drapeau « point vu » peut provoquer des trous, de l'occlusion incorrecte et des artefacts temporels.
- L'algorithme n'élimine pas automatiquement le problème de netteté des mipmaps RGB565 et du post-traitement.

## 3. Protocole de benchmark
Scènes : hall Energetik (fresque et sièges), façade de Pripyat, route oblique et végétation.
Résolutions : 1366x768, 1920x1080, 3840x2160.
Mesures : temps frame médian, temps visibilité/shading, fragments testés/rejetés, texels, RAM, nombre de pixels invalidés, qualité des silhouettes, artefacts d'occlusion.
Comparaison A/B : moteur ray tracing actuel, rendu hybride sans cache, rendu hybride avec cache.
Critère : aucun défaut de visibilité et détails des textures au moins aussi fidèles, avant toute optimisation de FPS.

## Ordre de réalisation
D'abord terminer la netteté/mipmapping et le diagnostic pixel-exact ; ensuite prototype CPU de visibilité par tuiles et G-buffer ; enfin stéréoscopie indépendante et éventuelle réutilisation multi-vues.


## 4. Précision utilisateur : resserrement vers le point de fuite selon Z
La surface de recherche doit tenir compte de la perspective. Distinguer :
- **Frustum en espace caméra** : pour une profondeur Z>0, demi-largeur = Z*tan(FOV_horizontal/2), demi-hauteur = Z*tan(FOV_vertical/2). Le volume couvert **s'élargit** avec Z.
- **Empreinte projetée d'un objet de taille fixe** : largeur écran ≈ f*largeur/Z ; elle **rétrécit** avec Z. La surface en pixels décroît approximativement comme 1/Z² tant que l'objet reste petit et frontal.
- **Point de fuite** : les directions parallèles convergent dans l'image ; ce n'est pas une position 3D où tous les objets doivent être ramenés.

### Adaptation du prototype
1. Pour chaque tuile écran, construire un sous-frustum (pyramide tronquée), pas une enveloppe 3D qui rétrécit artificiellement avec Z.
2. Intersecter BVH / bounding boxes avec ce sous-frustum et utiliser un test d'occlusion conservateur par profondeur.
3. Projeter les bornes des objets pour déterminer leur empreinte écran et choisir la granularité de traitement, le LOD et les seuils de détails.
4. Pour les triangles, interpoler 1/Z, U/Z, V/Z, puis reconstituer U et V par division pour éviter les déformations de texture.
5. Ne jamais exclure un objet au seul motif qu'il est éloigné : silhouettes, fils, fenêtres et détails contrastés peuvent encore être visibles.
6. Prévoir les cas limites : Z proche de zéro, clipping near plane, caméra inclinée, géométrie non frontale, occlusions, changements de FOV.

Validation : scène avec rangées de fenêtres identiques à des profondeurs croissantes, comparaison du nombre de pixels couverts, de la fidélité UV et de l'absence de disparition prématurée.


## 5. Premier prototype indépendant simulé (2026-11-06)
Un démonstrateur C++17 CPU a été compilé et exécuté localement, hors du moteur Qt, avec une scène procédurale simplifiée (98 triangles, 640x360 pixels) et deux chemins de visibilité :
- classique : un rayon par pixel, intersection brute contre tous les triangles ;
- prototype : rasterisation de triangles par tuiles 16x16, profondeur inverse Z, UV/position corrigées par la perspective, shading identique.
Mesure sur une exécution locale, non représentative du moteur réel : classique ~214,8 ms, prototype ~16,4 ms ; pixels différents 8 810/230 400 (3,82 %) ; erreur absolue RGB moyenne ~0,745/255.
Ces chiffres ne démontrent **aucun gain** sur Pripyat : le chemin classique de cette maquette est volontairement naïf (pas de BVH, contrairement au vrai moteur), et le prototype n'intègre ni Hi-Z, ni ombres, ni réflexions, ni transparence, ni multithreading. La différence d'image impose de corriger les règles de couverture et les conventions de shading avant une intégration.
Prochaine étape : comparer au vrai BVH et intégrer un pipeline expérimental activable sans remplacer le rendu existant.
