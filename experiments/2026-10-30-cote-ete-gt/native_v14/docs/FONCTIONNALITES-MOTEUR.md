# Fonctionnalités du moteur 3D natif x64 — V13.14

Ce document décrit l’application Qt Widgets compilée pour Windows et Linux ainsi que les modules C++ de la branche `experiment/manhattan-silent-hill-2026-10-30`. Il distingue les fonctions présentes des limites actuelles. Pour les contrats entrée/sortie et l’état précis de chaque bloc, voir [ETAT-MODULES-ET-FONCTIONS.md](ETAT-MODULES-ET-FONCTIONS.md) et [CONTRAT-ENTREES-SORTIES.md](CONTRAT-ENTREES-SORTIES.md).

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

4K UHD output remains available. The default output is now 1366 × 768 for the Vostro display; Full HD and higher resolutions can still be selected. An optional Windows Direct3D 11 raster preview uses the GPU; the full CPU ray tracer remains the default and supports ray shadows, depth of field, reflections and refraction. / La sortie 4K UHD reste disponible. La résolution initiale est maintenant 1366 × 768 pour l’écran du Vostro ; Full HD et les résolutions supérieures restent sélectionnables. Un aperçu raster facultatif utilise le GPU Direct3D 11 sous Windows ; le raytracer CPU complet reste le mode initial et conserve les ombres par rayons, la profondeur de champ, les réflexions et la réfraction.

The perspective camera traces one ray per internal pixel and keeps the nearest hit. BVH bounds skip unnecessary intersections. Main-view depth comes from ray intersections rather than a traditional raster Z-buffer. Shadows use a separate 1024 × 1024 depth map with 5 × 5 PCF filtering and min/max bounds. The six native geometry types are triangle, box, rounded box, capsule, cone, and ellipsoid; triangle meshes carry smooth vertex normals and UVs.

## Matériaux, textures et lumière / Materials, textures, and lighting

- Les textures des matériaux sont générées de façon procédurale à la construction : asphalte, béton, sable, mer, pierre, bois, métal, verre, tissus, végétation, carrosseries et éclairages.
- L’exporteur natif accepte des textures de 512² à 2048². Le format V5 conserve les albedos en RGB888 (V3/V4 restent lisibles en RGB565); normales tangentielles et rugosité restent RGB565. L’import catalogue Pripiat garde les couleurs GLB en 8 bits par canal et exporte les chaînes mip jusqu’à 1². Le rendu CPU interpole bilinéairement dans les niveaux puis entre deux niveaux mip. L’aperçu DirectX demande le MSAA 4× avec repli 2×/1×, décode les albedos sRGB et utilise jusqu’à 16× d’anisotropie.
- L’heure couvre 06:00–22:00 par pas de 30 minutes et fait évoluer le ciel, le soleil, l’ambiance et les lampes.
- Les lampadaires et les phares de voitures contribuent comme sources locales à portée limitée.
- Les scènes V4 peuvent embarquer jusqu’à 256 lumières ponctuelles (position, couleur, portée, intensité) avec atténuation quadratique. Elles éclairent la scène, mais ne génèrent pas encore d’ombres propres. Les lumières globales historiques restent disponibles.
- L’étude Rapture utilise 44 lumières locales, dont une lampe visible dans la cabine de la bathysphère et un éclairage de vitrine. Les vrais trous de hublot traversent les deux couches de coque; lampe et instruments ambrés sont placés derrière la vitre avant. Un jeton de platine frappé d’un « R » est exposé devant la station.
- Son nouvel export contient 226 942 objets (167 311 triangles), 119 899 nœuds BVH et 44 lumières; le binaire fait 45,3 Mo. Le preset d’ouverture Qt adopte une vue oblique depuis l’avenue.
- Les matériaux portent une réflexion de 0 à 100 %. Une réflexion secondaire à un rebond et une lobe spéculaire directe utilisent la réflexion et la rugosité. Les matériaux V4 peuvent aussi définir transmission et indice de réfraction.
- Les rayons transparents utilisent Fresnel-Schlick et une déviation par réfraction, traversent jusqu’à huit couches et composent la teinte du verre avec l’arrière-plan. Les surfaces transmissives ne bloquent pas la carte d’ombres opaque. Le verre reste une surface mince : pas de dispersion, absorption spectrale ni volume épais.
- Les chaînes mip acceptent couleur RGB888 (ou RGB565 dans les fichiers historiques), normales tangentielles et rugosité RGB565. La scène Rapture génère des cartes normales/rugosité procédurales pour pierre, bronze et verre; l’orange utilise ses cartes dédiées. Le bump procédural reste le repli sans normal map.
- L’ombrage combine lumière diffuse, reflets spéculaires et réflexions secondaires à un rebond selon les matériaux qui les activent.
- Un bump mapping procédural perturbe les normales d’ombrage de l’asphalte, du sable, du béton, de la pierre, du bois, de la brique et de la mer. Son intensité diminue avec la distance pour limiter le scintillement; les silhouettes et le nombre de triangles restent inchangés.
- La mer, les peintures de voiture et les vitrages peuvent recevoir une réflexion secondaire à un rebond.
- Le bloom CPU optionnel extrait les hautes lumières au-dessus de 185, floute en deux passes séparables à 11 taps et les réinjecte à faible intensité. Il adoucit les néons et fenêtres; il ne constitue pas un pipeline HDR.
- Le brouillard optionnel atténue les objets selon la distance; le ciel reçoit aussi un motif nuageux procédural.

