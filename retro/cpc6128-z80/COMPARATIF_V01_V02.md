# CPC6128 — Raster Z80 : comparatif V0.1 / V0.2 Fast Registers

**Historique figé au 20 octobre 2026** · branche `raster`.  
Sources conservées indépendamment : [V0.1](CPC_RASTER_MODE0.asm) et [V0.2](CPC_RASTER_V02_FAST_REGISTERS.asm).

## Contexte matériel constant

CPC6128, Z80 nominal à 4 MHz, Mode 0 (**160 × 200 pixels, 16 encres**, 80 octets vidéo/ligne). Deux objets cubiques et **huit orientations 3D préprojetées** ; rendu de triangles à **couleurs pleines**, sans texture ni Z-buffer par pixel. Chaque octet vidéo rempli représente deux pixels horizontaux de même couleur.

La V0.2 réutilise le jeu secondaire de registres du Z80 : position X 8.8 dans `HL'`, pente dans `DE'`, changement de jeu avec `EXX`. Les segments longs utilisent `PUSH BC` après sauvegarde temporaire de SP. Elle supprime les faces arrière **dans les données préprojetées** et restaure seulement le rectangle englobant l'ancienne et la nouvelle pose.

## Travail effectué pour **une image** (moyenne arithmétique sur 8 poses)

| Mesure | V0.1 | V0.2 | Différence |
| :-- | --: | --: | --: |
| Triangles soumis au rasteriseur | 24 | 12 | **−50,0 %** |
| Stockage d'une pose | 528 octets | 264 octets | **−50,0 %** |
| Lignes de triangles parcourues | 652,5 | 331,6 | **−49,2 %** |
| Itérations d'arêtes (somme des 3 arêtes) | 1 305 | 663,3 | **−49,2 %** |
| Écritures d'octets pour les triangles (avant occultation) | 4 284,5 | 2 147,6 | **−49,9 %** |
| Effacement du fond par image animée | 16 000 octets | 3 690 octets en moyenne | **−76,9 %** |
| Total simplifié : écritures fond + triangles | 20 284,5 | 5 837,6 | **−71,2 %** |

**Important :** ce total additionne uniquement des écritures mémoire théoriques. Il n'exprime **pas** le temps d'exécution, car les accès écran, les boucles, l'adressage, les sauts, les `EXX`, les `PUSH` et les sauvegardes/restaurations de SP ont des coûts différents. Une même adresse peut être écrite plusieurs fois lorsque des triangles se recouvrent.

Les 8 poses V0.2 emploient entre **3 660 et 3 720 octets** d'effacement local. La V0.2 repeint néanmoins une fois les 16 000 octets de fond **au démarrage** : ce coût initial est exclu du coût par image **en animation continue**.

## Vérification de la géométrie et des écritures

- Pour la V0.2 : **846 spans longs** (`PUSH BC`) et **1 807 spans courts** sur 8 poses, soit **17 181 écritures vidéo de triangles**.
- La simulation des deux noyaux de spans donne les mêmes octets que l'écriture de référence, à partir des 8 poses enregistrées.
- Le contenu écran de chaque pose V0.2 après effacement localisé est identique, **octet pour octet**, au contenu V0.2 après effacement intégral du fond.
- Le *backface culling* change légèrement trois des huit poses comparées à la V0.1 (73, 14, 43 octets différents dans la simulation), car la V0.1 repeignait les faces arrière.
- Les adresses des écritures vidéo simulées restent dans la plage `&C000–&FFFF`.

## Temps de calcul : estimations antérieures, non mesurées

Pour garder trace des hypothèses discutées (elles **ne résultent pas d'un profilage de cycles Z80 de bout en bout**) :

| Hypothèse exploratoire | V0.1 | V0.2 |
| :-- | --: | --: |
| Temps indicatif par image à 4 MHz nominaux | ≈ 275 ms | ≈ 75 ms |
| Plage évoquée en tenant compte des incertitudes | 250–350 ms | 70–120 ms |
| Cadence correspondante à cette plage | ~3–4 FPS | ~8–14 FPS |

Le ratio central de ces hypothèses est **275 / 75 ≈ 3,67 fois**, mais **il ne constitue pas un gain mesuré, prouvé ou prédit avec précision**. Les coûts réels des boucles, du Gate Array et de la contention mémoire CPC6128 peuvent modifier sensiblement ces résultats.

**Le seul gain quantifié et vérifié à ce stade porte sur la quantité de travail simulée (−76,9 % d'effacement, −50 % de triangles), pas sur un FPS.** Le code assembleur est déposé et a été contrôlé statiquement ; **il n'a pas été assemblé avec RASM ni exécuté sur émulateur CPC**.

## Plan de validation des performances

1. Assembler **les deux sources** avec RASM, réparer toute erreur de syntaxe et conserver les binaires exacts qui passent.
2. Afficher les huit poses sur un émulateur CPC6128 à timing matériel fidèle ; vérifier couleurs, adressage écran, banques RAM 4–7 et code avec ROMs désactivées.
3. Mesurer les T-states/frame ou la durée d'une boucle de huit images en bloquant/réduisant les effets d'affichage.
4. Faire la moyenne de plusieurs cycles complets et publier le min/moy/max des durées, ainsi que les FPS mesurés.
5. Distinguer le gain dû au culling, celui du nettoyage régional, celui des registres secondaires et celui de `PUSH BC` par variantes à fonction désactivée.
6. Ne convertir les résultats en cadence de jeu qu'après validation visuelle de la synchronisation et des artefacts.

## Fichiers de versioning associés

- [Historique des versions CPC Z80](VERSIONING.txt)
- [Historique général de la branche `raster`](../../VERSIONING.txt)
- [Détails d'implémentation V0.2](README_V02_FAST_REGISTERS.md)
- [Architecture proposée initialement](OPTIMISATION_V02_FAST_REGISTERS.md)

**État :** sauvegarde GitHub des deux sources et du comparatif effectuée ; benchmark matériel encore à réaliser.


## Vérification de compilation — 20 octobre 2026

La V0.1 (**5 069 octets**) et la V0.2 (**3 160 octets**) sont maintenant **assemblées avec succès sous RASM 3.2.4bis** dans [GitHub Actions](https://github.com/glaby91/moteur3d/actions/runs/39394874757). Correction appliquée aux deux sources : syntaxe hexadécimale RASM `#xxxx`. Les précédentes mentions « non assemblé » décrivent l'état antérieur à cette vérification. **Toujours aucune exécution graphique ni mesure de FPS CPC6128.**
