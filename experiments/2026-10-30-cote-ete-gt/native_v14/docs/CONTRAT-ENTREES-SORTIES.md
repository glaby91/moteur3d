# Contrat d’entrées et de sorties — Moteur 3D natif x64

**Version du document :** 1.2  
**Identifiant de build :** 261101-0819  
**Scène source :** V13.14  
**Révision GitHub :** injectée au build (SHA court du commit)  
**Branche GitHub :** `experiment/manhattan-silent-hill-2026-10-30`  
**Run GitHub Actions :** identifiant CI séparé, attribué par GitHub

Ce document définit les données attendues par chaque partie du moteur et les résultats qu’elle doit fournir. Il s’applique au découpage modulaire reconstruit par `assemble_native.js`.


### Scène modifiable / Editable scene

**FR —** La géométrie est centralisée dans `scene_coastal_v1314.js`. Modifiez uniquement ce fichier pour changer les objets, leurs matériaux déclarés, leurs dimensions ou leur placement. Les fonctions de rendu restent dans `source_v1314.html`; ses primitives géométriques sont utilisées par la définition de scène. L’aperçu HTML charge le fichier de scène externe, et `export_scene.js` combine ce même fichier avec le moteur V13.14 pour produire la scène native. Gardez ces deux chemins de chargement synchronisés si le nom ou l’emplacement du module change.

**EN —** Scene geometry is centralized in `scene_coastal_v1314.js`. Edit only this file to change objects, declared materials, dimensions, or placement. Rendering functions remain in `source_v1314.html`; its geometry helpers are used by the scene definition. The HTML preview loads the external scene file, and `export_scene.js` combines that same file with the V13.14 engine to produce the native scene. Keep both loading paths synchronized if the module name or location changes.

## 1. Contrat global de l’application

### Français

**Entrée obligatoire à l’exécution :** `moteur3d_v1314_scene.bin`, placé dans le même dossier que l’exécutable. L’application ne prend pas d’arguments de ligne de commande; elle déduit le chemin de la scène à partir du chemin de son propre EXE.

Le fichier binaire doit provenir de `export_scene.js` exécuté sur `source_v1314.html`. Il contient la géométrie réelle V13.14, les normales, UV, matériaux, mipmaps RGB565 et le BVH. Le moteur natif ne crée pas de scène de remplacement. La scène comprend notamment la promenade pavée élargie, neuf bancs, quatre parasols et serviettes, quatre poubelles, des mouettes, des lampadaires et les vagues.

