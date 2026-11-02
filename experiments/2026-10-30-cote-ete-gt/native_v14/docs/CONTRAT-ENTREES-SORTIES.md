# Contrat d’entrées et de sorties — Moteur 3D natif x64

**Version du document :** 1.3
**Identifiant de build de référence :** 261102-0005
**Scène source :** V13.14  
**Révision GitHub :** injectée au build (SHA court du commit)  
**Branche GitHub :** `experiment/manhattan-silent-hill-2026-10-30`  
**Run GitHub Actions :** identifiant CI séparé, attribué par GitHub

Ce document définit les données attendues par chaque partie du moteur et les résultats qu’elle doit fournir. Il s’applique au découpage modulaire reconstruit par `assemble_native.js`.


### Scène modifiable / Editable scene

**FR —** Les scènes sont définies séparément dans `scene_coastal_v1314.js`, `scene_orange_macro.js` et `scene_rapture.js`. Modifiez le fichier concerné pour changer objets, matériaux, dimensions, placement et lumières locales. Les fonctions de rendu restent dans `source_v1314.html`; ses primitives géométriques sont utilisées par les définitions. `export_scene.js` combine le moteur V13.14 et le module de scène choisi pour produire un binaire V4. Le chargeur natif conserve la lecture des fichiers V3 existants.

**EN —** Scenes are defined separately in `scene_coastal_v1314.js`, `scene_orange_macro.js`, and `scene_rapture.js`. Edit the relevant file to change objects, materials, dimensions, placement, and local lights. Rendering functions remain in `source_v1314.html`; its geometry helpers are used by the scene definitions. `export_scene.js` combines the V13.14 engine and selected scene module to produce a V4 binary. The native loader still reads existing V3 files.

## 1. Contrat global de l’application

### Français

**Entrée obligatoire à l’exécution :** `moteur3d_v1314_scene.bin`, placé dans le même dossier que l’exécutable. L’application ne prend pas d’arguments de ligne de commande; elle déduit le chemin de la scène à partir du chemin de son propre EXE.

Le fichier binaire doit provenir de `export_scene.js` exécuté sur `source_v1314.html`. Il contient la géométrie réelle V13.14, les normales, UV, matériaux, mipmaps RGB565 couleur, normales et rugosité, ainsi que le BVH. Le moteur natif ne crée pas de scène de remplacement. La scène comprend notamment la promenade pavée élargie, neuf bancs, quatre parasols et serviettes, quatre poubelles, des mouettes, des lampadaires et les vagues.

Le format commence par 8 octets de signature `M3D1314`, puis des entiers little-endian : version, nombre d’objets, nombre de nœuds BVH, racine BVH, nombre d’indices d’ordre, nombre de matériaux, nombre de textures et dimension de texture. Le chargeur lit les scènes V3 historiques et V4. En V4, chaque matériau contient son nom UTF-8 sur 24 octets, réflexion et transmission (0–100), puis un indice de réfraction en millièmes dans le champ de 16 bits (0 signifie valeur par défaut), suivi des mipmaps RGB565 couleur, normale tangentielle et rugosité. Après les matériaux, V4 ajoute un nombre de lumières ponctuelles (maximum 256), puis pour chaque lumière position XYZ, portée, intensité et couleur RGB en flottants 32 bits. V3 n’a pas ces champs et conserve ses matériaux opaques et son indice par défaut.

Le chargeur vérifie la signature, la version, les limites de quantités, la racine et le graphe BVH (liens, cycles, portée des feuilles et couverture complète des objets), les bornes finies, les matériaux et indices, ainsi que les chaînes de mipmaps attendues (7 niveaux de 512×512 à 8×8). Il rejette les données manquantes ou supplémentaires avec un message explicite et convertit les erreurs d’allocation en erreur de chargement.

Les réglages d’entrée sont fournis par l’interface :

- Résolution de sortie : 1920×1080 (par défaut), 1600×900, 1280×720 ou 3840×2160 (4K UHD).
- Précision interne : 100 %, 87,5 %, 75 %, 66,7 %, 50 %, lissage local des contours à 4 voisins (par défaut) ou FSAA 16×.
- Caméra : distance, rotation (−180° à 180°), élévation (−5° à 50°) et champ horizontal (35° à 105°).
- Heure : positions 0–32, converties en 06:00–22:00 par pas de 30 minutes.
- Threads CPU : 1–32.
- Brouillard : activé ou désactivé.
- Bloom doux : option Qt optionnelle; activée automatiquement au chargement d’une scène nommée avec `rapture`.

