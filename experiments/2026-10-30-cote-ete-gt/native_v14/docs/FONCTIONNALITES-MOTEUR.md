# Fonctionnalités du moteur 3D natif x64 — V13.14

Ce document décrit l’application Windows native et les modules C++ compilés de la branche `experiment/manhattan-silent-hill-2026-10-30`. Il distingue les fonctions réellement présentes des limites actuelles.

## Résumé / Overview

| Français | English |
|---|---|
| Moteur logiciel x64 pour Windows. L’image est calculée sur le CPU par lancer de rayons accéléré par un BVH. | Native x64 software renderer for Windows. The CPU computes the image with BVH-accelerated ray tracing. |
| La géométrie, les matériaux, les textures et l’accélération spatiale sont exportés dans un fichier binaire placé à côté de l’exécutable. | Geometry, materials, textures, and spatial acceleration are exported into a binary file stored beside the executable. |
| L’interface règle la caméra, la qualité, l’heure, le brouillard et les threads CPU. | The UI controls the camera, quality, time of day, fog, and CPU threads. |

## Rendu et profondeur / Rendering and depth

- Une caméra perspective émet un rayon par pixel de calcul. Le moteur conserve l’impact le plus proche et utilise les limites du BVH pour éviter des intersections inutiles.
- La scène native utilise six formes : triangle, boîte, boîte arrondie, capsule, cône et ellipsoïde. Les triangles portent des normales lissées et des coordonnées UV.
- Le chemin principal calcule la profondeur par intersections de rayons; il n’utilise pas de Z-buffer raster traditionnel.
- Les ombres disposent de leur propre carte de profondeur de 1024 × 1024 pixels, filtrée en PCF 5 × 5. Des bornes min/max réduisent les lectures.
- Le BVH est créé lors de l’export, puis parcouru de l’impact le plus proche au plus éloigné. Les inverses de direction du rayon sont calculés une seule fois et réutilisés lors des tests AABB de chaque nœud.

4K UHD output is available while Full HD remains the default. At 4K output, the selected internal scale still controls render cost; 200% calculates at 7680 × 4320 and can be substantially slower on a CPU. / La sortie 4K UHD est disponible, Full HD restant le réglage initial. En 4K, l’échelle interne choisie détermine toujours le coût; le mode 200 % calcule en 7680 × 4320 et peut être nettement plus lent sur CPU.

The perspective camera traces one ray per internal pixel and keeps the nearest hit. BVH bounds skip unnecessary intersections. Main-view depth comes from ray intersections rather than a traditional raster Z-buffer. Shadows use a separate 1024 × 1024 depth map with 5 × 5 PCF filtering and min/max bounds. The six native geometry types are triangle, box, rounded box, capsule, cone, and ellipsoid; triangle meshes carry smooth vertex normals and UVs.

## Matériaux, textures et lumière / Materials, textures, and lighting

- Les textures des matériaux sont générées de façon procédurale à la construction : asphalte, béton, sable, mer, pierre, bois, métal, verre, tissus, végétation, carrosseries et éclairages.
- Pour ce build, les matériaux disposent de chaînes mip RGB565 allant de 512 × 512 à 8 × 8, avec échantillonnage bilinéaire dans le niveau choisi.
- L’heure couvre 06:00–22:00 par pas de 30 minutes et fait évoluer le ciel, le soleil, l’ambiance et les lampes.
- Les lampadaires et les phares de voitures contribuent comme sources locales à portée limitée.
- Chaque matériau porte un pourcentage de réflexion de 0 à 100, exporté dans le format de scène V2. L’ombrage module cette force par un profil angulaire de type Fresnel. Les valeurs de la scène sont notamment : mer 51 %, verre 42 %, métal 35 %, peinture courante 29 % et carrosserie argentée 95 %.
- L’ombrage combine lumière diffuse, reflets spéculaires et réflexions secondaires à un rebond selon les matériaux qui les activent.
- Un bump mapping procédural perturbe les normales d’ombrage de l’asphalte, du sable, du béton, de la pierre, du bois, de la brique et de la mer. Son intensité diminue avec la distance pour limiter le scintillement; les silhouettes et le nombre de triangles restent inchangés.
- La mer, les peintures de voiture et les vitrages peuvent recevoir une réflexion secondaire à un rebond.
- Le brouillard optionnel atténue les objets selon la distance; le ciel reçoit aussi un motif nuageux procédural.

