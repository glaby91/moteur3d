# Pripyat — géométrie métrique et mesures (étape 1)
Cette branche ajoute un noyau de **mesures en mètres**, sans modifier la scène rendue.
Les contours initiaux (place, Energetik, Polissya) proviennent de dimensions de travail antérieures : **non vérifiés historiquement**. Le module refuse implicitement de les présenter comme exacts (`verified: false`).
## Utilisation
`node experiments/pripyat-survey/test-pripyat-survey.mjs`
L'API permet les distances 3D, horizontales, dénivelés, surfaces et distances minimales entre emprises polygonales.
## Étapes nécessaires avant une reconstruction fidèle
1. Collecter des points de contrôle géographiques fiables, orientation et système de projection.
2. Tracer les contours polygonaux réels de la place et des voiries; conserver une provenance et une incertitude par point.
3. Remplacer les emprises rectangulaires provisoires par les véritables plans de masse.
4. Reconstituer Energetik par blocs architecturaux et étages, avec photographies multi-angles, dimensions vérifiées et LOD.
5. Connecter le mode deux-clics à la caméra et au picking du moteur natif Qt/Win32, avec mesures dans la même fenêtre.
6. Tester l'absence de collision entre les bâtiments et les voiries et comparer les distances au plan de référence.
Aucune compilation Windows ou intégration à l'interface n'est revendiquée ici.
