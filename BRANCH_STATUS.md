# État des branches — moteur CSE WARP

## Référence principale — V17.031

Promotion demandée après amélioration visuelle de V17.030 (L03 partiellement résolu), [CI V17.030 réussie](https://github.com/glaby91/moteur3d/actions/runs/46064723180). V17.031 ajoute le profil Ultra/LumaTrace et la caméra piscine V17.634. [Build 4 réussie](https://github.com/glaby91/moteur3d/actions/runs/46068889675), démarrage V17.031 confirmé par l'utilisateur. [Commit compilé et validé](https://github.com/glaby91/moteur3d/commit/64ecbb19c14d2d2ff12a92ab79890b686247551f). Les performances et anomalies L03–L05 restent à mesurer. Les correctifs graphiques V17.030 sont repris sans modification. La branche `feature/v17-zmaterial-001` conserve la V17.030 testée. Les révisions DX11 018/009, Vulkan 010, LumaTrace 010/003, Scene Runtime 003 et Z Material 001 sont enregistrées.

[Audit rendu V17.031 et notation des backends](docs/audits/AUDIT_RENDU_V17_031_2026-11-27.md) : propositions uniquement, aucun changement de moteur.

## Portage macOS expérimental — 28 novembre 2026

Historique de la branche `test/macos-metal-vulkan` : V17.031 Build 4, [source validée](https://github.com/glaby91/moteur3d/commit/a49db9a2e21e937c16c959c87e3fad11cd722e9b), [CI réussie](https://github.com/glaby91/moteur3d/actions/runs/46094113757), scène V17.634. Vulkan Raster 011 / Resources 003, API GPU 1.3.357, MoltenVK 1.4.2. Validation hors écran, pas encore de vraie fenêtre ni de parité visuelle. [Documentation](v17/docs/MACOS_VULKAN.md). Intégration dans `master` demandée explicitement le 28 novembre 2026 ; tests de non-régression de la fusion à exécuter.

Optimisation 001 : [Build 7 réussi](https://github.com/glaby91/moteur3d/actions/runs/46095834402), [source](https://github.com/glaby91/moteur3d/commit/bd7e48b5190802ac3a1176b25f8241d668795bda), Raster 012 / Resources 003. 384 images mesurées, 288/288 optimisées identiques, 1 508 → 36 draws statiques. [Résultats et données](v17/docs/MACOS_VULKAN_OPT_001.md). Options non activées par défaut ; Build 4 reste la référence de portage.

## Dernière référence L01/L02 validée — 27 novembre 2026

**V17.029**, validée visuellement par l'utilisateur sur DX11 Raster, Vulkan et LumaTrace. [Build Windows 5 réussie](https://github.com/glaby91/moteur3d/actions/runs/46056898833) · [Commit source testé](https://github.com/glaby91/moteur3d/commit/f8eebe88a7e7207c47065bc89d077140b2556a7d). La branche principale `master` reçoit le code exact de cette version, avec les mêmes tests CI et les scripts de lancement.

- L01 (anneaux concentriques) et L02 (éclairage triangulaire) : résolues sur les trois backends. [Registre](v17/docs/L01_SOLAR_PCF_V17029.md).
- DX11 R001, Z Atmosphere 002, Z Reflect, préparation géométrique et outils de benchmark conservés.
- La V17.025 de master était une tentative sur base ancienne V17.024, supersédée par V17.029. Aucun changement de scène ; la scène conserve son dépôt et sa version.
- Les performances HD620 de V17.029 restent à mesurer.

## Règle de développement anti-régression

Avant tout changement, identifier la version effectivement validée, sa branche, son commit et sa build, comparer les branches, préserver les fonctionnalités cumulées, puis vérifier le diff, les versions et les tests. Une compilation réussie n'est pas une validation graphique. Ne pas repartir d'une branche historique simplement parce que son nom paraît officiel.

| Branche | Rôle |
|---|---|
| `master` | Référence principale V17.031 ; portage macOS ARM64 intégré, optimisations Apple optionnelles ; repartir de cette tête après contrôle du registre et validation. |
| `test/macos-metal-vulkan` | Historique du portage et du benchmark macOS Build 7. |
| `feature/v17-zmaterial-001` | V17.030, CI réussie et amélioration L03 constatée ; historique. |
| `feature/v17-dx11-r001` | Source de la V17.029 validée ; historique de R001 et Z Atmosphere. |
| `official/v17-lumatrace` | Référence historique, ne pas supposer à jour. |
| `main` dans moteur3d-scene | Dépôt distinct de la scène. |

## Historique précédent

# État des branches — moteur CSE WARP

État vérifié le 26 novembre 2026 après la compilation Windows de la V17.009.

## Repère immédiat

Le moteur V17.009 et son paquet Windows avec la scène de la piscine V17.629 proviennent de la branche principale [`master`](https://github.com/glaby91/moteur3d/tree/master). [Télécharger la build 759](https://github.com/glaby91/moteur3d/actions/runs/45736640875/artifacts/15080081395)

La V17.009 réunit la préparation des triangles, les commandes d’exploration et le benchmark de parcours. [PR #90](https://github.com/glaby91/moteur3d/pull/90) · [Fusion sur `master`](https://github.com/glaby91/moteur3d/commit/c83dd1e482826e604bee324e0c53018f5745f478)

**Base moteur :** [`codex/scene-updater-background`](https://github.com/glaby91/moteur3d/tree/codex/scene-updater-background), qui contient le moteur modulaire, les backends, les contrôles d’exploration et la mise à jour de scène en arrière-plan.

La branche `codex/v17-engine-benchmarks-readme` est conservée comme historique de la livraison, mais ses changements sont maintenant intégrés dans `master`. Pour poursuivre le développement principal, partir de `master` ou d’une branche dédiée créée depuis cette tête.

## Branches à distinguer

| Branche | Rôle observé | Utilisation |
|---|---|---|
| `master` | Branche principale du moteur | V17.009 avec scène V17.629, préparation des triangles, saut, accroupissement et Pip-Boy. |
| `perf/v17-static-geometry-prepare-261125` | Branche du benchmark de préparation des triangles | V17.002, build 754. La PR #90 intègre cette optimisation dans le moteur principal. |
| `codex/v17-engine-benchmarks-readme` | Branche de la [PR #88](https://github.com/glaby91/moteur3d/pull/88), fusionnée | Historique de la livraison V17.006 avec la scène V17.623. |
| `codex/scene-updater-background` | Base moteur de cette série | Architecture V17 modulaire et gestionnaire de scène. |
| `official/v17-lumatrace` | Branche nommée comme référence officielle V17 + LumaTrace | Référence historique à consulter ; ne pas supposer qu’elle contient les commits les plus récents. |
| `main` dans `moteur3d-scene` | Branche par défaut du dépôt de scène | Source de la scène de la piscine V17.629. |

Les branches `feature/*`, `codex/*` et versions historiques restent disponibles. Leur nom seul ne suffit pas à indiquer la branche à compiler : vérifier la PR et son statut avant de reprendre le travail.