Procedural material textures are built into RGB565 mip chains from 512 × 512 to 8 × 8; sampling is bilinear within the selected mip level. Time of day ranges from 06:00 to 22:00 in 30-minute steps and changes the sky, sun, ambient light, and lamps. Streetlights and car headlights are local lights. Shading combines diffuse and material-dependent specular light. Procedural bump mapping perturbs shading normals on asphalt, sand, concrete, stone, wood, brick, and sea; it fades with distance and does not change silhouettes or triangle count. The sea, car paint, and glass support one-bounce reflections. Optional fog attenuates distant geometry and adds procedural clouds to the sky.

## Contrôles de l’interface / UI controls

| Réglage | Valeurs |
|---|---|
| Résolution de sortie | 1920 × 1080 (par défaut), 1600 × 900, 1280 × 720, 3840 × 2160 (4K UHD) |
| Précision interne | 100 %, 87,5 %, 75 %, 66,7 %, 50 %, FSAA 16×; case AA contours indépendante |
| Caméra | Distance; rotation −180° à 180°; élévation −5° à 50°; champ horizontal 35° à 105° |
| Heure | 33 positions entre 06:00 et 22:00 |
| Threads CPU | 1 à 32 |
| Brouillard | Activé / désactivé |
| Vues rapides | Initiale, avant, arrière |
| Rendu | Recalcul de l’image à la demande et lors des changements de réglage |

La case AA contours applique, après mise à l’échelle, un filtre de quatre voisins à l’image de sortie, quel que soit le mode interne choisi. Elle peut être cochée ou décochée pour Natif, Équilibré, Détails, Rapide, Très rapide et FSAA 16×; elle est activée par défaut. Le filtre détecte les fortes transitions de luminance et adoucit localement ces pixels, tandis que les zones sans contraste marqué restent inchangées. Le SSAA 4× est retiré de l’interface; les échelles internes inférieures sont adaptées par interpolation bilinéaire.

The AA contours checkbox applies a four-neighbour filter to the output image after scaling, regardless of the selected internal mode. It can be enabled or disabled for Native, Balanced, Details, Fast, Very Fast and FSAA 16×; it is enabled by default. The filter detects strong luminance transitions and locally softens those pixels, while areas without marked contrast remain unchanged. SSAA 4× has been removed from the UI; lower internal scales are resized with bilinear interpolation.

At 200%, the renderer doubles both internal dimensions (four times as many pixels), then reduces the result with a 2 × 2 average. Other internal sizes that differ from the output are enlarged with bilinear interpolation.

## Parallélisme et diagnostics / Parallelism and diagnostics

Le rendu répartit les pixels en tuiles de 16 × 16 entre 1 et 32 threads. Une nouvelle demande invalide les travaux précédents par un identifiant de génération; le calcul reste séparé de l’interface Windows.

Après le rendu, l’interface affiche séparément le temps de rendu, la reconstruction éventuelle de la carte d’ombres, le post-traitement et le total, le nombre de threads, les rayons, impacts, rayons de ciel, tests d’intersection, nœuds BVH visités, tests AABB, objets, triangles et texels échantillonnés.

Rendering distributes 16 × 16 pixel tiles across 1–32 CPU threads. A generation token cancels stale work when a new render is requested, while a separate coordinator keeps the UI responsive. Diagnostics report render time, shadow-map build time when rebuilt, post-processing time, and total time, threads, rays, hits, sky rays, intersection tests, visited BVH nodes, AABB tests, objects, triangles, and sampled texels.

## Scène actuelle et limites / Current scene and limitations

La scène de référence comprend route côtière, promenade pavée, bordure, garde-corps, neuf bancs, lampadaires, palmiers, plage, rochers, oyats, parasols, serviettes, poubelles, mouettes et voitures. L’export validé contient **1 537 833 triangles**, **1 541 767 objets** et **920 433 nœuds BVH**; le binaire non compressé mesure **254 942 648 octets**. Le chargeur accepte jusqu’à **4 200 000 objets et nœuds BVH**.

