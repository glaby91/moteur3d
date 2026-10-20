# Proposition V0.2 — Raster Z80 « Fast Registers » pour CPC6128

**Statut : CONCEPTION PROPOSÉE, PAS ENCORE IMPLÉMENTÉE NI BENCHMARKÉE.**  
Branche `raster`. Référence actuelle : [CPC_RASTER_MODE0.asm](CPC_RASTER_MODE0.asm) (Mode 0, 16 couleurs unies, 24 triangles par pose, huit poses préprojetées).

## Règles de matériel

- CPC6128 Z80 4 MHz nominaux, avec arbitrage RAM / Gate Array : le coût CPC réel diffère des T-states théoriques Zilog. Instrumenter l'émulateur ou la machine réelle, ne pas extrapoler les MIPS.
- Mode 0 : 160×200, **deux pixels 4 bits entrelacés dans un octet**, 80 octets par ligne ; écran 16 Ko (16 000 octets réellement visibles).
- Pas de textures ni de Z-buffer par pixel ; tri « painter » sur les objets simples et exclusion des faces arrière.
- L'écran CPC est entrelacé : `&C000 + (y AND 7)*&800 + (y >> 3)*80 + xOctet` ; table de 200 adresses = 400 octets.
- Les ROMs peuvent rester désactivées et le moteur propriétaire garde `DI`, sans appel firmware. Les banques RAM4–RAM7 sont commutées dans la fenêtre 4000–7FFF, **pas gratuitement**.
- Le Z80 expose deux jeux de BC/DE/HL (6 octets courants + 6 secondaires) et deux AF ; `EXX` échange **toutes** les paires BC, DE, HL ensemble (4 T nominal), `EX AF,AF'` échange A/F et A'/F' (4 T). Il n'existe pas de `LD r,r'` direct. Ne pas abuser des permutations par ligne et ne pas toucher aux registres alternatifs dans des sous-routines appelées sans convention.

## 1 — Cache registre proposé, par phase

Les registres sont réalloués selon la phase pour éviter de trop souvent changer de jeu.

| Phase « pas d'arête » | Jeu principal | Jeu secondaire |
|---|---|---|
| Interpolation X de l'arête | `HL` = pointeur de buffer X gauche/droit ; `C` = nombre de lignes restant ; `A` = X courant (partie entière) | `HL'` = X en signé 8.8 ; `DE'` = pente dX/dY en 8.8 ; `BC'` = valeurs auxiliaires / 16 bits suivant sous-programme |
| Remplissage d'un segment | `HL` = destination VRAM ; `BC` = largeur/compteur selon la routine ; `A` = octet pen ; `SP` = pile normale **ou** pointeur écran temporaire | `AF'` = couleur sauvegardée pendant les calculs ; `BC'` = deux octets couleur identiques pour le noyau PUSH ; `HL'/DE'` = valeurs de la ligne suivante |

**Exemple concret d'interpolation d'une arête**, valide conceptuellement pour un côté du triangle, après initialisation :
```z80
; Main: HL = buffer des X de la première ligne ; C = nb de lignes
; Alt : HL' = X en 8.8 ; DE' = variation de X à chaque ligne
EdgeStep:
    EXX                ; 4 T
    LD A,H             ; 4 T : X partie entière (jeu alternatif)
    ADD HL,DE          ; 11 T : X += dx, sans RAM
    EXX                ; 4 T : revenir au jeu principal
    LD (HL),A          ; 7 T : un seul octet d'écriture du span
    INC HL             ; 6 T
    DEC C              ; 4 T
    JR NZ,EdgeStep     ; 12 T quand pris
```
Soit **52 T nominaux par ligne** (hors préparation, clipping et coût CPC) pour incrémenter une arête et consigner son X. Dans V0.1, `EdgeX`, `StepX`, `EdgeY` sont relus/réécrits en RAM pratiquement à chaque ligne : c'est un point d'optimisation concret.

**Attention au découpage :** faire la gauche, puis la droite (deux passages) ou découper le triangle en partie supérieure/inférieure pour éviter les min/max systématiques. Une fois les deux buffers préparés, les parcourir de manière séquentielle sans changement `EXX` par pixel.

## 2 — Routines de peinture spécialisées

- Préparer `PackedPen[0..15]` (un octet « deux pixels de même couleur » par encre).
- Tant que possible, n'utiliser que des spans de largeur entière en **octets vidéo** (80 colonnes de paires de pixels). Pour retrouver la résolution de 160 pixels, ne masquer que le premier et dernier octet des spans dont une extrémité tombe au demi-octet ; ne PAS imposer un read-modify-write pour tous les octets intérieurs.
- Largeur 1–7 octets : routine courte `LD (HL),A; INC HL`, partiellement déroulée.
- Largeur 8–15 octets : tester un déroulage conditionnel, ou passer au PUSH après seuil mesuré.
- Spans longs : **écriture via SP** en ordre décroissant ; utiliser `PUSH BC` (B=C=octet couleur). **Un PUSH BC stocke 2 octets en 11 T nominaux**, soit 5,5 T par octet dans son cœur, contre 21 T par octet pour un `LDIR` répétitif (sans compter préparation). Les résultats CPC varient avec ses WAIT states.
- Avant tout usage détourné de SP : `DI`, sauver le vrai SP dans un emplacement RAM fixe, placer `SP = finDuSpan`, exécuter un bloc de `PUSH BC` déroulé, et restaurer le SP **avant un CALL, RET, PUSH de sauvegarde classique ou interruption**. Prendre en compte l'écriture **décroissante** et les spans de longueur impaire.
- Le coût de sauvegarde/restauration de SP pénalise les petits spans : la méthode PUSH n'est pas automatiquement la plus rapide sur tous les triangles.

