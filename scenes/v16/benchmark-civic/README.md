# CSE WARP V17.325 — Palais unique, deux mosaïques distinctes et piscine olympique

Scène **fictive** du moteur CSE WARP. Ce bâtiment n'est pas une reconstitution
historique du Palais de la Culture Energetik : seule sa **véritable mosaïque
artistique déjà reconstituée** est réutilisée comme ressource visuelle,
sans modifier l'original. La nouvelle statue soviétique est **entièrement
originale**, sans attribution historique.

## Architecture et navigation

- **Un seul grand bâtiment** public, emprise 101 × 50 m : suppression des
  ailes adjacentes, de la passerelle, des immeubles lointains et de l'abribus.
- Parvis continu, avec quatre marches visibles qui rejoignent une dalle
  d'entrée au niveau du palais : plus de 14 m de vide entre escalier et façade.
- Porte centrale de largeur supérieure à 10 m : suppression de la verrière
  basse et du poteau central qui bloquaient physiquement le franchissement.
- Piscine **olympique 50 × 25 m au RDC**, avec fosse, murs en céramique,
  eau, 10 couloirs, lignes d'eau, marquages et plots de départ.
- Dalle du sol et terrain découpés pour **laisser une ouverture réelle** sur
  le bassin ; aucun plancher fictif ne traverse les 25 m de largeur de la piscine.
- Galeries sur plusieurs niveaux, long escalier sur la plage est et **25 marches
  continues dans le hall jusqu'au premier étage** ; foyer ouvert.
- Deux lignes de luminaires au-dessus des grands côtés du bassin (sur 53 m),
  éclairage ponctuel discret répété sur toute la longueur.

## Fresque réellement réutilisée

Fresque d'Energetik à **9,5 × 4,3 m**, exclusivement sur le mur du **hall
au-dessus du grand escalier**, dans un cadre métallique. L'ancien emplacement
sur le mur du fond de la piscine a été supprimé. Le fichier source est
`experiments/2026-10-30-cote-ete-gt/native_v14/assets/pripyat/textures/energetik_fresco_mosaic.jpg`
(2048 × 930, 790 231 octets) et est réutilisé **à l'identique**, comme
`textures/energetik_fresco_mosaic.jpg`. Un nouveau chemin facultatif
`pbr.sets[].imageFile` permet au moteur V16/V17 de charger le JPEG,
préparer ses mipmaps sRGB en lumière linéaire, et donner une texture
identique aux rendus CPU, Compute et DirectX. Les anciennes textures PBR
restent inchangées. La fresque elle-même est une reconstruction artistique
préexistante, pas une photographie certifiée d'époque.

## Les deux œuvres ne doivent jamais être interverties

**Mosaïque extérieure (façade centrale)** : reproduit le costume multicolore
et le personnage fournis par l'utilisateur le 11 novembre 2026. Le JPEG
`textures/personnage_facade_mosaique.jpg` est un dérivé optimisé pour un
premier rendu 3D (160 × 261 px, SHA-256
`9336fd983194cfc4d1f13475049a5405fc182e4f20a6220c6f447fc454c1c9eb`).
Les dimensions physiques sont **8 × 13,05 m**, de y=5,725 à 18,775 m,
sur le mur **extérieur** central, au-dessus de l'entrée. Les joints
de tesselles de 20 cm et le cadre sont des objets réels du moteur.
Le premier rendu WARP de V17.323 a révélé une répétition de la texture
causée par les UV de l'instancing GPU. V17.324 corrige cette divergence :
**une œuvre = une image entière sur la face**, UV 0..1 sur CPU/Compute,
mode d'instancing DirectX 11 dédié (UV mode 2), sans changer les UV
répétitifs des autres matériaux PBR.
Pour une reproduction de près, le PNG source 244 × 398 fourni dans la
conversation pourra ultérieurement remplacer la version JPEG optimisée.

**Fresque Energetik intérieure** : asset préexistant 2048 × 930 conservé
**octet pour octet** (9,5 × 4,3 m), situé sur le mur du **hall** au-dessus
d'un grand escalier de **25 marches**, jusqu'à la galerie de 4,5 m.
Le vieux placement sur le mur de fond de la piscine a été retiré.

