# Pripiat — place centrale

Cette scène utilise le moteur natif V14 et son exporteur V4. Elle constitue le **premier secteur** parcourable automatiquement : la place centrale, le Palais de la Culture Energetik, l’hôtel Polissya et les abords immédiats.

## Plans et références photographiques

- Atlas cartographique de Pripiat fourni par Rodolphe : plan d’urbanisme 1988 et plan général (fichier conservé dans la Library).
- [Plan OpenStreetMap de Pripiat](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat.svg)
- [Plan des microdistricts de Pripiat](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat_microdistricts.svg)
- [Photos Wikimedia Commons — bâtiments de Pripiat](https://commons.wikimedia.org/wiki/Category:Buildings_in_Pripyat)
- [Photos Wikimedia Commons — intérieurs de bâtiments](https://commons.wikimedia.org/wiki/Category:Building_interiors_in_Pripyat)
- [Panoramas de Pripiat](https://commons.wikimedia.org/wiki/Category:Panoramas_of_Pripyat)

Le script `tools/pripyat/build_reference_index.py` prépare un CSV de liens vers les pages et originaux, avec auteur et licence, jusqu’à 500 références. Il ne copie pas les images dans le dépôt. Les pages de fichiers restent la source de vérité pour les droits. Les références intégrées doivent être de véritables photos documentaires ; les images générées de cette conversation restent des pistes de composition, pas des preuves historiques.

## Construction actuelle

Source de scène : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.

Le premier passage contient :

- une place pavée avec joints, reprises de dalles, flaques et végétation spontanée ;
- les volumes et façades principales d’Energetik et de l’hôtel Polissya ;
- des fenêtres, colonnes, marches, galerie de liaison et bâtiments résidentiels de bordure ;
- des bancs, jardinières, peupliers, jeunes arbres et lampadaires ;
- un parcours automatique qui traverse uniquement cette zone.

C’est un premier assemblage géométrique procédural. Les proportions et détails de façade devront être confrontés aux plans et aux photos documentaires avant d’étendre la ville aux secteurs voisins.

Le catalogue des surfaces et les trois niveaux prévus sont détaillés dans [PRIPYAT-MATERIALS-CATALOGUE.md](PRIPYAT-MATERIALS-CATALOGUE.md).\n\n## Parcours automatique

Dans le moteur, ouvrir `scene_pripyat_central.bin`, choisir le moteur de rendu disponible, puis cocher **Parcours automatique — place centrale (800 × 600)**. Le point de vue suit une boucle à travers la place et ses façades ; chaque nouvelle image s’affiche dès qu’elle est rendue. Le trajet suit la vitesse réelle du rendu. Les autres scènes conservent leur aperçu rotatif habituel.

Le parcours ne prétend pas couvrir la ville entière : il correspond à la zone effectivement modélisée dans cette première étape.

## Export et compilation

Depuis `experiments/2026-10-30-cote-ete-gt/native_v14` :

```bash
M3D_SKIP_IMPORTED_CAR=1 node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Le workflow Windows ajoute la scène au paquet de l’application. Le fichier binaire exporté n’est pas versionné ; la source JavaScript reste la référence modifiable.

## Matériaux et qualité

La scène utilise les matériaux existants du moteur : béton, pierre, asphalte, vitrage, métal, bois, rouille, végétation, éclairages, débris et flaques. Leur réglage de réflexion/transmission est dans la source de scène. Les chaînes de texture du moteur vont jusqu’à 512 × 512 avec mipmaps.

| Niveau | Cible pour le secteur |
|---|---|
| Économique | Textures procédurales existantes, végétation et mobilier simplifiés, parcours fluide. |
| Standard | Cartes couleur, normale et rugosité 512 × 512 dédiées au béton, au pavage, au verre et à la végétation ; façades et mobilier plus détaillés. |
| Détaillé | Même plafond de texture, mais variantes par surface, dégâts localisés, cadres et éléments architecturaux plus fins ; mesurer le coût sur les scènes complètes. |

Le premier export utilise la palette existante et la géométrie de base. Les cartes PBR dédiées à Pripiat et les niveaux de détail restent à produire et valider dans le moteur.