**Exemple illustratif** (blocs sans appel entre changement de SP et restauration) :
```z80
    LD (SavedSP),SP   ; pile de retour ordinaire préservée
    LD SP,HL          ; HL = adresse de fin de span (exclusive)
    ; BC = [pen,pen], chargé une fois hors boucle
    PUSH BC           ; écrit deux octets en décrémentant SP
    PUSH BC
    PUSH BC
    PUSH BC           ; exemple = 8 octets pleins
    LD SP,(SavedSP)   ; restauration obligatoire AVANT RET
```
Pour la remise à zéro du fond, les 80 octets par ligne se remplissent par 40 `PUSH BC` déroulés. C'est **8 000 PUSH par écran** et donc environ **88 000 T nominaux pour les écritures seules**, soit 22 ms au Z80 4 MHz idéal, **sans** adressage des lignes, changement de couleur, contention CPC, ni géométrie. Le temps réel sera supérieur : il faudra le chronométrer.

## 3 — Ne pas repeindre toute la VRAM à chaque image

La V0.1 repeint 16 000 octets de fond à chaque frame en plus de ~4 100 à 4 500 octets de triangles : ce poste peut annuler les gains du « cache » registre.

Pour la V0.2 :
1. Dessiner le fond ciel/sol une fois.
2. Mémoriser les boîtes englobantes des objets de l'image précédente dans la RAM de base.
3. Pour chaque image, former l'union des anciennes/nouvelles boîtes, **restaurer le fond seulement sur les rectangles touchés**.
4. Redessiner les triangles intersectant les zones modifiées, dans le bon ordre peintre. En cas de doute sur recouvrement des objets, redessiner la région commune entière.
5. Garder la table des 200 adresses de lignes (400 octets), pour ne jamais refaire le calcul vidéo depuis `y` dans la boucle intérieure.

Le tampon double réduit les déchirures d'image, **mais pas le travail de calcul** : il ne doit pas être confondu avec un gain automatique de FPS.

## 4 — Trigonométrie, multiplications et projection

- Pour une rotation libre sur Z80 : 256 entrées cosinus **signées sur 16 bits, format 8.8 = 512 octets** ; sinus = même table avec décalage modulo 256 de 64 unités d'angle. Précision angulaire ~1,40625°.
- Pour la projection : table de réciproques de distance avec bornes de proximité, ou approximation en quelques classes de Z (512 à 1 024 octets). Une table complète de tous les produits 8×8 sur 16 bits occuperait 128 Ko et n'est donc **pas adaptée à ce projet**.
- Réutiliser les sommets projetés : deux cubes ont 16 sommets uniques, pas 72 sommets à projeter pour 24 triangles.
- Backface culling : pour un cube fermé, environ 3 faces visibles sur 6 depuis un angle générique, soit 6 triangles visibles sur 12 ; pour deux cubes, souvent ~12 triangles réellement rasterisés au lieu de 24. Attention aux arêtes et transparences.
- Tri painter possible pour scène opaque et simple ; les cas d'interpénétration et de cycles d'occlusion nécessitent subdivision ou une autre méthode.
- **Point important :** la V0.1 utilise déjà 8 poses entièrement préprojetées. Ajouter une table cosinus **n'accélérera pas V0.1** telle quelle : elle permettra surtout une rotation dynamique moins coûteuse en RAM de poses.

## 5 — Budget de cycles et stratégie de mesure

Budget brut à 4 MHz (avant WAIT states CPC) :
- 50 FPS : 80 000 T/frame.
- 25 FPS : 160 000 T/frame.
- 20 FPS : 200 000 T/frame.
- 12,5 FPS : 320 000 T/frame.
- 10 FPS : 400 000 T/frame.

Le coût de remplissage seul et les WAIT states empêchent de garantir de tels FPS par un simple tableau. **Objectif exploratoire V0.2 : atteindre 12,5 à 20 FPS sur 24 triangles préprojetés et objets compacts** en combinant culling + registres secondaires + spans rapides + effacement localisé. Ce n'est pas une mesure. Pour une rotation **3D entièrement dynamique**, viser d'abord 8–12 FPS sous réserve de tests.

Ordre d'optimisation et validation avec instrumentation émulateur :
1. Réparer et compiler le prototype Z80 V0.1 sous RASM (contrôle syntaxe et fonctionnalité).
2. Mesurer séparément cycles de `ClearBackdrop`, `StepEdge`, `DrawSpans` (nombre d'appels / bytes écrits).
3. Remplacer l'edge step par la boucle `EXX` ; comparer un frame *identique pixel pour pixel*.
4. Spans spécialisés par longueur ; comparer la technique `PUSH` avec `LD (HL),A` sur CPC et non seulement au temps Zilog.
5. Culling des triangles cachés et effacement par dirty rectangles.
6. Seulement ensuite projection 3D dynamique avec LUTs cosinus / réciproques.
7. Mesurer image complète au 50 Hz sur émulateur à timing Gate Array fidèle ; tester aussi sur matériel CPC6128 si disponible.

Références :
- https://www.cpcwiki.eu/index.php/Z80
- https://www.cpcwiki.eu/index.php/Programming:Filling_memory_with_a_byte
- https://asmtradcpc.zilog.fr/cours/Optimisation_de_l_affichage_des_sprites.php
- https://www.zilog.com/docs/z80/um0080.pdf

Les sources existantes V0.1 et les moteurs HTML/QB64 restent inchangés. Cette documentation ne constitue pas une V0.2 compilée.
