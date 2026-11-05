# Matériaux — scène Pripiat

La scène V14 emploie la palette intégrée et les cartes procédurales du moteur. Le format natif V5 accepte des albedos RGB888 jusqu’à 2048 × 2048 avec mipmaps; les scènes V3/V4 et leurs albedos RGB565 restent lisibles. Les cartes normales et rugosité natives sont actuellement encodées en RGB565. Le catalogue GLB Pripiat peut embarquer les cartes couleur, normale et rugosité importées dans son export natif.

## Palette employée dans `scene_pripyat_central.js`

| Surface | Matériau moteur | Usage | Réflexion / transmission réglée |
|---|---|---|---:|
| Béton vieilli | `concrete` | Façades, piliers, bordures | 2 % / 0 % |
| Pierre et dalles | `stone` | Place, marches, appuis | 5 % / 0 % |
| Asphalte | `asphalt` | Chaussée et fond | 3 % / 0 % |
| Vitrage | `glass` | Ouvertures où les photos montrent encore du verre ; pas sur la grande façade ouverte | 4 % / 62 %, IOR 1,48 |
| Fenêtre sombre | `windowdark` | Vitrages cassés ou opaques | 1 % / 0 % |
| Fenêtre éclairée | `windowlit` | Quelques fenêtres de repère | 3 % / 0 % |
| Métal peint | `metal` | Lampadaires, ferrures | 18 % / 0 % |
| Rouille | `rust` | Pièces exposées et bancs | 7 % / 0 % |
| Bois | `wood` | Assises des bancs | 4 % / 0 % |
| Écorce | `bark` | Troncs et branches | 1 % / 0 % |
| Feuillage | `leaves` | Peupliers et jeunes arbres | 2 % / 0 % |
| Herbes | `grass` | Joints et jardinières | 1 % / 0 % |
| Flaques | `puddle` | Eau stagnante | 28 % / 0 % |
| Débris | `debris` | Fragments de dalles | 2 % / 0 % |

Ces valeurs sont les réglages de réflexion et transmission du matériau dans le moteur ; elles ne constituent pas des mesures physiques du site.

## Niveaux de qualité prévus

| Niveau | Résolution et cartes | Géométrie / effets | Usage |
|---|---|---|---|
| Économique | Textures procédurales intégrées, mipmaps jusqu’à 8 × 8 | Arbres simplifiés, peu de mobilier, peu de lumières de scène | Parcours fluide et machines modestes |
| Standard | Cartes couleur, normale et rugosité dédiées en 512 × 512 | Façades différenciées, végétation variée, éclairage local sélectif | Mode de référence de la place |
| Détaillé | Même résolution maximale 512 × 512, plusieurs variantes par matériau | Dégâts localisés, encadrements, mobilier et végétation plus fins | Captures et rendu lent de validation |

Le mode détaillé augmente la diversité des cartes et la géométrie ; il ne dépasse pas la résolution de texture réellement prise en charge. Les options du moteur (résolution, échelle interne, AA, DOF et DirectX 11 si disponible) restent réglées dans l’interface.

## Étape suivante pour les cartes spécifiques

Créer des cartes documentées pour le béton écaillé, les dalles, les cadres peints, le verre sale, l’écorce, les feuilles et l’herbe. Chaque texture récupérée devra garder sa page source et sa licence ; les photos de Pripiat servent à caler l’aspect, tandis que les textures répétables distribuées doivent avoir des droits compatibles. La branche n’intègre pour l’instant aucune photo documentaire comme texture.
