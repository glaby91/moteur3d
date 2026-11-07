# OpenGL — possibilité étudiée, développement gelé

**Décision : 7 novembre 2026**  
**État : PARKING LOT / NON PLANIFIÉ**  
**Décision utilisateur : trop de backends pour le moment.**

## Décision de périmètre

Aucun nouveau backend OpenGL n'est à développer actuellement.

Les priorités restent :
- **CPU** : ray tracing logiciel de référence, fidélité du rendu et repli.
- **D3D11 Compute (HLSL)** : exploitation et optimisation mesurée du GPU sans DXR sur Windows, notamment Intel HD Graphics 620.
- **D3D11 raster déjà présent** : préserver son fonctionnement existant, sans lancer de nouveau chantier.
- **GPU benchmark** : FPS, médiane, P95, mesures WDDM, coût GPU du shader et lecture GPU→CPU.
- **Pripyat** : géométrie, fidélité visuelle et matériaux.

**Cette note n'autorise aucun développement OpenGL, aucune activation de fonctionnalité ni aucune modification du CMake ou de l'interface.**

## Possibilité technique à réexaminer seulement sur demande

Si un jour une justification technique réelle apparaît, évaluer **un seul** chemin OpenGL en premier (ne pas engager simultanément Compute et raster) :

1. **OpenGL Compute (GLSL 4.30+)** : portage éventuel du ray tracing HLSL, du BVH et du PBR vers un backend portable. Version OpenGL effective à interroger sur le GPU et son pilote au lancement, sans supposer la présence de Compute.
2. **OpenGL raster** : option séparée, qui n'offre pas les mêmes effets que le ray tracing ; ne pas confondre gain de fluidité et rendu de qualité équivalente.
3. **Qt** : examiner une texture GPU présentée par un contexte OpenGL au lieu d'un aller-retour GPU→CPU ; vérifier l'impact sur le mode deux fenêtres, les fils Qt et les contraintes de contexte.
4. **Compatibilité** : Windows et Linux, capacités OpenGL/GLSL réelles et repli CPU. Ne pas déclarer la compatibilité HD 620 validée sans essai matériel.
5. **Budget de complexité** : coûts de maintenance de deux langages shaders (HLSL/GLSL), duplication des tests, compilation des drivers, parité des images.

## Critères avant tout dégel

- Les limitations actuelles de D3D11 Compute sont quantifiées, pas seulement supposées.
- Le coût de **lecture GPU→CPU**, de calcul du shader et les FPS sont mesurés sur le même cadrage de Pripyat.
- Le besoin OpenGL (portabilité Linux, meilleur mode de présentation, etc.) est explicitement priorisé.
- Un prototype *isolé*, indépendant du moteur livré, passe des tests de pixels et de rendu comparables aux références CPU/D3D11.
- Aucun remplacement des chemins existants ni activation par défaut sans validation de l'utilisateur.

## Interdictions actuelles

- Pas de fichier `m3d_opengl.cpp`, pas de shader GLSL, pas de `QOpenGLWidget` dans la build.
- Pas de modification de `CMakeLists.txt` pour ajouter des modules OpenGL.
- Pas de nouveau choix de renderer dans l'interface.
- Pas de chiffres de performance inventés ou de promesse de gain.

**Conclusion : sujet consigné sur GitHub, mais officiellement gelé. Le moteur actuel reste la seule priorité d'optimisation.**