**Sortie à l’exécution :** une image calculée en mémoire, redimensionnée à la résolution choisie et affichée dans la fenêtre. Le programme ne sauvegarde pas spontanément de PNG ou de JPEG. L’interface publie les temps de rendu, de reconstruction des ombres, de post-traitement et le total, ainsi que les rayons, impacts, tests BVH/AABB, nœuds visités, texels et threads.

### English

**Required runtime input:** `moteur3d_v1314_scene.bin`, placed beside the executable. The application accepts no command-line arguments; it derives the scene path from its own executable path.

The binary must be produced by running `export_scene.js` on `source_v1314.html`. It contains the actual V13.14 geometry, normals, UVs, materials, color/normal/roughness RGB565 mipmaps and BVH. The native renderer does not create a substitute scene. The scene includes the widened paved promenade, benches facing the sea, street lamps and waves.

The file begins with an 8-byte `M3D1314` signature, followed by little-endian integers for version, object count, BVH-node count, BVH root, order-index count, material count, texture count and texture dimension. The loader reads historical V3 and V4. In V4, each 24-byte UTF-8 material record stores reflectivity, transmission and refractive index in thousandths in the 16-bit field (zero means default), followed by RGB565 color, tangent-space normal and roughness mip chains. After materials, V4 stores a point-light count (maximum 256), followed by each light’s XYZ position, radius, intensity and RGB color as 32-bit floats. V3 has no light records and retains opaque materials and the default index.

The loader validates the signature, version, count limits, root and BVH graph (links, cycles, leaf ranges, and complete object coverage), finite bounds, material indices, and the fixed seven mip levels from 512×512 to 8×8. Missing, truncated, trailing or invalid data returns a clear error; allocation failures are caught.

Inputs supplied by the user interface:

- Output resolution: 1920×1080 (default), 1600×900, 1280×720, or 3840×2160 (4K UHD).
- Internal scale: 100%, 87.5%, 75%, 66.7%, 50%, four-neighbour edge smoothing (default), or FSAA 16×.
- Camera: distance, rotation (−180° to 180°), elevation (−5° to 50°) and horizontal field of view (35° to 105°).
- Time: slider positions 0–32 map to 06:00–22:00 in 30-minute steps.
- CPU threads: 1–32.
- Fog: enabled or disabled.
- Soft bloom: optional Qt UI control; enabled automatically when a scene filename contains `rapture`.

**Runtime output:** an in-memory rendered image, resized to the selected output resolution and displayed in the window. The program does not automatically save a PNG or JPEG. The UI also reports render, shadow-map rebuild, post-processing and total times, along with ray, hit, BVH/AABB test, node, texel and thread counts.

## 2. Contrat par module / Module contracts

### `m3d_engine.h` — Types partagés / Shared types

**Entrée / Input :** définitions communes des vecteurs, rayons, impacts, objets, matériaux, scène, caméra, lumières, statistiques et carte d’ombres. / Shared definitions for vectors, rays, hits, objects, materials, scene, camera, lights, statistics and shadow map.

**Sortie / Output :** déclarations et types utilisés par tous les modules; aucune donnée de scène n’est chargée par cet en-tête. / Declarations and types used by all modules; this header loads no scene data.

### `src/m3d_physics.h` — Contrat backend physique / Physics backend contract

**Entrée / Input :** descriptions de corps (masse, pose, vitesse et options de collision), demandes d’initialisation, d’étape de simulation et de lecture/écriture de pose. / Body descriptions (mass, pose, velocity, and collision options), initialization, simulation-step, and pose read/write requests.

**Sortie / Output :** interface indépendante du rendu (`Backend`) et choix `Disabled` / `PhysX`. `physXAvailable()` retourne `false` jusqu’à ce que l’adaptateur et le SDK soient ajoutés; aucun calcul physique n’est effectué dans cette révision. / Renderer-independent `Backend` interface and `Disabled` / `PhysX` selection. `physXAvailable()` returns `false` until the adapter and SDK are added; this revision performs no physics simulation.

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

