# Bibliothèque de matériaux — Pripiat

Catalogue de départ pour reconstruire la ville avec le moteur natif **après des décennies d’abandon**. Le but visuel est une ville encore reconnaissable, marquée par les affres du temps : façades usées, vitrages brisés, corrosion, débris et végétation qui reprend les rues. Les noms sont des identifiants stables à réutiliser dans les objets de scène. Les cartes de couleur, normale et rugosité sont prévues au format PBR déjà consommé par l’exporteur natif (mipmaps 512→8, RGB565). Les valeurs de rugosité ci-dessous sont des cibles artistiques, à ajuster avec les vraies photos de référence.

## Matériaux de base

| ID | Surface et usage | Couleur visée | Rugosité | Cartes | Notes de rendu |
|---|---|---:|---:|---|---|
| `prp_asphalt_old` | Chaussées | gris anthracite | 0,82–0,96 | couleur + normale + rugosité | fissures, réparations, granulats; flaques en objets séparés |
| `prp_concrete_panel` | Panneaux préfabriqués des immeubles | gris chaud délavé | 0,78–0,94 | les 3 | joints, coulures et variations d’âge; garder les joints aussi en géométrie |
| `prp_concrete_raw` | Béton brut, escaliers, socles | gris moyen | 0,82–0,97 | les 3 | éclats peu profonds, taches et agrégats |
| `prp_plaster_pale` | Enduit peint clair | crème / gris vert | 0,76–0,94 | les 3 | peinture écaillée par zones, salissures sous les fenêtres |
| `prp_brick_red` | Brique visible et maçonnerie | rouge brun assourdi | 0,78–0,94 | les 3 | joints de mortier lisibles sans surdimensionner les briques |
| `prp_roof_bitumen` | Toitures plates | noir brun | 0,84–0,97 | les 3 | bandes de membrane, raccords et flaques localisées |
| `prp_roof_metal` | Tôles, auvents, éléments de toiture | gris brun oxydé | 0,48–0,82 | les 3 | nervures principales en géométrie, corrosion dans les cartes |
| `prp_steel_painted` | Garde-corps, cadres, mobilier | peinture bleu/vert passée | 0,42–0,76 | les 3 | écaillage local laissant apparaître le métal sombre |
| `prp_steel_rusted` | Structures métalliques exposées | brun rouille | 0,72–0,94 | les 3 | rouille mate; pas de reflet métallique miroir |
| `prp_window_glass` | Vitrage intact, sale ou légèrement teinté | gris froid / bleu faible | 0,08–0,30 | couleur + rugosité | transmission et indice selon capacités moteur; vitrage cassé modélisé séparément |
| `prp_window_frame` | Dormants de fenêtres | blanc cassé / vert gris | 0,50–0,82 | les 3 | PVC ou métal peint, décliner par bâtiment |
| `prp_wood_weathered` | Bancs, portes, planches | brun gris délavé | 0,68–0,92 | les 3 | fibres orientées; lattes et bords cassés en géométrie |
| `prp_tile_ceramic` | Sols et murs intérieurs carrelés | crème, vert pâle, bleu passé | 0,22–0,48 | les 3 | trame de joints régulière; carreaux cassés en géométrie |
| `prp_terrazzo_floor` | Halls et bâtiments publics | gris clair moucheté | 0,30–0,56 | les 3 | éclats minéraux fins, dalles et joints distincts |
| `prp_lino_floor` | Linoléum intérieur | vert / brun passé | 0,48–0,78 | les 3 | motif discret, usure des passages et bords décollés |
| `prp_wallpaper_faded` | Papiers peints intérieurs | beige / vert pâle / motifs sourds | 0,72–0,93 | couleur + rugosité | déchirures et grandes plaques manquantes en géométrie/masques |
| `prp_soil_leaf_litter` | Terre, feuilles et débris au sol | brun froid | 0,88–0,99 | les 3 | mélanger feuilles, boue et gravillons par zones |
| `prp_moss_lichen` | Mousse et lichens sur béton/toiture | vert sombre / gris vert | 0,90–0,99 | couleur + normale | couches par taches/masques, pas une texture uniforme |
| `prp_bark_wet` | Troncs et branches | brun foncé | 0,72–0,95 | les 3 | écorce orientée; varier les échelles pour éviter le répétitif |
| `prp_foliage` | Feuilles et herbes | verts désaturés | 0,55–0,88 | couleur + normale | feuilles en géométrie simple avec alpha si le chemin le gère |
| `prp_puddle_water` | Flaques et eau stagnante | gris brun / reflet du ciel | 0,04–0,24 | couleur + normale + rugosité | reflets locaux; faible ondulation, profondeur limitée |
| `prp_sign_paint` | Panneaux, lettrage et numéros | selon panneau | 0,55–0,88 | couleur + rugosité | texture dédiée à chaque panneau lisible; conserver les caractères exacts des références |

