# Contrat d’entrées et de sorties — Moteur 3D natif x64

**Version du document :** 1.3
**Identifiant de build de référence :** 261102-0006
**Scène source :** V13.14  
**Révision GitHub :** injectée au build (SHA court du commit)  
**Branche GitHub :** `experiment/manhattan-silent-hill-2026-10-30`  
**Run GitHub Actions :** identifiant CI séparé, attribué par GitHub

Ce document définit, en termes concrets, ce que chaque module reçoit, ce qu’il retourne ou modifie, et ce que le module suivant est en droit d’attendre. Il décrit la cible réellement compilée : les six modules moteur C++ et l’interface Qt Widgets, construits directement par CMake sous Windows et Linux.


### Scène modifiable / Editable scene

**FR —** Les scènes sont définies séparément dans `scene_coastal_v1314.js`, `scene_orange_macro.js`, `scene_rapture.js` et `scene_pripyat_central.js`. Modifiez le fichier concerné pour changer objets, matériaux, dimensions, placement et lumières locales. Les fonctions de rendu restent dans `source_v1314.html`; ses primitives géométriques sont utilisées par les définitions. `export_scene.js` combine le moteur V13.14 et le module de scène choisi pour produire un binaire V5. Le chargeur natif conserve la lecture des fichiers V3/V4.

**EN —** Scenes are defined separately in `scene_coastal_v1314.js`, `scene_orange_macro.js`, and `scene_rapture.js`. Edit the relevant file to change objects, materials, dimensions, placement, and local lights. Rendering functions remain in `source_v1314.html`; its geometry helpers are used by the scene definitions. `export_scene.js` combines the V13.14 engine and selected scene module to produce a V4 binary. The native loader still reads existing V3 files.

## 1. Contrat global de l’application

### Français

**Entrée obligatoire à l’exécution :** `moteur3d_v1314_scene.bin`, placé dans le même dossier que l’exécutable. L’application ne prend pas d’arguments de ligne de commande; elle déduit le chemin de la scène à partir du chemin de son propre EXE.

Le fichier binaire doit provenir de `export_scene.js` exécuté sur `source_v1314.html`. Il contient la géométrie réelle V13.14, les normales, UV, matériaux, mipmaps RGB565 couleur, normales et rugosité, ainsi que le BVH. Le moteur natif ne crée pas de scène de remplacement. La scène comprend notamment la promenade pavée élargie, neuf bancs, quatre parasols et serviettes, quatre poubelles, des mouettes, des lampadaires et les vagues.

Le format commence par 8 octets de signature `M3D1314`, puis des entiers little-endian : version, nombre d’objets, nombre de nœuds BVH, racine BVH, nombre d’indices d’ordre, nombre de matériaux, nombre de textures et dimension de texture. Le chargeur lit les scènes V3, V4 et V5. En V4/V5, chaque matériau contient son nom UTF-8 sur 24 octets, réflexion et transmission (0–100), puis un indice de réfraction en millièmes dans le champ de 16 bits (0 signifie valeur par défaut). V4 stocke les mipmaps couleur, normale tangentielle et rugosité en RGB565; V5 stocke les mipmaps couleur en RGB888 puis les normales et rugosités en RGB565. Après les matériaux, V4/V5 ajoutent un nombre de lumières ponctuelles (maximum 256), puis pour chaque lumière position XYZ, portée, intensité et couleur RGB en flottants 32 bits. V3 n’a pas ces champs et conserve ses matériaux opaques et son indice par défaut.

Le chargeur vérifie la signature, la version, les limites de quantités, la racine et le graphe BVH (liens, cycles, portée des feuilles et couverture complète des objets), les bornes finies, les matériaux et indices, ainsi que les chaînes mip (de 7 à 12 niveaux, en puissances de deux de 512² à 2048², terminées à 8² ou 1²). Il rejette les données manquantes ou supplémentaires avec un message explicite et convertit les erreurs d’allocation en erreur de chargement.

