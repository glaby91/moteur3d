# Moteur 3D natif x64 — build 261104-0001

## Repères de version

- **Build natif :** `261105-0001`.
- **Révision GitHub :** SHA court du commit, injecté à la compilation.
- **Branche GitHub :** nom exact de la branche, injecté à la compilation.
- **Scène et géométrie source :** V13.14.
- **Exécution GitHub Actions :** numéro de run CI indépendant de la révision et de la branche.

La révision et la branche apparaissent dans l’interface et les propriétés Windows de l’exécutable. `build_info.h` fournit des valeurs de secours pour une compilation locale; le workflow GitHub le régénère à chaque build.

Le contrat bilingue des entrées/sorties, module par module, est documenté dans [CONTRAT-ENTREES-SORTIES.md](docs/CONTRAT-ENTREES-SORTIES.md).

La carte d’architecture, l’inventaire des fonctions exposées, leur statut et les blocs restant à développer sont suivis dans [ETAT-MODULES-ET-FONCTIONS.md](docs/ETAT-MODULES-ET-FONCTIONS.md). Ce fichier est la référence de reprise du projet; il sépare ce qui est implémenté, préparatoire et planifié.

Une note sur la continuité x86 entre Athlon et Zen et ses limites pour les optimisations SIMD est disponible dans [AMD-ATHLON-ZEN.md](docs/AMD-ATHLON-ZEN.md).

## Sources de scène modifiables

Le build de démonstration `261105-0001` ouvre automatiquement Energetik/Pripiat et n’embarque que cette scène. Les sources des décors côtier, Orange et Rapture restent conservées dans le dépôt GitHub.

Les décors restent des sources indépendantes : `scene_coastal_v1314.js`, `scene_orange_macro.js`, l’étude Rapture `scene_rapture.js` et la scène Pripiat `scene_pripyat_central.js`. Chaque fichier décrit géométrie, placement et matériaux; `source_v1314.html` fournit les primitives et l’aperçu, tandis que `export_scene.js` produit un binaire natif. La bathysphère Rapture reçoit une coque segmentée et rivetée, un grand hublot avant réellement ouvert dans les deux enveloppes, des hublots latéraux élargis et un poste de pilotage ambré visible avec instruments et commandes. L’ouverture de `scene_rapture.bin` sélectionne un cadrage rapproché face à la bathysphère. La vue se pilote par glisser gauche (orbite), glisser droit (déplacement), molette (zoom) et touches AZERTY Z/Q/S/D et R/X (Z avancer, Q gauche, S reculer, D droite, R monter, X descendre); pendant les mouvements, l’image est recalculée en aperçu 480 × 270, puis à la qualité et à la résolution choisies après 450 ms d’arrêt. Le jeton commémoratif argenté avec un « R » en relief reste dans sa vitrine. Le build 261103-0003 affine la découpe du hublot, recule le cadrage initial et atténue les reflets de la coque. Le build 261103-0004 éloigne le parcours vidéo rotatif Rapture pour éviter que la caméra ne longe la coque ou les murs voisins. Les matériaux pierre, bronze et verre ont des cartes couleur, normale et rugosité; le verre utilise Fresnel/réfraction et les lumières locales n’ont pas encore d’ombres propres.

## Scène Pripiat — secteur central

La branche `pripiat` ajoute `scene_pripyat_central.js`, exportée par le même pipeline que Rapture et la scène côtière. Le catalogue GLB optionnel produit un binaire V5. La scène décrit la place centrale, les façades principales d’Energetik et de l’hôtel Polissya, les abords résidentiels, les arbres et le mobilier.

Le parcours automatique dédié suit une boucle dans cette zone et affiche chaque vue au rythme du moteur. Les autres scènes gardent leur aperçu rotatif. Les références cartographiques, le registre de photos documentaires et les niveaux de matériaux sont décrits dans [docs/pripyat/README.md](../../../docs/pripyat/README.md). La vue « Cinéma / auditorium » sert de salle de contrôle visuel pour le build `261104-0011`.

Le build `261104-0011` corrige le cadrage de contrôle, dégage l’écran des bandes de rideau, réduit la largeur du plateau et arrondit les volumes rembourrés des sièges. Il conserve la composition double hauteur, les rangées de sièges, le parement en tuf volcanique et les cartes procédurales de relief et rugosité. La lecture visuelle reste à valider sur le paquet Windows.