## Variantes réutilisables

Ne pas multiplier les matériaux quasi identiques : décliner teinte, vieillissement ou humidité via la couleur et des masques d’objet. Prévoir au minimum les états `sec`, `humide`, `mousseux` et `écaillé` pour béton, enduit, métal peint et bois. Les fenêtres ont les variantes `intacte`, `sale`, `opaque` et `brisée`; les trois dernières ne doivent pas être simulées par une simple teinte uniforme.

## Niveaux de détail et découpe de la ville

Les quatre zones prévues restent des **macro-zones de travail**. Pour la reconstruction complète, je recommande de suivre les quartiers et microdistricts du plan historique, puis de les subdiviser en cellules de chargement : les cartes historiques distinguent plusieurs secteurs (dont les microdistricts 1 à 5 et des secteurs adjacents), donc quatre blocs seraient trop grossiers pour organiser finement les bâtiments et le streaming. La ville doit néanmoins former un seul espace continu à explorer, sans écran de chargement ni changement visible au passage d’une limite. Toutes les zones partagent le catalogue de matériaux et un repère de coordonnées global.

| Niveau | Quand l’utiliser | Matières et détail |
|---|---|---|
| `LOD0_economique` | Bâtiment lointain ou cellule voisine préchargée | Couleur seule ou petite texture; géométrie de silhouette |
| `LOD1_standard` | Rue et façades dans le champ de vision | Couleur + normale + rugosité en 512 × 512, avec mipmaps |
| `LOD2_detaille` | Bâtiment proche, place centrale ou point d’intérêt | Cartes les plus détaillées disponibles, usure locale, débris et éléments cassés en géométrie |

Le choix de LOD dépend de la distance à la caméra et de la taille projetée à l’écran, jamais du numéro de zone. Pour le premier découpage, viser **8 à 12 secteurs urbains** alignés sur les quartiers et points d’intérêt, puis **32 à 48 cellules de streaming** de taille variable selon la densité des bâtiments (cellules plus petites au centre, plus grandes dans les espaces ouverts). Une bande de recouvrement charge progressivement les cellules voisines; les textures et matériaux partagés ne sont chargés qu’une fois. Le joueur peut rejoindre n’importe quel point de la ville sans transition visible.

Le moteur actuel charge une scène monolithique : le streaming par cellules est une évolution à concevoir dans le chargeur et l’exporteur, pas une capacité déjà active. Le moteur plafonne aussi les cartes natives à 512 × 512 (mipmaps jusqu’à 8 × 8). `LOD2_detaille` signifie donc d’abord davantage de géométrie et de variations locales; des textures 1K/2K nécessiteront une évolution explicite du format de scène et de l’exporteur.

## Ce que le moteur doit recevoir

- **Couleur/albédo** : teinte de la surface sans éclairage peint dans la texture.
- **Normale** : relief fin; garder fissures profondes, trous, joints et éclats de silhouette dans la géométrie.
- **Rugosité** : échelle de gris, élevée pour béton/bois/rouille, faible pour verre/eau.
- **Réflexion/transmission** : paramètres de matériau existants; le verre et l’eau demandent une vérification particulière dans le rendu DirectX temps réel, dont le chemin actuel est une approximation raster.
- **Échelle UV** : renseignée par objet selon les dimensions réelles; vérifier à l’écran qu’une brique, dalle ou feuille garde une taille crédible.

## Ordre de production conseillé

1. `prp_concrete_panel`, `prp_concrete_raw`, `prp_asphalt_old`, `prp_steel_rusted` et `prp_window_glass` : ces surfaces dominent les vues extérieures.
2. `prp_plaster_pale`, `prp_brick_red`, `prp_steel_painted`, `prp_wood_weathered`, `prp_soil_leaf_litter`, `prp_moss_lichen` et `prp_foliage`.
3. Matériaux intérieurs, panneaux, toitures et flaques, en suivant les photos des bâtiments choisis.

Les photos documentaires prises après l’évacuation servent de référence principale pour les teintes, motifs, dégâts, végétation et degrés d’usure. Les photos d’avant 1986 ne servent qu’à retrouver les formes, détails d’origine et éléments désormais disparus. Chaque texture source devra garder son URL, son auteur, sa date et sa licence dans un manifeste d’assets; une photo de référence n’est pas automatiquement une texture réutilisable.
