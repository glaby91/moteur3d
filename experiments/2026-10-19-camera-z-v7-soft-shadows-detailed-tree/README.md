# V7 — Arbre détaillé et ombres diffuses

19 octobre 2026. Expérimentation distincte de V6, conservée dans le dépôt.

## Développements

- Arbre généré par branches courbes effilées et 32 lobes ellipsoïdaux de feuillage (7 bandes de latitude, 14 divisions angulaires).
- 7 972 triangles originaux de géométrie d'arbre contre un arbre V6 moins détaillé et subdivisé à plat.
- 23 916 triangles de l'arbre après subdivision en trois ; 3 500 triangles des autres objets et 1 000 du personnage.
- 28 416 triangles visibles par la caméra.
- Deux cartes Z directionnelles de la même lumière, interpolées, conservées de V4.
- Filtre PCF spatial 4 × 4 pondéré et continu ; réglages Faible / Douces (défaut) / Très douces.
- Adoucissement facultatif des silhouettes : couleurs des pixels de bord seulement, sans changer le Z-buffer caméra, distinct du MSAA.
- Optimisation des ombres : 8 322 triangles natifs pour le décor et 100 pour le personnage, au lieu de subdiviser inutilement les occludeurs.
- Les 28 416 triangles caméra restent présents ; les enfants coplanaires réutilisent les calculs lumineux de leur face d'origine.

## Contrôles exécutés

- JavaScript validé syntaxiquement dans V8.
- Scène et compteurs vérifiés : 27 416 triangles décor, 1 000 personnage.
- Auto-test Z de deux triangles superposés dans les deux ordres de dessin réussi.
- Rendu complet 320 × 240 exécuté dans le banc de simulation V8 avec Canvas simulé ; anciens modes V4 exécutables.
- Avec / sans l'adoucissement de silhouettes : 3 594 pixels couleur différents et zéro modification de profondeur.
- Mesure indicative du rendu dans ce banc : environ 472 ms à l'une des images, dont 30 ms de filtre silhouettes. Ce n'est pas une mesure de FPS dans Firefox ou Chrome.

## Test de diffusion

Sur 17 161 récepteurs du sol dans un essai préalable au cache géométrique, nombre de points dans la zone intermédiaire de pénombre : Faible 2 458 ; Douces 3 928 ; Très douces 4 376. Ce sont des mesures numériques et non une inspection visuelle.

## Limites et prochaines vérifications

- Une unique source directionnelle active ; aucune autre lumière calculée.
- Les ombres sur les objets sont calculées par face de forme d'origine, et sur le sol via une grille interpolée.
- Le filtre de silhouette ne remplace pas un vrai MSAA.
- Qualité visuelle et cadence dans un vrai navigateur non vérifiées ; les calculs sont encore coûteux.
- Comparer V6 et V7, vérifier l'ombre de l'arbre à douceur Faible puis Douces puis Très douces, et vérifier le P95 des images.
- Prochaine optimisation : rasteriseur caméra tuilé 16 × 16, suppression des projections cachées et cache lumineux.