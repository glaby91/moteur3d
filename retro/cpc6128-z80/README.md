# CPC6128 — Raster 3D Z80 à triangles pleins, Mode 0 (version 0.1.0)

**Source :** [CPC_RASTER_MODE0.asm](CPC_RASTER_MODE0.asm) · **Branche :** `raster` · **Date :** 19 octobre 2026.

Une démonstration autonome d'un **rasteriseur de triangles** sur **Z80 CPC6128 à 4 MHz**, dans une scène simplifiée de deux volumes 3D tournants. **Aucun texturage, aucun Z-buffer par pixel : 16 encres plates en Mode 0**, avec tri des triangles dans les données de chaque pose.

## Fait et enregistré sur GitHub

- **80 octets vidéo par ligne en Mode 0, soit 160×200 pixels avec 16 couleurs** ; chaque octet contient deux pixels identiques de couleur pleine, donc **précision horizontale de remplissage limitée ici à 80 colonnes de paires de pixels**.
- **24 triangles par image**, appartenant à deux cuboïdes 3D.
- **8 poses de rotation 3D préprojetées** hors Z80, et triées par profondeur avant d'être intégrées à l'assembleur. Le Z80 **n'effectue donc pas en temps réel les transformations matricielles et la projection 3D** ; il exécute bien en revanche la rasterisation des triangles dans l'écran.
- Trois arêtes par triangle, coordonnées X en **virgule fixe signée 8.8**, accumulation sur les lignes, génération des intervalles horizontaux (spans) ; chaque span est rempli par une seule valeur d'octet couleur. Les arêtes horizontales sont ignorées car les autres arêtes délimitent les spans.
- **Pas de mipmap, de textures, de multiplication par pixel ou de Z-buffer** ; priorité d'affichage par ordre peintre, suffisante pour cette démonstration simple.
- **Pas d'appels firmware dans la boucle d'animation**. Le binaire est chargé auparavant avec BASIC/chargeur et le moteur exécute `DI`, pile en RAM de base, **Mode 0 avec les ROMs haute/basse désactivées** au Gate Array (`&8C`).
- **128 Ko physiques du CPC6128 :** code, écran et buffers dans la RAM de base ; les banques supplémentaires **RAM4 à RAM7** sont commutées via `&C4` à `&C7`, contenant chacune **deux poses** du moteur.
- Animation en boucle sans accès au firmware ni gestion clavier ; la sortie requiert **RESET matériel**.

## Organisation mémoire

| Adresse Z80 | Rôle |
|---|---|
| `0000–3FFF` | RAM commune ; `3000` / `3100` buffers de spans de 200 octets chacun ; pile en `3FF0` |
| `4000–7FFF` | Fenêtre bancaire, RAM4 à RAM7, deux poses préprojetées par banque |
| `8000–BFFF` | Programme, tables d'origine (8 poses) et table des 200 adresses vidéo |
| `C000–FFFF` | RAM écran Mode 0 |

Une pose occupe **528 octets**, deux poses **1 056 octets**, et l'ensemble des 8 poses **4 224 octets**. Les 4 banques supplémentaires sont **utilisées**, mais **ne sont pas intégralement remplies** ; les 128 Ko constituent une capacité disponible, et non la mémoire effectivement consommée.

## Validation statique réalisée

Vérifications du fichier déposé sur GitHub, **sans compilateur ni émulateur CPC** :

| Vérification | Résultat |
|---|---|
| Poses | 8 |
| Triangles par pose | 24 |
| Longueur d'un triangle | 22 octets |
| Table vidéo | 200 lignes, toutes les adresses standard CPC correctes |
| Dépassement des spans hors des 80 colonnes | Aucun sur les 8 poses |
| Total d'intervalles verticaux traités / pose | 620 à 684 lignes de triangles cumulées |
| Total d'octets vidéo peints par les triangles / pose | 4 128 à 4 462 octets avant occultation |
| Remplissage du fond | 16 000 octets vidéo / image |
| Total d'écritures vidéo visibles planifiées | Environ 20 100 à 20 500 octets / image |

**Ces validations sont statiques.** Il reste indispensable de **l'assembler avec RASM ou un assembleur compatible CPC, puis de tester son exécution et son timing sur un émulateur fidèle ou un CPC6128 réel**. En particulier, elles ne garantissent ni l'assemblage ni un FPS mesuré.

## Performances : objectifs et incertitude

Les hypothèses discutées pour un futur rasteriseur **ne sont pas des FPS vérifiés pour ce code** :

| Configuration théorique | Fourchette envisagée (non mesurée) |
|---|---|
| Scène très simple, 10–20 triangles | 12–20 FPS |
| Scène légère, 30–60 triangles | 8–12 FPS |
| Scène plus chargée, 100–150 triangles | 3–6 FPS |
| **Cette V0.1.0, 24 triangles préprojetés** | **FPS non mesuré** |

Ces ordres de grandeur sont **des objectifs exploratoires**, fortement dépendants du remplissage des surfaces, du coût de remise à zéro des 16 Ko de l'écran, du Gate Array et de la cadence effective du Z80. Les deux cuboïdes de la V0.1.0 sont volontairement très remplis : l'exemple écrit environ 20 Ko de données vidéo par image (fond et triangles), avec un nombre important de sauts et d'adressages vidéo. **Ne pas présenter 12 FPS comme une performance atteinte.**

## Assemblage visé

1. Assembler `CPC_RASTER_MODE0.asm` avec un assembleur CPC compatible `ORG &8000`, `DB`, `DW`, `IX`, `EQU`.
2. Générer un binaire brut chargé en `&8000` sur un CPC6128 ou un émulateur ; amorçage possible depuis BASIC après `MEMORY &7FFF`, puis `LOAD "RASTER.BIN",&8000` et `CALL &8000` suivant le support utilisé.
3. Le programme passe ensuite directement en Mode 0 et désactive les ROMs, et peint en boucle ses huit orientations.
4. Sortie par RESET. **Aucun .DSK ou .BIN exécutable n'a encore été validé dans cet environnement.**

## Améliorations prévues

- Projection 3D réellement calculée sur Z80, si le budget de cycles le permet.
- Clipping proche, exclusion des faces arrière, tri plus fin des surfaces.
- Remplissage fin des deux pixels distincts d'un octet vidéo, pour une vraie précision 160 colonnes.
- Effacement partiel des zones modifiées, synchronisation avec le VBL et double tampon vidéo si la mémoire disponible le permet.
- Instrumentation précise du temps par frame, sur émulateur CPC à synchronisation fidèle.

Le **raycaster 80 rayons du CPC6128** reste séparément dans la branche [`raycasting`](https://github.com/glaby91/moteur3d/tree/raycasting/retro/cpc6128-z80) ; les deux architectures peuvent être comparées ultérieurement.

## Étape suivante : conception V0.2 Fast Registers

Une [spécification détaillée d'optimisation Z80](OPTIMISATION_V02_FAST_REGISTERS.md) a été ajoutée : jeu secondaire `BC'/DE'/HL'` comme cache de calcul, `EXX` pour interpoler les arêtes, écriture vidéo rapide par `PUSH`, effacement partiel et LUTs trigonométriques. **Proposition seulement : aucune V0.2 compilée, aucun nouveau FPS mesuré.**
