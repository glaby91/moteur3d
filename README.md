# moteur3d

Moteur 3D expérimental (archives QBasic + démonstrations HTML/JavaScript).

## Parcours détaillé du moteur 3D — branche côte d’été (30–31 octobre 2026)

### Version de travail actuelle : V13.15

**[Ouvrir le moteur V13.15 — backend CPU](experiments/2026-10-30-cote-ete-gt/moteur3d_v1315_cpu_backend_2026.html)** · [V13.14 — référence Full HD mesurée](experiments/2026-10-30-cote-ete-gt/moteur3d_v1314_fhd_upscale_2026.html) · [Architecture CPU / FPGA](docs/CPU_FPGA_RENDER_BACKEND.md) · [Dossier des versions](experiments/2026-10-30-cote-ete-gt/)

Cette branche est une démonstration **HTML/JavaScript autonome de lancer de rayons sur CPU**, affichée dans Canvas. Elle assemble une caméra à perspective rectilinéaire, des intersections de primitives et de surfaces, un BVH pour accélérer les rayons caméra, des textures, l’éclairage et des ombres par carte de profondeur. Ce n’est pas un jeu avec moteur physique : la conduite, les collisions entre voitures et l’animation physique des dépassements ne sont pas implémentées.

La V13.15 introduit une interface de backend versionnée (début de frame, rendu par tuile, fin de frame) et route le calcul par le backend de référence **CPU logiciel**. Le rendu reste identique et sur un seul thread JavaScript ; le nombre de threads logiques rapporté par le navigateur est affiché à titre indicatif, sans prétendre les utiliser. Cette étape prépare la séparation entre moteur, backend CPU natif multithreadé et éventuelle carte FPGA, mais n’ajoute pas encore de parallélisme ni de gain de vitesse.

La V13.14 propose une sortie **Full HD (1920 × 1080)** et plusieurs résolutions internes. Le mode réduit recommandé calcule **1680 × 945**, puis agrandit l’image en 1920 × 1080. Il lance **1 587 600 rayons au lieu de 2 073 600**, soit **23,4 % de rayons en moins**. Cette économie de rayons ne garantit pas la même baisse du temps total : elle dépend aussi du BVH, des textures, des ombres et du navigateur.

**Comparaison utilisateur V13.14, Full HD et mêmes réglages :** le mode natif (1920 × 1080) lance 2 073 600 rayons et prend **4 882,9 ms** ; le mode équilibré (1680 × 945, agrandi en Full HD) lance 1 587 600 rayons et prend **4 429,6 ms**. Les rayons diminuent de **23,4 %**, mais le temps total baisse de **9,3 %** (453,3 ms). Le temps BVH passe de 2 868,5 à 2 545,5 ms (−11,3 %) et l’éclairage/textures de 1 151,4 à 989,7 ms (−14,0 %). En équilibré, le moteur a compté 3 799 490 tests géométriques, 20 233 042 nœuds BVH visités, 39 623 644 tests AABB, 4 712 540 texels lus et 7 557 852 lectures de profondeur d’ombre. La charge moyenne par rayon reste proche : 25,0 tests AABB, 2,4 tests géométriques, 3,0 texels et 4,8 lectures d’ombre. Ce relevé provient des deux captures du poste utilisateur ; les millisecondes dépendent du navigateur et de l’ordonnancement.

L’agrandissement Canvas lisse l’image, mais ne recrée pas les détails absents du calcul interne. L’anti-crénelage FXAA en post-traitement et le calcul fovéé (davantage de détails au centre, réduction légère aux bords) restent des pistes, non des fonctions intégrées à la V13.14.

### Étapes de conception et de construction