## Fonctionnalités détaillées

Voir [FONCTIONNALITES-MOTEUR.md](docs/FONCTIONNALITES-MOTEUR.md) pour l’inventaire des capacités, réglages, limites et diagnostics, en français et en anglais.

L’audit du build `261102-0005` reste dans [AUDIT-TECHNIQUE-261102-0005.md](docs/AUDIT-TECHNIQUE-261102-0005.md). L’optimisation ciblée du BVH issue du benchmark et ses mesures de référence sont décrites dans [OPTIMISATION-BVH-261102-0006.md](docs/OPTIMISATION-BVH-261102-0006.md).

## Modules du moteur

Les sources de travail sont séparées dans `src/` afin que chaque partie puisse évoluer indépendamment :

- `src/m3d_scene.cpp` : lecture/validation des binaires V3 historiques, V4 et V5.
- `src/m3d_lighting.cpp` : soleil, ciel et sources lumineuses pilotés par l’heure.
- `src/m3d_intersection.cpp` : intersections des primitives, calcul des normales et parcours BVH avec directions inverses par rayon et données de triangle préparées au chargement.
- `src/m3d_shading.cpp` : ombres solaires, lumières globales et propres à la scène, cartes normales/rugosité, bump, réflexions et transmission réfractée par Fresnel.
- `src/m3d_render.cpp` : caméra, lancer de rayons et rendu parallèle par tuiles.
- `src/m3d_post.cpp` : mise à l’échelle, lissage adaptatif des contours et bloom doux optionnel.
- `src/m3d_physics.h` : contrat indépendant du rendu pour un backend physique; le point d’intégration PhysX est réservé, sans SDK lié ni simulation active dans ce build.
- `src/moteur3d_win32.cpp` : interface native, réglages et orchestration.
- `src/moteur3d_qt.cpp` : interface multiplateforme Qt Widgets et orchestration du rendu.
- `m3d_engine.h` : types et fonctions partagés entre les modules.

La cible actuellement livrée est `moteur3d_qt`. CMake compile directement les six modules C++ du moteur et l’interface Qt; le workflow `build-moteur3d-v14-qt.yml` vérifie la compilation Windows et Linux. `moteur3d_win32.cpp` est conservé comme ancienne interface, mais n’est pas inclus dans cette cible ni dans le workflow courant.

## Ordonnancement

À l’exécution, l’application charge et valide d’abord la scène, met à jour l’éclairage selon l’heure, puis construit la carte d’ombres. Le rendu calcule ensuite les pixels par tuiles en s’appuyant sur les intersections BVH et l’ombrage des matériaux. Enfin, le post-traitement adapte l’image à la résolution choisie avant l’affichage.

L’ordre de reconstruction/compilation est : scène → éclairage → intersections → ombrage → rendu → post-traitement → interface. Les interfaces partagées sont déclarées dans `m3d_engine.h`.

## Téléchargement et compilation

Le paquet Qt contient l’exécutable, les scènes côtière, Rapture et Pripiat avec leurs sources JavaScript, ainsi que la documentation. La révision Git, la branche et le numéro de run sont intégrés à l’application.

Le workflow GitHub exporte les scènes côtière et Rapture, configure CMake avec Qt 6, puis compile et empaquette `moteur3d_qt` sur Windows et Linux. Le build 261103-0018 recoupe les hublots dans les deux épaisseurs de coque avec des contours circulaires alignés sur les cadres. Pour compiler une autre scène, passez son fichier JS au troisième argument de `export_scene.js`, puis ouvrez le binaire produit dans le menu **Scène → Ouvrir une scène…**.


Le lissage des contours fonctionne à la résolution de sortie : les pixels présentant une forte transition de luminance sont adoucis en mélangeant leur couleur avec celles des quatre voisins directs. Le SSAA 4× est retiré de l’interface; le FSAA 16× reste disponible comme mode coûteux de comparaison. La case **AA contours** active le lissage quatre voisins après mise à l’échelle, indépendamment des modes Natif, Équilibré, Détails, Rapide et Très rapide; elle est cochée par défaut. La profondeur de champ reste optionnelle et passe à 16 rayons par pixel, avec une ouverture réduite pour limiter les images fantômes.

