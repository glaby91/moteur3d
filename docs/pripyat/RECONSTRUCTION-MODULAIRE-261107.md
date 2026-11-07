# Reconstruction modulaire de Pripiat — lot 01 (2026-11-07)

## Code réellement intégré
Source : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`

- `M3D_PRIPYAT_BLOCKOUT` : registre métrique des emprises provisoires.
- `m3dBuildPlaza` : place, joints de dallage, réparations et drainage périphérique.
- `m3dBuildConnector` : volume de liaison Energetik–Polissya.
- `m3dBuildPolissya` : hôtel, socle, étages, fenêtres, couronnement et auvent.
- `m3dBuildHousing` : quatre immeubles de contexte indépendants.
- Energetik : intérieur, fresque, auditorium, escalier, façade et enseigne **préservés**, mais structure à reprendre.
- Outil de mesure : panneau Qt de saisie de deux points, distances horizontale et 3D. Pas encore de picking souris.
- Vérification CI : syntaxe de la scène + tests mathématiques (résultats à consulter dans Actions).

## Niveaux de fiabilité
- **Documenté** : bâtiment Energetik, projet-type 2С-06-9/70; corps principal d'environ 81,2 × 24,4 m et hauteur 11,4 m selon reconstruction tierce, tour 14 m.
- **Approximatif** : place actuelle 106 × 76 m, positions relatives des bâtiments, contours des routes, logement, détail des ouvertures.
- **Non déterminé** : coordonnées géodésiques exactes des coins, cotes de façade, géométrie réelle de l'arche de liaison, hauteurs exactes du Polissya, emprise du complexe sportif.

Ne pas confondre coordonnées du blockout et levé topographique. Les éléments modulaires facilitent précisément la substitution de cotes vérifiées sans réécrire les bâtiments voisins.

## Références documentaires
- [Skeiron — numérisation du palais](https://artsandculture.google.com/story/ggUh4kq6wkwB7w)
- [Wikimedia — photographies de l'Energetik](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat)
- [Domophoto — projet-type et historique](https://domophoto.ru/building/80424/)
- [Reconstruction tierce : dimensions publiées](https://www.fab.com/listings/39bd11e2-0185-48e4-b75f-f2bf45d5baef)
- [Wikimedia — plan de la ville](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat.svg)

## Ordre de travail restant
1. Fixer l'orientation et le référentiel géographique de la place à partir d'un fond cartographique mesurable.
2. Substituer des polygones de bâtiments et de routes aux rectangles provisoires.
3. Refaire Energetik : volumes avant/arrière, colonnade, vitrages selon époque représentée, salle, tour, aile gymnase, arche.
4. Vérifier Polissya et le nombre d'étages sur photographie avant d'arrêter sa géométrie.
5. Raccorder les clics de souris au picking 3D (rayon caméra, BVH/triangles, point d'impact).
6. Exporter la scène, compiler Windows et produire des captures de comparaison à cadrage identique.
