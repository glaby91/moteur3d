# État des modules et des fonctions

**Référence de reprise :** build `261102-0005`, branche `experiment/manhattan-silent-hill-2026-10-30`, commit `8993a4faef822757d68f0a2ca0ab718d7972769f`.
**Dernière compilation vérifiée :** GitHub Actions run 14, Qt Windows et Qt Linux réussis : https://github.com/glaby91/moteur3d/actions/runs/41413327310.

Ce document sert de carte de suivi. Les API listées ci-dessous existent dans les sources. « Implémenté » signifie présent dans le code et compilé par la cible Qt; « préparatoire » désigne un contrat sans exécution; « à faire » désigne un bloc absent. Les helpers privés restent regroupés par responsabilité dans leur module.

## Cible réellement compilée

`CMakeLists.txt` construit la bibliothèque statique `m3d_core` à partir de six fichiers C++ puis l’exécutable Qt Widgets `moteur3d_qt`. Le workflow GitHub compile cette même cible sur Windows et Linux. Le calcul de l’image est logiciel et exécuté sur CPU; Qt reçoit le tampon de pixels et l’affiche.

`moteur3d_win32.cpp` est un ancien frontend encore présent dans le dépôt, mais il n’est pas compilé dans la cible courante. Il ne faut pas le confondre avec l’interface Qt livrée. Le workflow Windows natif historique a été retiré.

## Inventaire des modules moteur

| Module | Fonctions publiques et rôle | État actuel |
|---|---|---|
| `m3d_engine.h` | Contrats/types communs `V`, `Ray`, `Hit`, `Obj`, `Node`, `Material`, `Scene`, `Camera`, `Stats`, `Light`, `ShadowMap`; opérations vectorielles `dot`, `cross`, `length`, `norm`, `clampf`; déclarations des entrées de chaque étape. | Implémenté; les types sont partagés, ce n’est pas un module d’exécution. |
| `src/m3d_scene.cpp` | `loadScene(path, Scene&, error)` charge les formats V3/V4, contrôle les limites, matériaux, textures/mipmaps, références et graphe BVH. Helpers privés : lecture little-endian, vecteurs/valeurs finies, validation des bornes et erreurs. | Implémenté et rétrocompatible V3/V4. |
| `src/m3d_lighting.cpp` | `updateLighting(hour)` met à jour soleil, ciel, ambiance jour/nuit, facteur des lampes et liste des sources locales. | Implémenté; lumières locales jusqu’à 256 dans le format V4. Elles n’ont pas encore d’ombres propres. |
| `src/m3d_intersection.cpp` | `closest(...)` cherche l’impact le plus proche dans le BVH; `groundMaterial(scene, point)` choisit le matériau du sol. Helpers : cache d’inverses de direction, intersection rayon/AABB, intersection des primitives et calcul de normale. Primitives : triangle, boîte, boîte arrondie, capsule, cône, ellipsoïde. | Implémenté; BVH préconstruit à l’export. |
| `src/m3d_shading.cpp` | `buildShadow(scene, stats)` crée la carte d’ombres solaire; `shade(...)` calcule l’éclairage d’un impact; `traceColor(...)` gère les rebonds/transmissions et la couleur finale du rayon. Helpers : ciel/brouillard, visibilité PCF, échantillonnage des mipmaps, UV, normal map, bump, rugosité, réflexion et réfraction Fresnel. | Implémenté CPU; ombres des sources ponctuelles et éclairage global indirect absents. Verre simplifié en surface mince. |
| `src/m3d_render.cpp` | `camera(angle, elevation, distance, width, height)` construit la caméra; `render(...)` calcule l’image, répartit des tuiles 16×16 sur 1–32 threads, collecte les statistiques et interrompt un rendu périmé via son jeton de génération. Les options d’échantillons et de profondeur de champ passent par cette entrée. | Implémenté CPU multithread. Pas de pipeline GPU. |
| `src/m3d_post.cpp` | `upscaleBilinear(...)`, `downsample2x(...)`, `antialiasEdges4(...)`, `bloomGlow(...)`. Ces étapes changent l’échelle, adoucissent les contours selon le contraste et ajoutent le bloom optionnel. | Implémenté CPU; aucun post-traitement GPU. |
| `src/m3d_physics.h` | Contrat `Backend`, `BackendKind`, `BodyDesc`, `BodyHandle`, poses et vitesses. `physXAvailable()` indique la disponibilité du backend. | Préparatoire seulement : retourne `false`; pas de SDK PhysX, pas de simulation. |

## Interface, orchestration et données