The native exporter accepts 512²–2048² textures. Format V5 keeps albedo in RGB888 while V3/V4 RGB565 scenes remain readable; tangent normals and roughness are RGB565. The Pripyat catalog importer keeps GLB colors at 8 bits per channel and exports mip chains to 1². CPU sampling is bilinear within each level and blends between adjacent mip levels. DirectX preview requests 4× MSAA (falls back to 2× or 1×), decodes albedo as sRGB and uses up to 16× anisotropic filtering. Time of day ranges from 06:00 to 22:00 in 30-minute steps and changes the sky, sun, ambient light, and lamps. Streetlights and car headlights are local lights. Shading combines diffuse and material-dependent specular light. Procedural bump mapping perturbs shading normals on asphalt, sand, concrete, stone, wood, brick, and sea; it fades with distance and does not change silhouettes or triangle count. The sea, car paint, and glass support one-bounce reflections. Optional fog attenuates distant geometry and adds procedural clouds to the sky.

V4 scenes can embed up to 256 point lights with position, color, radius and intensity. They use quadratic attenuation but do not yet cast shadows. Materials support reflectivity, normal and roughness maps. Transparent rays use Schlick Fresnel and surface refraction, traverse up to eight layers, and do not block the opaque shadow map. Glass remains a thin-surface approximation without thick-volume refraction, dispersion or spectral absorption. The optional CPU bloom extracts highlights above 185 and applies a separable 11-tap blur; this is not HDR.

The Rapture study uses 44 local lights. The bathysphere has true apertures through both hull layers, with a warm cabin lamp and instruments behind the forward viewport. A platinum Rapture token is displayed in a lit case. The scene also has chandeliers, wall sconces, illuminated kiosks and railings, detailed pipework, and marine planters.
Its current export contains 207,661 objects (148,203 triangles); the Qt opening preset uses an oblique view from the avenue.

## Contrôles de l’interface / UI controls

| Réglage | Valeurs |
|---|---|
| Résolution de sortie | 1366 × 768 (par défaut, Vostro), 1920 × 1080, 1600 × 900, 1280 × 720, 3840 × 2160 (4K UHD) |
| Précision interne | 100 %, 87,5 %, 75 %, 66,7 %, 50 %, FSAA 16×; case AA contours indépendante |
| Caméra | Distance, rotation et élévation; molette pour zoomer; orbite par glisser gauche; déplacement du point visé par glisser droit et touches WASD/R/F |
| Heure | 33 positions entre 06:00 et 22:00 |
| Threads CPU | 1 à 32 |
| Brouillard | Activé / désactivé |
| Bloom doux | Optionnel; activé automatiquement pour un fichier dont le nom contient `rapture` |
| Vues rapides | Initiale, avant, arrière |
| Aperçu rotatif | Fenêtre 800 × 600, calcul 400 × 300 et rotation automatique autour de la scène |
| Rendu | Aperçu interne 480 × 270 pendant la navigation, puis qualité/résolution choisies après 450 ms d’arrêt |

