# Recalage Pripyat 1:1 — audit du 6 novembre 2026

Base: feature/pripyat-roads-2km-261105. Ne pas modifier les maillages architecturaux avant validation des emprises.

## Défaut
PRIPYAT-ROUTES-TERRAIN-2KM.md documente une conversion GPS → scène de 0,488. Le rayon géographique de 2 km devient environ 976 m en scène, incompatible avec 1 unité = 1 m.

Le plan existant indique Energetik X[-50.5,30.5], Z[-43.4,-18.8] et Polissya X[40,63], Z[-42,-20]. Centres : (-10,-31.1) et (51.5,-31), soit 61.5 m.

L'atlas interne donne Energetik (51.40667,30.05666) et Polissya (51.4069094,30.0584328). Écart approximatif : 123 m est, 27 m nord. Attention : ce sont des points de repère publiés, pas nécessairement les centres des emprises. D'autres sources divergent de quelques dizaines de mètres.

## Convention cible
1 unité = 1 mètre, X vers est, Z vers nord, Y vertical. Origine provisoire au repère Energetik de l'atlas. Calcul local : X = 6371000*cos(lat0)*radians(lon-lon0), Z = 6371000*radians(lat-lat0).

## Procédure
1. Récupérer les contours de bâtiments Energetik et Polissya (Polissya : OSM way 357266685), puis identifier l'emprise du comité exécutif.
2. Vérifier les façades, entrées et la liaison arquée sur un plan et des photos historiques.
3. Recaler les emprises par transformation rigide ; NE PAS agrandir les bâtiments ou la fresque pour compenser le facteur 0,488.
4. Régénérer routes et terrain à échelle métrique 1:1, vérifier collisions et accès, puis mettre à jour mobilier et caméras.
5. Comparer captures avant/après et seulement ensuite exporter une scène.

Sources internes : experiments/2026-10-30-cote-ete-gt/native_v14/docs/pripyat/carte-reperes-photo.html et docs/pripyat/PRIPYAT-ROUTES-TERRAIN-2KM.md.
Sources externes : https://en.wikipedia.org/wiki/Palace_of_Culture_Energetik et https://mapcarta.com/W357266685

État : audit consigné ; géométrie et binaire non modifiés.
