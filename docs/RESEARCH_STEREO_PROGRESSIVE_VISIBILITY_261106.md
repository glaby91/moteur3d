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
