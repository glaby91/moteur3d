# Audit technique global — Moteur3D V15 (Qt, Win32, CPU, DirectX et GPU Compute)

**Date :** 2026-11-06  
**Branche auditée :** `integration/moteur3d-unified-v15`  
**Nature :** revue statique des sources GitHub et de la configuration CI. **Aucune compilation Windows, exécution sur Intel HD 620, mesure de FPS ni validation d'image n'a été réalisée dans cet audit.** Les observations sont des constats de code, pas des résultats d'essais.

## Synthèse et décision

**État global : prototype multi-backend intégré au dépôt, non qualifié pour une diffusion exécutable.**

- Le moteur CPU logiciel est effectivement présent (ray tracing, BVH, multithreading, modes SIMD, post-traitement, moteur hybride). Son comportement sur les scènes et configurations cibles n'est pas couvert par des tests reproductibles dans cet audit.
- Le backend DirectX 11 raster existe, distinct du ray tracing CPU : comparaison de performance non équivalente visuellement.
- Le module DXR détecte certaines capacités mais ne fournit pas encore de backend DXR opérationnel (`dxrBackend=false`).
- Le shader HLSL Compute 5.0 et son pont C++ D3D11 sont présents, et les deux interfaces Qt et Win32 exposent un mode Compute. **Ce mode n'est pas un ray tracer général utilisable sur la scène Pripyat actuelle** : seules les scènes entièrement triangulaires sont acceptées ; sinon repli CPU.
- CMake possède les cibles `moteur3d_qt` et `moteur3d_win32` (Windows). Le workflow GitHub Actions Windows a été modifié pour vérifier et empaqueter les deux exécutables, mais aucune réussite de build n'a été vérifiée.

## Constats classés par priorité

### P0 — Blocages de livraison

1. **Compilation et exécution non attestées.** Vérifier la dernière exécution du workflow `.github/workflows/build-moteur3d-v14-qt.yml`, ses journaux et son artifact. Le fait que le YAML existe ne garantit pas qu'un binaire ait été produit.
2. **Compute incompatible avec les scènes contenant des primitives non triangulaires.** `src/m3d_compute11.cpp` rejette explicitement tout `Obj.kind != Kind::Triangle` et déclenche un repli CPU. Il faut soit implémenter toutes les primitives dans le shader, soit convertir/tesseller la scène avec contrôle de fidélité. Ne pas annoncer « Pripyat en ray tracing GPU » avant cela.
3. **Shader HLSL non testé sur HD 620.** Compiler `CSMain` en `cs_5_0`, vérifier les diagnostics HLSL et réaliser un essai sur l'adaptateur Intel cible.
4. **Absence de rendu matériel complet en Compute.** Le shader produit une visualisation de normales et une couleur de fond ; pas de textures, matériaux PBR, éclairage, ombres, transparence ni réflexions comparables au CPU.

### P1 — Robustesse, qualité et performances

5. **Création GPU répétée à chaque image.** `renderCompute11Preview` recrée device/contexte, shader, buffers BVH, textures et staging puis compile le HLSL à chaque appel. Cette architecture est très coûteuse pour la navigation interactive. Mettre en cache le device, le shader, les ressources persistantes et ne réimporter le BVH qu'au changement de scène.
6. **Lecture GPU→CPU bloquante.** `CopyResource` suivi de `Map(D3D11_MAP_READ)` impose une synchronisation ; mesurer séparément temps d'upload, dispatch et readback. Envisager une présentation GPU native ou un staging asynchrone.
7. **Traversal BVH borné à une pile de 64 entrées.** Le shader affiche du magenta en cas de dépassement. Tester des scènes profondes et prévoir une construction BVH adaptée ou une stratégie sans perte de géométrie.
8. **Protection incomplète des indices dans le shader.** `PrimitiveOrder[node.first+i]` ne teste pas explicitement la taille du buffer d'ordre. Le chargeur de scène vérifie les plages, mais le contrat de buffer devrait être formalisé côté GPU et couvert par des tests.
9. **Contrôle insuffisant de l'équivalence visuelle.** CPU, DX11 raster, hybride et Compute n'utilisent pas encore les mêmes modèles de rendu. Comparer des temps sans indiquer le niveau de qualité serait trompeur.
10. **Limites mémoire HD 620.** La scène et le BVH sont copiés vers des buffers GPU et la sortie est une texture `R32G32B32A32_FLOAT` (16 octets/pixel), plus une texture staging de même taille. À 4K, ces deux textures seules représentent environ 253 Mio (3840×2160×16×2), hors géométrie, pilotes et textures. Prévoir une sortie `R8G8B8A8_UNORM` ou un autre format compact si possible.
11. **Gestion d'erreur shader peu exploitable.** `D3DCompileFromFile` récupère `diagnostics` mais ne les restitue pas dans le message d'erreur. Les logs doivent montrer l'erreur HLSL et le chemin réellement chargé.
12. **Chemin shader dépendant du dossier de l'exécutable.** Vérifier l'archive distribuée : `shaders/m3d_raytrace_compute.hlsl` doit être présent à côté des exécutables dans la structure attendue.
13. **UI Win32 : position du sélecteur à y=740.** Sur fenêtre de 900 px de haut, le contrôle est bas dans le panneau et peut être masqué à des résolutions inférieures ; ajouter une mise en page adaptative/défilement. Qt possède son propre panneau.
14. **Mode Auto Qt : DXR annoncé conditionnel.** L'interface doit expliquer clairement que la détection matérielle DXR ne signifie pas que le backend DXR fonctionne.

