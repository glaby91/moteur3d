# Moteur 3D natif x64 — build 261102-0005

## Repères de version

- **Version :** `261102-0005`.
- **Révision GitHub :** SHA court du commit, injecté à la compilation.
- **Branche GitHub :** nom exact de la branche, injecté à la compilation.
- **Scène et géométrie source :** V13.14.
- **Exécution GitHub Actions :** numéro de run CI indépendant de la révision et de la branche.

La révision et la branche apparaissent dans l’interface et les propriétés Windows de l’exécutable. `build_info.h` fournit des valeurs de secours pour une compilation locale; le workflow GitHub le régénère à chaque build.

Le contrat bilingue des entrées/sorties, module par module, est documenté dans [CONTRAT-ENTREES-SORTIES.md](docs/CONTRAT-ENTREES-SORTIES.md).

## Sources de scène modifiables

Les décors restent des sources indépendantes : `scene_coastal_v1314.js`, `scene_orange_macro.js` et l’étude Rapture `scene_rapture.js`. Chaque fichier décrit géométrie, placement et matériaux; `source_v1314.html` fournit les primitives et l’aperçu, tandis que `export_scene.js` produit un binaire natif. Rapture comprend maintenant **42 lumières locales**, quatre lustres, appliques, kiosques vitrés, tuyauteries à vannes, garde-corps lumineux et jardinières marines, en plus de ses façades, du dôme et de la bathysphère. Son export actuel contient **207 661 objets**, dont **148 203 triangles**. La caméra d’ouverture Qt adopte une vue oblique depuis l’avenue pour éviter le cadrage extérieur et symétrique. Ses matériaux pierre, bronze et verre ont des cartes couleur, normale et rugosité; le verre utilise Fresnel/réfraction et l’interface Qt peut activer le bloom doux. Les lumières locales n’ont pas encore d’ombres et le verre reste une approximation mince. La scène côtière comprend le coupé CC0; son export de référence contient 1 537 833 triangles, 1 541 767 objets et 920 433 nœuds BVH.

## Fonctionnalités détaillées

Voir [FONCTIONNALITES-MOTEUR.md](docs/FONCTIONNALITES-MOTEUR.md) pour l’inventaire des capacités, réglages, limites et diagnostics, en français et en anglais.

L’audit du build `261102-0005`, les évolutions récentes, les résultats des compilations Windows/Linux et les limites identifiées sont consignés dans [AUDIT-TECHNIQUE-261102-0005.md](docs/AUDIT-TECHNIQUE-261102-0005.md).

## Modules du moteur

Les sources de travail sont séparées dans `src/` afin que chaque partie puisse évoluer indépendamment :

- `src/m3d_scene.cpp` : lecture/validation des binaires V3 historiques et V4.
- `src/m3d_lighting.cpp` : soleil, ciel et sources lumineuses pilotés par l’heure.
- `src/m3d_intersection.cpp` : intersections des primitives, calcul des normales et parcours BVH avec directions inverses mises en cache par rayon.
- `src/m3d_shading.cpp` : ombres solaires, lumières globales et propres à la scène, cartes normales/rugosité, bump, réflexions et transmission réfractée par Fresnel.
- `src/m3d_render.cpp` : caméra, lancer de rayons et rendu parallèle par tuiles.
- `src/m3d_post.cpp` : mise à l’échelle, lissage adaptatif des contours et bloom doux optionnel.
- `src/m3d_physics.h` : contrat indépendant du rendu pour un backend physique; le point d’intégration PhysX est réservé, sans SDK lié ni simulation active dans ce build.
- `src/moteur3d_win32.cpp` : interface native, réglages et orchestration.
- `src/moteur3d_qt.cpp` : interface multiplateforme Qt Widgets et orchestration du rendu.
- `m3d_engine.h` : types et fonctions partagés entre les modules.

`assemble_native.js` reconstruit, dans un ordre déterministe, le fichier monolithique `moteur3d_v14_native.cpp`. C’est ce fichier reconstruit que MSVC compile : les sources modulaires restent la référence de travail et l’exécutable utilise le même parcours de rendu.

## Ordonnancement

À l’exécution, l’application charge et valide d’abord la scène, met à jour l’éclairage selon l’heure, puis construit la carte d’ombres. Le rendu calcule ensuite les pixels par tuiles en s’appuyant sur les intersections BVH et l’ombrage des matériaux. Enfin, le post-traitement adapte l’image à la résolution choisie avant l’affichage.

L’ordre de reconstruction/compilation est : scène → éclairage → intersections → ombrage → rendu → post-traitement → interface. Les interfaces partagées sont déclarées dans `m3d_engine.h`.

## Téléchargement et compilation

Le ZIP de chaque exécution porte son numéro de run et son SHA court; il contient l’exécutable x64, les scènes côtière et orange macro, le module JS de la scène orange et `build-manifest.json` avec la provenance et les statistiques d’export. Extraire l’EXE et la scène à ouvrir dans le même dossier.

