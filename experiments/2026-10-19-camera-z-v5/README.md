# Moteur 3D V5 — Z-buffer caméra + lumière V4 stabilisée

**Date :** 19 octobre 2026 · **statut :** expérience de référence fonctionnelle, performance CPU à optimiser.

## Pourquoi V5

Les ombres V4 étaient visuellement plus stables, mais des triangles masqués apparaissaient encore, laissant entrevoir les fonds à travers le décor. La V4 triait les triangles par profondeur moyenne sur Canvas, ce qui n'est pas un test de visibilité par pixel.

## Changements implémentés

- Nouveau mode par défaut **V5 — Z caméra par pixel** ; les modes V4 restent accessibles pour comparaison directe.
- **Z-buffer caméra réel** : un `Float32Array(W*H)` garde l'inverse de la profondeur de chaque pixel. La valeur la plus proche gagne, indépendamment de l'ordre des triangles.
- Rasteriseur logiciel par triangles opaques, avec coordonnées écran et interpolation perspective correcte de 1/Z.
- Clipping géométrique des triangles au plan proche avant projection, pour éviter de traverser l'écran avec des sommets derrière la caméra.
- **Un seul Z-buffer commun** au sol, aux murs, aux arbres, aux bancs, aux pierres et au personnage en mouvement.
- Framebuffer couleur RGBA `Uint32Array`, écrit par le rasteriseur et copié avec `putImageData`.
- Lumière Z de la V4 préservée : **une source directionnelle unique**, deux cartes de profondeur de lumière interpolées pour stabiliser les ombres. Aucune autre lumière activée.
- Ombres au sol reconstruites depuis des échantillons de la carte Z de la lumière sur une grille de taille **1/5 de l'écran**, interpolées et appliquées **uniquement aux pixels du sol identifiés par le Z caméra**. Cela empêche le masque d'ombre de déborder sur les objets.
- Affichage séparé de la carte Z caméra et de la carte Z lumineuse; compteurs d'écritures et tests Z.
- Auto-test de deux triangles superposés, dessinés dans les deux ordres : la face proche reste visible.

## Vérifications réalisées

- Syntaxe JavaScript vérifiée.
- Simulation d'exécution avec DOM/Canvas simulés : 2 310 triangles de décor + 100 triangles du personnage, deux images calculées et mode V4 toujours accessible.
- **Test d'occlusion déterministe réussi**, pour les deux ordres de dessin.
- À 640×480 dans l'environnement V8 simulé : profondeur calculée sur environ 214 000 pixels, dont environ 111 000 pixels de décor opaque et 103 000 pixels de sol ; plus de 500 000 tests Z par image. Ces chiffres dépendent de la pose caméra.
- Le rendu caméra est volontairement non tuilé pour établir une référence de qualité.
- Les temps dans le DOM/Canvas simulé sont élevés (plusieurs centaines de millisecondes par frame) et **ne doivent pas être présentés comme des FPS Firefox/Chrome**.
- **L'affichage et la fluidité en navigateur réel ne sont pas encore validés.** Pas de benchmark matériel FPGA.

## Limitations

1. Pas encore d'accélération par tuiles 16×16 du rasteriseur caméra; seule l'ancienne architecture V4 conserve cette optimisation.
2. Pas encore de MSAA 4× adaptatif; petits détails et contours sont échantillonnés à un seul point par pixel.
3. Ombres des triangles d'objets évaluées à leur centre; ombres du sol échantillonnées sur une grille puis interpolées. Ce n'est pas un ombrage par pixel universel.
4. La mémoire RGB565 des 32 buffers temporels historiques est conservée mais le framebuffer V5 est un tampon RGBA 32 bits.
5. Les prévisions N+1 à N+5 restent des métadonnées; pas cinq rendus Z caméra complets prêts à afficher.
6. Les faces coplanaires de même profondeur peuvent encore présenter des ambiguïtés d'égalité Z; un mode de départage déterministe par objet pourra être ajouté.
7. Le Z-buffer est une preuve de concept CPU. Son coût n'indique pas la cadence d'une implémentation FPGA.

## Feuille de route immédiate

1. Validation visuelle sur le poste utilisateur : rechercher si les murs et le personnage masquent enfin le décor.
2. Comparatif V5 vs V4 à 320×240, 640×480 et 800×600. Relever médiane et P95 frame CPU, tris et raster.
3. Optimisation en **tuiles 16×16**, binning des triangles et rejet des triangles complètement masqués, sans supprimer la précision Z.
4. Mémoire framebuffer et Z-buffer réduite (RGB565 couleur et profondeur fixe), étudier une approximation numérique adaptée au FPGA.
5. Préparer l'antialiasing adaptatif à 4 échantillons sur silhouettes et très petits triangles.

**Important :** la V4 reste disponible sans modification dans le dossier voisin.

## Tests complémentaires automatisés (19 octobre 2026)

Dans le banc d'exécution JavaScript avec Canvas simulé :

| Résolution | Pixels couverts | Tests Z | Écritures Z |
| --- | ---: | ---: | ---: |
| 320 × 240 | 53 490 | 136 005 | 92 008 |
| 640 × 480 | 214 004 | 543 668 | 367 949 |
| 800 × 600 | 334 359 | 849 708 | 574 981 |

L'entièreté du décor a été rendue à 320 × 240 une fois dans l'ordre de triangles original, puis avec l'ordre des 2 310 triangles du décor complètement inversé. **Résultat : 0 pixel couleur différent sur 76 800**, avec l'éclairage direct recalculé de façon identique. Ce test valide l'absence de dépendance à l'ordre des faces pour cette scène et cette pose, mais ne remplace pas une inspection visuelle des silhouettes.

Temps dans cet environnement simulé (comprennent le rendu Z, les ombres et la projection) : environ 147 ms (320×240), 266 ms (640×480), 388 ms (800×600). **Ces durées ne sont pas des FPS mesurés dans Firefox ou Chrome.** Il faudra optimiser le rendu logiciel avant de poursuivre sur une mise à jour interactive rapide.