| Versions | Étapes réalisées |
|---|---|
| **V9.2** | Base de rendu logiciel 16:10 : rayons caméra, primitives analytiques, BVH, textures RGB565, mipmaps, ombres par carte Z, rendu progressif et compteurs de coût. Le ciel et les nuages peuvent être évalués sur une grille angulaire interpolée. Le réglage de résolution interne propose des compromis entre précision et nombre de rayons. |
| **V9.3** | Création de la scène urbaine Manhattan/Silent Hill : façades, mobilier de rue, escaliers de secours, chaussée humide, éclairages locaux chauds, brume et créature organique à silhouette élancée, bras levé et appendices courbés. |
| **V9.4–V9.8** | Ajout de maillages lissés et augmentation du niveau de détail. Construction d’un coupé GT classique dans la scène ; amélioration du vitrage fastback et de la géométrie de la créature. |
| **V9.9–V10.5** | Intégration plus complète de la carrosserie et de l’habitacle ; vitrage arrière raccordé au pavillon ; corrections de proportions et de silhouette avec une caisse GT plus longue, basse et large. |
| **V11.0–V11.2** | Remplacement de la scène urbaine par une route côtière estivale. Conception d’un coupé restomod 2026 à partir du fastback initial, avec carrosserie cuivre, pavillon effilé, lunette plongeante, phares ronds, pneus et jantes détaillés. La V11.2 est décrite dans le projet avec une coque d’environ 93 000 triangles. |
| **V11.3–V11.9** | Corrections successives du modèle : placement des roues, léger débord latéral, dégagement des pneus dans les ailes, arches continues, vitre latérale arrière et raccords des vitrages ; détails de carrosserie, pneus et jantes. |
| **V12.0** | Révision des volumes : coffre raccourci, arrière plus massif, arches élargies et grille avant mieux alignée. |
| **V13.0–V13.1** | Reprise du design fastback d’origine avec une interprétation Manta/Cayman restomod. Refonte des raccords pare-brise/pavillon/vitres/lunette/coffre ; correction des pointes d’ailes et de la hauteur apparente des pneus. |
| **V13.2–V13.5** | Ajout des éléments de scène côtière : soleil bas, deux sources lumineuses locales, éclairage de route, mer et vagues, palmiers et trois voitures rouge, violette et orange. Une capture de rendu V13.5 est archivée dans le dossier. |
| **V13.6–V13.8** | Optimisation du lancer de rayons : passage des rayons caméra par le BVH spatial, réutilisation des inverses de direction et stockage scalaire des bornes de nœuds. La capture utilisateur V13.7 indique 3 889,4 ms à 640 × 400 ; celle de V13.8 indique 3 351,5 ms. Ces temps dépendent du poste et des réglages. |
| **V13.9–V13.10** | Une représentation compacte des bornes a d’abord causé une erreur de rendu ; la version suivante a corrigé les limites de projection. La régression reste dans l’historique, pas dans la version courante. |
| **V13.11–V13.12** | Essai de parcours BVH en tableaux typés, abandonné après une hausse du temps mesuré. Retour au parcours scalaire, ajout d’une construction BVH SAH à 12 compartiments et de compteurs de travail : tests AABB, tests géométriques, lectures de texels et accès à la carte d’ombre. Sur une capture utilisateur V13.12 à 640 × 400 : 559 612 tests géométriques, 3 045 933 nœuds BVH visités, 5 990 398 tests AABB et 474,3 ms au total. |
| **V13.13–V13.14** | Ajout de la sortie Full HD 16:9, distincte de la résolution interne. La V13.14 propose le calcul interne à 1680 × 945, agrandi en 1920 × 1080, soit 23,4 % de rayons en moins que le natif. Plusieurs niveaux restent sélectionnables pour comparer qualité et coût. |
| **V13.15** | Première frontière logicielle de backend : API versionnée début/rendu-tile/fin de frame, backend actif clairement identifié comme référence CPU. Le moteur reste sur un thread dans le navigateur. La détection réelle des capacités et le multithread natif sont les étapes suivantes ; aucun gain n’est revendiqué pour cette version. |

### Bancs d’essai et éléments non intégrés

Les pages HTML comportent aussi des panneaux de diagnostic et d’expérimentation : compteurs de parcours BVH, carte des régions de visibilité, comparaison entre interpolation RGB et patches de surface, essai de reprojection d’une image N vers N+1, et charge de workers concurrents.

Ces panneaux servent à comparer des méthodes. La carte de visibilité vérifie ses zones par comparaison avec l’image complète ; les patches et la reprojection restent des prototypes ; les images N+1 du banc workers sont synthétiques et ne sont pas produites par le moteur principal. Ils ne constituent donc pas des optimisations intégrées ni des gains validés.

Les versions V1 à V9.2 et les expériences historiques sont conservées plus bas dans ce README. Pour comparer des performances, il faut garder le même navigateur, le même poste, la même caméra, les mêmes options d’éclairage et d’ombre et comparer les compteurs avec une résolution interne identique. Les millisecondes ne sont comparables que sur le même environnement.

---