Chaque image est calculée dans un tampon privé; l’interface ne l’affiche qu’une fois le rendu et le post-traitement terminés. L’image précédente reste visible pendant le calcul. Pendant la navigation, l’application rend un aperçu interne plafonné à 480 × 270, sans DOF ni AA contours; après 450 ms sans entrée caméra, elle revient automatiquement à la qualité, à la résolution, au DOF et à l’AA choisis. Fermer l’aperçu rotatif arrête la rotation et restaure les réglages précédents.

La case AA contours applique, après mise à l’échelle, un filtre de quatre voisins à l’image de sortie, quel que soit le mode interne choisi. Elle peut être cochée ou décochée pour Natif, Équilibré, Détails, Rapide, Très rapide et FSAA 16×; elle est activée par défaut. Le filtre détecte les fortes transitions de luminance et adoucit localement ces pixels, tandis que les zones sans contraste marqué restent inchangées. Le SSAA 4× est retiré de l’interface; les échelles internes inférieures sont adaptées par interpolation bilinéaire.

The AA contours checkbox applies a four-neighbour filter to the output image after scaling, regardless of the selected internal mode. It can be enabled or disabled for Native, Balanced, Details, Fast, Very Fast and FSAA 16×; it is enabled by default. The filter detects strong luminance transitions and locally softens those pixels, while areas without marked contrast remain unchanged. SSAA 4× has been removed from the UI; lower internal scales are resized with bilinear interpolation.

At 200%, the renderer doubles both internal dimensions (four times as many pixels), then reduces the result with a 2 × 2 average. Other internal sizes that differ from the output are enlarged with bilinear interpolation.

## Parallélisme et diagnostics / Parallelism and diagnostics

Le rendu répartit les pixels en tuiles de 16 × 16 entre 1 et 32 threads. Une nouvelle demande invalide les travaux précédents par un identifiant de génération; le calcul reste séparé de l’interface Windows.

Après le rendu, l’interface affiche séparément le temps de rendu, la reconstruction éventuelle de la carte d’ombres, le post-traitement et le total, le nombre de threads, les rayons, impacts, rayons de ciel, tests d’intersection, nœuds BVH visités, tests AABB, objets, triangles et texels échantillonnés.

Rendering distributes 16 × 16 pixel tiles across 1–32 CPU threads. A generation token cancels stale work when a new render is requested, while a separate coordinator keeps the UI responsive. Diagnostics report render time, shadow-map build time when rebuilt, post-processing time, and total time, threads, rays, hits, sky rays, intersection tests, visited BVH nodes, AABB tests, objects, triangles, and sampled texels.

The separate orbit preview renders at 400 × 300 into an 800 × 600 window and rotates the camera around the scene. Each complete image is published only after rendering and post-processing finish; the previous complete frame stays visible during calculation. Closing the preview stops the rotation and restores the previous settings.

## Scène actuelle et limites / Current scene and limitations

La scène de référence comprend route côtière, promenade pavée, bordure, garde-corps, neuf bancs, lampadaires, palmiers, plage, rochers, oyats, parasols, serviettes, poubelles, mouettes et voitures. L’export validé contient **1 537 833 triangles**, **1 541 767 objets** et **920 433 nœuds BVH**; le binaire non compressé mesure **254 942 648 octets**. Le chargeur accepte jusqu’à **4 200 000 objets et nœuds BVH**.

