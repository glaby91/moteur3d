# Moteur 3D — antialiasing adaptatif

Statut : **spécification validée ; implémentation et benchmarks à réaliser**.

## Objectif

Obtenir une qualité visuelle proche du MSAA 16× avec un coût se rapprochant du MSAA 4×, sans garantir ce résultat avant mesures.

## Stratégie

- MSAA 4× par défaut si le pipeline graphique le permet.
- Adaptation du traitement selon la taille projetée des objets, les silhouettes, les détails fins et la stabilité temporelle.
- Préservation des objets de quelques pixels ou sous-pixel pour limiter scintillement, disparition et aliasing en mouvement.
- Qualité renforcée localement sur les contours seulement si le gain visuel justifie son coût ; un niveau MSAA différent par objet dans un même framebuffer n'est pas présumé possible. Évaluer passes dédiées, filtrage temporel et post-traitement.
- Réduire les calculs d'éclairage inutiles sur surfaces homogènes, sans confondre échantillonnage de couverture MSAA et calculs de shading.
- Conserver la conception d'objets par assemblage de primitives mathématiques et la conversion optionnelle en triangles dans l'éditeur 3D.

## Validation

Comparer en scènes identiques sans AA, MSAA 4× et MSAA 16× lorsque disponible ; mesurer temps GPU médian/p95 et mémoire ; vérifier contours, fins détails et stabilité lors des déplacements. Ne pas supposer qu'un passage de 16× à 4× multiplie la vitesse par quatre : il réduit le nombre d'échantillons de couverture de 75 %, mais le gain effectif dépend du pipeline et du matériel.
