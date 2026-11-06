# AUDIT COMPLET DES BRANCHES — Moteur3D / Pripyat
Date : 2026-11-06
État : audit de pré-consolidation ; aucune branche supprimée
Base candidate unique : `feature/pripyat-square-v15-beta2`
Build vérifié : GitHub Actions #127, run 42219461811, succès Windows, artifact 13502129826.

## 1. Inventaire GitHub
35 branches observées. Le dépôt comprend plusieurs projets historiques (QBasic/CPC, QEdit, raster/raycasting), qui ne doivent PAS être fusionnés dans le moteur natif Qt par défaut.

### Branches moteur natif / scène
- `feature/pripyat-square-v15-beta2` : référence candidate, fresque, néon, arche, place, UI hybride, build #127 OK.
- `feature/pripyat-catalog-assets-261105` : ancêtre direct de la candidate, **0 commit en avance / 7 en retard** ; les ressources de cette branche sont conservées.
- `fix/native-sharpness-diagnostic-261106` : **21 commits en avance / 43 en retard** (historique divergent). 13 fichiers diffèrent ; **diagnostic netteté absent de Beta2**. Son moteur hybride a été repris partiellement, mais pas toutes les fonctions UI.
- `fix/pripyat-georeference-1to1-261106` : **3 en avance / 10 en retard**. Apporte géoréférencement, routes et documentation ; non fusionné.
- `feature/pripyat-roads-2km-261105` : **2 en avance / 10 en retard**. Réseau routier 2 km, non fusionné.
- `feature/pripyat-topography-261105` : **1 en avance / 10 en retard**. Carte de repères photo, non fusionnée.
- `feature/cpu-pbr-hdr-denoise-2026-11-05` : **1 en avance / 43 en retard**. Comparer les options PBR/HDR/débruitage avant clôture.
- `feature/offline-photo-cpu-dxr-2026-11-04` : **1 en avance / 44 en retard**. Vérifier les interfaces DXR et le rendu hors ligne.
- `feature/reference-material-textures-261104` et `fix/energetik-seat-vinyl-render-261104` : même commit de tête, **7 en avance / 61 en retard**. Comparer les matériaux et textures avant clôture.
- `perf/aabb-early-exit-261103` : ancêtre, 0 en avance / 123 en retard.
- `perf/simd-sibling-leaves-261103` : ancêtre, 0 en avance / 120 en retard.
- `perf/simd-triangle-leaves-261103` : ancêtre, 0 en avance / 121 en retard.
- `experiment/manhattan-silent-hill-2026-10-30` : ancêtre, 0 en avance / 114 en retard.
- `pripiat` : ancêtre, 0 en avance / 42 en retard.
- `repair/pripiat-build-0009` : ancêtre, 0 en avance / 85 en retard.

### Autres branches / projets
- `master` : diverge (2 commits en avance / 388 en retard), ne pas écraser.
- `raster` : diverge (48 en avance / 388 en retard), autre développement historique.
- `raycasting` : diverge (10 en avance / 388 en retard), autre développement historique.
- `feature/zbuffer-primitives` : diverge (5 en avance / 464 en retard).
- `experiment/v9.3-temporal-workers-2026-10-29` : archive d'expérience à préserver.
- Branches `qbasic-cpc6128*` et `qedit*` : projets distincts, conserver en archives ; ne pas mélanger au Qt/C++ natif.

## 2. Audit des composants critiques
| Composant | Base Beta2 | Statut |
|---|---|---|
| Windows Qt 6 / compilation | Build #127 réussi | VALIDÉ en CI |
| Export scènes binaires | Étape CI réussie | VALIDÉ en CI |
| Fresque / néon / arche | Présence testée en CI | Présence des sources validée ; rendu visuel NON vérifié |
| CPU ray tracing / BVH | Sources présentes | À tester sur scènes de référence |
| CPU hybride V3 | Sources et option UI présentes | À comparer, performances auparavant inférieures au ray tracing |
| Diagnostic netteté natif | Absent de Beta2 | **RÉGRESSION IDENTIFIÉE** |
| Géoréférencement routes 2 km | Sur branche séparée | **NON INTÉGRÉ** |
| PBR / HDR / débruitage | Branches divergentes | **AUDIT DE DIFF REQUIS** |
| Matériaux sièges / textures | Branches divergentes | **AUDIT DE DIFF REQUIS** |
| Distance Energetik–Polissya | Géométrie provisoire | NON VALIDÉE HISTORIQUEMENT |
| Place dans rayon de 100 m | Volumes et voirie ajoutés | NON VALIDÉE GÉOGRAPHIQUEMENT |
| Rendu DirectX et illumination | Compilation réussie | Contrôle visuel réel à faire |

## 3. Éléments déjà conservés depuis V15
Comparaison de fichiers avec `fix/native-sharpness-diagnostic-261106` :
- `src/m3d_render.cpp` : contenu identique.
- `src/m3d_post.cpp` : contenu identique.
- `CMakeLists.txt` : contenu identique.
- `src/m3d_shading.cpp` : contenu différent, Beta2 plus volumineux (matériaux et scène), audit ligne par ligne requis.
- `src/moteur3d_qt.cpp` : contenu différent ; l'option `sharpnessDiagnostic` manque dans Beta2.
- `m3d_engine.h` : contenu différent, hybride ajouté ; vérifier signatures et compatibilité.

## 4. Stratégie de consolidation sans perte
1. **Geler un point de référence** : conserver Beta2 build #127 et SHA `f4b8fe2331def5610b0be104fd629f9bf276e416`.
2. Créer une branche unique de développement `integration/moteur3d-unified-v15` depuis Beta2, sans réécrire les branches existantes.
3. Réintroduire le diagnostic netteté avec ses réglages de rendu, UI et tests.
4. Comparer puis reprendre les différences utiles PBR/HDR, DXR, textures et matériaux, fichier par fichier.
5. Importer les routes/topographie **en couche distincte activable**, avec repères d'origine et unités documentés, sans déplacer arbitrairement les bâtiments existants.
6. Ajouter tests de non-régression : scène binaire, matériaux fresque, géométrie néon, lumière toit, archétype hôtel, caméra, exports et benchmark.
7. Compiler Windows + tester les scènes et benchmarks CPU/DirectX/hybride, inspecter les rendus.
8. Promouvoir la branche intégrée comme seule branche active après validation ; archiver (ne pas supprimer) les anciennes.

## 5. Règles pour le futur
- Un seul dossier natif de référence : `experiments/2026-10-30-cote-ete-gt/native_v14/` tant qu'une migration de chemin n'est pas validée.
- Une seule branche active de développement après intégration ; les fonctionnalités passent par commits ou PR sur celle-ci.
- Tout ajout de scène nécessite un export automatique + contrôle d'objets essentiels + build Windows.
- Ne jamais déclarer une scène historiquement fidèle sans contrôle géographique et photographique.
- Les branches historiques restent récupérables ; aucune suppression lors de cette phase.

## 6. Décision
La branche Beta2 est **le meilleur socle compilable vérifié**, mais **pas encore une version totalement consolidée**. L'intégration complète nécessite les vérifications ci-dessus. Ne pas renommer `master` ni fusionner aveuglément les branches divergentes.