L’orange et les matériaux Rapture utilisent des cartes couleur/normale/rugosité intégrées au format scène V4; les scènes V3 restent lisibles. Le rendu reste CPU et RGB 8 bits, sans illumination globale ni pipeline GPU. Les lumières ponctuelles n’ont pas d’ombres; le bloom ne remplace pas le HDR. Le contrat PhysX est réservé mais aucun SDK n’est lié et aucune simulation ne tourne. L’application ne propose pas encore d’éditeur interactif ni d’animation. Les changements de décor ou de matériau nécessitent un nouvel export. Le temps d’image dépend du processeur, de la résolution, des ombres, lumières et réflexions; le nombre de triangles n’est pas un benchmark de FPS.

The reference scene includes a coastal road and paved promenade, curb, guardrail, nine benches, streetlights, palms, beach, rocks, beach grass, parasols, towels, bins, gulls, and cars. The validated export contains **1,537,833 triangles**, **1,541,767 objects**, and **920,433 BVH nodes**; the uncompressed binary is **254,942,648 bytes**. The loader accepts up to **4,200,000 objects and BVH nodes**.

The orange and Rapture materials use color/normal/roughness maps embedded in V4 scenes; V3 scenes remain readable. This renderer is CPU-based and RGB 8-bit, with no global illumination or GPU pipeline. Point lights do not cast shadows, and bloom is not HDR. The PhysX backend is reserved but no SDK is linked and no simulation runs. Scene edits require a new export. There is no interactive scene editor or animation yet. Frame time depends on processor, output size, shadows, lights and reflections; triangle count is not an FPS benchmark.

## Modules et compilation / Modules and build flow

La bathysphère Rapture utilise désormais une coque à rivets, un grand hublot avant avec cabine visible et des hublots latéraux élargis. L’ouverture de `scene_rapture.bin` place automatiquement la caméra face au véhicule, à courte distance. Les scènes sont définies dans `scene_coastal_v1314.js`, `scene_orange_macro.js`, `scene_rapture.js` et `scene_pripyat_central.js`. `export_scene.js` produit géométrie, textures PBR, lumières de scène et BVH dans des binaires V5; le chargeur reste rétrocompatible avec V3/V4. CMake compile directement les modules moteur et l’interface Qt; le workflow courant produit des paquets Windows et Linux. `moteur3d_win32.cpp` reste une source historique non compilée par ce workflow.

Scenes are defined in `scene_coastal_v1314.js`, `scene_orange_macro.js`, `scene_rapture.js`, and `scene_pripyat_central.js`. `export_scene.js` writes geometry, PBR textures, scene lights and the BVH to V5; the loader remains backward compatible with V3/V4. CMake builds the engine modules and Qt frontend; the current workflow packages Windows and Linux builds. `moteur3d_win32.cpp` remains a legacy source and is not compiled by this workflow.

Voir aussi [CONTRAT-ENTREES-SORTIES.md](CONTRAT-ENTREES-SORTIES.md) pour les formats et échanges propres à chaque module.


Le mode FSAA 16× utilise un motif décalé et stratifié de 16 rayons par pixel. La profondeur de champ optionnelle échantillonne une ouverture de lentille réduite avec 16 rayons par pixel, pour fondre les échantillons en un flou régulier plutôt qu’en images fantômes. Le DOF impose 16 rayons par pixel; le FSAA 16× et le DOF partagent ces 16 rayons.

Le rendu reprend ainsi deux idées du T-Buffer (échantillonnage spatial et profondeur de champ), sous forme logicielle. La scène ne contenant aucune animation d’objet, le flou de mouvement temporel n’est pas activé : il nécessite plusieurs positions d’un objet pendant l’exposition. Les ombres filtrées PCF et les reflets multi-échantillonnés de l’eau étaient déjà pris en charge.


FSAA 16× uses a shifted, stratified pattern of 16 rays per pixel. Optional depth of field samples a reduced lens aperture with 16 rays per pixel to blend into a smooth blur instead of visible ghost copies. DOF enforces 16 rays per pixel; FSAA 16× and DOF share those 16 rays.

This software renderer borrows two T-Buffer ideas: spatial sampling and depth of field. Temporal motion blur is not enabled because this scene has no animated objects; it requires an object's positions during the exposure. Filtered PCF shadows and multi-sampled water reflections were already supported.
