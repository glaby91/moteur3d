# CPC6128 — mini moteur 3D Z80, 128 Ko, ROM désactivée

**Version 0.1.0 — expérimentation « pour le fun »**, dans la branche `raycasting`.

[Code assembleur](CPC_RAY80.asm) · [Historique](VERSIONING.txt)

## Ce que fait la démonstration

Un véritable **ray-marcher 2,5D**, en perspective de couloir, avec une carte 16×16, un point de vue fixe et des murs dont la hauteur dépend de leur distance.

- Processeur **Z80** sur CPC6128 (4 MHz nominaux).
- Résolution matérielle **Mode 1 : 320×200, quatre encres**.
- **80 rayons**, un par *colonne d'un octet vidéo*, donc un rayon pour **quatre pixels horizontaux identiques**. Ce compromis est volontaire pour obtenir une vitesse raisonnable sur Z80 : **ce n'est pas un rayon par pixel** et ce n'est pas la V9.2 entière.
- Intersections par **marches successives en coordonnées fixes 8.8**, avec une grille 16×16. Pas de calcul flottant, trigonométrie ou multiplication matricielle à chaque rayon.
- Tables de hauteur des murs et motifs d'octets CPC Mode 1.
- Fond bicolore sol/ciel et murs en bandes verticales.
- Vidéo directement écrite à partir d'une table des 200 adresses de lignes CPC.

La vue est fixe dans cette V0.1 ; pas de souris, déplacement, rotation, objets 3D véritablement courbes, ombres calculées ni texture UV complète. **Le code source est un prototype d'assembleur à assembler et tester en émulateur CPC** ; je ne peux pas garantir qu'il est exempt d'erreurs tant qu'un assemblage et une exécution sur machine CPC n'ont pas eu lieu.

## Désactivation de l'OS

L'initialisation effectue :

```asm
DI
LD SP,&3FF0
LD BC,&7F00
LD A,&8D         ; Mode 1 + ROM basse OFF + ROM haute OFF
OUT (C),A
```

Après ce réglage, le programme **n'appelle aucun point d'entrée du firmware**, n'utilise pas l'interruption standard du CPC et reste dans une boucle infinie. On revient à l'ordinateur par **RESET matériel**.

**La ROM de démarrage/BASIC reste nécessaire uniquement pour charger le binaire en mémoire avant le transfert du contrôle**, sauf si l'on produit un chargeur disquette ou cartouche autonome.

## Utilisation effective des 128 Ko de RAM

Le processeur ne voit que 64 Ko à la fois. Le CPC6128 possède **128 Ko physiques**, sélectionnables par le circuit RAM :

| Espace logique | Affectation |
| --- | --- |
| `0000–3FFF` | RAM de base, pile en `3FF0` |
| `4000–7FFF` | Fenêtre paginable vers RAM étendue 4, 5, 6, 7 |
| `8000–BFFF` | Binaire Z80 + tables incorporées |
| `C000–FFFF` | RAM vidéo 16 Ko, Mode 1 |

| Sélection vers `4000–7FFF` | Données réellement stockées et utilisées |
| --- | --- |
| `C4` → banque 4 | Carte 16×16 des murs |
| `C5` → banque 5 | LUT des hauteurs suivant la profondeur du rayon |
| `C6` → banque 6 | Octets couleur/texturage Mode 1 suivant la profondeur |
| `C7` → banque 7 | Cache des profondeurs des 80 rayons |

**Attention :** ce n'est pas parce que les quatre banques supplémentaires sont utilisées que les **64 Ko étendus** sont remplis. Pour l'instant, cette version ne consomme qu'une petite partie de leur capacité. L'intérêt est de valider le mécanisme de commutation pour un futur moteur plus riche.

## Organisation vidéo CPC

En configuration CRTC standard, l'adresse vidéo d'une ligne est :

```text
C000h + (y AND 7) * 0800h + INT(y / 8) * 80 + xOctet
```

La vidéo est entrelacée en huit blocs. Le **Mode 1** contient quatre pixels par octet (320 pixels horizontaux = 80 octets/ligne). La source comporte une table complète des 200 lignes et évite de recalculer leur adresse à chaque écriture.

## Assemblage et exécution envisagés

Assembleur cible : **RASM ou assembleur compatible WinAPE** acceptant `ORG &8000`, `DB`, `DW`, `LD` et le registre `IX`.

1. Assembler `CPC_RAY80.asm` en binaire brut, à destination de `&8000`.
2. Charger le binaire sur CPC6128/émulateur à `&8000`. Exemple conceptuel d'amorçage depuis BASIC :
   `MEMORY &7FFF` puis `LOAD "CPC_RAY80.BIN",&8000` et `CALL &8000` (à adapter au nom de fichier et au format de sortie de l'assembleur).
3. Le programme désactive les ROMs et les interruptions, configure sa palette et dessine la scène **sans firmware**.
4. Arrêt par **RESET**.

**Limite des tests actuels :** pas d'assembleur Z80 ni d'émulateur CPC disponibles dans l'environnement d'exécution. Une vérification de syntaxe et de l'affichage sur WinAPE, ACE-DL ou Retro Virtual Machine reste indispensable avant de promettre un binaire opérationnel sur un vrai CPC.

## Feuille de route 0.2.x

- **Rotation de la caméra** par tables de sinus/cosinus 8.8 stockées en banque 5.
- **Pas de DDA exact** : remplacer le marcheur fixe par un DDA de grille en entiers, pour un tracé précis des murs.
- Textures 8×8 / 16×16 réellement échantillonnées selon la hauteur de la colonne.
- Réutilisation de la banque 7 pour un framebuffer logique et temps de rendu mesuré.
- Palette dynamique et temporisation compatible 50 Hz.
- Option Mode 0 (160×200, 16 encres), ou Mode 1 conservé pour la netteté.

## Références matérielles

- https://cpcwiki.eu/index.php/Gate_Array
- https://cpctech.cpcwiki.de/docs/garray.html
- https://www.cpcwiki.eu/index.php/Programming:Calculating_xpos,ypos_to_screen_address_in_assembly
- https://www.cpcalive.com/docs/amstrad_cpc_6128_memory_map.html