| Composant | Responsabilité suivie | État actuel |
|---|---|---|
| `src/moteur3d_qt.cpp` | `ImageView` affiche le tampon; `MainWindow::buildUi()` construit les contrôles; `readSettings()` transforme l’interface en réglages; `openSceneFile()` charge une scène; `requestRender()` et `renderLoop()` coordonnent le worker, annulation, rendu et post-traitement; `queueNextVideoFrame()` construit puis lit le cache rotatif; `setVideoControlsEnabled()` bloque les réglages pendant ce calcul. `ellipseOrbitPose()` produit la trajectoire elliptique. | Interface livrée. L’interface et l’orchestration sont encore réunies dans un seul fichier. |
| Aperçu rotatif Qt | 180 vues à pas de 2°; caméra sur ellipse avec variation verticale; calcul à 400×300, affichage 800×600; progression affichée en `image N / 180` et angle caméra; images mises en cache avant lecture; fenêtre distincte pour garder l’image standard stable. | Implémenté. Tampon brut maximal des 180 images RGBA 800×600 : environ 330 Mio, hors surcoûts. Cache libéré à la fermeture. |
| `src/moteur3d_win32.cpp` | Ancienne fenêtre Win32 et orchestration historique. | Source conservée, hors cible et hors paquet actuel. |
| `export_scene.js`, `source_v1314.html` | Assemblent les définitions JS, primitives, matériaux/textures, lumières, maillage importé et BVH, puis exportent le binaire consommé par `loadScene`. | Implémenté; l’édition de décor passe par les scènes JS et un nouvel export. |
| `scene_coastal_v1314.js`, `scene_orange_macro.js`, `scene_rapture.js` | Données propres à chaque scène : objets, matériaux, placement et éclairage local. | Sources de scène séparées; elles ne sont pas des modules du moteur de rendu. |
| `CMakeLists.txt`, `.github/workflows/build-moteur3d-v14-qt.yml` | Dépendent de C++17, CMake, Threads et Qt 6 Widgets; compilent le même code source sur Windows et Linux et produisent les paquets. | Compilation des deux plateformes vérifiée au run 14. Windows embarque les DLL Qt; le paquet Linux s’appuie sur Qt 6 installé sur la machine. |

## Fonctions de bout en bout

1. **Préparer une scène :** modifier une source JS → `export_scene.js` → fichier binaire V3/V4 avec géométrie, matériaux, mipmaps, lumières et BVH.
2. **Charger et préparer le rendu :** `loadScene` valide le fichier → `updateLighting` règle l’heure → `buildShadow` reconstruit si nécessaire la carte d’ombres solaire.
3. **Calculer l’image :** `camera` prépare les axes → `render` distribue les pixels aux threads → `closest` parcourt le BVH → `traceColor`/`shade` calculent matériau et éclairage.
4. **Finir et afficher :** mise à l’échelle, AA contours et bloom éventuel → publication atomique du tampon complet dans Qt → affichage par `QPainter`.
5. **Pré-calcul vidéo :** la fenêtre rotative réutilise le même moteur CPU pour chaque pose; les images terminées sont conservées en RAM, puis lues en boucle.

## Éléments explicitement absents ou à concevoir

- Rendu GPU Direct3D/OpenGL/Vulkan : aucun backend ni shader de rendu n’est lié à `m3d_core`.
- FPGA : pas de blocs RTL/HLS, d’interface PCIe, de firmware, ni de description de carte.
- Mémoire HBM/cache FPGA : aucun contrôleur, planificateur de transferts, partitionnement ou budget matériel implémenté.
- Prévision d’images N+1/N+2 : aucun ordonnanceur spéculatif, gestion de validité ou stratégie d’annulation dédiée. Le cache actuel concerne seulement les 180 images de l’aperçu rotatif.
- Séparation en modules propres à l’interface : `moteur3d_qt.cpp` contient encore UI, orchestration, worker et mode rotatif; le pool de threads du rendu est interne à `m3d_render.cpp`.
- Physique : l’interface de backend est définie, mais aucun moteur n’est raccordé.
- Limites visuelles connues : pas d’ombres des lumières ponctuelles ni d’illumination globale; verre volumique et réfraction épaisse absents; pas d’éditeur interactif de géométrie.

## Règle de mise à jour

À chaque changement fonctionnel, mettre à jour la ligne du module concerné, l’état de compilation, les limites et la chaîne de données ci-dessus, puis référencer le commit et le run CI qui valident le changement. Ne marquer un bloc comme implémenté qu’après compilation de la cible qui l’inclut. Le présent inventaire couvre l’API moteur publique et les responsabilités des helpers privés; les détails d’entrée/sortie binaire restent dans [CONTRAT-ENTREES-SORTIES.md](CONTRAT-ENTREES-SORTIES.md), et la description utilisateur dans [FONCTIONNALITES-MOTEUR.md](FONCTIONNALITES-MOTEUR.md).