Les réglages d’entrée sont fournis par l’interface :

- Résolution de sortie : 1366×768 (par défaut, résolution native du Vostro), 1920×1080, 1600×900, 1280×720 ou 3840×2160 (4K UHD).
- Backend : ray tracing CPU complet par défaut ; aperçu raster facultatif Direct3D 11 sous Windows. Ce dernier n’inclut pas les ombres par rayons, la profondeur de champ ni les réflexions/réfraction du rendu CPU.
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

The file begins with an 8-byte `M3D1314` signature, followed by little-endian integers for version, object count, BVH-node count, BVH root, order-index count, material count, texture count and texture dimension. The loader reads V3, V4 and V5. In V4/V5, each 24-byte UTF-8 material record stores reflectivity, transmission and refractive index in thousandths in the 16-bit field (zero means default). V4 stores RGB565 color, tangent-space normal and roughness mip chains; V5 stores RGB888 color followed by RGB565 normal and roughness chains. After materials, V4/V5 store a point-light count (maximum 256), followed by each light’s XYZ position, radius, intensity and RGB color as 32-bit floats. V3 has no light records and retains opaque materials and the default index.

The loader validates the signature, version, count limits, root and BVH graph (links, cycles, leaf ranges, and complete object coverage), finite bounds, material indices, and mip chains (7 to 12 power-of-two levels from 512² to 2048², ending at 8² or 1²). Missing, truncated, trailing or invalid data returns a clear error; allocation failures are caught.

Inputs supplied by the user interface:

- Output resolution: 1366×768 (default, Vostro native resolution), 1920×1080, 1600×900, 1280×720, or 3840×2160 (4K UHD).
- Backend: full CPU ray tracing by default; optional Direct3D 11 raster preview on Windows. The preview omits ray-traced shadows, depth of field, and the CPU renderer's reflections/refraction.
- Internal scale: 100%, 87.5%, 75%, 66.7%, 50%, four-neighbour edge smoothing (default), or FSAA 16×.
- Camera: distance, rotation (−180° to 180°), elevation (−5° to 50°) and horizontal field of view (35° to 105°).
- Time: slider positions 0–32 map to 06:00–22:00 in 30-minute steps.
- CPU threads: 1–32.
- Fog: enabled or disabled.
- Soft bloom: optional Qt UI control; enabled automatically when a scene filename contains `rapture`.

**Runtime output:** an in-memory rendered image, resized to the selected output resolution and displayed in the window. The program does not automatically save a PNG or JPEG. The UI also reports render, shadow-map rebuild, post-processing and total times, along with ray, hit, BVH/AABB test, node, texel and thread counts.

## 2. Contrat par module / Module contracts

Chaque entrée précise les préconditions. Chaque sortie précise ce que le module garantit au suivant. Les helpers privés sont regroupés selon le travail qu’ils réalisent; ils ne constituent pas des interfaces publiques séparées.

### `m3d_engine.h` — Types et primitives communes / Shared types and math

**Entrée :** aucune entrée d’exécution; l’en-tête définit les données et signatures communes. Les vecteurs `V` représentent des coordonnées 3D flottantes; `Ray` porte une origine et une direction; `Hit` porte distance, position, normale, matériau et identifiant. `Scene` regroupe objets, BVH, ordre, matériaux, lumières et racine.

**Sortie garantie :** types compatibles entre modules et fonctions inline `dot`, `cross`, `length`, `norm`, `clampf`. `loadScene(const wchar_t*, ...)` adapte le chemin Windows à l’API `std::filesystem`. Aucun stockage ni calcul de scène n’est exécuté par cet en-tête.

### `m3d_scene.cpp` — Lecture et validation / Loading and validation

**Fonction :** `loadScene(path, Scene&, error)`.

**Entrée :** chemin vers un binaire V3/V4 complet; `Scene&` reçoit le résultat; `error` reçoit le détail d’échec. Les sous-lecteurs lisent les nombres little-endian et refusent les flottants non finis.

