# Correction CPU PBR — reflectivité distincte de la métallicité

## Défaut constaté

Le shader hors-ligne convertissait `reflectivityPercent` en `metallic`. Le matériau du siège Energetik était donc partiellement traité comme un métal. Dans V4, l’enregistrement matériau expose `reflectivityPercent`, transmission, indice de réfraction et les cartes couleur/normale/rugosité; il n’existe pas de canal métallique.

Cette confusion diminuait la composante diffuse (multiplication supplémentaire par `1-metallic`) et colorait le reflet spéculaire avec l’albédo, assombrissant le siège tout en donnant des reflets cuivrés.

## Changement

- `metallic = 0` dans le chemin CPU PBR V4, tant que le format n’a pas de champ dédié.
- `reflectivityPercent` devient le F0 diélectrique en niveaux linéaires; zéro conserve le F0 par défaut de 4 %.
- La probabilité de choisir un rebond spéculaire reprend ce même F0 et le rebond diffus conserve son énergie.
- La normale géométrique est retournée si nécessaire pour faire face au rayon avant d’appliquer la normale tangentielle et d’évaluer BRDF.

Aucune métallicité n’est déduite du nom ou de la valeur de réflexion. Les métaux véritables restent une évolution explicite du format matériau V4/V5; le correctif ne prétend pas ajouter ce canal.

## Vérification

- `g++ -std=c++20 -fsyntax-only` sur `src/m3d_shading.cpp` passe avec l’en-tête moteur.
- Une capture de rendu dans l’application Windows et une comparaison CPU/DirectX restent à faire. Le shader DirectX de prévisualisation demeure son chemin raster existant et n’utilise pas cette BRDF hors-ligne.
