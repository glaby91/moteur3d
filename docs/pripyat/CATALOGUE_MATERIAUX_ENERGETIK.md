# Catalogue des matériaux — Energetik / Pripyat

Généré à partir des identifiants et des paramètres déclarés dans `scene_pripyat_central.js` sur `integration/moteur3d-unified-v15`. Valeurs de **réflectivité = paramètre logiciel en %**, non mesures physiques de laboratoire. `—` signifie non déclaré (pas zéro). Les textures procédurales, cartes PBR et couleurs finales doivent être vérifiées dans l'exporteur et le moteur ; ce tableau ne prétend pas inventorier tous les matériaux des autres scènes ni des assets importés.

| ID moteur | Usage dans la scène | Réflectivité (%) | Transmission (%) | IOR | Remarque |
|---|---|---:|---:|---:|---|
| `asphalt` | Chaussée | 3 | — | — |  |
| `bark` | Écorce | 1 | — | — |  |
| `brick` | Maçonnerie apparente | 2 | — | — |  |
| `cinemaScreen` | Écran | 0 | — | — |  |
| `concrete` | Structure, dalles, piliers | 2 | — | — |  |
| `curtain` | Rideaux | 1 | — | — |  |
| `debris` | Débris | 2 | — | — |  |
| `energetikFresco` | Fresque Energetik, texture dédiée | 2 | — | — | Texture PBR de fresque chargée par l'exporteur |
| `glass` | Vitrages conservés | 4 | 62 | 1.48 | Seul matériau avec transmission/IOR explicites |
| `grass` | Végétation basse | 1 | — | — |  |
| `lamp` | Lampes et luminaires | — | — | — | Utilisé dans la scène, absent du tableau de réflectivité |
| `leaves` | Feuillage | 2 | — | — |  |
| `metal` | Métal de structure et ferrures | 18 | — | — |  |
| `mural` | Peintures murales | 1 | — | — |  |
| `neon` | Néons | 2 | — | — |  |
| `plaster` | Cloisons et enduits | 1 | — | — |  |
| `puddle` | Flaques | 28 | — | — |  |
| `rust` | Métal oxydé | 7 | — | — |  |
| `seatFabric` | Tissu de siège | 1 | — | — |  |
| `seatFabricClean` | Tissu de siège propre | 1 | — | — |  |
| `seatSupportPaint` | Structure peinte des sièges | 12 | — | — |  |
| `seatTrim` | Finitions de siège | 2 | — | — |  |
| `seatVinylBrown` | Vinyle brun de fauteuil | 28 | — | — |  |
| `sign` | Enseignes | 3 | — | — |  |
| `stone` | Sols, marches, corniches | 5 | — | — |  |
| `volcanicTuff` | Parement de l'auditorium | 3 | — | — |  |
| `windowdark` | Baies sombres / détériorées | 1 | — | — |  |
| `windowlit` | Fenêtres éclairées | 3 | — | — |  |
| `wood` | Menuiserie, estrades, mobilier | 4 | — | — |  |

## Contrôles techniques

- Les surfaces `windowdark` ne sont pas des vitrages physiquement cassés : il faut modéliser les fragments, les trous et les châssis pour représenter les dégradations fidèlement.
- Le fichier `export_scene.js` construit les mipmaps des matériaux de la scène, puis surcharge la fresque Energetik par une texture PBR. Une texture PBR de béton de façade peut être activée par `M3D_FACADE_PBR=1`.
- Le moteur charge aussi des textures d'autres assets (orange, carrosserie et pneus) : elles ne sont pas des matériaux du bâtiment.
- Vérifier les matériaux ajoutés par `__m3dAppendPripyatCatalog` et les assets externes lors de l'export final.
- Le rendu DX11 et CPU doit être comparé pour le verre, les reflets, les textures et la transparence.

## Calage visuel sur les photographies (version de travail)

Les valeurs suivantes sont des **cibles artistiques indicatives**, pas des couleurs mesurées dans les photographies : exposition, vieillissement, appareil photo et balance des blancs varient. Les codes HEX sont proposés en **sRGB**, la rugosité est une estimation PBR entre 0 (miroir) et 1 (mat). Ne pas remplacer directement les paramètres du moteur avant validation de captures comparatives CPU/DX11.

