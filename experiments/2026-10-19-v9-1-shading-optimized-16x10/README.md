# V9.1 — Ombres adaptatives et shaders CPU allégés

**19 octobre 2026** — Variante indépendante de la V9, suite aux mesures réelles de la scène 1920×1200 : **1 371,3 ms / image**, dont **1 005,8 ms pour l'éclairage et les textures** et 267,5 ms pour les intersections BVH, en filtrage *Plus proche*.

**Source :** `index.html`, version HTML autonome ; la V9 originale est conservée à `../2026-10-19-v9-spherical-raycasting-16x10/index.html`.

## Principe conservé

- Projection mathématique sphérique rectilinéaire, écran **16:10**.
- **Un seul rayon primaire par pixel**, sans rebond ni rasterisation de triangles.
- Premières intersections analytiques sur 119 primitives via BVH de 63 nœuds et présélection 16×16.
- Le calcul de la carte d'ombre de l'unique lumière reste une passe séparée, réutilisée par la caméra. Aucun rayon d'ombre supplémentaire n'est lancé à partir des pixels caméra.
- Même scène, mêmes cartes d'ombres, même UV, mêmes textures RGB565 et mipmaps 8² à 4096².

## Mode de comparaison

Le sélecteur **Traitement de couleur** permet :

- **Optimisé — ombres et brouillard** (défaut) : optimisations suivantes actives.
- **V9 — référence intacte** : ombrage V9 original, mêmes rayons primaires, mêmes coordonnées et paramètres, pour comparer les timings et les pixels.

## Optimisations intégrées

1. **Ombres PCF 3×3 à sortie anticipée exacte.** Au moment de générer la shadow-map, on calcule les **minimum et maximum** de profondeur pour chaque voisinage 3×3. Si le point est assurément entièrement éclairé, la fonction retourne 1 immédiatement ; s'il est entièrement occulté, elle retourne 0,28 immédiatement. Sinon, elle exécute les neuf consultations du PCF V9 original. Les comparaisons utilisent exactement le même biais de profondeur (0,095). Cette classification est conservatrice : elle n'introduit aucun échantillonnage supplémentaire ni décalage voulu de l'ombre.
2. **Brouillard exponentiel par table de correspondance.** La fonction `1-exp(-t*0.025)` est tabulée tous les 0,08 unités de distance jusqu'à 327,68, puis interpolée linéairement. Au-delà, la formule exacte est utilisée. Les écarts de couleur observés restent au plus de 1 sur 255 par canal.
3. **Shader sans allocations temporaires de normales et d'UV.** Les vecteurs scratch réutilisables remplacent la création de tableaux pour chaque point éclairé. Les normales analytiques des ellipsoïdes, boîtes et cônes restent calculées au point d'impact ; les UV conservent leur calcul géométrique original.
4. **Cache de matériaux.** Les pointeurs vers les textures et les couleurs plates sont rattachés aux primitives une fois, et actualisés automatiquement lorsque la texture d'écorce 4096² remplace sa version de secours.
5. **Choix de la voie shader par tuile**, et non à chaque pixel. Les paramètres d'ombre et de brouillard sont conservés pour la durée de l'image.
6. **Mesures explicites** du nombre d'ombres PCF complètes et d'ombres PCF évitées.

## Tests d'équivalence

Banc JavaScript avec DOM / Canvas simulés, configuration reproduisant la capture utilisateur : caméra à 10 m, rotation 115°, élévation 8°, champ 93°, texture « Plus proche », carte Z de lumière 160².

| Mesure | V9 référence dans la V9.1 | V9.1 optimisée |
|---|---:|---:|
| Résolution | 1920×1200 | 1920×1200 |
| Rayons primaires | 2 304 000 | **2 304 000** |
| Tests de primitives | 2 475 520 | **2 475 520** |
| Lectures de texels | 1 457 714 | **1 457 714** |
| Temps de la recherche BVH | 3 053 ms | 3 071 ms |
| **Temps de calcul couleur** | **5 824 ms** | **5 039 ms** |
| Temps complet de l'image | 8 926 ms | 8 153 ms |
| PCF complets (3×3) | par point visible | 435 836 |
| PCF complets évités | — | **826 762** |

**Bénéfice du shader : environ 13,5 % dans ce banc ; temps total ≈ 8,7 % plus court.** Attention : ces durées sont celles du **simulateur d'exécution JavaScript sans vrai navigateur** et ne prédisent pas le temps réel en Firefox. Le véritable temps observé par l'utilisateur sur V9 est **1 371,3 ms/image**, dont **1 005,8 ms de couleur**. Il faut absolument relever la valeur V9.1 sur la même machine pour confirmer le gain.

### Exactitude et qualité

- Même nombre de rayons primaires, **mêmes premières intersections**, donc même géométrie et visibilité.
- Comparaison des ombres sur une grille de points : **aucune différence dans les facteurs PCF** ; zones homogènes seulement court-circuitées.
- À 1920×1200, **148 pixels de couleur** diffèrent entre le shader V9 de référence et celui accéléré (écart maximal **1/255 sur un canal**), essentiellement dû au brouillard interpolé.
- À 640×400, 17 pixels de couleur différents au maximum de 1/255 dans le scénario de test.
- Tests supplémentaires à 320×200 : bilinéaire, trilinéaire, nearest, ombres OFF, brouillard OFF, textures OFF et debug mip ; tous les écarts restent de 0 ou 1/255, avec la même quantité de rayons.
- L'auto-test des premières intersections V9 reste valide.

## Utilisation

1. Ouvrir le `index.html` localement dans Firefox/Chrome.
2. Sélectionner les paramètres utilisés dans la capture précédente : **1920×1200, caméra à 10 m, 115°, 8°, FOV 93°, filtrage Plus proche**, ombres activées.
3. Attendre l'image complète, puis comparer « **V9 — référence intacte** » et « **Optimisé — ombres et brouillard** ».
4. Noter surtout **ms éclairage / texels**, puis **ms image complète**, le nombre de PCF évités, et vérifier que les contours des ombres restent naturels.
5. Le rendu progressif continue à compter **une seule fois chaque rayon primaire par image**.

## Limites et prochaines pistes

- Le ray casting CPU à 1920×1200 reste **coûteux**. À ce stade, on n'a pas démontré 10, 30 ou 60 FPS. Les chiffres ci-dessus sont ceux du banc simulé.
- Les nuages procéduraux restent visuellement rudimentaires ; leur apparence n'a pas été modifiée dans cette révision ciblée sur la performance.
- La table d'exp du brouillard est une approximation à interpolation linéaire, même si son effet visuel mesuré est extrêmement faible.
- Les gains futurs devraient viser les **nombreux pixels de ciel et de sol**, le calcul des UV/LOD et la coloration des surfaces homogènes, tout en mesurant les compromis de qualité.
