# CSE WARP V17.317 — Palais unique, piscine olympique et parvis monumental

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
