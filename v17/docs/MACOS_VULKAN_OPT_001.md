# Optimisation Vulkan macOS 001 — résultats Build 7

**28 novembre 2026 — CSE WARP V17.031, Vulkan Raster 012 / Resources 003, scène V17.634.** CI réussie sur GPU Apple paravirtualisé via MoltenVK, macOS ARM64, hors écran, MSAA 1×. Validation historique sur `test/macos-metal-vulkan`, intégration dans `master` demandée le 28 novembre 2026.

[Source testée](https://github.com/glaby91/moteur3d/commit/bd7e48b5190802ac3a1176b25f8241d668795bda) · [Build 7](https://github.com/glaby91/moteur3d/actions/runs/46095834402) · [Package](https://github.com/glaby91/moteur3d/actions/runs/46095834402/artifacts/15243449476) · [Diagnostics, mesures par image, CSV et PNG](https://github.com/glaby91/moteur3d/actions/runs/46095834402/artifacts/15243835709).

## Méthode

Quatre profils : référence (RGBA, sans fusion), merge (fusion des plages contiguës visibles), BGRA, combiné (merge + BGRA). Aucune modification de shader, texture, Z-prepass, éclairage ni résolution. L'option Apple `CSE_WARP_MAC_MERGE_DRAWS=1` calcule la visibilité une fois par frame et réunit les plages contiguës dans chaque catégorie géométrique, tout en conservant l'ordre et les décisions de culling. `CSE_WARP_VK_BGRA_READBACK=1` utilise le format BGRA compatible avec Qt ARGB32.

16 sessions indépendantes (4 profils × 2 résolutions × 2 répétitions, ordre inversé au second passage), 3 warmups et 24 images mesurées par session (12 fixes, 12 avec déplacement latéral). 384 images mesurées, 48 warmups supplémentaires. Éclairage fixé à 14 h, scène V17.634 vérifiée SHA-256. FPS équivalents = 1000 / médiane du temps de rendu mesuré, pas des FPS mesurés en présentation GUI continue. Tous les tests de non-régression et les 16 sessions passent.

## Résultats

| Résolution / phase | Référence ms | Merge ms | BGRA ms | Combiné ms | FPS équiv. réf. → combiné | Gain FPS | P95 réf. → combiné (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|
| 640x360 fixe | 16,89 | 19,36 | 19,31 | 14,85 | 59,22 → 67,35 | +13,72 % | 20,25 → 17,09 |
| 640x360 mobile | 17,54 | 17,42 | 18,14 | 15,23 | 57,03 → 65,68 | +15,17 % | 20,55 → 17,80 |
| 1920x1080 fixe | 24,66 | 23,83 | 22,46 | 22,40 | 40,55 → 44,65 | +10,10 % | 26,93 → 25,74 |
| 1920x1080 mobile | 23,34 | 22,85 | 22,28 | 22,22 | 42,85 → 45,01 | +5,04 % | 25,11 → 23,60 |

- Appels de dessin médians : 1 508 → 36 en statique (-97,61 %), 1 506 → 38 en mouvement (-97,48 %).
- Les 288 images des profils optimisés sont identiques aux références correspondantes (hash SHA-256 des pixels ARGB32). 96 images de référence sont comptées séparément. Parité A/B uniquement, non validation visuelle absolue CPU/DX11.
- En Full HD mobile combiné : médiane fence 20,3190835 ms, soumission 0,13575 ms, conversion/lecture CPU 0,3294375 ms. Attente GPU dominante ; ces médianes ne constituent pas une décomposition additive exacte.
- Les options seules n'améliorent pas toutes les situations ; le profil combiné est le meilleur en médiane dans les quatre cas. BGRA seul a toutefois un P95 inférieur au combiné en Full HD fixe. Deux répétitions sur GPU virtuel ne suffisent pas à garantir une amélioration sur tous les Macs.

## Données pérennes

- [Synthèse JSON exacte avec provenance](../../benchmarks/macos-vulkan-012/build7-summary.json)
- [Synthèse CSV des 16 cellules expérimentales](../../benchmarks/macos-vulkan-012/build7-summary.csv)
- [Artefact original complet](https://github.com/glaby91/moteur3d/actions/runs/46095834402/artifacts/15243835709) : mesures par image, télémétrie et rendus ; rétention GitHub Actions limitée.

## Reproduction

Dans le package ARM64 avec Qt et SDK Vulkan configurés :

```bash
export CSE_WARP_MAC_MERGE_DRAWS=1
export CSE_WARP_VK_BGRA_READBACK=1
./lancer.sh benchmark-civic/scene.json
```

Remettre les deux options à 0 pour la référence. Les options restent désactivées par défaut. Le package utilise QImage et un readback synchrone. La suite : présentation native, synchronisation asynchrone, test physique, MSAA supérieur, parcours plus long et vérification de la vraie GI. Aucune réduction de qualité ; options toujours facultatives après intégration à master.
