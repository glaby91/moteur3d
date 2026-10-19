# V6 — Test de charge ×10 en triangles (19 octobre 2026)

## Travail effectué
- Base : V5 avec véritable Z-buffer caméra par pixel, et une lumière directionnelle Z-bufferisée stabilisée comme dans V4.
- Subdivision exacte de chaque triangle du décor et du personnage en **10 triangles coplanaires**, par grille barycentrique de 9 triangles dont un bissecté en 2.
- Décor : **2 310 → 23 100 triangles**.
- Personnage : **100 → 1 000 triangles**.
- Total : **2 410 → 24 100 triangles**.
- Les contours, volumes de collision et matériaux d'origine ne changent pas. Il s'agit d'une épreuve de charge géométrique, **sans ajout de détail visuel**.
- Aucune source lumineuse additionnelle activée : une seule lumière directionnelle reste présente.
- Anciennes expériences V4 et V5 conservées.

## Vérifications
- Validation de la syntaxe JavaScript.
- Vérification géométrique d'une subdivision de référence : **10 triangles**, aire totale conservée, orientations conservées.
- Initialisation de la scène dans un banc V8 avec Canvas simulé : **23 100 triangles décor, 1 000 personnage**.
- Auto-test de deux triangles superposés dans les deux ordres : réussi.
- Rendu d'une image 320 × 240 dans le banc simulé : **53 490 pixels couverts**, **136 005 tests Z caméra**, **92 008 écritures Z caméra**, environ **24 062 triangles effectivement soumis au rasteriseur** (certains éliminés au clipping/à la projection).
- Sur cette exécution simulée, temps observés : projection 153 ms, rasterisation Z caméra 91 ms, préparation Z lumière 174 ms, calcul des ombres sol 12 ms, temps total environ 455 ms. **Ces mesures ne sont pas des performances réelles dans un navigateur**.
- Pas de contrôle visuel interactif Firefox/Chrome exécuté ici. Fluidité, stuttering et qualité des ombres à valider par l'utilisateur.

## Recommandation
Commencer à **320 × 240** si 640 × 480 est trop lent, puis comparer V5 et V6 à résolution identique.
L'augmentation de triangles multiplie le travail de projection et de shadow-map, même si le nombre de pixels de caméra effectivement couverts reste similaire.

Prochaine évolution : binning de triangles par tuile 16 × 16, rejet anticipé, structure de données compacte, et profilage exact des étapes CPU/FPGA.