## Référence historique — V9.2, rendu 16:10 plus rapide (19 octobre 2026)

**[Ouvrir V9.2 — ray casting 16:10, ciel optimisé et précision réglable](experiments/2026-10-19-v9-2-fast-sky-16x10/index.html)** · [Documentation et tests V9.2](experiments/2026-10-19-v9-2-fast-sky-16x10/README.md)

La V9.2 conserve les **primitives mathématiques, le BVH, les ombres Z, les textures RGB565 et le format 16:10**. Le ciel et les nuages sont calculés sur une **grille angulaire de 2 à 4 pixels**, interpolée, avec comparaison au ciel original ; les rayons et les intersections caméra ne changent pas en mode natif. Une commande explicite permet de choisir entre **Native (100 % : 1 rayon par pixel affiché)**, **Équilibré (75 % des axes, agrandi)** ou **Rapide (50 % des axes, agrandi : 4 fois moins de rayons)**. Le mode natif reste le défaut pour respecter la contrainte « un rayon par pixel ». À 1920×1200 affiché, le test de simulation CPU a mesuré 10 932 ms en Native contre 2 915 ms en Rapide (**environnement JS/Canvas simulé, pas des mesures de Firefox**). Les compromis de netteté sont documentés.

---

## Version précédente — V9.1, ombrage CPU optimisé (19 octobre 2026)

**[Ouvrir la V9.1 — ray casting 16:10 et éclairage accéléré](experiments/2026-10-19-v9-1-shading-optimized-16x10/index.html)** · **[Documentation, comparaison et mesures V9.1](experiments/2026-10-19-v9-1-shading-optimized-16x10/README.md)**

À la suite d'une mesure **1920×1200 : 1 371,3 ms par image, dont 1 005,8 ms d'éclairage et textures** dans le navigateur utilisateur, la V9.1 conserve **exactement un rayon primaire par pixel** et la caméra sphérique 16:10 de la V9. Elle améliore le calcul de couleur par **courts-circuits rigoureux du PCF 3×3 dans les zones d'ombre ou de lumière homogènes**, **brouillard exponentiel tabulé** et **suppression d'allocations par pixel pour normales et UV**, avec matériaux mis en cache. Un sélecteur permet de revenir au shader **V9 de référence** pour une comparaison A/B en vrai navigateur. À 1920×1200 dans un banc JavaScript simulé, la passe couleur passe de **5 824 ms à 5 039 ms** (−13,5 %), sans changement de rayons ou d'intersections ; les couleurs diffèrent au maximum de 1/255 sur 148 pixels. **Le gain réel sur le poste utilisateur reste à mesurer.**

---

## Version précédente — V9, ray casting sphérique 16:10 (19 octobre 2026)

**[Ouvrir le code HTML autonome du simulateur V9](experiments/2026-10-19-v9-spherical-raycasting-16x10/index.html)** · [Documentation V9, tests et limites](experiments/2026-10-19-v9-spherical-raycasting-16x10/README.md)

La V9 explore une nouvelle architecture : **un seul rayon primaire par pixel**, format **16:10** (640×400 par défaut, puis 800×500, 1280×800, 1600×1000 et 1920×1200), caméra **sphérique mathématique à perspective rectilinéaire**, objets analytiques (ellipsoïdes, boîtes, cônes effilés, plan de sol), première intersection uniquement, et aucune rasterisation de triangles. La scène comporte **119 primitives**, organisées en **63 nœuds BVH** et présélectionnées par tuiles 16×16. Couleurs RGB565, filtrage bilinéaire et pyramide d'écorce **4096²→8²** sont conservés ; une unique lumière et une carte Z d'ombre orthographique indépendante apportent l'éclairage. Le rendu est progressif et ses coûts sont instrumentés. **Cette V9 CPU est expérimentale, sans performance navigateur validée et pas encore plus rapide que V8.1.**

---

## Version précédente — V8.1, rasteriseur CPU optimisé (19 octobre 2026)

**[Ouvrir le simulateur V8.1 — raster optimisé Z puis textures visibles](experiments/2026-10-19-v8-1-optimized-raster/index.html)** · [rapport et tests V8.1](experiments/2026-10-19-v8-1-optimized-raster/README.md)

