# GPU Compute sans DXR — piste Moteur3D

Statut : **proposition technique, non implémentée et non benchmarkée**.

## Objectif
Ajouter un backend de ray tracing par **Compute Shaders HLSL** indépendant de DXR, tout en conservant le ray tracer CPU comme référence. Cible initiale : **Intel HD Graphics 620** sous Windows avec pilote D3D11 compatible compute shaders (feature level à vérifier à l'exécution).

## Architecture envisagée
- `CPU` : ray tracing logiciel, BVH, SIMD et multithreading ; référence de correction.
- `D3D11 raster` : pipeline raster existant, à conserver pour comparaison.
- `D3D11 compute` : rayons primaires, traversée BVH et intersections sur GPU, puis ombres et shading dans une seconde phase.
- `DXR` : backend facultatif si disponible, sans dépendance obligatoire.

## Étapes
1. Détecter `D3D_FEATURE_LEVEL`, support compute, limites des buffers et ressources, et afficher le motif du fallback.
2. Définir un format GPU stable pour les triangles, matériaux, nœuds BVH et caméra ; vérifier alignement et taille des structures CPU/HLSL.
3. Créer un shader `CSMain` de rayons primaires (8×8 threads initialement), avec garde des limites image.
4. Comparer identifiants d'objets, distances d'intersection et images de normales à la référence CPU sur de petites scènes déterministes.
5. Ajouter albedo, éclairage et ombres, puis comparer les images à tolérance définie.
6. Produire l'image dans une texture GPU directement présentable ; éviter les readbacks GPU→CPU à chaque frame.
7. Benchmarker séparément construction/transfert BVH, dispatch, temps GPU, présentation, CPU, mémoire et FPS.

## Contraintes HD Graphics 620
- Compute D3D11 possible selon pilote et niveau de fonctionnalité ; **aucun DXR matériel**.
- GPU intégré avec mémoire partagée ; capacité et performances à mesurer, surtout pour la scène Pripyat ~1,6 Go.
- Traversée BVH irrégulière : divergence de threads, pression registres et accès mémoire potentiellement limitants.
- Tester 800×600 puis 1920×1080 avant 4K ; réduire scène et matériaux pour le premier prototype.

## Critères d'acceptation
- Fallback CPU automatique et non destructif.
- Tests de parité géométrique CPU/compute reproductibles.
- Mesures distinctes du temps GPU et du temps total image, sans présenter de gain théorique comme acquis.
- Aucun changement du rendu CPU existant lors de l'intégration.

## État du dépôt au moment de la proposition
Le backend `src/m3d_directx11.cpp` utilise déjà vertex/pixel shaders et `Draw()` pour la rasterisation GPU ; `src/m3d_render.cpp` appelle `traceColor()` côté CPU. Le document décrit une **nouvelle voie compute** et ne signifie pas qu'elle existe déjà.