**Sortie / Output :** couleur RGB calculée pour un rayon; ajoute au besoin les contributions des lumières globales et de scène, ombres, textures, normal maps, bump mapping procédural, réflexions et transmission réfractée par Fresnel. Les rayons traversent jusqu’à huit surfaces transparentes et composent leur teinte avec l’arrière-plan; ces surfaces ne bloquent pas la carte d’ombres opaque. Les normal maps tangentiels modifient la normale d’ombrage des triangles texturés sans changer leur géométrie. Le bump mapping perturbe aussi la normale, avec un relief procédural dépendant du matériau et atténué avec la distance. `buildShadow` produit la carte d’ombres solaire de la scène; les lumières ponctuelles n’ont pas encore d’ombres propres. / Computed RGB color for a ray, including global and scene lights, shadows, textures, tangent-space normal maps, procedural bump mapping, reflections and Fresnel refraction. `buildShadow` returns the sun shadow map; point lights do not yet cast shadows. Rays pass through up to eight transparent surfaces and composite their tint with the background; these surfaces do not block the opaque shadow map. Normal maps change shading normals on textured triangles without changing geometry. Procedural bump also perturbs shading normals and fades with distance. `buildShadow` returns the scene’s shadow map.

### `m3d_render.cpp` — Caméra et rendu / Camera and rendering

**Entrée / Input :** scène, carte d’ombres, caméra, largeur/hauteur internes, nombre de threads, brouillard et jeton d’annulation de rendu. / Scene, shadow map, camera, internal width/height, thread count, fog and render-cancellation token.

**Sortie / Output :** `render` remplit un tampon de `largeur × hauteur` pixels 32 bits et retourne `Stats`. Un jeton de génération périmé interrompt le travail après un changement de réglage. / `render` fills a 32-bit pixel buffer of `width × height` and returns `Stats`. A stale generation token cancels work after settings change.

Les interfaces publient uniquement un tampon complet, après le rendu, la mise à l’échelle et les effets de sortie, si son jeton de génération est encore courant. L’image affichée reste intacte pendant le calcul. / The frontends publish only a complete buffer after rendering, scaling and output effects, provided its generation token is still current. The displayed image remains unchanged during calculation.

### `m3d_post.cpp` — Mise à l’échelle et contours / Scaling and edges

**Entrée / Input :** tampon source, dimensions, nombre de threads et intensité du bloom. / Source buffer, dimensions, thread count and bloom strength.

**Sortie / Output :** nouveau tampon aux dimensions cibles : `upscaleBilinear` pour interpolation bilinéaire, `downsample2x` pour réduction 2× et `antialiasEdges4` pour adoucir sélectivement les pixels de fort contraste avec leurs quatre voisins et `bloomGlow` pour ajouter un halo doux aux hautes lumières. / New destination-sized buffer: `upscaleBilinear` for bilinear interpolation, `downsample2x` for 2× reduction, `antialiasEdges4` for selective edge smoothing, and `bloomGlow` for soft highlight halos.

### `moteur3d_win32.cpp` — Interface et orchestration / UI and orchestration

**Entrée / Input :** événements Windows, curseurs, listes de résolution/précision, case de brouillard et fichier scène chargé. / Windows events, sliders, resolution/quality lists, fog checkbox and loaded scene file.

**Sortie / Output :** requêtes de rendu, affichage de l’image et des diagnostics, contrôles de caméra et messages d’erreur. / Render requests, displayed image and diagnostics, camera controls and error messages.

### `assemble_native.js` — Reconstruction source / Source reconstruction

**Entrée / Input :** les fichiers C++ modulaires dans l’ordre déclaré dans le script. / Modular C++ files in the order declared by the script.

**Sortie / Output :** le fichier unique `moteur3d_v14_native.cpp`, reconstruit de façon déterministe puis compilé par MSVC. / The single `moteur3d_v14_native.cpp` file, deterministically reconstructed and then compiled by MSVC.

## 3. Sorties de compilation / Build outputs

Le workflow exporte `moteur3d_v1314_scene.bin`, reconstruit le source, compile l’EXE x64 puis crée une archive identifiée par son numéro de run et son SHA court. L’archive contient l’EXE et le fichier scène requis. Le SHA du commit et la branche sont injectés dans l’interface et les propriétés Windows; le numéro de run GitHub Actions reste séparé.

The workflow exports `moteur3d_v1314_scene.bin`, reconstructs the source, compiles the x64 EXE and creates an archive identified by its run number and short SHA. The archive contains the EXE and required scene file. The commit SHA and branch are embedded in the UI and Windows file properties; the GitHub Actions run number remains separate.


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