Le workflow GitHub injecte la révision, la branche et le numéro de run, charge `scene_coastal_v1314.js` avec le moteur de `source_v1314.html`, reconstruit le fichier source et compile avec MSVC. Pour compiler une autre scène, passez son fichier JS au troisième argument de `export_scene.js` et chargez ce même module dans l’aperçu HTML. Le mode **Ultra** conserve les ombres adoucies et les réflexions à un rebond sur l’eau, la peinture et les vitrages.


Le lissage des contours fonctionne à la résolution de sortie : les pixels présentant une forte transition de luminance sont adoucis en mélangeant leur couleur avec celles des quatre voisins directs. Le SSAA 4× est retiré de l’interface; le FSAA 16× reste disponible comme mode coûteux de comparaison. La case **AA contours** active le lissage quatre voisins après mise à l’échelle, indépendamment des modes Natif, Équilibré, Détails, Rapide et Très rapide; elle est cochée par défaut. La profondeur de champ reste optionnelle et passe à 16 rayons par pixel, avec une ouverture réduite pour limiter les images fantômes.

Le menu **Scène → Ouvrir une scène…** permet de charger un fichier `.bin` exporté par `export_scene.js`, dont les scènes côtière, orange ou Rapture. Le chargement et la reconstruction des ombres se font sur le thread de rendu; la scène active reste conservée si le fichier choisi ne peut pas être chargé. Pour Rapture, l’interface Qt choisit le cadrage de départ et active le bloom; la case reste réglable.

La case **Aperçu rotatif — 800 × 600** ouvre une seconde fenêtre qui calcule en 400 × 300 et fait tourner la caméra autour de la scène. Les images sont assemblées dans un tampon et publiées après le rendu et les effets de sortie; l’image terminée précédente reste visible pendant le calcul. Les cartes PBR de l’orange et de Rapture sont intégrées dans les binaires V4; le chargeur conserve la lecture des anciennes scènes V3.

Le lissage des contours et le bloom sont des post-traitements CPU. Les textures utilisent des mipmaps de 512×512 à 8×8 et une interpolation bilinéaire à quatre texels.

## Interface Qt multiplateforme

Une cible Qt Widgets partage le moteur de rendu C++ avec l’application Win32. La cible `moteur3d_qt` se construit avec CMake sous Windows ou Linux; elle propose résolution, qualité, caméra, heure, threads, brouillard, profondeur de champ, AA contours et bloom doux. Le preset de caméra Rapture et son activation automatique du bloom sont intégrés à cette interface Qt. L’interface Win32 existante reste disponible via le workflow Windows historique.

Dépendances : CMake 3.20 ou plus récent, un compilateur C++17, Node.js et Qt 6 Widgets. Sous Ubuntu/Debian, installez `build-essential`, `cmake`, `nodejs` et `qt6-base-dev`. Sous Windows, installez Node.js, Qt 6 avec le composant Qt Widgets et CMake.

Depuis ce dossier, exportez d’abord la scène, puis configurez et compilez avec :

```sh
node export_scene.js source_v1314.html moteur3d_v1314_scene.bin scene_coastal_v1314.js
cmake -S . -B build -DCMAKE_PREFIX_PATH="/chemin/vers/Qt/6.x/gcc_64"
cmake --build build --config Release
```

Pour exporter l’étude Rapture, remplacez le nom de scène et le fichier de sortie, par exemple `node export_scene.js source_v1314.html scene_rapture.bin scene_rapture.js`.

Sur une distribution Linux où Qt 6 est installé dans les chemins système, `-DCMAKE_PREFIX_PATH=...` peut être omis. Sur Windows, indiquez le répertoire Qt correspondant à votre compilateur, par exemple `.../Qt/6.x/msvc2022_64`. Lancez `moteur3d_qt` avec `moteur3d_v1314_scene.bin` dans le même dossier; `cmake --install build --prefix dist` prépare ce dossier et y copie la scène si elle est présente.


## Voiture 1970s Coupe

Le fichier source CC0 est dans `assets/1970s-coupe/1970s-coupe.glb`. L’exporteur extrait le maillage glTF (positions, normales, UV et matériaux), applique son placement déclaré dans `scene_coastal_v1314.js`, puis l’ajoute au BVH de la scène. Les textures de couleur de carrosserie et de pneus sont préconverties en chaînes RGB565 512→8 afin que le workflow Windows n’ait pas besoin d’un décodeur JPEG. Le moteur applique son éclairage et ses reflets natifs au matériau carrosserie; les pourcentages de réflexion se règlent dans la source de scène. Le format V4 transporte les cartes couleur, normale et rugosité; les scènes V3 historiques restent lisibles.

La source est publiée sous CC0 1.0 : https://grab3d.com/models/vehicles/1970s-coupe/.
