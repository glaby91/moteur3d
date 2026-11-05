# Pripyat — routes et terrain sur 2 km

La scène `scene_pripyat_central.js` conserve le noyau existant d’Energetik et de l’hôtel Polissya, puis l’étend par le réseau routier et piéton de Pripyat sur un rayon géographique de 2 km autour d’Energetik. La conversion géographique vers les axes X/Z reprend l’écart entre les centres déjà modélisés d’Energetik et de Polissya ; le facteur résultant est d’environ 0,488. La nouvelle extension couvre donc près de 1,95 km de diamètre dans la scène.

## Données et géométrie

- Voies OpenStreetMap, instantané du 5 novembre 2026 : routes résidentielles et tertiaires, voies de service, chemins, pistes, zones piétonnes et escaliers. Les lignes sont découpées au rayon de 2 km, simplifiées pour limiter le nombre de triangles, puis converties en bandes 3D dont la largeur dépend du type de voie.
- Terrain SRTM30m interrogé via Open Topo Data, échantillonné sur une grille de 100 m. Les hauteurs sont exprimées par rapport au niveau central SRTM de 113 m et mises à l’échelle avec la scène. Le relief est aplani progressivement à proximité du centre pour raccorder la zone détaillée existante.
- Les bâtiments, la place et les voies déjà détaillées du noyau central restent en place. Les routes importées ne recouvrent pas la zone détaillée centrale.
- Source de contexte topographique : IAEA, qui décrit le terrain de Pripyat comme principalement plat, avec une pente vers la plaine inondable et un dénivelé de terrasse d’environ 5 à 7 m.

## Attribution et limites

Données de voies © contributeurs OpenStreetMap, disponibles sous licence ODbL. Altitudes issues du modèle SRTM à résolution nominale d’environ 30 m, via l’API Open Topo Data. Ces données conviennent au relief général et au réseau de circulation ; elles ne décrivent pas les bordures, trottoirs ou niveaux de sol à l’échelle des bâtiments. Les voies cartographiées actuellement servent de référence pour la reconstruction et peuvent différer de certains détails de 1986.

## Export de la scène

```sh
M3D_SKIP_IMPORTED_CAR=1 M3D_TEXTURE_SIZE=1024 \
node export_scene.js source_v1314.html scene_pripyat_central_2km_1024.bin scene_pripyat_central.js
```

Le binaire se charge dans Moteur 3D par **Scène → Ouvrir une scène…**.