Le format commence par l’identifiant `M3D1314`, la version de format `1), les nombres d’objets, de nœuds BVH, d’indices et de matériaux, puis la racine du BVH et la taille de texture. Les matériaux ont un nom UTF-8 de 24 octets. Les objets ont un type, un indice de matériau, des limites et les paramètres propres à leur primitive. Les nœuds BVH contiennent leurs limites et leurs liens; la liste des indices et les mipmaps suivent.

Le chargeur vérifie notamment la signature et la version, limite les objets et nœuds à 4 200 000, exige que le nombre d’indices BVH soit égal au nombre d’objets, vérifie chaque indice et limite une pyramide de mipmaps à 10 niveaux. Un fichier absent, tronqué ou invalide doit produire un message explicite et arrêter le rendu.

Les réglages d’entrée sont fournis par l’interface :

- Résolution de sortie : 1920×1080, 1600×900 ou 1280×720.
- Précision interne : 100 %, 87,5 %, 75 %, 66,7 %, 50 % ou 200 % par axe.
- Caméra : distance, rotation (−180° à 180°), élévation (−5° à 50°) et champ horizontal (35° à 105°).
- Heure : positions 0–32, converties en 06:00–22:00 par pas de 30 minutes.
- Threads CPU : 1–16.
- Brouillard : activé ou désactivé.

**Sortie à l’exécution :** une image calculée en mémoire, redimensionnée à la résolution choisie et affichée dans la fenêtre. Le programme ne sauvegarde pas spontanément de PNG ou de JPEG. L’interface publie aussi les temps de rendu et de post-traitement, le nombre de rayons, d’impacts, de tests BVH/AABB, de nœuds visités, de texels et de threads.

### English

**Required runtime input:** `moteur3d_v1314_scene.bin`, placed beside the executable. The application accepts no command-line arguments; it derives the scene path from its own executable path.

The binary must be produced by running `export_scene.js` on `source_v1314.html`. It contains the actual V13.14 geometry, normals, UVs, materials, RGB565 mipmaps and BVH. The native renderer does not create a substitute scene. The scene includes the widened paved promenade, benches facing the sea, street lamps and waves.

The file begins with the `M3D1314` signature, format version `1`, object, BVH-node, index and material counts, then the BVH root and texture size. Materials have a 24-byte UTF-8 name. Objects have a primitive type, material index, bounds and primitive-specific parameters. BVH nodes contain their bounds and links; the index list and mipmaps follow.

The loader checks the signature and version, caps objects and nodes at 4,200,000, requires the BVH index count to equal the object count, validates each index and limits each mip chain to 10 levels. Missing, truncated or invalid input must produce a clear error and stop rendering.

Inputs supplied by the user interface:

- Output resolution: 1920×1080, 1600×900 or 1280×720.
- Internal scale: 100%, 87.5%, 75%, 66.7%, 50% or 200% per axis.
- Camera: distance, rotation (−180° to 180°), elevation (−5° to 50°) and horizontal field of view (35° to 105°).
- Time: slider positions 0–32 map to 06:00–22:00 in 30-minute steps.
- CPU threads: 1–16.
- Fog: enabled or disabled.

**Runtime output:** an in-memory rendered image, resized to the selected output resolution and displayed in the window. The program does not automatically save a PNG or JPEG. The UI also reports render and post-processing times, ray, hit, BVH/AABB test, node, texel and thread counts.

## 2. Contrat par module / Module contracts

### `m3d_engine.h` — Types partagés / Shared types

**Entrée / Input :** définitions communes des vecteurs, rayons, impacts, objets, matériaux, scène, caméra, lumières, statistiques et carte d’ombres. / Shared definitions for vectors, rays, hits, objects, materials, scene, camera, lights, statistics and shadow map.

**Sortie / Output :** déclarations et types utilisés par tous les modules; aucune donnée de scène n’est chargée par cet en-tête. / Declarations and types used by all modules; this header loads no scene data.

### `m3d_scene.cpp` — Chargement de scène / Scene loading

**Entrée / Input :** chemin du binaire V13.14; flux conforme au format décrit plus haut. / Path to the V13.14 binary; stream conforming to the format above.

**Sortie / Output :** `loadScene(path, Scene&, error)` remplit la scène et retourne `true`; en échec, retourne `false` et renseigne `error`. / `loadScene(path, Scene&, error)` fills the scene and returns `true`; on failure it returns `false` and sets `error`.

### `m3d_lighting.cpp` — Éclairage selon l’heure / Time-of-day lighting

**Entrée / Input :** heure entre 6 et 22, en heures décimales. / Time from 6 to 22, expressed in decimal hours.

**Sortie / Output :** met à jour le soleil, les couleurs du ciel, l’intensité jour/nuit et la liste des lumières ponctuelles. / Updates the sun, sky colors, day/night intensity and point-light list.

### `m3d_intersection.cpp` — Géométrie et BVH / Geometry and BVH

**Entrée / Input :** scène chargée, rayon (origine et direction), limite de distance et option d’intersection avec le sol. / Loaded scene, ray (origin and direction), distance limit and ground-intersection option.

**Sortie / Output :** primitive la plus proche sous forme de `Hit` (distance, position, normale, matériau, identifiant) et compteurs de parcours dans `Stats`. / Nearest primitive as a `Hit` (distance, position, normal, material, identifier) and traversal counters in `Stats`.

### `m3d_shading.cpp` — Ombres, textures et matériaux / Shadows, textures and materials

**Entrée / Input :** scène, carte d’ombres, caméra, rayon, impact, statistiques et indicateur de brouillard; l’état lumineux a déjà été préparé par `m3d_lighting.cpp`. / Scene, shadow map, camera, ray, hit, statistics and fog flag; lighting state is prepared by `m3d_lighting.cpp`.

**Sortie / Output :** couleur RGB calculée pour un rayon; ajoute au besoin les contributions des lumières, ombres, textures et réflexions. `buildShadow` produit la carte d’ombres de la scène. / Computed RGB color for a ray, including applicable lights, shadows, textures and reflections. `buildShadow` returns the scene’s shadow map.

### `m3d_render.cpp` — Caméra et rendu / Camera and rendering

**Entrée / Input :** scène, carte d’ombres, caméra, largeur/hauteur internes, nombre de threads, brouillard et jeton d’annulation de rendu. / Scene, shadow map, camera, internal width/height, thread count, fog and render-cancellation token.

**Sortie / Output :** `render` remplit un tampon de `largeur × hauteur` pixels 32 bits et retourne `Stats`. Un jeton de génération périmé interrompt le travail après un changement de réglage. / `render` fills a 32-bit pixel buffer of `width × height` and returns `Stats`. A stale generation token cancels work after settings change.

### `m3d_post.cpp` — Mise à l’échelle / Scaling

**Entrée / Input :** tampon source, dimensions source et cible, nombre de threads. / Source buffer, source and destination dimensions, thread count.

**Sortie / Output :** nouveau tampon aux dimensions cibles : `upscaleBilinear` pour interpolation bilinéaire, `downsample2x` pour le filtre de réduction 2× du mode Ultra. / New destination-sized buffer: `upscaleBilinear` for bilinear scaling and `downsample2x` for Ultra mode’s 2× downsampling filter.

### `moteur3d_win32.cpp` — Interface et orchestration / UI and orchestration

**Entrée / Input :** événements Windows, curseurs, listes de résolution/précision, case de brouillard et fichier scène chargé. / Windows events, sliders, resolution/quality lists, fog checkbox and loaded scene file.

**Sortie / Output :** requêtes de rendu, affichage de l’image et des diagnostics, contrôles de caméra et messages d’erreur. / Render requests, displayed image and diagnostics, camera controls and error messages.

### `assemble_native.js` — Reconstruction source / Source reconstruction

**Entrée / Input :** les fichiers C++ modulaires dans l’ordre déclaré dans le script. / Modular C++ files in the order declared by the script.

**Sortie / Output :** le fichier unique `moteur3d_v14_native.cpp`, reconstruit de façon déterministe puis compilé par MSVC. / The single `moteur3d_v14_native.cpp` file, deterministically reconstructed and then compiled by MSVC.

## 3. Sorties de compilation / Build outputs

Le workflow exporte `moteur3d_v1314_scene.bin`, reconstruit le source, compile l’EXE x64 puis crée `Moteur3D-Natif-x64-261101-0819.zip`. L’archive contient l’EXE et le fichier scène requis. Le SHA du commit et la branche sont injectés dans l’interface et les propriétés Windows; le numéro de run GitHub Actions reste séparé.

The workflow exports `moteur3d_v1314_scene.bin`, reconstructs the source, compiles the x64 EXE and creates `Moteur3D-Natif-x64-261101-0819.zip`. The archive contains the EXE and required scene file. The commit SHA and branch are embedded in the UI and Windows file properties; the GitHub Actions run number remains separate.


## Annexe A — Bibliothèques de textures CC0 / Appendix A — CC0 texture libraries

| Matière / Material | Sources ciblées / Suggested sources | Utilisation conseillée / Suggested use |
|---|---|---|
| Bois / Wood | [Poly Haven — bois](https://polyhaven.com/textures?c=wood), [ambientCG](https://ambientcg.com/) | Planches, bancs, mobilier; base color/albedo, avec normal et roughness si disponibles. / Planks and benches; albedo, plus normal and roughness maps when available. |
| Métal / Metal | [Poly Haven — métal](https://polyhaven.com/textures?c=metal), [ambientCG](https://ambientcg.com/) | Acier peint, tôle, métal rouillé; régler la réflexion par matériau. / Painted steel, sheet metal, rust; tune reflectivity per material. |
| Béton et asphalte / Concrete and asphalt | [Poly Haven — textures](https://polyhaven.com/textures), [ambientCG](https://ambientcg.com/) | Chaussée, trottoir, bordures; privilégier les matériaux « outdoor », usés ou mouillés. / Road, pavement and curb; prefer outdoor, worn or wet surfaces. |
| Pierre, sable et sol / Stone, sand and ground | [Poly Haven — textures](https://polyhaven.com/textures) | Roches, plage, gravier et terrain; utiliser des UV à grande échelle pour éviter les répétitions visibles. / Rocks, beach, gravel and terrain; use world-scale UVs to avoid obvious tiling. |
| Feuilles / Leaves | [Poly Haven — sol couvert de feuilles](https://polyhaven.com/a/leaves_forest_ground), [Poly Haven — textures organiques](https://polyhaven.com/textures?c=organic) | Litière et sols végétalisés. Ce ne sont pas des feuilles détourées pour les palmiers; chercher un modèle botanique avec textures et licence précisées. / Leaf litter and vegetated ground. These are not cutout palm fronds; use a botanical model with explicit texture and license instead. |
| Eau et vagues / Water and waves | [ProcTexture — normal map de vagues](https://proctexture.com/textures/water/normal-maps/wave-normal-map) | Source de vagues annoncée CC0; la carte normale aide à produire du relief de surface. Mouvement, écume et réfraction restent à gérer par le shader. / Wave source is marked CC0; the normal map helps create surface detail. Motion, foam and refraction still require shader support. |

**Licence / License.** Poly Haven publie ses textures CC0; ambientCG indique CC0 1.0 pour ses ressources. La carte de vagues ProcTexture indique également CC0 sur sa page. Re-vérifier la licence de chaque asset et conserver une copie du fichier de licence ou de la page de téléchargement dans le dossier de l’asset. / Poly Haven textures are CC0; ambientCG states CC0 1.0 for its assets. The ProcTexture wave page also marks that texture CC0. Re-check each asset’s license and keep a copy of its license file or download page with the asset.

**Compatibilité moteur / Engine compatibility.** Ces bibliothèques fournissent souvent plusieurs cartes PBR (albedo, normal, roughness, height et occlusion). Le pipeline natif actuel n’importe pas automatiquement tout cet ensemble : il faudra étendre l’importeur et le format matériau pour exploiter les cartes autres que la couleur. Commencer par la couleur de base, puis ajouter normal/roughness au shader lors d’une évolution dédiée. / These libraries often provide several PBR maps (albedo, normal, roughness, height and occlusion). The current native pipeline does not automatically import the full set; the importer and material format need extending to use maps beyond color. Start with base color, then add normal/roughness support in a dedicated shader change.
