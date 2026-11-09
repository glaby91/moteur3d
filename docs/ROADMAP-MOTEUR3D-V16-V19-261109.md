# Moteur3D — Feuille de route V16 à V19
Date de planification : 2026-11-09. Statut : **proposé / non implémenté**.
Les dates sont indicatives, conditionnées par l'audit des branches, les ressources et la validation des tests.

## Principes
- Un cœur de scène indépendant des backends graphiques.
- Voie A : ray tracing logiciel CPU multithread et GPU compute HLSL, repli CPU sans DXR.
- Voie B : rasterisation temps réel DirectX 12, DXR facultatif.
- Assets indépendants des scènes : objets, UV, matériaux PBR, textures sources, mipmaps en cache, LOD, identifiants stables.
- Lumières décrites dans les scènes, calculées par les renderers.
- Export glTF/GLB et importeur Unreal Engine 5 versionné; ne pas supposer la parité visuelle automatique.
- Compatibilité avec les scènes existantes (Pripiat, Rapture, benchmarks); ne pas supprimer les anciens formats avant migration validée.

## V16 — Architecture fondamentale (9 novembre – 4 décembre 2026)
1. Inventorier branches, builds, sources et données réelles; établir la baseline de performance et de rendu.
2. Définir contrats : scene graph, entités/composants, transformations, coordonnées, unités, identifiants d'assets.
3. Créer Asset Manager : partage des géométries/textures, chargement, cache mipmaps, références et durée de vie.
4. Séparer scenes/, assets/, engine/, app/; fournir convertisseur de scène et tests de régression.
5. Définir interfaces RenderBackend A et B, puis conserver un chemin de rendu fonctionnel.
6. Vérifier build Windows, lancement Qt et chargement de Pripiat.
**Critère de sortie :** même scène chargée sans duplication d'assets ni recompilation du cœur; build reproductible.

## V17 — Rendu et performances (7 décembre 2026 – 15 janvier 2027)
1. BVH/accélération CPU, instanciation, culling et LOD.
2. Valider compute HLSL (compatibilité GPU, repli CPU), pipeline DX12 raster et DXR optionnel.
3. PBR, lumières/ombres, HDR, anti-aliasing, correction du flou et cohérence colorimétrique.
4. Navigation B puis accumulation progressive A à caméra immobile, sans blocage de l'UI.
5. Profiling détaillé et comparatifs mêmes caméra/scène/résolution.
**Critère de sortie :** rendus reproductibles, performances instrumentées, absence de régression majeure.

## V18 — Outils et simulation (18 janvier – 26 février 2027)
1. Éditeur Qt : hiérarchie, inspecteur, gizmos, matériaux, éclairages, sauvegarde.
2. Animation, collisions et physique minimale découplées du renderer.
3. Atmosphère, cycle jour/nuit et météo paramétrables.
4. Streaming de zones et budgets mémoire.
5. Export glTF/GLB + manifest d'instances, importeur Unreal Engine 5 testé sur version cible.
**Critère de sortie :** modification et sauvegarde d'une scène, export/réimport vérifié dans UE5.

## V19 — Stabilisation (1 – 26 mars 2027)
1. Tests automatiques, captures de référence, non-régression CPU/GPU.
2. Mesure mémoire, chargement, stabilité longue durée, scènes complexes.
3. Documentation API, formats, pipeline assets et procédures de build.
4. Packaging Windows et Linux selon backends supportés (DX12 Windows).
5. Livrables et démonstrations Pripiat/Rapture.
**Critère de sortie :** builds reproductibles, documentation, tests et limites connues publiées.

## Ordre d'exécution immédiat
- P0 : audit des branches et du code, état réel des backends, inventaire des assets.
- P1 : spécification des formats et interfaces, migration d'une petite scène de test.
- P2 : migration Pripiat et validation de non-régression.
- P3 : développement parallèle des deux backends seulement après stabilisation du cœur.

## Risques
- Divergence entre branches historiques et builds.
- Matériaux non équivalents entre CPU/Compute/DX12/UE5.
- Compute limité par certains GPU/pilotes.
- Coût mémoire des textures 2048² et mipmaps.
- Export UE5 nécessitant conversion des lumières, instances, collisions et matériaux.

Aucun de ces jalons n'est réputé terminé sans commit, build et tests vérifiés.
