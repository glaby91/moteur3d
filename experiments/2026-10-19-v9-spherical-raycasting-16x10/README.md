# V9 — Caméra sphérique mathématique, un rayon primaire par pixel, format 16:10

**Version expérimentale — 19 octobre 2026.** La V8.1 et toutes les anciennes expériences sont conservées. Le fichier `index.html` est autonome et ne nécessite pas de dépendance JavaScript externe.

## Ce qui est programmé

- **Cinq résolutions 16:10** : 640×400 (256 000 rayons), 800×500 (400 000), 1280×800 (1 024 000), 1600×1000 (1 600 000) et 1920×1200 (2 304 000).
- **Un seul rayon primaire par pixel**, depuis le centre de la caméra jusqu'à la première intersection. Aucune réflexion secondaire ou rayon secondaire généré à chaque pixel caméra.
- **Caméra sur surface sphérique virtuelle** : la direction du rayon est perpendiculaire à la sphère unitaire centrée sur la caméra. Les échantillons sont distribués de façon rectilinéaire pour conserver la perspective naturelle, sans effet fisheye marqué.
- **Aucun triangle rasterisé** : l'arbre est construit avec des cônes tronqués de direction quelconque et des ellipsoïdes ; les murs et le banc utilisent des boîtes analytiques ; le sol est un plan infini. Un personnage est assemblé à partir de primitives.
- **119 primitives mathématiques** dans la scène initiale et **63 nœuds BVH** dans son accélérateur spatial ; les volumes englobants BVH sont projetés conservativement à l'écran afin de présélectionner les primitives candidates dans chaque tuile **16×16**.
- **Z caméra implicite**, obtenu par la distance de la première intersection. Il n'existe pas de passe raster caméra et aucune subdivision en triangles.
- **Aucun impact** : ciel dégradé, halo solaire et nuages procéduraux ; **impact** : normale analytique, UV de matériau, texture RGB565, choix du mip, éclairage diffus et brouillard.
- **Une source lumineuse directionnelle**. Les ombres sont fournies par une **carte Z orthographique pré-calculée depuis la lumière** (96²/160²/256², PCF 3×3), qui projette les ombres sur le sol et les objets. Cette passe indépendante emploie des rayons pour construire la carte de lumière, mais **ne lance aucun rayon secondaire depuis les pixels caméra**.
- **Mipmaps RGB565** pour les matériaux écorce, feuillage, herbe, pierre, bois, tissu et peau. Tous les matériaux commencent avec des textures jusqu'à 256² (ou 128²). L'écorce est ensuite améliorée progressivement jusqu'à une véritable chaîne de **4096² → 8²**, composée de **10 niveaux**, construite à partir du matériau procédural initial et de bruit fin par texel.
- Filtres nearest, **bilinéaire par défaut** et trilinéaire, forçage manuel du niveau mip de 8 à 4096 (lorsque le matériau dispose réellement de ce niveau), diagnostic coloré des mipmaps et on/off des textures, ombres, nuages et brouillard.
- **Rendu progressif** avec un budget de calcul par rafraîchissement (8, 14, 24 ou 40 ms). Les pixels sont calculés et affichés tuile par tuile ; la mesure « ms / image complète » inclut le temps entre les rafraîchissements du navigateur.

## Vérifications effectuées

Les tests ci-dessous ont été exécutés sur JavaScript V8 avec Canvas/DOM simulés ; **ce ne sont pas des mesures Firefox ni des FPS réels sur le poste utilisateur**.

| Contrôle | Résultat |
|---|---|
| Départ initial | Auto-tests ciel sans intersection, sol plan, ellipsoïde, boîte, cône analytique réussis |
| Nombre de rayons caméra, image 640×400 | **256 000 exactement** |
| Répartition premier impact, image de test 640×400 | 222 720 pixels d'objets/sol, 33 280 pixels de ciel |
| Vérification BVH / géométrie exhaustive | 3 080 rayons de contrôle sur 5 poses caméra : **0 désaccord** |
| Nombre de primitives et nœuds BVH | 119 primitives, 63 nœuds |
| Filtrage spatial par tuiles | Environ 944 000 → 526 000 tests de primitives à 640×400 sur le scénario de test |
| Cartes d'ombre | Construction testée à 160×160, pixels dans l'ombre et éclairés présents |
| Pyramide d'écorce | **10 niveaux réels**, 4096, 2048, 1024, 512, 256, 128, 64, 32, 16 et 8 |
| Mémoire pyramide d'écorce 4096²→8² | **44 739 200 octets** (42,67 Mio) |
| Forçage des mipmaps | Choix vérifié pour chacun des dix niveaux |

Un benchmark avec mocks a nécessité de l'ordre de **1,3–1,4 s pour une image 640×400**, dépendant du test et du pré-calcul des ombres. Ces chiffres montrent clairement que cette V9 **n'est pas à ce stade plus rapide** que le rendu rasteriseur V8.1 du poste utilisateur. Ne pas l'utiliser comme indication de FPS navigateur.

## Mode d'emploi

1. Ouvrir `index.html` dans un navigateur.
2. Garder **640×400 — 16:10** pour le premier test.
3. Observer la progression des tuiles et les compteurs « rayons caméra », « intersections BVH » et « ms / image complète ».
4. Modifier l'angle, la hauteur, la distance ou le champ de vue ; chaque modification redémarre proprement le calcul d'une image complète.
5. Comparer avec et sans textures et ombres, puis en bilinéaire et trilinéaire.
6. Choisir **800×500** puis **1280×800** uniquement pour comparer les temps de calcul ; à haute résolution, le délai d'une image complète peut être important sur CPU.

## Limites et prochaines étapes

- Le modèle est un **ray caster analytique** à un seul rebond nul ; il ne reproduit pas l'optique physiologique réelle de l'œil. « Sphère » désigne la direction normale des rayons, pas une lentille ou un vrai écran incurvé.
- Il n'y a qu'un rayon **primaire caméra** par pixel ; la carte Z lumineuse constitue un calcul indépendant. Si l'objectif matériel interdit également tout calcul supplémentaire pour les ombres, elle devra être remplacée par un cache géométrique ou un autre approximant.
- Les UV des primitives sont des projections procédurales simples, pas des dépliages UV d'artiste.
- L'empreinte de texture/mipmap est une approximation basée sur la distance, l'angle de la normale, l'échelle UV et l'angle pixel, sans lancer de rayons voisins.
- À haute résolution et sur des scènes complexes, le ray casting logiciel risque d'être plus cher que la V8.1 rasterisée : l'intérêt réel ne sera démontré qu'avec des optimisations matérielles et des benchmarks comparables.
- Le prochain axe de performance est l'élimination des intersections redondantes dans les tuiles, la mise en cache des directions et normales, et l'architecture FPGA par blocs de pixels.