À la suite d'une mesure utilisateur de **200 ms pour la rasterisation à 800 × 600**, la V8.1 conserve les textures RGB565, les mipmaps 8²–4096², le Z-buffer caméra et les ombres Z, mais rasterise les **8 422 triangles natifs** au lieu de 28 416 triangles dont beaucoup étaient subdivisés de manière coplanaire. Une passe Z identifie les faces visibles avant d'échantillonner les textures. Le niveau de mip est évalué par tuile 16×16 et face. Un sélecteur permet de comparer **Accéléré / Intermédiaire / Référence V8**, avec affichage séparé des temps **Z + textures**. Les tests automatisés confirment des profondeurs et occultations inchangées ; le gain réel doit être mesuré en navigateur.

---
## Version précédente — V8, textures adaptatives (19 octobre 2026)

**[Ouvrir V8 — textures RGB565 + mipmaps 8 à 4096](experiments/2026-10-19-v8-mipmaps-textures/index.html)** · [documentation, mesures et limites](experiments/2026-10-19-v8-mipmaps-textures/README.md)

Le moteur V8 utilise toujours les **28 416 triangles de V7**, son **Z-buffer caméra** et sa **source lumineuse Z stabilisée**. Les textures procédurales de l'écorce, du feuillage, de l'herbe, de la pierre et du bois sont échantillonnées par UV perspective-correctes. L'écorce possède une pyramide RGB565 **4096² → 8² (10 niveaux réels)**, construite progressivement ; les autres matériaux possèdent une pyramide 256² → 8². La sélection du mip dépend des dérivées UV par pixel (donc distance, taille apparente et inclinaison), avec filtres nearest/bilinéaire/trilinéaire, diagnostic coloré des niveaux et contrôle de la distance caméra 4–22 m. **C'est un prototype CPU, pas un rendu FPGA validé.**

---

## Référence précédente — V7 (19 octobre 2026)

**[Ouvrir le simulateur V7 — arbre détaillé et ombres diffuses](experiments/2026-10-19-camera-z-v7-soft-shadows-detailed-tree/index.html)**

[Documentation et tests V7](experiments/2026-10-19-camera-z-v7-soft-shadows-detailed-tree/README.md)

- Scène : **28 416 triangles côté caméra**, dont un arbre procédural avec **7 972 triangles de géométrie native**.
- Occlusion : **Z-buffer caméra par pixel** ; une seule source directionnelle avec Z-buffer lumineux stabilisé.
- Ombres : filtrage spatial **PCF 4 × 4** et choix de diffusion **Faible / Douces / Très douces**.
- Contours géométriques : adoucissement colorimétrique optionnel **sans altérer le Z-buffer** ; il ne s'agit pas encore de MSAA 4×.
- Optimisation lumineuse : **8 422 triangles natifs** utilisés pour les cartes Z des ombres, sans retrait de géométrie du rendu caméra.
- Statut : tests automatisés de fonctionnement et d'occultation réussis ; fluidité et qualité visuelle à confirmer dans un navigateur réel.

**Historique :** les expériences V1 à V6 ci-dessous sont archivées comme points de comparaison ; leurs mentions de « dernière version » reflètent la date de chaque expérimentation, pas la version recommandée aujourd'hui.

---


## Simulateur HTML expérimental (18 octobre 2026)

- [Cour Low Poly, collisions, lumière temporelle, 32 buffers](experiments/2026-10-18-cour-interactive-32buffers/README.md)
- [Ouvrir le code du simulateur](experiments/2026-10-18-cour-interactive-32buffers/index.html)

Cette démonstration HTML est conservée séparément du moteur QBasic historique.

## Dernière version expérimentale — anti-stuttering (18 octobre 2026)

- [Simulateur HTML : tuiles adaptatives et rendu stable](experiments/2026-10-18-anti-stuttering-tiles/index.html)
- [Notes techniques et mesures](experiments/2026-10-18-anti-stuttering-tiles/README.md)

Le stuttering ne se manifeste plus d'après l'essai utilisateur. Le compteur « Reprojection » (~40 ms) reste à analyser : il comprend le temps de transfert du Canvas, et ne mesure pas uniquement une reprojection 3D. En mouvement, le pré-rendu lourd est volontairement désactivé pour préserver la stabilité.

## Expérimentation actuelle — Z-buffer lumineux unique (19 octobre 2026)