**Sortie garantie :** `true` signifie que le graphe BVH, les indices, bornes, matériaux et mipmaps sont validés et que les tableaux de `Scene` sont utilisables. `false` signifie qu’aucune scène ne doit être rendue et que `error` explique l’échec. Le chargeur accepte V3 et V4; les champs absents de V3 gardent leurs valeurs par défaut.

### `m3d_lighting.cpp` — Éclairage global et sources locales / Lighting

**Fonction :** `updateLighting(hour)`.

**Entrée :** heure décimale de la journée dans l’intervalle utilisé par l’interface, 6 à 22 h. Les lumières ponctuelles de scène doivent déjà être dans la scène chargée.

**Sortie garantie :** état global actualisé (`sun`, couleurs du ciel, `daylight`, `lampFactor`, `pointLights`), consommé en lecture par ombrage et génération d’ombres. La fonction ne touche ni à la caméra ni aux pixels.

### `m3d_intersection.cpp` — Rayon, primitives et BVH / Intersections

**Fonctions :** `closest(scene, ray, best, stats, ground, ignoreObject, ignoreTransparent)` et `groundMaterial(scene, point)`.

**Entrée :** scène validée, rayon normalisé ou de longueur connue, distance maximale initiale dans `best.t`, options de sol/ignorance et compteur `Stats` optionnel. Le cache privé précalcule les inverses de direction pour les tests AABB.

**Sortie garantie :** si un impact plus proche existe, `best` devient le `Hit` le plus proche avant la limite; sinon le meilleur impact entrant est conservé. `Stats` reçoit les compteurs effectués. `groundMaterial` retourne l’indice du matériau de sol déterminé par le point. Les six types géométriques sont testés dans ce module.

### `m3d_shading.cpp` — Ombres, matériaux et couleur / Shading

**Fonctions :** `buildShadow(scene, stats)`, `shade(scene, shadow, camera, ray, hit, stats, fog, reflections)`, `traceColor(scene, shadow, camera, ray, stats, fog)`.

**Entrée :** scène et état d’éclairage initialisés; pour `shade`, impact `Hit` issu de `closest`; pour `traceColor`, rayon de caméra et paramètres de brouillard. `buildShadow` reçoit une scène valide et un compteur.

**Sortie garantie :** `buildShadow` retourne la carte solaire de profondeur et ses bornes min/max; `shade` retourne la contribution RGB locale d’un impact; `traceColor` retourne la couleur RGB du rayon, ciel compris. Normal/rugosité, UV, bump, spéculaire, réflexion et transmission sont consommés ici. Un échec de correspondance de texture utilise les replis du matériau; les lumières ponctuelles n’ont pas d’ombres dédiées.

### `m3d_render.cpp` — Caméra, échantillonnage et image / Camera and render

**Fonctions :** `camera(angle, elevation, distance, width, height)` et `render(scene, shadow, camera, width, height, pixels, threads, fog, generation, token, samplesPerPixel, depthOfField)`.

**Entrée :** scène et ombres prêtes, dimensions internes positives, vecteur pixel dimensionné pour `width × height`, 1–32 threads, options d’échantillonnage/DOF et couple génération-token. `camera` reçoit les angles en degrés et les dimensions pour l’aspect.

**Sortie garantie :** `camera` renvoie œil, base caméra, champ et aspect. Si le jeton reste courant, `render` remplit le tampon 32 bits et retourne `Stats` (temps, threads, rayons, intersections, texels, etc.); si la génération change, les travailleurs s’arrêtent et le demandeur ne doit pas publier cette image. Les tuiles sont 16×16; le module exécute sur CPU.

### `m3d_post.cpp` — Échelle, AA contours et bloom / Post-processing

**Fonctions :** `upscaleBilinear(src, sw, sh, dw, dh, threads)`, `downsample2x(src, sw, sh, threads)`, `antialiasEdges4(src, w, h, threads)`, `bloomGlow(src, w, h, threads, strength)`.

