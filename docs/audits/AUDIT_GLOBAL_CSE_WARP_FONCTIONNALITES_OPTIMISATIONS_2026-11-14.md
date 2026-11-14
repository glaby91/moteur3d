# CSE WARP BetaX — Audit global des fonctionnalités et des pistes d'optimisation

**Date :** 14 novembre 2026  
**Référence examinée :** dépôt [`glaby91/moteur3d`](https://github.com/glaby91/moteur3d), branche `master` fusionnée, moteurs historiques V9–V17 et rapports expérimentaux disponibles.  
**Méthode :** revue statique des sources et rapports GitHub, des résultats HD620 documentés et de l'état des workflows ; **pas de benchmark exécuté localement**, pas de mesure nouvelle ni de modification de code.

## Synthèse décisionnelle

- **Moteur interactif : DirectX11 Raster.** Le CPU ray tracing/GI est un étalon de comparaison, pas le mode de jeu cible ; D3D11 Compute est un second backend de recherche plus coûteux sur plusieurs scènes. Aucun backend DXR matériel opérationnel.
- **Gain matériel le plus solide : Z-prepass.** Sur Intel HD620, 1366×768, MSAA2, trajet de 90 images, GPU draw **64,4117 ms sans** contre **39,9421 / 41,1802 ms avec** ; temps moyen complet **72,5827** contre **48,3413 / 47,8654 ms**. Trois PNG de contrôle par campagne étaient identiques. Résultat ciblé, pas preuve toutes caméras.
- **Rendements insuffisants ou décevants :** 8 threads CPU au lieu de 4 (~2% seulement) ; feuilles BVH élargies à 8 triangles pour AVX2 (aucune accélération convaincante, +~33% de tests d'objets) ; triple buffering (+3,84% de débit moyen mais latence P50 accrue de 35,60 à 91,58 ms) ; CPU GI 64 spp toujours granuleux ; multirésolution périphérique ~12% sur WARP logiciel dans une variante avec artefacts (< seuil de 15%).
- **Améliorations de qualité ou de robustesse ≠ gains de FPS :** PBR 2048, mipmaps, éclairage linéaire/GGX, reflets, verre, sondes GI, table précalculée, cache PBR, outils de benchmark, Photo2Scene et contrôles d'exploration.
- **Principal coût GPU confirmé en matériel :** environ **64,4 ms de GPU Draw** sur la référence pré-Z ; le `Map` CPU d'environ 66 ms attend en grande partie ce travail, **ne pas additionner les deux temps**. Le Z-prepass abaisse le Draw à ~40,6 ms. Cible supplémentaire de 20–25 ms = objectif non réalisé.

## 1. État des fonctionnalités

| Domaine | Fonctions réellement présentes | État et réserves |
|---|---|---|
| CPU logiciel | BVH, ray tracing, triangles/boîtes/capsules/cônes/ellipsoïdes, ombres solaires, réflexions/réfraction simplifiées, PBR, mipmaps, depth-of-field, FSAA/AA contours, tuiles multithread, AVX2 partiel, GI/path tracing HDR et débruitage bilatéral | Fonctionne comme référence ; trop lent pour l'exploration ; LOD des triangles insuffisamment lié aux UV réels, fresque de piscine floue en CPU max ([issue #66](https://github.com/glaby91/moteur3d/issues/66)) |
| D3D11 Raster | Pipeline GPU avec vertex/pixel shaders, z-buffer, PBR diffuse/normal/roughness, MSAA, filtrage anisotrope, culling, instancing d'éléments, LOD distance, sun/shadows, eau/verre, lampes, transparence, mode Z-prepass | Cœur interactif de V17 ; fonctionnement validé sous Microsoft WARP, mesures réelles HD620 encore loin de 25 ms/image |
| D3D11 Compute | Compute Shader HLSL, BVH ray traversal, ombres, atlas PBR, rayons secondaires GI, GI à résolution réduite, caches | Fonctionnalité réelle mais backend secondaire ; traçage limité aux scènes entièrement triangulaires selon code, plus lent que Raster sur les essais cités ; réparer et tester son périmètre avant revendication de parité |
| DXR D3D12 | Détection du support matériel | **Pas de moteur DXR** : `dxrBackend=false` dans `src/m3d_dxr.cpp` |
| OpenGL | Prototype distinct Qt `QOpenGLWidget` / GLSL 3.3, rendu sous Mesa et pipeline GPU→GPU | **PR expérimentale #22**, non preuve d'un backend OpenGL intégré à la version `master` ni de gains HD620 |
| Qualité d'image | 2048×2048 PBR, chaînes mipmap couleur/normale/rugosité, GGX/Schlick, ombres, lumières locales, profil de correction colorimétrique, reflets piscine, filtres de post-traitement | Bonne base de qualité, mais GI approximative, transparence, fresques, ciel, luminosité et géométrie exigent revue visuelle. Les modes CPU/GPU ne sont pas photométriquement équivalents |
| GI indirecte raster | Volume de sondes 3D, 25 états horaires et interpolation ; exécutable de bake CPU, cache `.m3dgi` et deux textures 3D | Implémenté et testé avec WARP, mais approximatif, lumière parfois trop forte ; pas de gain de FPS confirmé sur HD620 et risque de fuites par murs fins |
| Scènes | Pavillon, atelier de matériaux, Photo2Scene proof, bibliothèque Pripyat/Energetik, complexe civique/piscine, fresque intérieure, monument, végétation, enseigne « БАССЕЙН » | La scène approuvée (186 matériaux, 183 ensembles PBR, 82 images et radio) est encore restaurée depuis un ancien artefact CI. Les arbres GLB importés avant cette restauration peuvent être effacés |
| Exploration Qt | Caméra libre ZQSD/flèches AZERTY, souris, marche/collisions/escaliers/échelles de piscine, vol vertical, Maj ×3, lunettes ×3, F11 plein écran, FPS, angles/caméras, mode anomalie PNG+JSON, radio XAudio2 spatialisée sur 4 HP, soleil et ciel | Fonctions présentes ou couvertes par des tests ciblés, mais les performances de l'UI réelle ne se déduisent pas du débit de benchmark. Le rafraîchissement du ciel n'est pas un FPS de géométrie |
| Benchmark/qualité | Timestamps GPU DX11, CPU submit/Map/convert, 1–3 buffers, métriques frametime P50/P95/P99, CSV/JSON/HTML/PNG, CI Windows WARP, tests de scène/qualité et total simulation | Outils solides pour diagnostiquer ; certains tests basés sur `assert()` deviennent inopérants avec `NDEBUG` en Release ([issue #68](https://github.com/glaby91/moteur3d/issues/68)) |
| Photo2Scene | Points de fuite, multivues, nuage non métrique, projections de photos vers des plans GLB, preuve de géométrie et essais Gaussian Splatting CPU hors moteur | **Outils/prototypes** de reconstruction ; pas de preuve d'atlas métrique entièrement fidèle ni d'accélération temps réel du moteur |
| Physique | Contrat et navigation/collisions personnalisées | Pas de simulation physique PhysX active (`physXAvailable=false`) |
| Présentation finale | Qt affiche encore un `QImage` issu du readback GPU→CPU | **Direct GPU Present non implémenté** ; limitation structurante du pipeline de présentation |

## 2. Essais avec gain insuffisant ou négatif

### A. CPU multithreading — plafond pratique
Benchmark Rapture initial, FHD et trois répétitions après échauffement : 1 thread **21 144,6 ms** total ; 2 threads **11 327,4 ms** ; 4 threads **6 102,6 ms** ; 8 threads **5 983,2 ms**. Ajouter quatre threads au-delà de 4 n'a diminué le temps total que de **1,96%** sur la machine test. Conclusion : conserver un nombre de travailleurs en rapport avec les cœurs/threads physiques, pas multiplier aveuglément les threads.

### B. AVX2/FMA3 — vectorisation sans gain global
Noyaux disponibles : génération de rayons par 8, triangles homogènes, AA contours, interpolation RGB565, accumulation de lumières par groupes et essais BVH. Les premières mesures ont signalé un **ralentissement global** en mode SIMD sur i5-7200U. La vectorisation des 2 boîtes enfants suivie d'extraction scalaire a été suspectée de coûter plus qu'elle ne rapporte. Le build 0021 a obtenu **12–16%** sur un relevé en court-circuitant les axes AABB rapidement (gain algorithmique scalaire, pas preuve d'une supériorité SIMD). En 0022, rendre accessibles les paquets de 8 triangles a entraîné ~**+33%** de tests d'objets et **aucune accélération convaincante**. La variante 0023 par feuilles-sœurs reste à prouver par timing matériel, au-delà de la seule présence de paquets SIMD.

### C. Triple buffering / lecture asynchrone — amélioration de débit, latence pénalisante
Sur campagne utilisateur V17.281 : 6 scènes, 2 caméras, 2 résolutions, 3 profondeurs, 72 cas, 5 184 images. Trois buffers vs un : **+3,84%** de débit arithmétique moyen (médian +4,13%), mais latence médiane par cas **35,60 → 91,58 ms** ; médianes de débit 26,79 → 28,02 FPS. Les FPS sont du **pipeline**, pas de l'écran Qt. **Ne pas passer le mode interactif en triple buffering par défaut** sans test de réactivité ; le chemin 1 buffer reste une référence légitime.

### D. CPU GI et suréchantillonnage — qualité n'augmentant pas à hauteur du coût
Pavillon 01 FHD : CPU 1 spp **4 025,9 ms**, 4 spp **15 866,1 ms**, 2 rebonds 16 spp débruités **94 911,4 ms**. À 64 spp le rendu restait perçu comme granuleux/artificiel, sans durée finale mesurée. De plus, le filtre bilatéral 5×5 intervient sur du 8 bits déjà tonemappé, sans guides de normale, albédo ni profondeur ; flou notable sur fresques. **Augmenter spp n'a pas réglé l'apparence**. Le CPU doit rester le comparateur.

### E. GI hybride v17.308–.310 — valeur visuelle à revoir, pas accélération mesurée
Le premier éclairage de sondes a éclairci excessivement façades et ombres. Deux seules captures mesuraient **81,2 ms avec** contre **76,9 ms sans** ; elles ne prouvent pas une régression répétable, mais ne montrent aucun gain. Le tone mapping supplémentaire relevait à tort les demi-teintes, puis a été séparé et corrigé. Les sondes et tables GI restent des outils de rendu, **pas une optimisation des FPS démontrée**.

### F. Multirésolution périphérique — seuil de gain manqué et artefacts
V17.444 variante rectangulaire : WARP logiciel 640×360, piscine **280,136 → 246,770 ms** (-11,91%), vue latérale **376,518 → 331,195 ms** (-12,04%). Le protocole exigeait **≥15%** de gain matériel **et** qualité acceptable. L'ancien masque produisait des bandes/contours ; une variante elliptique a été codée pour les adoucir, **sans benchmark Intel HD620 validé**. État : prototype optionnel, *pas* optimisation acquise ; pas de reprojection temporelle et la géométrie est dessinée en deux passes.

### G. Suréchantillonnage / anti-crénelage
SSAA 4× a été retiré du choix interactif CPU en faveur d'un AA léger sur contours et du FSAA de comparaison. Le filtre léger prend ~**2 ms** à 960×540 sur un test historique ; il peut aussi adoucir des motifs contrastés, n'améliore pas le calcul du moteur et n'est pas comparable à un vrai MSAA géométrique GPU. Augmenter MSAA vers 8× peut au contraire coûter davantage sur HD620. Le bon réglage est un compromis visuel explicite.

### H. Compute GI adaptative — gain WARP réel mais pas celui du moteur principal
Sur WARP logiciel Full HD, le passage indirect half-res de Build336 vers Build341 est passé de **5 068 ms à 2 513 ms**, différence moyenne CPU GI 4 spp pratiquement stable (12,409 → 12,369 RGB/255). C'est un gain **sur Compute/WARP**, sans validation identique sur la HD620, et ne rend pas Compute préférable au raster interactif ; une comparaison plus ancienne sur scène extrême indiquait Raster **65,3 ms** à chaud contre Compute **341,5 ms**, chemins visuellement différents.

### I. Chargement et préparation PBR
Le pavillon V17.281 mettait ~**66,25 s** à préparer géométrie/PBR sur le PC test et le processus atteignait un pic **2 307 Mio** ; 94% de RAM système indiqués. Le parallélisme PBR ×8 et le cache disque PBR (v17.298 et .307) sont opérationnels et vérifiés bit-à-bit dans CI, **mais aucun gain exact de démarrage/rechargement sur HD620 n'est consigné**. Les coûts de première écriture du cache peuvent même augmenter le temps initial.

## 3. Pistes étudiées sans preuve suffisante de gain matériel

| Piste | Ce qui existe / a été testé | Pourquoi ne pas la considérer comme gagnante |
|---|---|---|
| Optimisation pixel shader V17.271 | Code HLSL optimisé + test de parité WARP | Parité de pixels ≠ mesure répétée d'accélération GPU HD620 |
| Suppression ombres PCF 9 taps ou normal/roughness maps | Campagnes de diagnostic « coût maximal » / variantes sans effet | Retirer un effet change la qualité ; ce n'est pas une optimisation « à image égale » |
| 171 lumières / limiter influence | Études 0/16/48/171, AABB exact, frustum CPU, tuiles 32×32 et masques de 256 lumières | Prototypes/PR séparées ; pas de séries HD620 avec gain stable et parité validée dans les documents consultés |
| Instancing / culling / LOD plus agressifs | Actifs pour certaines boîtes/ellipsoïdes/canopées, compteurs de triangles | Pas de gain FPS global HD620 isolé et répétable publié ; le culling trop grossier et l'ordre des lots peuvent réduire son effet |
| Direct GPU Present Qt/DXGI | Architecture proposée et issue #25 | **Pas encore implémenté**. Sa promesse ne doit pas être assimilée à 66 ms économisés : `Map` attend le GPU, et GPU Draw reste dominant |
| Quart de rayons primaires | Option `M3D_PRIMARY_RAYS_25=1`, traces CPU/Compute sur 25% des pixels puis duplication 2×2 | 25% de rayons primaires ≠ 4× FPS ; blocs visibles, qualité insuffisante ; prototype adaptatif PHOTON-RT sur PNG hors moteur |
| Gaussian Splatting CPU / OpenGL alternatif | Prototypes de recherche et tests d'images | Pas de moteur interchangeable et performant validé à qualité égale sur HD620 |
| Baked GI 864 sondes ×25 heures | Calcul hors ligne, chargement de deux volumes 3D GPU, test d'intégrité WARP | Vise surtout stabilité/coût de l'éclairage indirect, pas de FPS mesurés sur Dell ; fuite d'énergie possible entre pièces |
| Variantes SIMD BVH feuilles-sœurs | Passage au noyau réellement accessible et test de sortie | Temps d'image et gain net final non confirmés dans les rapports consultés |
| 30 Hz ciel Qt, overlay FPS | Ciel animé indépendamment des textures de la scène, compteur FPS de présentation | L'animation du ciel ne transforme pas le moteur 3D à 30 FPS ; risque de recalcul 3D à l'arrêt, échantillonnage des nuages sans bilinéaire |
| FPGA/DXR matériel/PhysX | Plans, détecteurs, interfaces | Aucun de ces accélérateurs/moteurs n'est aujourd'hui implémenté pour le rendu ou la physique générale du master |

## 4. Les gains confirmés et les progrès réels à conserver

1. **Z-prepass (HD620 réel) :** 64,41 → ~40,56 ms GPU Draw sur trajectoire; 72,58 → ~48 ms/image calculée. Conserver l'interrupteur A/B et vérifier plus d'angles, transparence et captures.
2. **AABB CPU early-out :** ~12–16% constaté sur relevé Rapture, utile pour référence CPU sans sacrifier la qualité ; moins pertinent pour le backend interactif GPU.
3. **GI Compute half-res sur WARP :** coût d'une partie du Compute divisé environ par deux, utile comme recherche, sans extrapolation au moteur raster matériel.
4. **Raster D3D11 contre Compute :** la différence de temps est importante pour l'usage interactif sur scène extrême (65,3 vs 341,5 ms dans une comparaison historique), mais ce ne sont pas deux images équivalentes et il ne faut pas appeler cela une optimisation du même algorithme.
5. **Mesure elle-même :** GPU timestamps, validité des requêtes, P50/P95/P99, vidéo/CSV/PNG, A/B par caméra, comparaisons SHA256, segmentation entre Draw, copie, Map et conversion ; c'est l'amélioration méthodologique qui permet de cesser les compilations à l'aveugle.
6. **Qualité PBR et fonctionnalités de navigation :** progrès fonctionnels et visuels réels, à préserver même si leur mise en œuvre n'augmente pas les FPS.

## 5. Diagnostic des goulots confirmés

**Mesure de référence du 13 novembre, GPU matériel Intel HD620** : 90 images en 1366×768 MSAA2, environ **695 215 triangles soumis/image**, GPU Draw **64,4117 ms**, moyenne GPU Copy **1,3328 ms**, CPU Submit **1,6796 ms**, CPU Pixel Convert **4,609 ms**, CPU Map Wait **66,286 ms**, GPU occupation ~84,5%. L'attente CPU Map recouvre le travail GPU, et ne s'additionne pas au Draw. Avec Z-prepass, Draw ~40,6 ms : il reste **~20–25 ms GPU de plus à récupérer** pour approcher la cible de 25 ms/image complète, mais ce gain n'est pas acquis.

Le shader exact (PCF ombres, lumières locales, PBR/normal map, transparence, vertex) n'est pas encore désigné par un profil matériel reproduit avec images identiques. Aucune preuve qu'abaisser le nombre de triangles seul ait le rendement espéré à qualité constante. Le budget mémoire et la préparation PBR sont des problèmes de chargement séparés de la latence de chaque frame.

## 6. Recommandation technique de poursuite

**P0 fiabilité avant « stable »** : figer en dépôt durable la scène approuvée 186 matériaux / 82 textures et audio avec SHA et licences (issue #67) ; CI Windows master vert sur le commit effectivement fusionné ; corriger tests Release basés sur assert (#68). Le build V17.494 master était annulé lors de la consultation ; V17.495 sur branche candidate encore en cours à cette date. Ne pas dire « release stable » sans ces contrôles.

**P1 GPU HD620, à image et matériel constants** : prendre `master` + son premier paquet vert, scène V17.493, caméra/heure identiques, 1366×768 MSAA2 PBR 2048. Instrumenter explicitement l'état du Z-prepass, comparer 90 images ×3 répétitions pour plusieurs caméras (extérieur, piscine, fresque, verrière). Chercher le coût **GPU Draw**, pas « CPU Map Wait » pris isolément.

**P1 optimisation ciblée** : établir un profil A/B de coût du PCF 9 taps, de la boucle des lumières, du nombre de fragments transparents, du normal mapping et de la géométrie. Évaluer rejet exact des lumières et éventuel tiled light seulement si les images sont identiques et le gain ≥10% dans plusieurs scènes. Conserver anciens shaders et interrupteurs de retour arrière.

**P2 présentation et chargement** : prototype Direct GPU Present, mesurer FPS réellement affichés et latence des commandes; mesurer chargements froid/chaud PBR et RAM séparément. Ne pas confondre suppression d'attente Map et accélération du Draw GPU.

**P3 sans urgence** : LOD CPU/fresques, GI CPU 64 spp, multi-backends alternatifs, DXR/FPGA/PhysX. Ne pas réécrire le moteur ni multiplier des builds sans hypothèse testable, image de référence et protocole comparatif.

## 7. Index des preuves (sources de première main)

- [CPU : benchmark, AVX2, BVH et feuilles SIMD](https://github.com/glaby91/moteur3d/blob/master/experiments/2026-10-30-cote-ete-gt/native_v14/docs/BENCHMARK_CPU.md)
- [CPU BVH point de départ 1–8 threads](https://github.com/glaby91/moteur3d/blob/master/experiments/2026-10-30-cote-ete-gt/native_v14/docs/OPTIMISATION-BVH-261102-0006.md)
- [V16 : GPU GI adaptative et benchmarks WARP](https://github.com/glaby91/moteur3d/blob/master/docs/V16-PAVILLON01-GPU-GI-ADAPTATIVE-261110.md)
- [V16 : grain GI 64 spp](https://github.com/glaby91/moteur3d/blob/master/docs/V16-PAVILLON01-AUDIT-GRAIN-TEMPS-261110.md)
- [V17.281 : 5 184 images / comparaison de buffers](https://github.com/glaby91/moteur3d/blob/master/v17/docs/BENCHMARK_CAMPAIGN_2026-11-10_DELL.md)
- [V17.298 : PBR parallel et Compute mipmap](https://github.com/glaby91/moteur3d/blob/master/docs/V17_OPTIMISATION_PBR_COMPUTE_2026-11-10.md)
- [V17.307 : cache PBR et upload d'ombres](https://github.com/glaby91/moteur3d/blob/master/v17/docs/V17_307_LOSSLESS_PBR_SHADOW.md)
- [V17.308/310 : GI et correction d'exposition](https://github.com/glaby91/moteur3d/blob/master/v17/docs/V17_310_BALANCED_GI_TONEMAP.md)
- [V17.312 : tables GI par heure](https://github.com/glaby91/moteur3d/blob/master/v17/docs/V17_312_BAKED_GI_TABLES.md)
- [V17.432 : performance Intel HD620 sans Z](https://github.com/glaby91/moteur3d/blob/master/docs/performance/V17.432-HD620-GPU-D3D11-TRAJET-6S-20261113.md)
- [V17.432 : A/B Z-prepass HD620](https://github.com/glaby91/moteur3d/blob/master/docs/performance/V17.432-Z-PREPASS-RESULTATS-HD620-20261113.md)
- [V17.444 : multirésolution périphérique expérimentale](https://github.com/glaby91/moteur3d/blob/master/docs/performance/V17.444-PERIPHERAL-MULTIRES-DX11-EXPERIMENT.md)
- [V17 : roadmap et limites du Raster](https://github.com/glaby91/moteur3d/blob/master/v17/docs/RASTER_PERFORMANCE_ROADMAP.md)
- [V17.493 : audit promotion master](https://github.com/glaby91/moteur3d/blob/master/docs/V17_493_MASTER_AUDIT_2026-11-14.md)
- [Issue #25, Direct GPU Present](https://github.com/glaby91/moteur3d/issues/25) ; [#50, coût GPU restant](https://github.com/glaby91/moteur3d/issues/50) ; [#66, fresques CPU](https://github.com/glaby91/moteur3d/issues/66).

**Conclusion :** ne pas considérer une fonction codée comme une optimisation. Pour le moteur interactif, les seules performances qui comptent sont celles **du même GPU**, **de la même scène**, **de la même image** et **du FPS réellement présenté**. Le meilleur axe de recherche restant est le coût du Raster HD620 après Z-prepass, pas la puissance brute du CPU.