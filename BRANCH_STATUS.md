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
