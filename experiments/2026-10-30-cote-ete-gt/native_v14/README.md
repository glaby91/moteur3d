# Moteur 3D natif — données réelles V13.14

Cette application Windows native réutilise la scène construite par le moteur V13.14. Elle ne recrée ni les voitures ni le décor avec une scène de démonstration.

## Ce que le build reprend

`source_v1314.html` est le moteur de référence. `export_scene.js` l’exécute côté build et appelle ses propres `makeScene()`, `buildBVH()` et fonctions de génération RGB565. Le fichier `moteur3d_v1314_scene.bin` contient les primitives, les normales et coordonnées UV des triangles, l’ordre et les nœuds du BVH, ainsi que les pyramides de textures. À l’exécution, le programme charge ce binaire ; il n’ouvre ni navigateur ni WebView.

| Donnée de la scène | Valeur |
|---|---:|
| Primitives totales | 1 255 856 |
| Triangles | 1 254 730 |
| Boîtes arrondies | 106 |
| Cônes | 503 |
| Capsules | 447 |
| Ellipsoïdes | 68 |
| Nœuds BVH exportés | 746 457 |
| Matériaux avec textures RGB565 | 21 |

## Calcul d’une image

1. La caméra calcule un rayon primaire par pixel de la résolution interne.
2. Le rayon traverse le BVH source, puis teste les primitives candidates : triangles, boîtes, boîtes arrondies, capsules, cônes et ellipsoïdes.
3. Le point d’impact, la normale lissée et les coordonnées UV déterminent le matériau et son niveau mip.
4. Le moteur échantillonne les textures RGB565 et applique l’éclairage directionnel, les deux lumières ponctuelles, la carte d’ombre et son filtrage, puis le ciel et le brouillard.
5. Les tuiles de 16 × 16 pixels sont réparties entre les threads CPU.
6. En mode Équilibré, l’image calculée à 87,5 % sur chaque axe est réadaptée à la résolution de sortie par filtrage bilinéaire.

L’interface permet de régler la résolution de sortie, l’échelle de calcul, le nombre de threads et la caméra. Les diagnostics affichent les compteurs réellement observés pendant ce calcul : rayons, intersections, tests de primitives, visites du BVH, tests AABB et texels lus. Le temps du post-traitement est indiqué séparément.

## Construire et lancer

Le workflow GitHub Actions exporte la scène, compile le programme Win32 en C++17 avec MSVC, puis crée l’artefact `Moteur3D-V14-Natif-Scene-V1314.zip`. L’archive contient l’exécutable et son fichier de scène de 196 Mio ; extraire les deux fichiers dans le même dossier avant de lancer l’exécutable.

Le moteur reste logiciel : les calculs de rayons, intersections, textures et éclairage sont effectués par le CPU. Les threads sont configurables dans l’interface.