- [Simulateur HTML : une lumière directionnelle avec profondeur Z](experiments/2026-10-19-single-light-shadow-z/index.html)
- [Notes et limites de l'expérience](experiments/2026-10-19-single-light-shadow-z/README.md)
- [Architecture prévue pour plusieurs sources lumineuses](docs/ECLAIRAGE_MULTI_SOURCES.md)

**Une seule lumière avec ombres Z est active.** Le Z-buffer caméra et l'éclairage multi-sources ne sont pas encore implémentés. Cette expérience est distincte de la version anti-stuttering stable du 18 octobre.


## Révision expérimentale V4 — stabilisation des ombres (19 octobre 2026)

- [Ouvrir le code du simulateur V4](experiments/2026-10-19-shadow-stabilized-v4/index.html)
- [Diagnostic et protocole de test](experiments/2026-10-19-shadow-stabilized-v4/README.md)

L'image géométrique reste identique à V3. Les ombres utilisent maintenant **deux cartes Z de la même et unique lumière**, avec interpolation continue des tests de profondeur pour limiter les sauts. Le mode « Ombres stabilisées » est activé par défaut et désactivable pour comparer; les performances en navigateur réel restent à mesurer.

## Nouvelle expérience V5 — Z-buffer caméra (19 octobre 2026)

- [Ouvrir le simulateur V5 : visibilité caméra par pixel et ombres V4](experiments/2026-10-19-camera-z-v5/index.html)
- [Notes de développement, protocole et limites V5](experiments/2026-10-19-camera-z-v5/README.md)

**Nouveau mode par défaut : Z caméra logiciel par pixel**, pour corriger les triangles du fond affichés devant les objets lorsque les triangles étaient triés par profondeur moyenne. Le tampon couleur et le tampon Z sont communs au sol, au décor et au personnage. Une seule lumière Z stabilisée est conservée. L'ancien mode V4 reste sélectionnable pour comparaison. La V5 nécessite encore une optimisation de la rasterisation et un essai visuel dans un vrai navigateur.

## V6 — Test de charge géométrique ×10 (19 octobre 2026)

- [Simulateur V6 : 24 100 triangles, Z caméra et lumière Z stabilisée](experiments/2026-10-19-camera-z-v6-24100-triangles/index.html)
- [Compte rendu technique et mesures du test](experiments/2026-10-19-camera-z-v6-24100-triangles/README.md)

Toute la géométrie de la scène V5 est divisée en dix triangles coplanaires par triangle initial, sans déformation des objets : 2 310 → 23 100 triangles du décor et 100 → 1 000 triangles du personnage. Un seul éclairage Z est conservé. Le premier essai logiciel à 320 × 240 fonctionne, mais le surcoût CPU est élevé ; priorité ensuite à la rasterisation tuilée et à l'optimisation des shadow-maps.

## Dernière expérimentation V6 — 10 fois plus de triangles (19 octobre 2026)

- [Simulateur HTML V6 : 24 100 triangles, caméra Z et lumière Z](experiments/2026-10-19-geometry-x10-v6/index.html)
- [Résultats des tests de charge et limites](experiments/2026-10-19-geometry-x10-v6/README.md)

Par rapport à V5 (2 410 triangles), le décor et le personnage sont réellement subdivisés en **24 100 triangles**. La scène conserve ses silhouettes et sa géométrie, afin d'isoler le coût lié au nombre de primitives. Le sélecteur « Maillage » compare ×1 et ×10, avec 320×240 au démarrage par prudence. Le Z-buffer caméra par pixel et l'unique source lumineuse à ombres Z restent actifs. Tests d'occlusion et de couverture réussis ; performances sur navigateur réel à mesurer et optimisations CPU nécessaires.

## V7 — Arbre détaillé et ombres plus diffuses (19 octobre 2026)

- [Simulateur V7 : arbre haute géométrie et ombres PCF adoucies](experiments/2026-10-19-camera-z-v7-soft-shadows-detailed-tree/index.html)
- [Compte rendu et mesures V7](experiments/2026-10-19-camera-z-v7-soft-shadows-detailed-tree/README.md)

L'arbre comporte 7 972 triangles originaux réellement modelés et 32 volumes ellipsoïdaux de feuillage. La caméra en rend 28 416 pour la scène complète. Les contours des ombres sont adoucis avec PCF 4 × 4 à trois niveaux, et les silhouettes disposent d'un lissage colorimétrique optionnel sans changer les profondeurs. Le tampon Z lumière traite les 8 422 triangles natifs pertinents, avec une source directionnelle unique. Fluidité et rendu à valider visuellement en navigateur.
