# État des branches — moteur CSE WARP

État vérifié le 24 novembre 2026 après la fusion de la livraison V17.006. Les branches historiques et expérimentales restent conservées.

## Repère immédiat

**Branche de référence actuelle :** [`master`](https://github.com/glaby91/moteur3d/tree/master), avec le moteur V17.006, les benchmarks documentés et la scène V17.623 intégrée au paquet Windows.

**Livraison fusionnée :** [PR #88 — V17.006, benchmarks, documentation et scène V17.623](https://github.com/glaby91/moteur3d/pull/88), commit `c82adfa`.

**Base moteur :** [`codex/scene-updater-background`](https://github.com/glaby91/moteur3d/tree/codex/scene-updater-background), qui contient le moteur modulaire, les backends, les contrôles d’exploration et la mise à jour de scène en arrière-plan.

La branche `codex/v17-engine-benchmarks-readme` est conservée comme historique de la livraison, mais ses changements sont maintenant intégrés dans `master`. Pour poursuivre le développement principal, partir de `master` ou d’une branche dédiée créée depuis cette tête.

## Branches à distinguer

| Branche | Rôle observé | Utilisation |
|---|---|---|
| `master` | Branche par défaut GitHub | Référence actuelle : V17.006, benchmarks étendus et intégration de la scène V17.623 dans le package moteur. |
| `codex/v17-engine-benchmarks-readme` | Branche de la PR #88, fusionnée | Historique de la livraison V17.006 + scène V17.623 ; ne constitue plus une candidate en attente. |
| `codex/scene-updater-background` | Base moteur de cette série | Architecture V17 modulaire et gestionnaire de scène. |
| `official/v17-lumatrace` | Branche nommée comme référence officielle V17 + LumaTrace | Référence historique à consulter ; ne pas supposer qu’elle contient les commits les plus récents. |
| `main` dans `moteur3d-scene` | Branche par défaut du dépôt de scène | Source des scènes publiées, dont la release V17.623. |

Les branches `feature/*`, `codex/*` et versions historiques restent disponibles. Leur nom seul ne suffit pas à indiquer la branche à compiler : vérifier la PR et son statut avant de reprendre le travail.