Le bump mapping actuel est procédural et intégré à l’ombrage; le moteur ne charge pas encore de fichiers de normal map. Le rendu est CPU et aucun pipeline GPU n’est présent dans ce code natif. Le contrat de backend physique est préparé dans `src/m3d_physics.h` et réserve un adaptateur PhysX; aucun SDK PhysX n’est lié et aucune simulation ne tourne dans ce build. L’application ne propose pas encore d’éditeur de scène interactif ou d’animation. Les changements de décor et les pourcentages de réflexion s’effectuent dans `scene_coastal_v1314.js`, puis nécessitent un nouvel export. Le chargeur conserve la compatibilité avec les scènes binaires V1 en leur attribuant les valeurs de réflexion par défaut. Le temps d’image dépend du processeur, de la résolution, du mode interne, des ombres et des réflexions; le nombre de triangles n’est pas un benchmark de FPS. Le fichier `moteur3d_v1314_scene.bin` doit rester dans le même dossier que l’EXE.

The reference scene includes a coastal road and paved promenade, curb, guardrail, nine benches, streetlights, palms, beach, rocks, beach grass, parasols, towels, bins, gulls, and cars. The validated export contains **1,537,833 triangles**, **1,541,767 objects**, and **920,433 BVH nodes**; the uncompressed binary is **254,942,648 bytes**. The loader accepts up to **4,200,000 objects and BVH nodes**.

Bump detail is procedural and built into shading; the engine does not yet load external normal-map files. This native code renders on the CPU and has no GPU pipeline. A backend contract in `src/m3d_physics.h` reserves a PhysX adapter, but the PhysX SDK is not linked and no simulation runs in this build. Edit `scene_coastal_v1314.js` to tune per-material reflection percentages, then export again. The loader remains compatible with V1 scene binaries and supplies their default reflection values. The application has no interactive scene editor or animation yet. Frame time depends on the processor, output size, internal scale, shadows, and reflections; triangle count is not an FPS benchmark. Keep `moteur3d_v1314_scene.bin` beside the EXE.

## Modules et compilation / Modules and build flow

La scène est définie dans `scene_coastal_v1314.js`. `export_scene.js` exécute cette scène avec le moteur V13.14 et produit textures et BVH dans `moteur3d_v1314_scene.bin`. `assemble_native.js` reconstruit le programme C++ depuis `src/`, puis le workflow Windows compile l’application x64 avec MSVC.

Scene geometry and per-material reflection percentages live in `scene_coastal_v1314.js`. `export_scene.js` evaluates it with the V13.14 engine and writes textures, material properties, and the BVH to scene format V2 in `moteur3d_v1314_scene.bin`. The loader also accepts V1. `assemble_native.js` reconstructs the C++ program from `src/`, and the Windows workflow compiles the x64 app with MSVC.

Voir aussi [CONTRAT-ENTREES-SORTIES.md](CONTRAT-ENTREES-SORTIES.md) pour les formats et échanges propres à chaque module.


Le mode FSAA 16× utilise un motif décalé et stratifié de 16 rayons par pixel. La profondeur de champ optionnelle échantillonne une ouverture de lentille réduite avec 16 rayons par pixel, pour fondre les échantillons en un flou régulier plutôt qu’en images fantômes. Le DOF impose 16 rayons par pixel; le FSAA 16× et le DOF partagent ces 16 rayons.

Le rendu reprend ainsi deux idées du T-Buffer (échantillonnage spatial et profondeur de champ), sous forme logicielle. La scène ne contenant aucune animation d’objet, le flou de mouvement temporel n’est pas activé : il nécessite plusieurs positions d’un objet pendant l’exposition. Les ombres filtrées PCF et les reflets multi-échantillonnés de l’eau étaient déjà pris en charge.


FSAA 16× uses a shifted, stratified pattern of 16 rays per pixel. Optional depth of field samples a reduced lens aperture with 16 rays per pixel to blend into a smooth blur instead of visible ghost copies. DOF enforces 16 rays per pixel; FSAA 16× and DOF share those 16 rays.

This software renderer borrows two T-Buffer ideas: spatial sampling and depth of field. Temporal motion blur is not enabled because this scene has no animated objects; it requires an object's positions during the exposure. Filtered PCF shadows and multi-sampled water reflections were already supported.
