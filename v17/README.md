# CSE WARP Beta X — V17 temps réel (premier moteur fonctionnel)

**V17 s'appuie intégralement sur les acquis de V16 : aucune reconstruction inutile de la scène, des textures ou du moteur CPU.** Ce répertoire contient la nouvelle interface d'exploration GPU OpenGL, la V16 reste livrée à côté.

## Ce qui est implémenté

- **Rendu OpenGL 3.3 en continu**, basé sur les **triangles réellement compilés par `m3d::v16::buildRuntime`** ; aucun nouveau format de scène et aucun objet réinventé
- **Caméra libre** : **ZQSD et quatre flèches** (WASD optionnel), bouton droit + souris, E/espace pour monter, Ctrl pour descendre, Maj pour accélérer, touches 1 à 6 pour les caméras existantes, F11 plein écran
- **Rasterisation matérielle** avec Z-buffer et shaders GLSL PBR de première génération (GGX/Smith/Schlick, normal mapping, rugosité, métallicité, lumières architecturales)
- **Carte d'ombres GPU 1024×1024**, recalculée lorsque l'heure solaire change, pas à chaque déplacement
- **Génération des textures GPU** depuis les **mipmaps PBR de V16** : albedo sRGB, normal, roughness et constantes matériaux ; pas de textures artificielles séparées pour V17
- **Vitrages transparents** : passe dédiée alpha, tri arrière-vers-avant
- **Rendu GPU vers une FBO**, puis copie GPU→GPU vers la surface Qt `QOpenGLWidget` : **aucun transfert GPU→CPU obligatoire à chaque image**
- **1366×768 natif Dell**, Full HD 1920×1080, QHD disponibles, sans échelle de calcul réduite implicite
- **Mesures** : FPS affichées et minuterie GPU OpenGL `GL_TIME_ELAPSED` (temps de passage GPU, indépendant de la cadence écran)
- **Capture PNG à la demande** : lecture GPU→CPU seulement lors de l'export
- **Moteur V16 CPU/HLSL accessible** depuis le menu/bouton V17 ; livré avec le même DLL `moteur3d_v16_core`

## Ce que nous réutilisons, précisément

| Actif V16 | V17 |
|---|---|
| `m3d_scene_v16.cpp`, `m3d_v16_bridge.cpp` | Chargement, géométrie, instances, matériau, caméras, UV et triangles |
| `m3d_lighting.cpp` | Horloge du soleil et couleurs de ciel via `captureLighting()`, ajout API non destructif |
| `m3d_engine.h`, `m3d_render.cpp`, `m3d_shading.cpp` | Code CPU, ray/path tracing et GI conservés |
| `m3d_compute11.cpp`, shaders HLSL | Backend GPU Compute V16 conservé, utilisable par le programme V16 |
| `m3d_directx11.cpp` | Rasteriseur historique conservé comme candidat à la parité DirectX 11 |
| `scenes/v16/pavillon01/` | Scène Pavillon 01 utilisée **sans conversion ou réécriture** |
| CMake et Qt6 V16 | `moteur3d_v17` ajouté au même build `v16/CMakeLists.txt`, DLL partagée |

## Construire

Sur Windows (Qt6 6.2+, MSVC et CMake) :

```powershell
cmake -S v16 -B build-v17 -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="<Qt6>"
cmake --build build-v17 --config Release --parallel 4
./build-v17/Release/moteur3d_v17.exe
```

Les shaders `v17/shaders/` sont copiés vers `shaders/v17/` à côté de l'exécutable. Les ressources Pavillon 01 et le programme CPU V16 sont aussi attendus à côté de V17. Les artefacts complets sont générés par `.github/workflows/build-moteur3d-v17.yml`.

## Contrôles automatisés

- GitHub Windows : construction de V17 + du moteur V16, test de chargement de la même scène, rendu de référence CPU par V16 et package Qt déployé
- GitHub Linux : validation des quatre shaders GLSL par `glslangValidator`
- GitHub Linux Mesa : chargement d'un contexte OpenGL réel via `Xvfb/llvmpipe`, rendu d'une capture 640×360 et vérification PNG (lorsque CI réussie)
- **Les performances Mesa/WARP ne représentent pas les FPS sur l'Intel HD 620** : les tests d'acceptation de 30/40 FPS sont à faire sur le Dell réel

## Limites explicites de ce premier jalon

Ce V17 est un **socle rasterisé interactif**, pas encore le rendu final de la roadmap :
- Pas encore de GI multirebonds temps réel, de SSAO/GTAO, de LOD/occlusion culling ni de réflexions planaires avancées
- Éclairage indirect actuellement approximé par un environnement hémisphérique, **pas par du path tracing**
- Le verre utilise Fresnel/alpha approximatifs, sans réfraction géométrique complète
- Comparaison OpenGL / DirectX 11 à qualité égale **non encore effectuée**
- **40 FPS à 1366×768 non démontrés sur le Dell** ; à mesurer réellement. La qualité de *S.T.A.L.K.E.R.: Call of Pripyat* reste une direction artistique, pas une caractéristique acquise

Voir **[les objectifs V17 détaillés](../docs/V17-OBJECTIFS-TEMPS-REEL-QUALITE-261110.md)** avant les prochaines phases.