**Piscine** : aucune œuvre figurative sur le mur de fond. Ce sont le
carrelage, les vitrages et la lumière sur toute la longueur qui
forment le décor. Tous les profils de charge gardent ces mêmes
positions architecturales.

### Validation progressive V17.323

Le validateur contrôle le SHA-256 du portrait et la fidélité binaire de
l'asset Energetik, l'orientation spatiale des deux images, l'absence de
recouvrement du passage d'entrée, les 25 marches du hall et leur
raccord à la galerie, les deux peintures **interdites** dans le bassin,
les 96 modules du banc ovale et la suppression des immeubles secondaires.

## Aménagement extérieur

- **Ancien bassin extérieur et bancs rectilignes supprimés**.
- **Grand banc ovale en béton** : 96 modules continus autour du centre
  (X=37 m, Z=52 m), rayons de 16,45 × 10,5 m, assise à 48 cm.
- Butte de terre elliptique de 29 × 17,2 m, relief ~1,5 m, fleurie de
  feuillages rouges, dorés, bleus, violets et verts.
- Statue de style civique soviétique des années 1970, composition d'auteur
  intitulée **« L'Élan »** : silhouette héroïque en bronze stylisé,
  bras levé et flambeau, socle en béton, rayons abstraits.
  Environ 8 m de haut ; **ce n'est pas un véritable monument de Pripiat**.
- Lampadaires et arbres paysagers périphériques conservés ; aucune
  construction supplémentaire à l'extérieur.

## Continuité intérieure — V17.325

- Le premier plancher de la galerie est désormais **percé d'une trémie** au
  droit du nouvel escalier d'entrée de 25 marches. La hauteur libre du corps
  (1,76 m) est protégée et la dernière marche rejoint un vrai palier.
- Le hall dispose d'un escalier praticable vers le niveau 4,5 m, sans
  traverser une dalle. L'autre escalier d'accès aux galeries côté piscine est
  conservé.
- Le RDC du bâtiment principal comprend des **vestiaires et douches
  intérieurs**, notamment un espace à l'ouest de la piscine avec 12 casiers,
  5 postes de douche, 3 bancs intérieurs et luminaires, ainsi qu'un local
  de service avec 6 casiers à l'est. Aucun bâtiment annexe n'est ajouté.
- Une caméra dédiée **Vestiaires et douches** permet de contrôler
  l'aménagement depuis CSE WARP.
- En plus des tests dimensionnels, `benchmark_scene_load_smoke` inclut
  des tests BVH de marche depuis l'extérieur et de montée du grand escalier.
  Ces tests servent de garde-fou, pas de certificat d'accessibilité.

## Captures de contrôle depuis le vrai moteur — V17.326

Les 9 points de vue du palais sont rendus en **DX11 Raster sous Microsoft WARP**
avec `moteur3d_v17_benchmark --smoke --all-cameras`. Le dossier de
livraison contient ensuite les **9 images PNG réellement calculées** dans
`benchmark-civic/render-views-WARP`, et elles sont archivées aussi dans
les résultats de simulation totale. Cela permet de vérifier indépendamment
la façade, la mosaïque à courte distance, l'entrée, la piscine, la
fresque du hall, les vestiaires, la galerie et le monument.

Le test vérifie le nombre exact de vues et l'existence des PNG, **mais
ne remplace pas leur inspection visuelle** : un éventuel mauvais
positionnement reste à corriger avant validation esthétique finale.

## Test et génération

```powershell
node scenes/v16/benchmark-civic/generate.cjs --all
node scenes/v16/benchmark-civic/validate.cjs
```

Profils `scene.json` (léger 20k triangles de boîtes), `scene-lourd.json`
(150k) et `scene-extreme.json` (500k) : le volume principal, les espaces
navigables et les décors sont les mêmes ; seule la densité de matériaux
et de revêtements varie. `validate.cjs` contrôle les plans libres de la
piscine, le recul des galeries, l'entrée, la géométrie de l'ovale, le
monument, la couleur de la flore et l'identité **octet pour octet** du JPEG.

Les images Raster/CSE WARP et les benchmarks sur Windows sont à lire dans
les artefacts GitHub Actions. Des performances à 60 FPS n'ont pas encore
été démontrées sur l'Intel HD 620 ni sur un Ryzen 7 8700G.