Le menu **Scène → Ouvrir une scène…** permet de charger un fichier `.bin` exporté par `export_scene.js`, dont les scènes côtière, orange ou Rapture. Le chargement et la reconstruction des ombres se font sur le thread de rendu; la scène active reste conservée si le fichier choisi ne peut pas être chargé. Pour Rapture, l’interface Qt choisit le cadrage de départ et active le bloom; la case reste réglable.

La case **Aperçu rotatif — 800 × 600** ouvre une seconde fenêtre qui calcule en 400 × 300 et fait tourner la caméra autour de la scène. Les images sont assemblées dans un tampon et publiées après le rendu et les effets de sortie; l’image terminée précédente reste visible pendant le calcul. Les cartes PBR de l’orange et de Rapture sont intégrées dans le format de scène; le chargeur conserve la lecture des anciennes scènes V3/V4.

Le moteur natif accepte des textures de 512² à 2048². Le format de scène V5 ajoute les albedos RGB888, tout en gardant la lecture des albedos RGB565 des scènes V3/V4; les chaînes normales et rugosité restent encodées en RGB565. Les scènes Pripiat exportées avec la commande ci-dessous embarquent les albedos des GLB sans réduction RGB565 et les mipmaps couleur, normale et rugosité jusqu’à 1². Le rendu CPU filtre bilinéairement dans chaque niveau et interpole entre deux niveaux mip. L’aperçu DirectX 11 applique le décodage/encodage sRGB, demande le MSAA 4× (repli 2× puis sans MSAA selon le GPU), et utilise le filtrage anisotrope jusqu’à 16×.

### Import natif du catalogue GLB Pripiat

L’importeur lit les primitives triangulées glTF 2.0/GLB, les indices, transformations de nœuds, normales, UV0 et les textures glTF de couleur, normales et rugosité. Les cartes couleur gardent 8 bits par canal dans le format V5; les cartes normales et rugosité restent RGB565. Les mipmaps sont calculées au moment de l’export. Le catalogue de 26 modèles remplace les 20 peupliers procéduraux de la place et ajoute les six modèles de mobilier/façade du pack source. Les fichiers PNG référencés par les GLB doivent être présents à côté de leurs modèles; les images intégrées en data URI ou dans les chunks GLB, les vertex colors, l’alpha cutout/blend et le canal metallic ne sont pas encore interprétés par ce chemin natif.

Après extraction du pack `Pripyat-objets-PBR-2048`, exportez la scène depuis ce dossier :

L’export GLB nécessite Python 3 avec Pillow et NumPy, en plus de Node.js. Le pack d’objets et ses PNG doivent rester extraits pendant l’export.

```sh
M3D_SKIP_IMPORTED_CAR=1 \
M3D_IMPORT_PRIPYAT_GLB=1 \
M3D_PRIPYAT_OBJECTS_DIR="/chemin/vers/assets/objects" \
M3D_TEXTURE_SIZE=2048 \
node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
```

L’option catalogue est explicite afin que les builds standards restent reproductibles sans le pack source externe. L’export d’intégration a produit 561 340 objets, dont 538 788 triangles et 519 220 triangles apportés par les GLB. Le binaire généré est volumineux : prévoyez plusieurs centaines de mégaoctets de stockage et de mémoire selon la scène.

## Interface Qt multiplateforme

L’interface Qt Widgets est l’interface livrée et utilise le moteur C++ commun. La cible `moteur3d_qt` se construit avec CMake sous Windows ou Linux; elle propose résolution, qualité, caméra, heure, threads, brouillard, profondeur de champ, AA contours, bloom doux, ouverture de scène et pré-calcul rotatif. Le preset de caméra Rapture et son activation automatique du bloom sont intégrés à Qt. Le fichier Win32 historique reste dans les sources, mais n’est pas compilé par le workflow actuel.

Dépendances : CMake 3.20 ou plus récent, un compilateur C++17, Node.js et Qt 6 Widgets. Sous Ubuntu/Debian, installez `build-essential`, `cmake`, `nodejs` et `qt6-base-dev`. Sous Windows, installez Node.js, Qt 6 avec le composant Qt Widgets et CMake.

Depuis ce dossier, exportez d’abord la scène, puis configurez et compilez avec :

```sh
node export_scene.js source_v1314.html moteur3d_v1314_scene.bin scene_coastal_v1314.js
cmake -S . -B build -DCMAKE_PREFIX_PATH="/chemin/vers/Qt/6.x/gcc_64"
cmake --build build --config Release
```