**Entrée :** tampon source complet au format pixel 32 bits, dimensions cohérentes, nombre de threads; `upscaleBilinear` reçoit dimensions de sortie, `downsample2x` exige une image réductible par 2, `bloomGlow` reçoit sa force.

**Sortie garantie :** nouveau tampon indépendant; la source n’est pas modifiée. L’appelant applique la chaîne correspondant au mode choisi : échelle interne, AA quatre voisins si coché, bloom si actif. Le résultat final n’est remis à Qt qu’après fin de toute la chaîne.

### `moteur3d_qt.cpp` — Interface, orchestration et publication / Qt frontend

**Fonctions/méthodes :** `ImageView::setFrame/paintEvent`, `MainWindow::buildUi`, `readSettings`, `openSceneFile`, `requestRender`, `renderLoop`, `setVideoControlsEnabled`, `queueNextVideoFrame`, `ellipseOrbitPose`.

**Entrée :** événements utilisateur, chemin de scène, options d’image, réglages de caméra/heure/threads, signaux d’annulation et résultats du travailleur.

**Sortie garantie :** réglages transmis au moteur; chargement de scène annoncé en cas de succès ou erreur sans remplacer la scène active; seuls un tampon complet et correspondant à la dernière génération sont affichés. Le tampon antérieur reste visible pendant le calcul. L’aperçu rotatif pré-calcule 180 images à 2° dans sa fenêtre dédiée, affiche sa progression, puis lit les images mémorisées; la fermeture libère le cache et restaure l’image standard.

### `src/moteur3d_win32.cpp` — Ancien frontend / Legacy frontend

**Entrée/sortie :** ancien code de fenêtre Win32 et appels au moteur commun.

**État :** fichier conservé à titre historique; il n’appartient pas à `m3d_core`, n’est pas construit par CMake et n’est pas livré par le workflow actuel. L’interface compilée et distribuée est Qt Widgets.

### `src/m3d_physics.h` — Contrat physique préparatoire / Physics placeholder

**Entrée prévue :** `BodyDesc` (pose, vitesse, masse, options), puis appels de `Backend` (`initialize`, `createBody`, `step`, lecture/écriture de pose, `removeBody`, `shutdown`).

**Sortie actuelle :** interface déclarée seulement; `physXAvailable()` vaut `false`, aucun backend n’est instancié, aucun corps ou pas de simulation n’existe dans l’exécutable courant.

### `export_scene.js` et fichiers de scène — Fabrication des données / Scene build

**Entrée :** moteur/primitives exposés par `source_v1314.html`, un module de scène JS, puis un chemin de sortie `.bin`.

**Sortie garantie :** binaire V4 contenant géométrie, matériaux/textures, normales, rugosité, lumières et BVH, plus des statistiques d’export écrites sur la sortie standard. `loadScene` est le consommateur correspondant. Les édits de scène passent par les fichiers JS puis nécessitent un nouvel export.

## 3. Contrat de compilation et livraison / Build contract

**Entrée :** sources C++17, `CMakeLists.txt`, scènes JS/HTML, Node.js, CMake, Threads et Qt 6 Widgets. Le workflow exporte les binaires côtière et Rapture avant configuration.

**Sortie :** `m3d_core` compilé depuis les six modules ci-dessus, exécutable `moteur3d_qt` et paquet Windows ou Linux contenant les scènes et documents. Le runner Windows ajoute les bibliothèques Qt nécessaires; sous Linux, le runtime Qt 6 est une dépendance système. Provenance SHA/branche/run enregistrée par CI.

**Vérification de référence :** compilation Qt des deux plateformes réussie au run 14 pour le commit indiqué en tête du document. Cette vérification confirme la compilation et la mise en paquet; elle ne signifie pas que les blocs indiqués « préparatoires » ou « absents » sont implémentés.


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
