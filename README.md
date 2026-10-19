# moteur3d
moteur 3D 2D Qbasic

## Simulateur HTML expérimental (18 octobre 2026)

- [Cour Low Poly, collisions, lumière temporelle, 32 buffers](experiments/2026-10-18-cour-interactive-32buffers/README.md)
- [Ouvrir le code du simulateur](experiments/2026-10-18-cour-interactive-32buffers/index.html)

Cette démonstration HTML est conservée séparément du moteur QBasic historique.

## Dernière version expérimentale — anti-stuttering (18 octobre 2026)

- [Simulateur HTML : tuiles adaptatives et rendu stable](experiments/2026-10-18-anti-stuttering-tiles/index.html)
- [Notes techniques et mesures](experiments/2026-10-18-anti-stuttering-tiles/README.md)

Le stuttering ne se manifeste plus d'après l'essai utilisateur. Le compteur « Reprojection » (~40 ms) reste à analyser : il comprend le temps de transfert du Canvas, et ne mesure pas uniquement une reprojection 3D. En mouvement, le pré-rendu lourd est volontairement désactivé pour préserver la stabilité.

## Expérimentation actuelle — Z-buffer lumineux unique (19 octobre 2026)

- [Simulateur HTML : une lumière directionnelle avec profondeur Z](experiments/2026-10-19-single-light-shadow-z/index.html)
- [Notes et limites de l'expérience](experiments/2026-10-19-single-light-shadow-z/README.md)
- [Architecture prévue pour plusieurs sources lumineuses](docs/ECLAIRAGE_MULTI_SOURCES.md)

**Une seule lumière avec ombres Z est active.** Le Z-buffer caméra et l'éclairage multi-sources ne sont pas encore implémentés. Cette expérience est distincte de la version anti-stuttering stable du 18 octobre.


## Révision expérimentale V4 — stabilisation des ombres (19 octobre 2026)

- [Ouvrir le code du simulateur V4](experiments/2026-10-19-shadow-stabilized-v4/index.html)
- [Diagnostic et protocole de test](experiments/2026-10-19-shadow-stabilized-v4/README.md)

L'image géométrique reste identique à V3. Les ombres utilisent maintenant **deux cartes Z de la même et unique lumière**, avec interpolation continue des tests de profondeur pour limiter les sauts. Le mode « Ombres stabilisées » est activé par défaut et désactivable pour comparer; les performances en navigateur réel restent à mesurer.
