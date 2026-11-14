# CSE WARP Beta X — V17.427 : Corrections globales (12 novembre 2026)

## Sources et livraison
- Branche : `feature/v17-v7-corrections-scene-261112`
- Commit de code testé : `5113d3e3aeae1288eea5397cc342a8dd8efeb5df`
- CI Windows : https://github.com/glaby91/moteur3d/actions/runs/43326184870 — **SUCCESS**
- Archive portable : artifact GitHub `13993337185`, `CSE-WARP-V17.427-V7-SCENE-RENDU-Windows-x64`
- ZIP exporté : `CSE-WARP-V17.427-CORRECTIONS-GLOBALES-Windows-x64.zip` (102068912 octets ; 255 entrées, ZIP CRC OK, SHA-256 `eaf09b77c3de379318d4c7b8fc48b0b9064c65a7c146a26978a4d3a88be79034`)

## Chantiers exécutés
| Domaine | Modification ancrée dans le code | Contrôle |
|---|---|---|
| Parvis | Suppression exacte des 24 sphères rouges/ocres `Place/Jardins/Fleurs basses`, fleurs et arbres GLB conservés | Assertions instance + CI |
| Butte / socle | Butte max 3,2m, plateau elliptique plat r≤0,28, socle enfoncé de 38 cm ; sculpture conservée | Contrôle de dimensions et profil |
| Gravures | Face FR à deux lignes, face UK à deux lignes, 8 tuiles natives 512² ; original 2048 px archivé sans modification | 8 éléments + textures natives |
| Lampadaires | 12 → 10 mâts avec diffuseurs lumineux, 10 sources de lumière réelles, suppression du doublon z=40 proche d'un arbre | Assertions et WARP soir |
| Corniche | 12 tubes avec équerres + 12 sources réelles à y=18,85 ; allumage progressif en soirée | WARP jour/nuit |
| Verrière | 16 baies de vitrage et 22 impostes hautes restaurées ; 468 jonctions métalliques | Assertions et vue dédiée |
| Circulation | Ancien escalier de piscine et flights expérimentaux retirés, trémies rebouchées, garde-corps de coursive repris | Assertions + 3 caméras |
| Entrées | Deux baies supplémentaires ouvertes à x=-27,25 et 26,35 au RDC ; traverses basses supprimées | Assertions de cardinalité dans générateur |
| Fresque | 8 panneaux natifs restaurés, galerie transverse et fanions devant l'image supprimés (précédente correction V7) | WARP camera 07 |
| Rendu nocturne | Ciel/horizon de nuit neutralisés, suppression du biais chaud omniprésent dans le pixel shader, ambient nocturne réduit | Nouveau moteur C++ compilé + WARP 19 h |
| Eau | Shader D3D11 : opacité réduite sur incidence normale (0,10) et rasante (max 0,45), teinte de surface réduite | Compilation + WARP piscine 14,75 h et 19 h |

## Validations effectuées
- GitHub Actions **Windows 2022**, Qt 6.8.3, compilation du moteur V17 avec le nouveau shader et les nouvelles sources lumineuses.
- Scène générée à partir de la V6 intacte ; assertions d'intégrité GLB, textures, panneaux, éclairages, garde-corps, plinth et escaliers.
- Images réelles WARP D3D11, 1366x768 et mini-rendus 640x360 aux caméras façade, piscine, fresque, galerie et entrée, à 14 h 45 et 19 h.
- Relecture visuelle des 6 rendus jour/nuit : plus de dominante sépia générale ; éclairage de corniche visible. ZIP portable et checksum vérifiés.
- Exécutable V17.421 faible RAM sauvegardé dans l'archive sous `BASELINE-V17.421-*` pour comparaison ; le nouveau exécutable est celui lancé par défaut.

## Limites encore ouvertes
- Les rendus automatiques ne remplacent pas des tests de collision, escaliers, portes et déambulation sur Dell HD620 réel.
- La transparence de l'eau est améliorée ; la vraie réfraction sous-marine pixel-à-pixel n'est **pas** implémentée et la qualité doit être évaluée sur la machine cible.
- La plaque reste contrainte par les pixels projetés à très longue distance : vérifier la lisibilité depuis les caméras de départ en résolution native.
- Les FPS mesurés sur GitHub DX11 logiciel WARP ne prédisent pas les FPS matériels Intel HD620.
- Les autres points historiques du backlog V17 non expressément couverts par la campagne restent ouverts ; ne pas déclarer les soixante points clos.
