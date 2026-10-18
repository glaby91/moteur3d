# Moteur 3D — cour interactive, collisions et prédictions

Prototype HTML autonome : ouvrir `index.html` dans un navigateur moderne. Il ne dépend ni d'une bibliothèque externe ni d'Internet.

## Nouveautés

- Cour Low Poly avec grand arbre, deux murs, banc, rocailles, arbustes, pots, colonnes de portail et dalles.
- Environ 2 410 triangles (2 310 pour le décor, 100 pour le personnage), soit une scène plus complexe que la précédente.
- 11 volumes de collision au sol : les murs, le tronc, les colonnes, le banc, les pots et les rochers. Personnage assimilé à un disque de rayon 0,26 unité ; tests cercle/AABB et sous-pas pour éviter le passage au travers d'obstacles minces. Glissement le long des obstacles.
- Patrouille automatique autour des obstacles avec deux itinéraires, vitesse réglable et inversion de sens.
- Mode de test « Tester le mur » : le personnage avance vers un mur et se bloque physiquement ; le panneau indique l'obstacle rencontré.
- Commandes manuelles WASD/flèches ; touche relâchée = retour au comportement automatique si activé.
- Le personnage partage le tri de profondeur Canvas avec les murs et le décor : il n'est plus systématiquement dessiné après les murs.
- Source lumineuse mobile, luminosité + direction stockées et anticipées jusqu'à N+5 ; 32 buffers RGB565 et 5 prévisions.
- Deux fenêtres : rendu de la caméra à gauche, représentation extérieure avec obstacles et trajectoires à droite.
- Mesures FPS, temps des étapes et opérations estimées. Modes Classique et Lumière prédictive comparables.

## Vérifications

- Syntaxe JavaScript vérifiée.
- Rendu contrôlé en Chromium aux résolutions 320×240, 640×480 et 800×600, sans erreur JavaScript détectée.
- Collision contre le mur testée : le personnage s'arrête avant le mur.
- Patrouille A testée sur 3 000 pas de simulation sans immobilisation ; patrouille B testée sur 1 800 pas.
- Les commandes et le comparateur fonctionnent.

## Limitations et prochaines étapes

- Canvas 2D emploie un **tri de triangles par profondeur moyenne** : cela améliore l'occlusion, mais ce n'est **pas** un Z-buffer par pixel, et il reste possible d'avoir des défauts visuels entre faces qui se croisent.
- Les collisions utilisent des volumes simplifiés au sol et ne simulent pas la hauteur, les sauts ou la collision triangle/triangle. Le portail est franchissable sous son linteau.
- Les ombres sont approximatives, calculées par projection géométrique. Le mur principal reçoit une ombre projetée ; tous les obstacles ne reçoivent pas encore leurs propres ombres.
- La reprojection Z expérimentale a été désactivée dans cette démonstration, pour ne pas réintroduire les tremblements précédents.
- La mémoire des 32 images sert de banc d'essai : les images futures ne sont pas des rendus complets validés. Les calculs de prédiction ont un coût à mesurer.
- Les « opérations estimées » ne sont pas les instructions machine réelles. Le balayage de l'écran est indépendant du calcul d'image.
## Mesures relevées sur le poste utilisateur (18 octobre 2026)

- Environ **50 FPS** en mode classique à 640 × 480 ; oscillations de cadence dues aussi au navigateur.
- Environ **50 à 53 FPS** avec la lumière prédictive : différence non concluante sans benchmark hors affichage.
- **Tracé des objets : environ 12 ms par image**, principal poste observé.

## Expérimentation suivante (non implémentée dans cette sauvegarde)

Comparer le Canvas actuel avec un rendu par tuiles 16 × 16 et un rasteriseur logiciel dans un framebuffer + Z-buffer ; mesurer les opérations par image, temps de tracé, lectures/écritures mémoire et calcul hors affichage.