### P2 — Maintenance et industrialisation

15. **Deux interfaces, même moteur :** factoriser la liste des capacités et les messages d'erreur afin d'éviter la divergence Qt/Win32.
16. **Benchmark :** ajouter mesures séparées CPU, DX11 raster, Compute et upload/readback, médiane de plusieurs essais, configuration matérielle et comparaison à qualité équivalente uniquement.
17. **Tests automatisés :** tests de chargement BVH, triangles, boîtes/capsules/cônes/ellipsoïdes, images de référence, scènes de stress, repli CPU et absence de fuite mémoire.
18. **CI :** conserver les journaux de build, exiger un succès Windows x64, conserver les artifacts et inclure un manifeste (commit, branche, numéro de run, compilateur, version Qt, checksum).
19. **Packaging :** vérifier les DLL Qt via `windeployqt`, les deux EXE, le shader, les scènes et les ressources avant publication.
20. **Documentation :** mettre à jour les commentaires du shader qui le décrivent encore comme « not yet wired to Qt », alors qu'un chemin d'appel C++ a été ajouté.

## Matrice des backends

| Backend | Source présente | Interface Qt | Interface Win32 | Qualité complète vérifiée |
|---|---|---|---|---|
| CPU ray tracing | Oui | Oui | Oui | Non testée ici |
| CPU hybride V3 | Oui | Oui | Non exposé dans le sélecteur Win32 | Non testée ici |
| DirectX 11 raster | Oui | Oui | Oui | Non testée ici |
| D3D11 Compute 5.0 | Prototype | Oui | Oui | Non : normales/triangles |
| DXR matériel | Détection uniquement | Mode Auto / repli CPU | Non | Non opérationnel |

## Points positifs vérifiés dans les sources

- Le chargeur de scène `src/m3d_scene.cpp` contrôle notamment les limites, la couverture des objets, les plages des feuilles et les cycles du BVH.
- Le moteur possède des rendus CPU distincts, un mode hybride et des modules SIMD AVX2.
- Le backend GPU Compute vérifie la disponibilité de D3D11 Feature Level 11.0 et retourne une erreur permettant un repli CPU.
- Le shader Compute signale les dépassements de pile par une couleur magenta plutôt que de masquer silencieusement l'erreur.
- Le workflow Windows utilise MSVC/Qt et inclut des vérifications explicites de présence des deux exécutables et du shader.
- Le shader est inclus dans la cible d'installation CMake.

## Plan d'action recommandé

**Étape A — CI bloquante :** obtenir un build Windows réussi des deux EXE ; corriger toutes les erreurs C++/HLSL, vérifier l'artifact et son démarrage.  
**Étape B — Validation GPU :** essai HD 620 sur scène triangulaire minimale, comparer image GPU/CPU et mesurer temps total et temps GPU.  
**Étape C — Géométrie complète :** gérer toutes les primitives de Pripyat, valider la BVH et les transformations.  
**Étape D — Rendu :** éclairage, matériaux, textures, ombres, transparence, reflets et colorimétrie ; tests de référence.  
**Étape E — Optimisation :** ressources GPU persistantes, caches, formats mémoire, lecture asynchrone, benchmarks 720p/1080p/4K.  
**Étape F — Publication :** archive reproductible Qt+Win32 avec manifeste, scène et shader ; tests de non-régression.

## Périmètre réellement inspecté

`m3d_engine.h`, `CMakeLists.txt`, `src/m3d_scene.cpp`, `src/m3d_render.cpp`, `src/m3d_hybrid.cpp`, `src/m3d_directx11.cpp`, `src/m3d_dxr.cpp`, `src/m3d_compute11.cpp`, `src/shaders/m3d_raytrace_compute.hlsl`, `src/moteur3d_qt.cpp`, `src/moteur3d_win32.cpp`, `.github/workflows/build-moteur3d-v14-qt.yml`.

**Limite de l'expression « audit total » :** l'audit couvre l'architecture et les chemins critiques du moteur et de la livraison, mais pas une revue ligne à ligne de toutes les branches, ressources et scènes historiques du dépôt. Aucune conformité graphique, absence de défaut ni performance cible ne peut être certifiée sans compilation et exécution.