| Matériau / zone | Couleur de base indicative | Rugosité cible | Normal / relief | Vieillissement à conserver | Confiance |
|---|---|---:|---|---|---|
| Béton extérieur | `#88877F` | 0.88 | Granulat fin, joints et épaufrures | Coulures sombres, mousses, éclats | Moyenne |
| Béton intérieur / poteaux | `#9A978D` | 0.90 | Pores et reprises de coffrage | Poussière, fissures, traces d'humidité | Moyenne |
| Enduit des cloisons | `#A8A59A` | 0.96 | Plâtre granuleux | Peinture écaillée et manques | Moyenne |
| Parement rouge/rose de l'auditorium | `#8A5955` | 0.91 | Pierre poreuse irrégulière | Panneaux manquants, salissures | Faible à moyenne |
| Fauteuils vinyle brun | `#67463B` | 0.47 | Grain fin du vinyle, coutures | Craquelures, déchirures, poussière | Moyenne |
| Rideaux de scène | `#50332F` | 0.95 | Plis textiles larges | Déchirures, décoloration | Faible |
| Bois des estrades / meubles | `#70563F` | 0.76 | Fibres longitudinales | Gonflement, éclats, usure | Moyenne |
| Châssis métalliques | `#595A55` | 0.67 | Peinture et corrosion localisées | Rouille, peinture écaillée | Moyenne |
| Rouille | `#754B37` | 0.92 | Croûtes et piqûres | Intensité variable par exposition | Moyenne |
| Verre conservé | `#9BA8A6` | 0.11 | Surface lisse | Salissures, traces de pluie | Moyenne |
| Verre cassé / manquant | Ne pas peindre un panneau plein | 0.11 | Fragments géométriques, bords cassés | Trous visibles, éclats résiduels | Moyenne |
| Fresque monumentale | **Texture photographique existante**, sans teinte uniforme | Selon carte source | Préserver les motifs et l'éclat d'origine | Dégradations limitées aux zones réellement documentées | Élevée pour le principe, faible pour les cotes |
| Sol du hall | `#827D73` | 0.73 | Joints, carreaux et débris | Poussière et usure irrégulière | Faible à moyenne |
| Gymnase / sol sportif | `#7B7770` | 0.85 | Revêtement usé, lignes éventuelles | Écaillage, poussière | Faible |
| Salle de boxe | `#88847A` | 0.91 | Plâtre et bois usés | Salissures, usure du ring | Faible |
| Bibliothèque | `#85715D` | 0.82 | Bois, papier, plâtre | Dépôts de poussière, humidité | Faible : implantation hypothétique |
| Asphalte | `#555651` | 0.94 | Granulat routier | Fissures et végétation | Moyenne |
| Végétation | `#526047` | 0.88 | Feuillage et écorce | Envahissement des abords | Moyenne |

### Sources visuelles et limites
- Photographies des intérieurs du palais : https://commons.wikimedia.org/wiki/Category:Interiors_of_the_palace_of_culture_in_Prypyat
- Fresques : https://commons.wikimedia.org/wiki/Category:Murals_in_the_palace_of_culture_in_Pripyat
- Gymnase, photographie attribuée à Timm Suess : https://commons.wikimedia.org/wiki/File:Palace_of_Culture-Gym_1_(indoor)-Pripyat.jpg
- Photographies d'ensemble du bâtiment : https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat
- Les photographies disponibles n'ont **pas** été échantillonnées pixel par pixel pour cette version. Ces couleurs sont des estimations visuelles à contrôler sur une planche de référence.
- La piscine figurant dans la simulation conceptuelle ne constitue **pas** une preuve d'une piscine dans l'Energetik. Ne pas lui créer de matériau spécifique sans confirmation.

### Prochain contrôle moteur
1. Faire correspondre chaque matériau de ce tableau à sa texture albedo, roughness et normal, en préservant la texture dédiée de la fresque.
2. Comparer les vues de façade, hall, auditorium et gymnase à angle et exposition comparables.
3. Ne jamais traiter une vitre absente comme un rectangle opaque `windowdark` ; supprimer la géométrie centrale et conserver seulement le cadre / les fragments.
4. Mesurer le coût mémoire des textures avant de monter leur résolution.
