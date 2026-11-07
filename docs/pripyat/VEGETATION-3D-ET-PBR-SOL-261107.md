# Pripyat — herbes 3D et correction du placage PBR au sol

Date : 7 novembre 2026  
Branche : `feature/pripyat-vegetation-3d-261107`

## Pourquoi la texture d'herbe était trop artificielle

Le terrain géoréférencé SRTM représente des dalles de **100 × 100 m**.
La fonction générique `flatQuadMesh` attribuait seulement une plage UV
`0…1` à **chaque dalle entière**, ce qui étirait une unique occurrence de
la texture PBR sur 10 000 m².

Le nouveau `m3dBuildTiledGrassTerrainQuad` utilise des UV dérivées des
coordonnées **monde X/Z**, avec une répétition de texture tous les **4 m**.
Cela donne 25 répétitions par longueur de 100 m et conserve la continuité
visuelle d'une tuile à sa voisine, sans altérer la topographie ni le réseau
routier métrique 1:1.

## Végétation désormais réellement exportée dans scene.bin

Un générateur déterministe `m3dPripyatGroundGrassSites` place des
touffes irrégulières (3 brins chacun) construites en triangles
**double face**, sans billboards dépendant de la caméra ni textures
transparentes. Même géométrie pour CPU et GPU Compute D3D11.

Le placement fait la distinction entre :

1. **Joints du revêtement** de la place centrale : petites herbes éparses,
   proches des vraies lignes des joints de dallage.
2. **Bordures** : herbes plus hautes et irrégulières.
3. **Jardinières** : départ des brins au sommet de la terre/végétation,
   jamais à Y=0 sous une dalle de béton.
4. **Terrains verts extérieurs** : petits amas autour du centre avec
   tests géométriques pour éviter le recouvrement des routes OSM et les
   emprises de bâtiments.

Les anciennes sphères vertes de substitution ont été supprimées.
Les arbres ramifiés, les peupliers, les jeunes arbres et les autres objets
urbains existants restent inchangés.

## Budget graphique et limites

- Maximum : **1 750 touffes**, soit au maximum **10 500 triangles**
  double face pour les herbes (trois brins × deux faces × 1 750).
- Répartition aléatoire **reproductible** à chaque export avec graine fixe.
- Le volume concerne l'environnement proche (rayon inférieur à 168 m du
  centre SRTM, topographie encore aplanie dans cet intervalle).
- Pas de végétation volumétrique générée sur toute l'étendue géographique
  de 2 km : la végétation lointaine reste représentée par les matériaux
  et objets déjà existants ; un système LOD serait nécessaire avant
  d'étendre massivement le nombre de brins.
- Aucune espèce précise n'est attribuée sans source botanique locale.
  Cette végétation est une **reconstitution générique de colonisation**
  des terrains délaissés, et non un relevé historique de chaque plante.
- Les performances du GPU Intel HD 620 **ne sont pas prouvées** par
  l'export. L'artefact doit être ouvert et comparé à cadrage identique
  dans Moteur3D CPU et GPU Compute.

## Contrôles automatiques

`experiments/pripyat-survey/test-pripyat-grass-3d.mjs` :

- contrôles de quantité, répartition et bornes du coût géométrique ;
- absence de touffes dans les bâtiments et les emprises routières
  pour la végétation périphérique ;
- hauteurs cohérentes avec la place et les jardinières ;
- génération entièrement déterministe ;
- triangles double face ayant les mêmes positions / orientations inversées ;
- 25 répétitions PBR sur une dalle de 100 m dans les deux directions.

## Livraison

Action GitHub `export-pripyat-grass-3d.yml`, artefact
`Pripyat-Grass3D-SceneOnly-1to1` : `scene_pripyat_central.bin`,
présélections de caméra et atlas.
**Pas de recompilation Qt ni de changement du format binaire.**
