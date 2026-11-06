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
