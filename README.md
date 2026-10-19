# moteur3d

Moteur 3D expérimental (archives QBasic + démonstrations HTML/JavaScript).

## Version expérimentale actuelle — V9.1, ombrage CPU optimisé (19 octobre 2026)

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