Pour exporter l’étude Rapture, remplacez le nom de scène et le fichier de sortie, par exemple `node export_scene.js source_v1314.html scene_rapture.bin scene_rapture.js
M3D_SKIP_IMPORTED_CAR=1 node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js`.

Sur une distribution Linux où Qt 6 est installé dans les chemins système, `-DCMAKE_PREFIX_PATH=...` peut être omis. Sur Windows, indiquez le répertoire Qt correspondant à votre compilateur, par exemple `.../Qt/6.x/msvc2022_64`. Lancez `moteur3d_qt` avec `moteur3d_v1314_scene.bin` dans le même dossier; `cmake --install build --prefix dist` prépare ce dossier et y copie la scène si elle est présente.


## Voiture 1970s Coupe

Le fichier source CC0 est dans `assets/1970s-coupe/1970s-coupe.glb`. L’exporteur extrait le maillage glTF (positions, normales, UV et matériaux), applique son placement déclaré dans `scene_coastal_v1314.js`, puis l’ajoute au BVH de la scène. Les textures source de carrosserie et de pneus restent stockées dans le format RGB565 historique 512→8; l’exporteur complète les niveaux jusqu’à 1² afin que le moteur ne bloque pas le LOD au niveau 8². Le moteur applique son éclairage et ses reflets natifs au matériau carrosserie; les pourcentages de réflexion se règlent dans la source de scène. Les scènes V3/V4 restent lisibles avec le format V5.

La source est publiée sous CC0 1.0 : https://grab3d.com/models/vehicles/1970s-coupe/.

Le build `261103-0018` découpe des ouvertures circulaires dans les deux couches de la coque de la bathysphère. Les hublots avant et latéraux sont alignés sur ces ouvertures; les nervures et sangles s'interrompent à leur bord pour laisser les ouvertures dégagées.


Le build `261103-0019` ajoute au benchmark le choix du mode SIMD, la comparaison x64 / AVX2 + FMA3, les barres de débit relatif et un export CSV enrichi avec le mode, les répétitions, l’architecture et les compteurs par configuration.


Le build `261103-0020` ajoute au benchmark les compteurs de dispatch des noyaux AVX2 (paquets de rayons, triangles, textures et lumières) et équilibre l’ordre de comparaison par configuration de threads.

## Siège rabattable du Palais Energetik

La géométrie réutilisable du siège est définie dans `assets/pripyat/energetik_cinema_seat.js`. La scène de prévisualisation présente trois états du même modèle, conformément à la référence : assise manquante, relevée et abaissée, reliées par un rail métallique continu. Le modèle compte 125 primitives pour la rangée, dont les coques et coussins, les côtés capitonnés, les charnières, les boulons et les coutures; ce sont des volumes natifs du moteur, pas un maillage photogrammétrique. Le manifeste conserve les pivots et profils physiques prévus; les valeurs non mesurées restent vides.

Le GLB `assets/pripyat/energetik_cinema_seat_row.glb` réunit quatre matériaux et leurs cartes PBR intégrées pour Unreal Engine. Les cartes de vinyle brun usé, peinture des supports, acier et bois sont procédurales et générées en 2048²; Unreal génère la chaîne mip à l’import. Elles couvrent le vinyle brun usé, la peinture des supports, l’acier et les garnitures en bois. L’import Unreal et le contrôle visuel dans le moteur Qt restent à effectuer.

Pour recalculer seulement le prototype, sans compiler l’application Qt ni reconstruire la ville :

```sh
M3D_SKIP_IMPORTED_CAR=1 node export_scene.js source_v1314.html scene_energetik_seat_preview.bin scene_energetik_seat_preview.js
python3 tools/generate_seat_maps.py
node tools/export_energetik_seat_row_glb.js
node export_energetik_seat_manifest.js scene_energetik_seat_preview.assets.json
```

Ouvrez ensuite `scene_energetik_seat_preview.bin` dans le moteur par **Scène → Ouvrir une scène…**. La scène autonome contient la rangée et un petit sol neutre. Le workflow de build Qt exporte la scène native et le GLB, puis les inclut dans le pack Windows. Le binaire contient des poses statiques; le manifeste décrit les pièces, les axes de charnière et les profils physiques prévus. Le même module peut être réutilisé dans d’autres scènes.
